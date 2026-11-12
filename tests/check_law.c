#include "check_law.h"
#include "check_util.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/combat.h"
#include "engine/core/mem.h"
#include "../apps/gfall/seam.h"
#include <stdio.h>
#include <string.h>

static void set_legal(int region, int legal)
{
    region_add_legal(region, legal - region_legal(region));
}

static df_u32 letter(int value)
{
    df_u32 id;

    id = item_create(game_state()->inventory_id, 27, 2, 0);
    object_edit(id)->value = value;
    return id;
}

void check_law(void)
{
    Court court;
    CourtStep step;
    CourtGuilds none;
    CourtGuilds guilds;
    int i, ok, severe, before, region = 17;

    none.brotherhood_rank = none.thieves_rank = -1;
    guilds.brotherhood_rank = guilds.thieves_rank = 19;

    /* FALL.EXE's tables: the losses by crime id, a fine row's four numbers, the names. */
    check_expect(crime_loss(4) == 8 && crime_loss(5) == 20 && crime_loss(11) == 75 && crime_loss(0) == 0, "law: the reputation losses are not FALL.EXE's");
    check_expect(!strcmp(crime_name(4), "Assault") && !strcmp(crime_name(5), "Murder") && !strcmp(crime_name(1), "Attempted Breaking and Entering"), "law: the crime names");

    /* Assault at no standing: the fine row 6 x 1000, 10..1500, so 1000 and 25 coins each of 40 gold or 3 days; the sums hold. */
    character_state()->gold = 100000;
    set_legal(region, 0);
    ok = 1;
    for (i = 0; i < 60; i++) {
        court_open(&court, CRIME_ASSAULT, region, &none, &step);
        ok = ok && court.severity == 2 && court.fine % 40 == 0 && court.days % 3 == 0 && court.fine / 40 + court.days / 3 == 25 && step.text[0] == TEXT_COURT_ASK;
    }
    check_expect(ok, "law: an assault at no standing is not 25 coins of 40 gold or 3 days");
    /* The standing raises the fine either way: -50 is 1000 + 50 * 6 = 1300 (32 coins); +20 is 1120 (28 coins). Murder is 10000 (250 coins). */
    set_legal(region, -50);
    court_open(&court, CRIME_ASSAULT, region, &none, &step);
    check_expect(court.fine / 40 + court.days / 3 == 32, "law: an assault at -50 is not a fine of 1300");
    set_legal(region, 20);
    court_open(&court, CRIME_ASSAULT, region, &none, &step);
    check_expect(court.fine / 40 + court.days / 3 == 28, "law: an assault at +20 is not a fine of 1120");
    set_legal(region, 0);
    court_open(&court, CRIME_MURDER, region, &none, &step);
    check_expect(court.fine / 40 + court.days / 3 == 250, "law: murder is not a fine of 10000");
    /* Smuggling's row has a most of 0: it is no fine at all (the table's own clamp). */
    court_open(&court, CRIME_SMUGGLING, region, &none, &step);
    check_expect(court.fine == 0 && court.days == 0, "law: smuggling's row does not clamp to nothing");
    check_expect(!court_open(&court, 0, region, &none, &step), "law: no crime opened a court");

    /* Short of gold: what cannot be paid is a day for each forty, and the fine is what there is. */
    character_state()->gold = 200;
    set_legal(region, 0);
    court_open(&court, CRIME_ASSAULT, region, &none, &step);
    check_expect(court.fine <= 200 && court.fine / 40 + court.days / 3 >= 0 && court.fine == (court.fine > 200 ? 200 : court.fine), "law: a short purse was not the fine");
    check_expect(court.fine == 200 || court.fine % 40 == 0, "law: a short purse's fine");

    /* The harsher the standing the likelier a banishment (severity 0): at -100 it is seven in eight. */
    character_state()->gold = 100000;
    set_legal(region, -100);
    severe = 0;
    for (i = 0; i < 400; i++) {
        court_open(&court, CRIME_ASSAULT, region, &none, &step);
        severe += court.severity == 0;
    }
    check_expect(severe > 300 && severe < 390, "law: at -100 a banishment is not about seven times in eight");
    set_legal(region, 0);

    /* Accepting a sentence of severity 2: half the fine and the days; the gold is taken and the sentence's reputation given back (3 for assault). */
    court_open(&court, CRIME_ASSAULT, region, &none, &step);
    {
        int fine, days, gold;

        fine = court.fine >> 1;
        days = court.days >> 1;
        gold = character_state()->gold;
        court_accept(&court, &step);
        check_expect(step.gold_taken == fine && character_state()->gold == gold - fine, "law: accepting did not take half the fine");
        check_expect(step.serve_days == days && step.release == RELEASE_STREET && step.done && !court.open, "law: accepting did not serve half the days and set free in the street");
        check_expect(region_legal(region) == 3 + (days ? 3 : 0), "law: the sentence did not give back 3 (twice, when days are served: the original's state 3 restores again)");
    }
    set_legal(region, 0);
    /* Not enough gold to pay: every coin and letter goes, days are added, texts 8052 then 8055. */
    character_state()->gold = 10;
    letter(5);
    court.open = 1;
    court.region = region;
    court.crime = 3;
    court.change = 4;
    court.severity = 2;
    court.fine = 400;
    court.days = 6;
    court_accept(&court, &step);
    check_expect(gold_total() == 0 && step.text[0] == TEXT_COURT_SHORT && step.text[1] == TEXT_COURT_SENTENCE && step.serve_days == 3 + ((200 - 15) / 40 + 1),
                 "law: an unpayable fine did not take everything and add a day for each forty short");

    /* Severity 0 and 1: the punishment flags and where they are put. */
    set_legal(region, 0);
    court.open = 1; court.region = region; court.severity = 0; court.fine = 40; court.days = 3; court.change = 4;
    court_accept(&court, &step);
    check_expect(step.text[0] == TEXT_COURT_BANISHED && step.flag == 1 && (law_state()->punishment[region] & 1) && step.release == RELEASE_STREET, "law: a banishment");
    court.open = 1; court.severity = 1;
    court_accept(&court, &step);
    check_expect(step.text[0] == TEXT_COURT_DEATH && step.flag == 2 && (law_state()->punishment[region] & 2) && step.release == RELEASE_BANISHED, "law: a death sentence");

    /* A plea: Personality and the skill over two, plus the standing, 5 to 95; the best plea at no standing is acquitted nine times in ten. */
    ok = 0;
    for (i = 0; i < 200; i++) {
        character_state()->gold = 100000;
        set_legal(region, 0);
        court_open(&court, CRIME_ASSAULT, region, &none, &step);
        court_plead(&court, 0, 100, 100, &step);
        ok += step.text[0] == TEXT_COURT_ACQUITTED && step.release == RELEASE_ACQUITTED;
    }
    check_expect(ok > 170 && ok < 200, "law: a plea of 100 and 100 was not acquitted about nineteen in twenty");
    ok = 0;
    for (i = 0; i < 200; i++) {
        set_legal(region, 0);
        court_open(&court, CRIME_ASSAULT, region, &none, &step);
        court_plead(&court, 1, 0, 0, &step);
        ok += step.text[0] == TEXT_COURT_ACQUITTED;
    }
    check_expect(ok < 40, "law: a plea of 0 and 0 was acquitted too often");

    /* A guild may clear it at once: the Dark Brotherhood murder and assault, the Thieves Guild breaking, trespass and pickpocketing. */
    court_open(&court, CRIME_MURDER, region, &guilds, &step);
    check_expect(step.done && step.text[0] == TEXT_GUILD_BROTHERHOOD && !court.open, "law: the Brotherhood did not clear a murder");
    court_open(&court, CRIME_PICKPOCKETING, region, &guilds, &step);
    check_expect(step.done && step.text[0] == TEXT_GUILD_THIEVES, "law: the Thieves Guild did not clear a pickpocketing");
    court_open(&court, CRIME_THEFT, region, &guilds, &step);
    check_expect(!step.done && step.text[0] == TEXT_COURT_ASK, "law: a guild cleared a theft");

    /* The penalty when a guard strikes: the loss off the standing. */
    set_legal(region, 0);
    crime_penalty(region, CRIME_ASSAULT);
    check_expect(region_legal(region) == -8, "law: a guard's blow for assault did not cost 8");
    set_legal(region, 0);

    /* The purse: a letter that covers it is lessened; else the letters in order, then the coins. */
    {
        df_u32 a, b;

        character_state()->gold = 50;
        a = letter(100);
        b = letter(30);
        before = gold_total();
        check_expect(before == 180, "law: the purse's total");
        gold_spend(120);
        check_expect(gold_total() == 60 && !object_at(a) && object_at(b) && object_at(b)->value == 10 && character_state()->gold == 50,
                     "law: 120 from letters of 100 and 30 did not use up the first and lessen the second");
        gold_spend(40);
        check_expect(character_state()->gold == 10 && object_at(b)->value == 10, "law: coins cover a spend first");
    }
    /* The decay at each 161280 minutes (not while a sentence is served), the crime noted by a caller. */
    set_legal(region, 5);
    state_pass_minutes(161280u - game_state()->minutes % 161280u);
    check_expect(region_legal(region) == 4, "law: the legal standing did not drift one point at 161280 minutes");
    law_serving(1);
    state_pass_minutes(161280u);
    law_serving(0);
    check_expect(region_legal(region) == 4, "law: the legal standing drifted while a sentence was served");
    law_note(CRIME_VAGRANCY);
    check_expect(law_state()->crime == CRIME_VAGRANCY, "law: the crime was not noted");
    check_expect(law_valid(law_state()), "law: the state is not valid after use");
}

