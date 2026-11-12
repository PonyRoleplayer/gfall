#include <string.h>
#include <stdio.h>
#include "check_potion.h"
#include "check_util.h"
#include "game/dagger/play/potion.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/spellbook.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/tables/itemdefs.h"

void check_potion(void)
{
    df_u32 id, container;
    int i, count, group[8], index[8], n, recipe, health_before;
    const SpellDef *spell;

    /* The recipes: twenty, from Stamina at 25 gold to Purification at 500, each with its spell registered. */
    check_expect(potion_count() == 20, "potion: the original has twenty recipes");
    check_expect(!strcmp(potion_name(0), "Stamina") && potion_value(0) == 25 && !strcmp(potion_name(19), "Purification") && potion_value(19) == 500 &&
                 !strcmp(potion_name(2), "Healing") && potion_value(2) == 50, "potion: the recipes' names or values");
    n = potion_ingredients(2, group, index, 8);
    check_expect(n == 4 && index[0] == 6 && group[0] == 20 && index[1] == 8 && group[1] == 15 && index[3] == 6 && group[3] == 17, "potion: Healing's ingredients are not 6, 8, 0 and 6 by the groups 20, 15, 21 and 17");
    n = potion_ingredients(19, group, index, 8);
    check_expect(n == 8, "potion: Purification takes eight ingredients");
    spell = spell_def(potion_spell(2));
    check_expect(spell && spell->target == 0 && spell->group[0] == 10 && !strcmp(spell->name, "Healing"), "potion: a recipe's spell is a spell of the caster that heals");

    /* A bottle: its price is the recipe's, its name the potion's; the recipe is read back. */
    container = game_state()->inventory_id;
    id = potion_create(container, 2);
    check_expect(id && potion_of(id) == 2 && object_at(id)->value == 50 && !strcmp(object_at(id)->name, "Potion of Healing"), "potion: a bottle of Healing");
    check_expect(potion_of(0) == -1, "potion: no item was a potion");
    check_expect(potion_create(container, 20) == 0 && potion_create(container, -1) == 0, "potion: a recipe that is none made a bottle");

    /* Drinking casts the spell on the player and the bottle is gone: Healing's effect restores health. */
    character_state()->health = 1;
    health_before = character_state()->health;
    character_state()->max_health = 40;
    check_expect(potion_drink(id) && !object_at(id), "potion: a bottle was not drunk and gone");
    check_expect(character_state()->health > health_before, "potion: Healing did not heal");

    /* A shop's shelf: quality + 1 bottles of any recipes. */
    container = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
    potion_stock(container, 5);
    count = 0;
    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *o;

        o = &game_state()->object[i];
        if (o->id && o->parent == container && potion_of(o->id) >= 0) { count++; }
    }
    check_expect(count == 6, "potion: a quality 5 shelf did not hold six bottles");
    recipe = 0;
    (void)recipe;

    /* Soul traps: quality / 2 + 1 of them at 5000, those with a soul dearer by its kind's value. */
    container = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
    soul_trap_stock(container, 10);
    count = 0;
    {
        int with_soul = 0, priced = 1;

        for (i = 0; i < OBJECTS; i++) {
            const ObjectInfo *o;

            o = &game_state()->object[i];
            if (o->id && o->parent == container && o->type == RECORD_ITEM && o->group == 27 && o->index == 1) {
                int j, soul;

                count++;
                soul = -1;
                for (j = 0; j < OBJECTS; j++) {
                    if (game_state()->object[j].id && game_state()->object[j].parent == o->id && game_state()->object[j].type == RECORD_SOUL) { soul = game_state()->object[j].index; }
                }
                if (soul >= 0) { with_soul++; priced = priced && soul_value(soul) > 0 && o->value == soul_value(soul) + 5000; }
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
        const Cauldron *c;

        n = potion_ingredients(2, group, index, 8);
        cauldron_clear();
        for (k = 0; k < n; k++) {
            item[k] = item_create(game_state()->inventory_id, group[k], index[k], -1);
            check_expect(item[k] && ingredient_at(item[k]), "potion: an ingredient of Healing is not one");
        }
        check_expect(ingredient_list(pack, 64) >= 4, "potion: the pack's ingredients were not listed");
        for (k = 0; k < n; k++) { cauldron_add(item[k]); }
        c = cauldron_state();
        check_expect(cauldron_count() == 4 && !cauldron_add(item[0]), "potion: four in the cauldron, and the same one twice");
        potency_ok = c->potency[0] == item_rarity(group[0], index[0]) && c->key[0] == item_def(group[0], index[0])->variants;
        check_expect(potency_ok, "potion: an ingredient's potency and key are not its template's rarity and variants");
        check_expect(potion_match(2, c) && !potion_match(0, c), "potion: Healing's ingredients do not match Healing alone");
        made = brew_state(&found_recipe, &bottle);
        check_expect(made == BREW_RECIPE && found_recipe == 2 && potion_of(bottle) == 2 && cauldron_count() == 0 && !object_at(item[0]), "potion: Healing was not brewed from its ingredients and used them up");
        /* The original does not clear a cell's key when a mix uses the ingredients up (only taking one out by hand does), so mixing again at once matches the same recipe with
         * nothing in the pot: kept as the decomp has it, and gone when the maker is opened again (potionmaker_open sets every key to 254). */
        check_expect(brew_state(&found_recipe, &bottle) == BREW_RECIPE && found_recipe == 2, "potion: the stale keys after a mix were not kept");
        cauldron_clear();
        check_expect(brew_state(&found_recipe, &bottle) == BREW_NOTHING, "potion: an empty cauldron brewed");
        /* A recipe in the pack is loaded from the ingredients in it. */
        for (k = 0; k < n; k++) { item[k] = item_create(game_state()->inventory_id, group[k], index[k], -1); }
        check_expect(recipe_create(game_state()->inventory_id, 2) && recipes_known(pack, 64) >= 1, "potion: a recipe item was not known");
        check_expect(recipe_load(2) && cauldron_count() == 4 && potion_match(2, cauldron_state()), "potion: a recipe did not load its ingredients");
        cauldron_clear();
        for (k = 0; k < n; k++) { object_delete(item[k]); }
        check_expect(!recipe_load(2), "potion: a recipe loaded without its ingredients");
        /* An unknown brew: two unrelated ingredients lend their effects; the bottle is worth nothing and drinks as a spell of its own. */
        cauldron_clear();
        item[0] = item_create(game_state()->inventory_id, 14, 0, -1);
        item[1] = item_create(game_state()->inventory_id, 21, 1, -1);
        check_expect(item[0] && item[1] && ingredient_at(item[0]) && ingredient_at(item[1]), "potion: groups 14 and 21 gave no ingredients");
        if (item[0] && item[1] && ingredient_at(item[0]) && ingredient_at(item[1])) {
            cauldron_add(item[0]); cauldron_add(item[1]);
            effects = brew_effects(cauldron_state(), type, subtype);
            check_expect(effects >= 1 && effects <= 3, "potion: two ingredients made no effects");
            made = brew_state(&found_recipe, &bottle);
            check_expect(made == BREW_UNKNOWN, "potion: two odd ingredients did not make an unknown brew");
            if (made == BREW_UNKNOWN) {
                check_expect(found_recipe == -1 && object_at(bottle)->value == 0 && potion_of(bottle) == -1 && !strncmp(object_at(bottle)->name, "Potion of Unknown", 17), "potion: an unknown brew is not an unknown potion");
                check_expect(potion_drink(bottle) && !object_at(bottle) && spell_def(BREW_SPELL) != 0, "potion: an unknown brew could not be drunk");
            }
        }
    }
}
