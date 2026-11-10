#include <string.h>
#include <stdio.h>
#include "check_ogpotion.h"
#include "check_util.h"
#include "game/ogfall/play/potion.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/spellbook.h"
#include "game/ogfall/play/magic.h"

void check_ogpotion(void)
{
    df_u32 id, container;
    int i, count, group[8], index[8], n, recipe, health_before;
    const OgSpellDef *spell;

    /* The recipes: twenty, from Stamina at 25 gold to Purification at 500, each with its spell registered. */
    check_expect(og_potion_count() == 20, "potion: the original has twenty recipes");
    check_expect(!strcmp(og_potion_name(0), "Stamina") && og_potion_value(0) == 25 && !strcmp(og_potion_name(19), "Purification") && og_potion_value(19) == 500 &&
                 !strcmp(og_potion_name(2), "Healing") && og_potion_value(2) == 50, "potion: the recipes' names or values");
    n = og_potion_ingredients(2, group, index, 8);
    check_expect(n == 4 && index[0] == 6 && group[0] == 20 && index[1] == 8 && group[1] == 15 && index[3] == 6 && group[3] == 17, "potion: Healing's ingredients are not 6, 8, 0 and 6 by the groups 20, 15, 21 and 17");
    n = og_potion_ingredients(19, group, index, 8);
    check_expect(n == 8, "potion: Purification takes eight ingredients");
    spell = og_spell_def(og_potion_spell(2));
    check_expect(spell && spell->target == 0 && spell->group[0] == 10 && !strcmp(spell->name, "Healing"), "potion: a recipe's spell is a spell of the caster that heals");

    /* A bottle: its price is the recipe's, its name the potion's; the recipe is read back. */
    container = og_state()->inventory_id;
    id = og_potion_create(container, 2);
    check_expect(id && og_potion_of(id) == 2 && og_object(id)->value == 50 && !strcmp(og_object(id)->name, "Potion of Healing"), "potion: a bottle of Healing");
    check_expect(og_potion_of(0) == -1, "potion: no item was a potion");
    check_expect(og_potion_create(container, 20) == 0 && og_potion_create(container, -1) == 0, "potion: a recipe that is none made a bottle");

    /* Drinking casts the spell on the player and the bottle is gone: Healing's effect restores health. */
    og_character()->health = 1;
    health_before = og_character()->health;
    og_character()->max_health = 40;
    check_expect(og_potion_drink(id) && !og_object(id), "potion: a bottle was not drunk and gone");
    check_expect(og_character()->health > health_before, "potion: Healing did not heal");

    /* A shop's shelf: quality + 1 bottles of any recipes. */
    container = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
    og_potion_stock(container, 5);
    count = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        const OgObject *o;

        o = &og_state()->object[i];
        if (o->id && o->parent == container && og_potion_of(o->id) >= 0) { count++; }
    }
    check_expect(count == 6, "potion: a quality 5 shelf did not hold six bottles");
    recipe = 0;
    (void)recipe;

    /* Soul traps: quality / 2 + 1 of them at 5000, those with a soul dearer by its kind's value. */
    container = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
    og_soul_trap_stock(container, 10);
    count = 0;
    {
        int with_soul = 0, priced = 1;

        for (i = 0; i < OG_OBJECTS; i++) {
            const OgObject *o;

            o = &og_state()->object[i];
            if (o->id && o->parent == container && o->type == OG_RECORD_ITEM && o->group == 27 && o->index == 1) {
                int j, soul;

                count++;
                soul = -1;
                for (j = 0; j < OG_OBJECTS; j++) {
                    if (og_state()->object[j].id && og_state()->object[j].parent == o->id && og_state()->object[j].type == OG_RECORD_SOUL) { soul = og_state()->object[j].index; }
                }
                if (soul >= 0) { with_soul++; priced = priced && og_soul_value(soul) > 0 && o->value == og_soul_value(soul) + 5000; }
                else { priced = priced && o->value == 5000; }
            }
        }
        check_expect(count == 6 && priced, "potion: a quality 10 hall did not stock six soul traps priced by their souls");
        (void)with_soul;
    }
}
