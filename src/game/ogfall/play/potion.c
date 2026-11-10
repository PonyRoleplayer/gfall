#include "game/ogfall/play/potion.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/spellbook.h"
#include "game/ogfall/play/magic.h"
#include "engine/core/log.h"
#include "game/ogfall/tables/potiondata.h"
#include "game/ogfall/tables/souldata.h"

/* See potion.h. */

void og_potions_register(void)
{
    int i;

    for (i = 0; i < OG_RECIPES; i++) {
        og_spell_register(g_recipe[i] + 20, OG_POTION_SPELL + i);
    }
}

int og_potion_count(void)
{
    return OG_RECIPES;
}

const char *og_potion_name(int recipe)
{
    const OgSpellDef *spell;

    spell = recipe >= 0 && recipe < OG_RECIPES ? og_spell_def(OG_POTION_SPELL + recipe) : 0;
    return spell ? spell->name : "";
}

int og_potion_value(int recipe)
{
    return recipe >= 0 && recipe < OG_RECIPES ? (int)(g_recipe[recipe][8] | g_recipe[recipe][9] << 8) : 0;
}

int og_potion_spell(int recipe)
{
    return OG_POTION_SPELL + recipe;
}

int og_potion_ingredients(int recipe, int *group, int *index, int cap)
{
    int n;

    if (recipe < 0 || recipe >= OG_RECIPES) {
        return 0;
    }
    for (n = 0; n < 8 && n < cap && (signed char)g_recipe[recipe][n] != -2; n++) {
        index[n] = (signed char)g_recipe[recipe][n];
        group[n] = (signed char)g_recipe[recipe][10 + n];
    }
    return n;
}

df_u32 og_potion_create(df_u32 container, int recipe)
{
    df_u32 id;
    OgObject *item;
    char name[32];

    if (recipe < 0 || recipe >= OG_RECIPES) {
        return 0;
    }
    id = og_item_create(container, OG_POTION_GROUP, OG_POTION_INDEX, -1);
    item = og_object_edit(id);
    if (!item) {
        return 0;
    }
    item->value = og_potion_value(recipe);
    item->message = recipe + 1;
    log_format(name, (int)sizeof(name), "Potion of %s", og_potion_name(recipe));
    df_copy(item->name, name, (int)sizeof(item->name));
    return id;
}

int og_potion_of(df_u32 item)
{
    const OgObject *o;

    o = og_object(item);
    if (!o || o->type != OG_RECORD_ITEM || o->group != OG_POTION_GROUP || o->index != OG_POTION_INDEX || o->message < 1 || o->message > OG_RECIPES) {
        return -1;
    }
    return o->message - 1;
}

int og_potion_drink(df_u32 item)
{
    int recipe;

    recipe = og_potion_of(item);
    if (recipe < 0) {
        return 0;
    }
    /* cast_player_spell on a spell of the potion: its target is the caster, so it lands on the player at once. */
    og_magic_apply(og_potion_spell(recipe), -1, -1, 0, 0);
    og_object_delete(item);
    return 1;
}

void og_potion_stock(df_u32 container, int quality)
{
    int i;

    for (i = 0; i < quality + 1; i++) {
        og_potion_create(container, og_random_range(0, OG_RECIPES - 1));
    }
}

int og_soul_value(int kind)
{
    return kind >= 0 && kind < 43 ? g_soul_value[kind] : 0;
}

void og_soul_trap_stock(df_u32 container, int quality)
{
    int i, kind;
    df_u32 trap;

    for (i = 0; i < (quality >> 1) + 1; i++) {
        trap = og_item_create(container, 27, 1, -1);
        if (!trap) {
            return;
        }
        og_object_edit(trap)->value = 5000;
        if (og_random_range(1, 100) < 25) {
            /* soul_trap_add_soul: a kind drawn again until one has a value. */
            kind = og_random_range(0, 42);
            while (!g_soul_value[kind]) {
                kind = og_random_range(0, 42);
            }
            if (og_object_new(OG_RECORD_SOUL, trap, kind, 100)) {
                og_object_edit(trap)->value = g_soul_value[kind] + 5000;
            }
        }
    }
}