/* ---- Guards, the arrest and the court screen ---- */
static int g_present, g_spawned, g_dungeon, g_bits, g_count, g_released;

static int fake_present(void) { return g_present; }
static int fake_count(void) { return g_count; }
static int fake_spawn(void) { g_spawned++; return 1; }
static int fake_witnesses(void) { return g_bits; }
static int fake_dungeon(void) { return g_dungeon; }
static void fake_release(int where) { g_released = where; }

static void shot(const char *name)
{
    const df_u8 *pixels, *palette;
    char path[96];
    FILE *file;
    int i;

    seam_present();
    pixels = seam_screen(&palette);
    snprintf(path, sizeof(path), "build/gfall-%s.ppm", name);
    file = fopen(path, "wb");
    if (!file || !pixels || !palette) {
        if (file) {
            fclose(file);
        }
        return;
    }
    fprintf(file, "P6\n320 200\n255\n");
    for (i = 0; i < 64000; i++) {
        fwrite(palette + pixels[i] * 3, 1, 3, file);
    }
    fclose(file);
}

static void click(int cx, int cy)
{
    PlayIn in;

    mem_zero(&in, sizeof(in));
    in.cx = cx;
    in.cy = cy;
    in.on_picture = 1;
    in.mouse_pressed = 1;
    possessions_frame(&in);
}

