#include "game/dagger/play/spellshop.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/magic.h"

/* See spellshop.h. */

int og_spellshop_list(unsigned person, int quality, int *ids, int cap)
{
    int i, count, base_chance, cost, chance, saved, standard, pass;
    const OgSpellDef *s;

    if (!og_inventory_book()) {
        return 0;
    }
    standard = og_spell_standard_count();
    base_chance = quality * 5 + 30;
    /* spellshop_open: the stream's next value is kept, the stream started at the person's number, and put back at the close. */
    saved = og_random_range(0, 32767);
    og_random_seed((df_u32)person);
    count = 0;
    /* The whole table is gone through, and again while the list is empty (the original's loop; a chance is never under 5 in 50, so it ends). */
    for (pass = 0; count == 0 && pass < 1000 && standard > 0; pass++) {
        for (i = 0; i < standard; i++) {
            s = og_spell_standard(i);
            if (!s || !s->name[0] || s->name[0] == '!') {
                continue;
            }
            cost = og_spell_cost(s, og_character());
            while (cost > 95) {
                cost >>= 1;
            }
            chance = base_chance - cost;
            if (chance < 5) {
                chance = 5;
            }
            if (og_random_range(1, 50) < chance && count < cap) {
                ids[count++] = s->id;
            }
        }
    }
    og_random_seed((df_u32)saved);
    return count;
}

int og_spellshop_price(const OgSpellDef *spell, int holiday)
{
    int price;

    price = og_spell_cost(spell, og_character()) << 2;
    return holiday == 43 ? price >> 1 : price;
}

int og_spellshop_add(int id)
{
    return og_spell_create(og_inventory_book(), id) != 0;
}
