#include "game/dagger/play/spellshop.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/magic.h"

/* See spellshop.h. */

int spellshop_list(unsigned person, int quality, int *ids, int cap)
{
    int i, count, base_chance, cost, chance, saved, standard, pass;
    const SpellDef *s;

    if (!inventory_book()) {
        return 0;
    }
    standard = spell_standard_count();
    base_chance = quality * 5 + 30;
    /* spellshop_open: the stream's next value is kept, the stream started at the person's number, and put back at the close. */
    saved = random_range(0, 32767);
    random_seed((df_u32)person);
    count = 0;
    /* The whole table is gone through, and again while the list is empty (the original's loop; a chance is never under 5 in 50, so it ends). */
    for (pass = 0; count == 0 && pass < 1000 && standard > 0; pass++) {
        for (i = 0; i < standard; i++) {
            s = spell_standard(i);
            if (!s || !s->name[0] || s->name[0] == '!') {
                continue;
            }
            cost = spell_cost(s, character_state());
            while (cost > 95) {
                cost >>= 1;
            }
            chance = base_chance - cost;
            if (chance < 5) {
                chance = 5;
            }
            if (random_range(1, 50) < chance && count < cap) {
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
    return holiday == 43 ? price >> 1 : price;
}

int spellshop_add(int id)
{
    return spell_create(inventory_book(), id) != 0;
}