void check_law_guards(void)
{
    static const GuardWorld world = { fake_present, fake_count, fake_spawn, fake_witnesses, fake_dungeon, 0, 0 };
    const Possessions *view;
    Court court;
    CourtStep step;
    CourtGuilds none;
    const int region = 17;
    int made, i, minutes, gold, steps;

    none.brotherhood_rank = none.thieves_rank = -1;
    guards_set_world(&world);
    guards_reset();

    /* Summoning: 2 to 5 guards made at once; nothing in a dungeon or past ten creatures; a witness starts a timer of 5 to 10 seconds. */
    g_present = g_dungeon = g_bits = g_count = g_spawned = 0;
    made = guards_summon(1);
    check_expect(made >= 2 && made <= 5 && g_spawned == made, "guards: summoning did not make two to five");
    g_spawned = 0;
    g_dungeon = 1;
    check_expect(guards_summon(1) == 0 && g_spawned == 0, "guards: the watch came in a dungeon");
    g_dungeon = 0;
    g_count = 11;
    check_expect(guards_summon(1) == 0, "guards: the watch came past ten creatures");
    g_count = 0;
    g_bits = 0;
    guards_summon(0);
    check_expect(guards_timer() < 0.0f, "guards: a crime no one saw started the timer");
    g_bits = 1;
    guards_summon(0);
    check_expect(guards_timer() >= 5.0f && guards_timer() <= 10.0f, "guards: a witness did not start a timer of 5 to 10 seconds");
    g_spawned = 0;
    for (i = 0; i < 12 && guards_timer() >= 0.0f; i++) {
        guards_tick(1.0f);
    }
    check_expect(guards_timer() < 0.0f && g_spawned >= 2, "guards: the timer ran out and brought no watch");
    g_bits = 2;
    g_spawned = 0;
    guards_summon(0);
    check_expect(g_spawned >= 2, "guards: a guard's witness did not call the watch at once");

    /* The first blow with guards about is the offer to surrender, once; the crime's loss is taken then. */
    law_note(CRIME_ASSAULT);
    set_legal(region, 0);
    g_present = 1;
    check_expect(law_blow(region) == BLOW_ASK && region_legal(region) == -8, "guards: the first blow did not ask, at a cost of 8");
    check_expect(law_blow(region) == BLOW_FREE, "guards: a second blow asked again");
    g_present = 0;
    law_calm();
    g_present = 1;
    check_expect(law_blow(region) == BLOW_ASK, "guards: the question did not come again after the guards were gone");
    law_calm();

    /* Brought down or surrendered: below -20 and not surrendered is death; surrendered is the court. */
    set_legal(region, -50);
    check_expect(law_guards_or_court(0, region, &none, &court, &step) == ARREST_DEATH && character_state()->health == 1, "guards: -50 and no surrender was not death at health 1");
    check_expect(law_guards_or_court(1, region, &none, &court, &step) == ARREST_COURT && court.open, "guards: a surrender at -50 did not open the court");
    set_legal(region, 5);
    check_expect(law_guards_or_court(0, region, &none, &court, &step) == ARREST_COURT, "guards: a good standing was not tried");

    /* The screen: a blow asks, Yes sits the court, Guilty is accepted through its messages, and the court sets the player free. */
    possessions_reset();
    possessions_set_court(0, fake_release);
    guards_reset();
    set_legal(region, 0);
    law_note(CRIME_ASSAULT);
    character_state()->gold = 5000;
    character_state()->max_health = 30;
    character_state()->health = 30;
    g_present = 1;
    view = possessions_state();
    damage_player(5, -1);
    check_expect(view->page == VIEW_COURT && view->prompt == PROMPT_ASK && character_state()->health == 30 && view->offer[0], "court: the first blow did not ask to surrender, holding the blow back");
    shot("court-surrender");
    click(117, 125);
    check_expect(view->court.open && view->prompt == PROMPT_ASK, "court: surrendering did not put the question of guilt");
    shot("court-ask");
    gold = character_state()->gold;
    minutes = (int)game_state()->minutes;
    click(117, 125);
    /* Messages follow: a click goes on through each; the court ends by setting the player free and the page closed. */
    for (steps = 0; steps < 8 && view->page == VIEW_COURT; steps++) {
        if (steps == 0) { shot("court-sentence"); }
        click(160, 100);
    }
    check_expect(view->page == VIEW_NONE && g_released == RELEASE_STREET, "court: the sentence did not end with the player set free in the street");
    check_expect(character_state()->gold < gold && character_state()->health == character_state()->max_health, "court: the fine was not taken, or vitals not restored");
    check_expect((int)game_state()->minutes >= minutes + 240, "court: no time passed (the court's 240 minutes at least)");
    guards_set_world(0);
    guards_reset();
    possessions_set_court(0, 0);
}
