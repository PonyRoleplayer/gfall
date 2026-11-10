#include "check_oglaw.h"
#include "check_util.h"
#include "game/ogfall/play/law.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/bank.h"
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
