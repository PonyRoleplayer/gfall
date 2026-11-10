#include "check_oglaw.h"
#include "check_util.h"
#include "game/ogfall/play/law.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/bank.h"
#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/combat.h"
#include "engine/core/mem.h"
#include "../apps/ogfall/seam.h"
#include <stdio.h>
#include <string.h>

static void set_legal(int region, int legal)
{
    og_region_add_legal(region, legal - og_region_legal(region));
}

static df_u32 letter(int value)
{
    df_u32 id;

    id = og_item_create(og_state()->inventory_id, 27, 2, 0);
    og_object_edit(id)->value = value;
    return id;
}

void check_oglaw(void)
{
    OgCourt court;
    OgCourtStep step;
    OgCourtGuilds none;
    OgCourtGuilds guilds;
    int i, ok, severe, before, region = 17;

    none.brotherhood_rank = none.thieves_rank = -1;
    guilds.brotherhood_rank = guilds.thieves_rank = 19;

    /* FALL.EXE's tables: the losses by crime id, a fine row's four numbers, the names. */
    check_expect(og_crime_loss(4) == 8 && og_crime_loss(5) == 20 && og_crime_loss(11) == 75 && og_crime_loss(0) == 0, "law: the reputation losses are not FALL.EXE's");
    check_expect(!strcmp(og_crime_name(4), "Assault") && !strcmp(og_crime_name(5), "Murder") && !strcmp(og_crime_name(1), "Attempted Breaking and Entering"), "law: the crime names");

    /* Assault at no standing: the fine row 6 x 1000, 10..1500, so 1000 and 25 coins each of 40 gold or 3 days; the sums hold. */
    og_character()->gold = 100000;
    set_legal(region, 0);
    ok = 1;
    for (i = 0; i < 60; i++) {
        og_court_open(&court, OG_CRIME_ASSAULT, region, &none, &step);
        ok = ok && court.severity == 2 && court.fine % 40 == 0 && court.days % 3 == 0 && court.fine / 40 + court.days / 3 == 25 && step.text[0] == OG_TEXT_COURT_ASK;
    }
    check_expect(ok, "law: an assault at no standing is not 25 coins of 40 gold or 3 days");
    /* The standing raises the fine either way: -50 is 1000 + 50 * 6 = 1300 (32 coins); +20 is 1120 (28 coins). Murder is 10000 (250 coins). */
    set_legal(region, -50);
    og_court_open(&court, OG_CRIME_ASSAULT, region, &none, &step);
    check_expect(court.fine / 40 + court.days / 3 == 32, "law: an assault at -50 is not a fine of 1300");
    set_legal(region, 20);
    og_court_open(&court, OG_CRIME_ASSAULT, region, &none, &step);
    check_expect(court.fine / 40 + court.days / 3 == 28, "law: an assault at +20 is not a fine of 1120");
    set_legal(region, 0);
    og_court_open(&court, OG_CRIME_MURDER, region, &none, &step);
    check_expect(court.fine / 40 + court.days / 3 == 250, "law: murder is not a fine of 10000");
    /* Smuggling's row has a most of 0: it is no fine at all (the table's own clamp). */
    og_court_open(&court, OG_CRIME_SMUGGLING, region, &none, &step);
    check_expect(court.fine == 0 && court.days == 0, "law: smuggling's row does not clamp to nothing");
    check_expect(!og_court_open(&court, 0, region, &none, &step), "law: no crime opened a court");

    /* Short of gold: what cannot be paid is a day for each forty, and the fine is what there is. */
    og_character()->gold = 200;
    set_legal(region, 0);
    og_court_open(&court, OG_CRIME_ASSAULT, region, &none, &step);
    check_expect(court.fine <= 200 && court.fine / 40 + court.days / 3 >= 0 && court.fine == (court.fine > 200 ? 200 : court.fine), "law: a short purse was not the fine");
    check_expect(court.fine == 200 || court.fine % 40 == 0, "law: a short purse's fine");

    /* The harsher the standing the likelier a banishment (severity 0): at -100 it is seven in eight. */
    og_character()->gold = 100000;
    set_legal(region, -100);
    severe = 0;
    for (i = 0; i < 400; i++) {
        og_court_open(&court, OG_CRIME_ASSAULT, region, &none, &step);
        severe += court.severity == 0;
    }
    check_expect(severe > 300 && severe < 390, "law: at -100 a banishment is not about seven times in eight");
    set_legal(region, 0);

    /* Accepting a sentence of severity 2: half the fine and the days; the gold is taken and the sentence's reputation given back (3 for assault). */
    og_court_open(&court, OG_CRIME_ASSAULT, region, &none, &step);
    {
        int fine, days, gold;

        fine = court.fine >> 1;
        days = court.days >> 1;
        gold = og_character()->gold;
        og_court_accept(&court, &step);
        check_expect(step.gold_taken == fine && og_character()->gold == gold - fine, "law: accepting did not take half the fine");
        check_expect(step.serve_days == days && step.release == OG_RELEASE_STREET && step.done && !court.open, "law: accepting did not serve half the days and set free in the street");
        check_expect(og_region_legal(region) == 3 + (days ? 3 : 0), "law: the sentence did not give back 3 (twice, when days are served: the original's state 3 restores again)");
    }
    set_legal(region, 0);
    /* Not enough gold to pay: every coin and letter goes, days are added, texts 8052 then 8055. */
    og_character()->gold = 10;
    letter(5);
    court.open = 1;
    court.region = region;
    court.crime = 3;
    court.change = 4;
    court.severity = 2;
    court.fine = 400;
    court.days = 6;
    og_court_accept(&court, &step);
    check_expect(og_gold_total() == 0 && step.text[0] == OG_TEXT_COURT_SHORT && step.text[1] == OG_TEXT_COURT_SENTENCE && step.serve_days == 3 + ((200 - 15) / 40 + 1),
                 "law: an unpayable fine did not take everything and add a day for each forty short");

    /* Severity 0 and 1: the punishment flags and where they are put. */
    set_legal(region, 0);
    court.open = 1; court.region = region; court.severity = 0; court.fine = 40; court.days = 3; court.change = 4;
    og_court_accept(&court, &step);
    check_expect(step.text[0] == OG_TEXT_COURT_BANISHED && step.flag == 1 && (og_law()->punishment[region] & 1) && step.release == OG_RELEASE_STREET, "law: a banishment");
    court.open = 1; court.severity = 1;
    og_court_accept(&court, &step);
    check_expect(step.text[0] == OG_TEXT_COURT_DEATH && step.flag == 2 && (og_law()->punishment[region] & 2) && step.release == OG_RELEASE_BANISHED, "law: a death sentence");

    /* A plea: Personality and the skill over two, plus the standing, 5 to 95; the best plea at no standing is acquitted nine times in ten. */
    ok = 0;
    for (i = 0; i < 200; i++) {
        og_character()->gold = 100000;
        set_legal(region, 0);
        og_court_open(&court, OG_CRIME_ASSAULT, region, &none, &step);
        og_court_plead(&court, 0, 100, 100, &step);
        ok += step.text[0] == OG_TEXT_COURT_ACQUITTED && step.release == OG_RELEASE_ACQUITTED;
    }
    check_expect(ok > 170 && ok < 200, "law: a plea of 100 and 100 was not acquitted about nineteen in twenty");
    ok = 0;
    for (i = 0; i < 200; i++) {
        set_legal(region, 0);
        og_court_open(&court, OG_CRIME_ASSAULT, region, &none, &step);
        og_court_plead(&court, 1, 0, 0, &step);
        ok += step.text[0] == OG_TEXT_COURT_ACQUITTED;
    }
    check_expect(ok < 40, "law: a plea of 0 and 0 was acquitted too often");

    /* A guild may clear it at once: the Dark Brotherhood murder and assault, the Thieves Guild breaking, trespass and pickpocketing. */
    og_court_open(&court, OG_CRIME_MURDER, region, &guilds, &step);
    check_expect(step.done && step.text[0] == OG_TEXT_GUILD_BROTHERHOOD && !court.open, "law: the Brotherhood did not clear a murder");
    og_court_open(&court, OG_CRIME_PICKPOCKETING, region, &guilds, &step);
    check_expect(step.done && step.text[0] == OG_TEXT_GUILD_THIEVES, "law: the Thieves Guild did not clear a pickpocketing");
    og_court_open(&court, OG_CRIME_THEFT, region, &guilds, &step);
    check_expect(!step.done && step.text[0] == OG_TEXT_COURT_ASK, "law: a guild cleared a theft");

    /* The penalty when a guard strikes: the loss off the standing. */
    set_legal(region, 0);
    og_crime_penalty(region, OG_CRIME_ASSAULT);
    check_expect(og_region_legal(region) == -8, "law: a guard's blow for assault did not cost 8");
    set_legal(region, 0);

    /* The purse: a letter that covers it is lessened; else the letters in order, then the coins. */
    {
        df_u32 a, b;

        og_character()->gold = 50;
        a = letter(100);
        b = letter(30);
        before = og_gold_total();
        check_expect(before == 180, "law: the purse's total");
        og_gold_spend(120);
        check_expect(og_gold_total() == 60 && !og_object(a) && og_object(b) && og_object(b)->value == 10 && og_character()->gold == 50,
                     "law: 120 from letters of 100 and 30 did not use up the first and lessen the second");
        og_gold_spend(40);
        check_expect(og_character()->gold == 10 && og_object(b)->value == 10, "law: coins cover a spend first");
    }
    /* The decay at each 161280 minutes (not while a sentence is served), the crime noted by a caller. */
    set_legal(region, 5);
    og_state_pass_minutes(161280u - og_state()->minutes % 161280u);
    check_expect(og_region_legal(region) == 4, "law: the legal standing did not drift one point at 161280 minutes");
    og_law_serving(1);
    og_state_pass_minutes(161280u);
    og_law_serving(0);
    check_expect(og_region_legal(region) == 4, "law: the legal standing drifted while a sentence was served");
    og_law_note(OG_CRIME_VAGRANCY);
    check_expect(og_law()->crime == OG_CRIME_VAGRANCY, "law: the crime was not noted");
    check_expect(og_law_valid(og_law()), "law: the state is not valid after use");
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
    snprintf(path, sizeof(path), "build/ogfall-%s.ppm", name);
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
    OgIn in;

    mem_zero(&in, sizeof(in));
    in.cx = cx;
    in.cy = cy;
    in.on_picture = 1;
    in.mouse_pressed = 1;
    og_possessions_frame(&in);
}

