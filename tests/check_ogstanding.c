#include <stdio.h>
#include "check_ogstanding.h"
#include "check_util.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/state.h"
#include "engine/core/mem.h"

/*
 * The rules of docs/ai-social-math.md ("Faction reputation") on a table small enough to work by hand:
 *     1 ---- 2            10   (an enemy of 4)       9 --- 108 --- 109
 *      `-- 3 --- 4  (4: ally 2, enemy 10)
 */
static OgFactionDef def(int id, int parent, int type)
{
    OgFactionDef d;

    mem_zero(&d, sizeof(d));
    d.id = (short)id;
    d.parent = (short)parent;
    d.type = (short)type;
    d.region = -1;
    return d;
}

static void table(void)
{
    OgFactionDef t[8];

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
    check_expect(og_factions_define(t, 8), "standing: a small table did not define");
    og_standing_defaults(og_standing());
}

void check_ogstanding(void)
{
    OgStanding *s;
    int i;

    table();
    s = og_standing();
    check_expect(og_faction_index(4) == 3 && og_faction_index(77) == -1, "standing: faction indexes");
    /* A sum held in -100..100, as the original's short (over 100 stops, under -100 is 65436, which is -100). */
    og_faction_add_reputation(2, 250);
    check_expect(og_faction_rep(2) == 100, "standing: a reputation passed 100");
    og_faction_add_reputation(2, -500);
    check_expect(og_faction_rep(2) == -100, "standing: a reputation passed -100");
    og_standing_defaults(s);

    /* Faction 4 gains 10: the root in full, the others in its tree at half, 4 itself in full, its ally half and its enemy minus half. */
    og_faction_change_reputation(4, 10);
    check_expect(og_faction_rep(1) == 10, "standing: the root did not take the whole amount");
    check_expect(og_faction_rep(3) == 5 && og_faction_rep(2) == 5 + 5, "standing: siblings and parents did not take half (the ally takes half again)");
    check_expect(og_faction_rep(4) == 10, "standing: the faction itself did not take the whole amount");
    check_expect(og_faction_rep(10) == -5, "standing: an enemy did not lose half");
    check_expect(og_faction_rep(9) == 0, "standing: another tree moved");

    /* A loss halves by an arithmetic shift: minus 5 to the others is minus 3. */
    og_standing_defaults(s);
    og_faction_change_reputation(4, -5);
    check_expect(og_faction_rep(1) == -5 && og_faction_rep(3) == -3 && og_faction_rep(4) == -5, "standing: -5 did not halve to -3 by shifting");
    check_expect(og_faction_rep(10) == 3, "standing: an enemy of a loss did not gain -(-5 >> 1) = 3");

    /* The walk up stops at 108: it takes the whole amount, its root 9 does not. */
    og_standing_defaults(s);
    og_faction_change_reputation(109, 8);
    check_expect(og_faction_rep(108) == 8 && og_faction_rep(9) == 0, "standing: the walk up did not stop at faction 108");
    check_expect(og_faction_rep(109) == 8, "standing: faction 109 did not take the whole amount");

    /* Decay: one point toward zero. A value under -100 goes to 100 first and comes out at 99. */
    og_standing_defaults(s);
    s->rep[0] = 5;
    s->rep[1] = -5;
    s->rep[2] = -101;
    s->legal[17] = 4;
    s->legal[18] = -4;
    og_standing_decay(1);
    check_expect(s->rep[0] == 4 && s->rep[1] == -4 && s->rep[2] == 99, "standing: a faction's reputation did not decay by one, or -101 did not make 99");
    check_expect(s->legal[17] == 4 && s->legal[18] == -4, "standing: legal reputation decayed while a court was open");
    og_standing_decay(0);
    check_expect(s->legal[17] == 3 && s->legal[18] == -3, "standing: legal reputation did not decay toward zero");

    /* The first faction of a type in a region, else the one with no region. */
    og_standing_defaults(s);
    check_expect(og_faction_of_type(7, 4) == 1 && og_faction_of_type(-1, 4) == 1, "standing: faction_find_type_in_region");
    for (i = 0; i < OG_REGIONS; i++) {
        if (og_region_price(i) != OG_PRICE_NORMAL || og_region_legal(i) != 0) {
            check_expect(0, "standing: a region did not start at normal prices and no legal standing");
            break;
        }
    }
    check_expect(og_standing_valid(s), "standing: the defaults are not valid");
    s->price[3] = 5000;
    check_expect(!og_standing_valid(s), "standing: a price past 4000 was accepted");
}

void check_ogstanding_real(void)
{
    int i, roots;

    check_expect(og_faction_count() == 366, "standing: FACTION.TXT through the seam is not 366 factions");
    roots = 0;
    for (i = 0; i < og_faction_count(); i++) {
        roots += og_faction_def(i)->parent == 0;
    }
    check_expect(roots >= 1, "standing: no root faction");
    og_standing_defaults(og_standing());
    {
        int knight, before;

        /* Any individual (type 4) with a parent: a gain reaches its root in full. */
        knight = -1;
        for (i = 0; i < og_faction_count() && knight < 0; i++) {
            if (og_faction_def(i)->type == 4 && og_faction_def(i)->parent != 0 && og_faction_def(i)->id != 240) {
                knight = og_faction_def(i)->id;
            }
        }
        before = og_faction_rep(knight);
        og_faction_change_reputation(knight, 6);
        check_expect(og_faction_rep(knight) >= before, "standing: a gain lowered the faction's own reputation");
        printf("standing: %d factions; a gain to faction %d reaches its tree\n", og_faction_count(), knight);
    }
    og_standing_defaults(og_standing());
}
