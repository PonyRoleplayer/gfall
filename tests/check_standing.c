#include <stdio.h>
#include <string.h>
#include "check_standing.h"
#include "check_util.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/play/state.h"
#include "engine/core/mem.h"
#include "game/dagger/play/people/politics.h"

/*
 * The rules of docs/ai-social-math.md ("Faction reputation") on a table small enough to work by hand:
 *     1 ---- 2            10   (an enemy of 4)       9 --- 108 --- 109
 *      `-- 3 --- 4  (4: ally 2, enemy 10)
 */
static FactionDef def(int id, int parent, int type)
{
    FactionDef d;

    mem_zero(&d, sizeof(d));
    d.id = (short)id;
    d.parent = (short)parent;
    d.type = (short)type;
    d.region = -1;
    return d;
}

static void table(void)
{
    FactionDef t[8];

    t[0] = def(1, 0, 2);
    t[1] = def(2, 1, 4);
    t[2] = def(3, 1, 4);
    t[3] = def(4, 3, 4);
    t[4] = def(10, 0, 2);
    t[5] = def(9, 0, 2);
    t[6] = def(108, 9, 2);
    t[7] = def(109, 108, 4);
    t[3].ally[0] = 2;
    t[3].enemy[0] = 10;
    check_expect(factions_define(t, 8), "standing: a small table did not define");
    standing_defaults(standing_state());
}

void check_standing(void)
{
    StandingState *s;
    int i;

    table();
    s = standing_state();
    check_expect(faction_index(4) == 3 && faction_index(77) == -1, "standing: faction indexes");
    /* A sum held in -100..100, as the original's short (over 100 stops, under -100 is 65436, which is -100). */
    faction_add_reputation(2, 250);
    check_expect(faction_rep(2) == 100, "standing: a reputation passed 100");
    faction_add_reputation(2, -500);
    check_expect(faction_rep(2) == -100, "standing: a reputation passed -100");
    standing_defaults(s);

    /* Faction 4 gains 10: the root in full, the others in its tree at half, 4 itself in full, its ally half and its enemy minus half. */
    faction_change_reputation(4, 10);
    check_expect(faction_rep(1) == 10, "standing: the root did not take the whole amount");
    check_expect(faction_rep(3) == 5 && faction_rep(2) == 5 + 5, "standing: siblings and parents did not take half (the ally takes half again)");
    check_expect(faction_rep(4) == 10, "standing: the faction itself did not take the whole amount");
    check_expect(faction_rep(10) == -5, "standing: an enemy did not lose half");
    check_expect(faction_rep(9) == 0, "standing: another tree moved");

    /* A loss halves by an arithmetic shift: minus 5 to the others is minus 3. */
    standing_defaults(s);
    faction_change_reputation(4, -5);
    check_expect(faction_rep(1) == -5 && faction_rep(3) == -3 && faction_rep(4) == -5, "standing: -5 did not halve to -3 by shifting");
    check_expect(faction_rep(10) == 3, "standing: an enemy of a loss did not gain -(-5 >> 1) = 3");

    /* The walk up stops at 108: it takes the whole amount, its root 9 does not. */
    standing_defaults(s);
    faction_change_reputation(109, 8);
    check_expect(faction_rep(108) == 8 && faction_rep(9) == 0, "standing: the walk up did not stop at faction 108");
    check_expect(faction_rep(109) == 8, "standing: faction 109 did not take the whole amount");

    /* Decay: one point toward zero. A value under -100 goes to 100 first and comes out at 99. */
    standing_defaults(s);
    s->rep[0] = 5;
    s->rep[1] = -5;
    s->rep[2] = -101;
    s->legal[17] = 4;
    s->legal[18] = -4;
    standing_decay(1);
    check_expect(s->rep[0] == 4 && s->rep[1] == -4 && s->rep[2] == 99, "standing: a faction's reputation did not decay by one, or -101 did not make 99");
    check_expect(s->legal[17] == 4 && s->legal[18] == -4, "standing: legal reputation decayed while a court was open");
    standing_decay(0);
    check_expect(s->legal[17] == 3 && s->legal[18] == -3, "standing: legal reputation did not decay toward zero");

    /* The first faction of a type in a region, else the one with no region. */
    standing_defaults(s);
    check_expect(faction_of_type(7, 4) == 1 && faction_of_type(-1, 4) == 1, "standing: faction_find_type_in_region");
    for (i = 0; i < REGIONS; i++) {
        if (region_price(i) != PRICE_NORMAL || region_legal(i) != 0) {
            check_expect(0, "standing: a region did not start at normal prices and no legal standing");
            break;
        }
    }
    check_expect(standing_valid(s), "standing: the defaults are not valid");
    s->price[3] = 5000;
    check_expect(!standing_valid(s), "standing: a price past 4000 was accepted");
}

