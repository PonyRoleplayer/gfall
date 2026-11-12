#include <string.h>
#include <stdio.h>
#include "check_ogpotion.h"
#include "check_util.h"
#include "game/dagger/play/potion.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/spellbook.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/tables/itemdefs.h"

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

    /* The cauldron: Healing is the ingredients 6, 8, 0 and 6 of the groups 20, 15, 21 and 17; their keys (variants) are what it matches by. */
    {
        df_u32 item[4], bottle, pack[64];
        int made, k, found_recipe, type[3], subtype[3], effects, potency_ok;
        const OgCauldron *c;

        n = og_potion_ingredients(2, group, index, 8);
        og_cauldron_clear();
        for (k = 0; k < n; k++) {
            item[k] = og_item_create(og_state()->inventory_id, group[k], index[k], -1);
            check_expect(item[k] && og_ingredient(item[k]), "potion: an ingredient of Healing is not one");
        }
        check_expect(og_ingredients(pack, 64) >= 4, "potion: the pack's ingredients were not listed");
        for (k = 0; k < n; k++) { og_cauldron_add(item[k]); }
        c = og_cauldron();
        check_expect(og_cauldron_count() == 4 && !og_cauldron_add(item[0]), "potion: four in the cauldron, and the same one twice");
        potency_ok = c->potency[0] == og_item_rarity(group[0], index[0]) && c->key[0] == og_item_def(group[0], index[0])->variants;
        check_expect(potency_ok, "potion: an ingredient's potency and key are not its template's rarity and variants");
        check_expect(og_potion_match(2, c) && !og_potion_match(0, c), "potion: Healing's ingredients do not match Healing alone");
        made = og_brew(&found_recipe, &bottle);
        check_expect(made == OG_BREW_RECIPE && found_recipe == 2 && og_potion_of(bottle) == 2 && og_cauldron_count() == 0 && !og_object(item[0]), "potion: Healing was not brewed from its ingredients and used them up");
        /* The original does not clear a cell's key when a mix uses the ingredients up (only taking one out by hand does), so mixing again at once matches the same recipe with
         * nothing in the pot: kept as the decomp has it, and gone when the maker is opened again (potionmaker_open sets every key to 254). */
        check_expect(og_brew(&found_recipe, &bottle) == OG_BREW_RECIPE && found_recipe == 2, "potion: the stale keys after a mix were not kept");
        og_cauldron_clear();
        check_expect(og_brew(&found_recipe, &bottle) == OG_BREW_NOTHING, "potion: an empty cauldron brewed");
        /* A recipe in the pack is loaded from the ingredients in it. */
        for (k = 0; k < n; k++) { item[k] = og_item_create(og_state()->inventory_id, group[k], index[k], -1); }
        check_expect(og_recipe_create(og_state()->inventory_id, 2) && og_recipes_known(pack, 64) >= 1, "potion: a recipe item was not known");
        check_expect(og_recipe_load(2) && og_cauldron_count() == 4 && og_potion_match(2, og_cauldron()), "potion: a recipe did not load its ingredients");
        og_cauldron_clear();
        for (k = 0; k < n; k++) { og_object_delete(item[k]); }
        check_expect(!og_recipe_load(2), "potion: a recipe loaded without its ingredients");
        /* An unknown brew: two unrelated ingredients lend their effects; the bottle is worth nothing and drinks as a spell of its own. */
        og_cauldron_clear();
        item[0] = og_item_create(og_state()->inventory_id, 14, 0, -1);
        item[1] = og_item_create(og_state()->inventory_id, 21, 1, -1);
        check_expect(item[0] && item[1] && og_ingredient(item[0]) && og_ingredient(item[1]), "potion: groups 14 and 21 gave no ingredients");
        if (item[0] && item[1] && og_ingredient(item[0]) && og_ingredient(item[1])) {
            og_cauldron_add(item[0]); og_cauldron_add(item[1]);
            effects = og_brew_effects(og_cauldron(), type, subtype);
            check_expect(effects >= 1 && effects <= 3, "potion: two ingredients made no effects");
            made = og_brew(&found_recipe, &bottle);
            check_expect(made == OG_BREW_UNKNOWN, "potion: two odd ingredients did not make an unknown brew");
            if (made == OG_BREW_UNKNOWN) {
                check_expect(found_recipe == -1 && og_object(bottle)->value == 0 && og_potion_of(bottle) == -1 && !strncmp(og_object(bottle)->name, "Potion of Unknown", 17), "potion: an unknown brew is not an unknown potion");
                check_expect(og_potion_drink(bottle) && !og_object(bottle) && og_spell_def(OG_BREW_SPELL) != 0, "potion: an unknown brew could not be drunk");
            }
        }
    }
}
