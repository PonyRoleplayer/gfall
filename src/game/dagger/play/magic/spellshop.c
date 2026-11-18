#include "game/dagger/play/magic/spellshop.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/people/trade.h"
#include "game/dagger/play/combat/combat.h"

/* spellshop_build_list (func_0006FDBE.c): the chance base is quality * SPELLSHOP_PER_QUALITY + SPELLSHOP_BASE; a spell's cost is halved while over SPELLSHOP_COST_MOST, the chance is
 * the base less the cost and at least SPELLSHOP_CHANCE_MIN, and a spell is on sale when 1..SPELLSHOP_ROLL is under it. Our guard on the "again while empty" loop is SPELLSHOP_PASSES_MOST (the original has none). */
#define SPELLSHOP_PER_QUALITY 5
#define SPELLSHOP_BASE 30
#define SPELLSHOP_COST_MOST 95
#define SPELLSHOP_CHANCE_MIN 5
#define SPELLSHOP_ROLL 50
#define SPELLSHOP_PASSES_MOST 1000

/* See spellshop.h. */

int spellshop_list(unsigned person, int quality, int *ids, int cap)
{
    int i, count, base_chance, cost, chance, saved, standard, pass;
    const SpellDef *s;

    if (!inventory_book()) {
        return 0;
    }
    standard = spell_standard_count();
    base_chance = quality * SPELLSHOP_PER_QUALITY + SPELLSHOP_BASE;
    /* spellshop_open: the stream's next value is kept, the stream started at the person's number, and put back at the close. */
    saved = random_range(0, RAND_MAX_WATCOM);
    random_seed((df_u32)person);
    count = 0;
    /* The whole table is gone through, and again while the list is empty (the original's loop; a chance is never under 5 in 50, so it ends). */
    for (pass = 0; count == 0 && pass < SPELLSHOP_PASSES_MOST && standard > 0; pass++) {
        for (i = 0; i < standard; i++) {
            s = spell_standard(i);
            if (!s || !s->name[0] || s->name[0] == '!') {
                continue;
            }
            cost = spell_cost(s, character_state());
            while (cost > SPELLSHOP_COST_MOST) {
                cost >>= 1;
            }
            chance = base_chance - cost;
            if (chance < SPELLSHOP_CHANCE_MIN) {
                chance = SPELLSHOP_CHANCE_MIN;
            }
            if (random_range(1, SPELLSHOP_ROLL) < chance && count < cap) {
                ids[count++] = s->id;
            }
        }
    }
    random_seed((df_u32)saved);
    return count;
}

int spellshop_price(const SpellDef *spell, int holiday)
{
    int price;

    price = spell_cost(spell, character_state()) << 2;
    return holiday == HOLIDAY_SPELLS_HALF ? price >> 1 : price;
}

int spellshop_add(int id)
{
    return spell_create(inventory_book(), id) != 0;
}