/* The politics on the real table: prices, flags, the rumours' flags and purge, borders, and a round that keeps every power in range and every alliance two-way. */
static void check_politics(void)
{
    StandingState *st = standing_state();
    PoliticsState *po = politics_state();
    int i, j, bad, tries, up, down, ok;
    df_u32 now = 600000u;

    standing_defaults(st);
    politics_defaults(po);
    /* The flags: a group's others go when one is set; the prices' flags follow the drift. */
    region_flag_set(3, REGION_FAMINE_BEGINNING); region_flag_set(3, REGION_FAMINE_ONGOING);
    check_expect(region_flag(3, REGION_FAMINE_ONGOING) && !region_flag(3, REGION_FAMINE_BEGINNING), "politics: setting a flag left another of its group set");
    region_flag_clear(3, REGION_FAMINE_ONGOING);
    check_expect(!region_flag(3, REGION_FAMINE_ONGOING) && !po->groups[3][GROUP_FAMINE], "politics: clearing a flag left its group");
    /* Prices: a step is 2 % up or down, held to 250..4000, and flags 19 and 20 mark over 2000 and under 500. */
    up = down = 0;
    for (tries = 0; tries < 200; tries++) {
        int before;

        for (j = 0; j < REGIONS; j++) { st->price[j] = 1000; }
        politics_prices();
        for (j = 0; j < REGIONS; j++) {
            before = 1000;
            if (st->price[j] == before * PRICE_DRIFT_UP / PRICE_DRIFT_DIV) { up++; } else if (st->price[j] == before * PRICE_DRIFT_DOWN / PRICE_DRIFT_DIV) { down++; }
        }
    }
    check_expect(up > 0 && down > 0, "politics: a price did not step 1020 or 980");
    st->price[17] = 4000; politics_prices();
    check_expect(st->price[17] <= PRICE_MOST && region_flag(17, REGION_PRICES_HIGH) && !region_flag(17, REGION_PRICES_LOW), "politics: a price over 2000 is not marked high");
    st->price[17] = PRICE_LEAST; politics_prices();
    check_expect(st->price[17] >= PRICE_LEAST && region_flag(17, REGION_PRICES_LOW) && !region_flag(17, REGION_PRICES_HIGH), "politics: a price under 500 is not marked low");
    /* Borders: a short row ends at its mark and its zero padding is not region 0. */
    check_expect(regions_border(0, 44) && regions_border(0, 2) && regions_border(2, 0) && !regions_border(2, 1) && !regions_border(0, 1), "politics: the borders are not FALL.EXE's");
    /* A round keeps every power in 0..100, every enemy and ally a faction, and writes news that a purge keeps or drops by its kind. */
    for (i = 0; i < 3; i++) { politics_round(now + (df_u32)i * POLITICS_PERIOD, i == 2); }
    bad = 0;
    for (i = 0; i < faction_total(); i++) {
        /* FACTION.TXT starts a few factions past 100 (386 at 400, 523 at 1000): only a faction that began in range must stay in it. */
        if (st->power[i] < 0 || (st->power[i] > 100 && faction_def(i)->power <= 100)) { bad++; }
        for (j = 0; j < FACTION_LINKS; j++) {
            if (st->ally[i][j] >= faction_total() || st->enemy[i][j] >= faction_total()) { bad++; }
            if (st->ally[i][j] == i || st->enemy[i][j] == i) { bad++; }
        }
    }
    check_expect(!bad, "politics: a round left a power out of range or a faction its own ally or enemy");
    ok = 1;
    for (i = 0; i < RUMOURS_MOST; i++) {
        if (po->rumour[i].used) {
            int regional = po->rumour[i].kind == NEWS_PLAGUE || po->rumour[i].kind == NEWS_FAMINE || po->rumour[i].kind == NEWS_WITCHES || po->rumour[i].kind == NEWS_TEMPLE ||
                           po->rumour[i].kind == NEWS_ALLIANCE || po->rumour[i].kind == NEWS_ENEMIES || po->rumour[i].kind == NEWS_WAR_STATE;
            ok &= po->rumour[i].flags == (regional ? NEWSFLAG_REGIONAL : NEWSFLAG_FACTION) && po->rumour[i].text[0] != 0 && po->rumour[i].expires == now + 2u * POLITICS_PERIOD + RUMOUR_LIFE;
        }
    }
    check_expect(ok, "politics: a rumour's flags, text or end are not by its kind");
    {
        int gossip = 0;

        for (i = 0; i < RUMOURS_MOST; i++) { gossip += po->rumour[i].used && po->rumour[i].kind == NEWS_GOSSIP && !po->rumour[i].faction1 && !po->rumour[i].faction2; }
        check_expect(gossip >= 3, "politics: a writing round left the general gossip (1450..1456) unwritten");
    }
    for (i = 0; i < RUMOURS_MOST; i++) { po->rumour[i].used = 0; }
    po->rumour[0].used = 1; po->rumour[0].flags = NEWSFLAG_REGIONAL; po->rumour[0].region = 5; po->rumour[0].expires = now + 10u; df_copy(po->rumour[0].text, "Plague.", RUMOUR_TEXT);
    po->rumour[1].used = 1; po->rumour[1].flags = NEWSFLAG_FACTION; po->rumour[1].expires = now + 10000000u; df_copy(po->rumour[1].text, "A war.", RUMOUR_TEXT);
    { char board[200]; check_expect(politics_board(5, board, sizeof(board)) > 0 && !politics_board(6, board, sizeof(board)), "politics: a notice board shows another region's rumour"); }
    politics_round(now, 1);
    check_expect(!po->rumour[0].used || strcmp(po->rumour[0].text, "Plague.") != 0, "politics: a round's purge kept a regional rumour");
    standing_defaults(st); politics_defaults(po);
}

void check_standing_real(void)
{
    int i, roots;

    check_expect(faction_total() == 366, "standing: FACTION.TXT through the seam is not 366 factions");
    roots = 0;
    for (i = 0; i < faction_total(); i++) {
        roots += faction_def(i)->parent == 0;
    }
    check_expect(roots >= 1, "standing: no root faction");
    check_politics();
    standing_defaults(standing_state());
    {
        int knight, before;

        /* Any individual (type 4) with a parent: a gain reaches its root in full. */
        knight = -1;
        for (i = 0; i < faction_total() && knight < 0; i++) {
            if (faction_def(i)->type == 4 && faction_def(i)->parent != 0 && faction_def(i)->id != 240) {
                knight = faction_def(i)->id;
            }
        }
        before = faction_rep(knight);
        faction_change_reputation(knight, 6);
        check_expect(faction_rep(knight) >= before, "standing: a gain lowered the faction's own reputation");
        printf("standing: %d factions; a gain to faction %d reaches its tree\n", faction_total(), knight);
    }
    standing_defaults(standing_state());
}