void check_oglaw_guards(void)
{
    static const OgGuardWorld world = { fake_present, fake_count, fake_spawn, fake_witnesses, fake_dungeon, 0, 0 };
    const OgPossessions *view;
    OgCourt court;
    OgCourtStep step;
    OgCourtGuilds none;
    const int region = 17;
    int made, i, minutes, gold, steps;

    none.brotherhood_rank = none.thieves_rank = -1;
    og_guards_set_world(&world);
    og_guards_reset();

    /* Summoning: 2 to 5 guards made at once; nothing in a dungeon or past ten creatures; a witness starts a timer of 5 to 10 seconds. */
    g_present = g_dungeon = g_bits = g_count = g_spawned = 0;
    made = og_guards_summon(1);
    check_expect(made >= 2 && made <= 5 && g_spawned == made, "guards: summoning did not make two to five");
    g_spawned = 0;
    g_dungeon = 1;
    check_expect(og_guards_summon(1) == 0 && g_spawned == 0, "guards: the watch came in a dungeon");
    g_dungeon = 0;
    g_count = 11;
    check_expect(og_guards_summon(1) == 0, "guards: the watch came past ten creatures");
    g_count = 0;
    g_bits = 0;
    og_guards_summon(0);
    check_expect(og_guards_timer() < 0.0f, "guards: a crime no one saw started the timer");
    g_bits = 1;
    og_guards_summon(0);
    check_expect(og_guards_timer() >= 5.0f && og_guards_timer() <= 10.0f, "guards: a witness did not start a timer of 5 to 10 seconds");
    g_spawned = 0;
    for (i = 0; i < 12 && og_guards_timer() >= 0.0f; i++) {
        og_guards_tick(1.0f);
    }
    check_expect(og_guards_timer() < 0.0f && g_spawned >= 2, "guards: the timer ran out and brought no watch");
    g_bits = 2;
    g_spawned = 0;
    og_guards_summon(0);
    check_expect(g_spawned >= 2, "guards: a guard's witness did not call the watch at once");

    /* The first blow with guards about is the offer to surrender, once; the crime's loss is taken then. */
    og_law_note(OG_CRIME_ASSAULT);
    set_legal(region, 0);
    g_present = 1;
    check_expect(og_law_blow(region) == OG_BLOW_ASK && og_region_legal(region) == -8, "guards: the first blow did not ask, at a cost of 8");
    check_expect(og_law_blow(region) == OG_BLOW_FREE, "guards: a second blow asked again");
    g_present = 0;
    og_law_calm();
    g_present = 1;
    check_expect(og_law_blow(region) == OG_BLOW_ASK, "guards: the question did not come again after the guards were gone");
    og_law_calm();

    /* Brought down or surrendered: below -20 and not surrendered is death; surrendered is the court. */
    set_legal(region, -50);
    check_expect(og_law_guards_or_court(0, region, &none, &court, &step) == OG_ARREST_DEATH && og_character()->health == 1, "guards: -50 and no surrender was not death at health 1");
    check_expect(og_law_guards_or_court(1, region, &none, &court, &step) == OG_ARREST_COURT && court.open, "guards: a surrender at -50 did not open the court");
    set_legal(region, 5);
    check_expect(og_law_guards_or_court(0, region, &none, &court, &step) == OG_ARREST_COURT, "guards: a good standing was not tried");

    /* The screen: a blow asks, Yes sits the court, Guilty is accepted through its messages, and the court sets the player free. */
    og_possessions_reset();
    og_possessions_set_court(0, fake_release);
    og_guards_reset();
    set_legal(region, 0);
    og_law_note(OG_CRIME_ASSAULT);
    og_character()->gold = 5000;
    og_character()->max_health = 30;
    og_character()->health = 30;
    g_present = 1;
    view = og_possessions();
    og_damage_player(5, -1);
    check_expect(view->page == OG_VIEW_COURT && view->prompt == OG_PROMPT_ASK && og_character()->health == 30 && view->offer[0], "court: the first blow did not ask to surrender, holding the blow back");
    shot("court-surrender");
    click(117, 125);
    check_expect(view->court.open && view->prompt == OG_PROMPT_ASK, "court: surrendering did not put the question of guilt");
    shot("court-ask");
    gold = og_character()->gold;
    minutes = (int)og_state()->minutes;
    click(117, 125);
    /* Messages follow: a click goes on through each; the court ends by setting the player free and the page closed. */
    for (steps = 0; steps < 8 && view->page == OG_VIEW_COURT; steps++) {
        if (steps == 0) { shot("court-sentence"); }
        click(160, 100);
    }
    check_expect(view->page == OG_VIEW_NONE && g_released == OG_RELEASE_STREET, "court: the sentence did not end with the player set free in the street");
    check_expect(og_character()->gold < gold && og_character()->health == og_character()->max_health, "court: the fine was not taken, or vitals not restored");
    check_expect((int)og_state()->minutes >= minutes + 240, "court: no time passed (the court's 240 minutes at least)");
    og_guards_set_world(0);
    og_guards_reset();
    og_possessions_set_court(0, 0);
}
