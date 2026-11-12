#include "game/dagger/play/potion.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/spellbook.h"
#include "game/dagger/play/magic.h"
#include "engine/core/log.h"
#include "game/dagger/tables/potiondata.h"
#include "game/dagger/tables/souldata.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/tables/makerdata.h"
#include "game/dagger/tables/magicdefs.h"
#include "engine/core/mem.h"

/* See potion.h. */

static int drink_unknown(df_u32 item);

void potions_register(void)
{
    int i;

    for (i = 0; i < RECIPES; i++) {
        spell_register(g_recipe[i] + 20, POTION_SPELL + i);
    }
}

int potion_count(void)
{
    return RECIPES;
}

const char *potion_name(int recipe)
{
    const SpellDef *spell;

    spell = recipe >= 0 && recipe < RECIPES ? spell_def(POTION_SPELL + recipe) : 0;
    return spell ? spell->name : "";
}

int potion_value(int recipe)
{
    return recipe >= 0 && recipe < RECIPES ? (int)(g_recipe[recipe][8] | g_recipe[recipe][9] << 8) : 0;
}

int potion_spell(int recipe)
{
    return POTION_SPELL + recipe;
}

int potion_ingredients(int recipe, int *group, int *index, int cap)
{
    int n;

    if (recipe < 0 || recipe >= RECIPES) {
        return 0;
    }
    for (n = 0; n < 8 && n < cap && (signed char)g_recipe[recipe][n] != -2; n++) {
        index[n] = (signed char)g_recipe[recipe][n];
        group[n] = (signed char)g_recipe[recipe][10 + n];
    }
    return n;
}

df_u32 potion_create(df_u32 container, int recipe)
{
    df_u32 id;
    ObjectInfo *item;
    char name[32];

    if (recipe < 0 || recipe >= RECIPES) {
        return 0;
    }
    id = item_create(container, POTION_GROUP, POTION_INDEX, -1);
    item = object_edit(id);
    if (!item) {
        return 0;
    }
    item->value = potion_value(recipe);
    item->message = recipe + 1;
    log_format(name, (int)sizeof(name), "Potion of %s", potion_name(recipe));
    df_copy(item->name, name, (int)sizeof(item->name));
    return id;
}

int potion_of(df_u32 item)
{
    const ObjectInfo *o;

    o = object_at(item);
    if (!o || o->type != RECORD_ITEM || o->group != POTION_GROUP || o->index != POTION_INDEX || o->message < 1 || o->message > RECIPES) {
        return -1;
    }
    return o->message - 1;
}

int potion_drink(df_u32 item)
{
    int recipe;

    recipe = potion_of(item);
    if (recipe < 0) {
        return drink_unknown(item);
    }
    /* cast_player_spell on a spell of the potion: its target is the caster, so it lands on the player at once. */
    magic_apply(potion_spell(recipe), -1, -1, 0, 0);
    object_delete(item);
    return 1;
}

void potion_stock(df_u32 container, int quality)
{
    int i;

    for (i = 0; i < quality + 1; i++) {
        potion_create(container, random_range(0, RECIPES - 1));
    }
}

int soul_value(int kind)
{
    return kind >= 0 && kind < 43 ? g_soul_value[kind] : 0;
}

void soul_trap_stock(df_u32 container, int quality)
{
    int i, kind;
    df_u32 trap;

    for (i = 0; i < (quality >> 1) + 1; i++) {
        trap = item_create(container, 27, 1, -1);
        if (!trap) {
            return;
        }
        object_edit(trap)->value = 5000;
        if (random_range(1, 100) < 25) {
            /* soul_trap_add_soul: a kind drawn again until one has a value. */
            kind = random_range(0, 42);
            while (!g_soul_value[kind]) {
                kind = random_range(0, 42);
            }
            if (object_new(RECORD_SOUL, trap, kind, 100)) {
                object_edit(trap)->value = g_soul_value[kind] + 5000;
            }
        }
    }
}

/* ---- The cauldron ---- */

static Cauldron g_cauldron;
/* An unknown brew is kept in its bottle's message as bit 30 and three effects of ten bits (the type plus one in six, the subtype in four). */
#define BREW_FLAG 0x40000000

const Cauldron *cauldron_state(void)
{
    return &g_cauldron;
}

void cauldron_clear(void)
{
    mem_zero(&g_cauldron, sizeof(g_cauldron));
    mem_fill(g_cauldron.key, 254, CAULDRON);
}

int ingredient_at(df_u32 item)
{
    const ObjectInfo *o;
    const ItemDef *def;

    o = object_at(item);
    def = o && o->type == RECORD_ITEM ? item_def(o->group, o->index) : 0;
    return def && (def->flags & 1);
}

static int in_cauldron(int group, int index)
{
    int i;

    for (i = 0; i < CAULDRON; i++) {
        const ObjectInfo *o;

        o = object_at(g_cauldron.item[i]);
        if (g_cauldron.item[i] && o && o->group == group && o->index == index) {
            return 1;
        }
    }
    return 0;
}

int cauldron_count(void)
{
    int i, n;

    for (i = n = 0; i < CAULDRON; i++) {
        n += g_cauldron.item[i] != 0;
    }
    return n;
}

int cauldron_add(df_u32 item)
{
    const ObjectInfo *o;
    const ItemDef *def;
    int cell, capacity;

    if (!ingredient_at(item)) {
        return 0;
    }
    o = object_at(item);
    if (cauldron_count() == CAULDRON || in_cauldron(o->group, o->index)) {
        return 0;
    }
    for (cell = 0; g_cauldron.item[cell]; cell++) {
    }
    def = item_def(o->group, o->index);
    capacity = item_capacity(o->group, o->index);
    g_cauldron.item[cell] = item;
    g_cauldron.key[cell] = (unsigned char)def->variants;
    g_cauldron.potency[cell] = (unsigned char)item_rarity(o->group, o->index);
    g_cauldron.type[cell] = (unsigned char)def->draw_order;
    /* item_init_from_template: the capacity is the effect's subtype only for an ingredient (bit 0 of the flags). */
    g_cauldron.subtype[cell] = (unsigned char)(capacity != 0 && (def->flags & 1) ? capacity : 0);
    return 1;
}

int cauldron_remove(int cell)
{
    if (cell < 0 || cell >= CAULDRON || !g_cauldron.item[cell]) {
        return 0;
    }
    g_cauldron.item[cell] = 0;
    g_cauldron.key[cell] = 254;
    return 1;
}

int ingredient_list(df_u32 *ids, int cap)
{
    int i, n, k, in;

    n = 0;
    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *o;

        o = &game_state()->object[i];
        if (!o->id || o->type != RECORD_ITEM || !ingredient_at(o->id)) {
            continue;
        }
        /* The pack and what is in it. */
        {
            const ObjectInfo *up;
            int depth, mine;

            mine = 0;
            for (up = o, depth = 0; up && depth < OBJECTS; depth++) {
                if (up->parent == game_state()->inventory_id) { mine = 1; break; }
                up = object_at(up->parent);
            }
            if (!mine) { continue; }
        }
        in = 0;
        for (k = 0; k < CAULDRON; k++) {
            in |= g_cauldron.item[k] == o->id;
        }
        if (in) {
            continue;
        }
        if (n < cap) {
            ids[n] = o->id;
        }
        n++;
    }
    return n < cap ? n : cap;
}

/* ---- Mixing ---- */

int potion_match(int recipe, const Cauldron *c)
{
    int i, unmatched, value, potency, group[8], index[8], n, k;
    unsigned char ids[8];
    int found;

    if (recipe < 0 || recipe >= RECIPES) {
        return 0;
    }
    n = potion_ingredients(recipe, group, index, 8);
    unmatched = 0;
    value = 0;
    for (i = 0; i < 8; i++) {
        if (i < n) {
            const ItemDef *def;

            def = item_def(group[i], index[i]);
            ids[i] = def ? (unsigned char)def->variants : 255;
            value += item_rarity(group[i], index[i]);
            unmatched++;
        } else {
            ids[i] = 255;
        }
    }
    potency = 0;
    for (i = 0; i < 8; i++) {
        if (c->key[i] == 254) {
            continue;
        }
        found = -1;
        for (k = 0; k < 8; k++) {
            if (ids[k] == c->key[i]) { found = k; break; }
        }
        if (found >= 0) {
            potency += c->potency[i];
            ids[found] = 255;
            unmatched--;
        } else {
            potency -= c->potency[i];
        }
    }
    if (unmatched != 0) {
        return 0;
    }
    return ((potency - value) >> 1) + 5 >= 1;
}

/* potion_sort_ingredients: a bubble sort of three arrays by the first, largest first. */
static void sort3(unsigned char *keys, unsigned char *second, unsigned char *third, int count)
{
    int i, pass, swapped;
    unsigned char t;

    pass = 1;
    if (count <= 1) {
        return;
    }
    swapped = 1;
    while (swapped) {
        swapped = 0;
        for (i = 0; count - pass > i; i++) {
            if (keys[i] < keys[i + 1]) {
                t = keys[i]; keys[i] = keys[i + 1]; keys[i + 1] = t;
                t = second[i]; second[i] = second[i + 1]; second[i + 1] = t;
                t = third[i]; third[i] = third[i + 1]; third[i + 1] = t;
                swapped = 1;
            }
        }
        pass++;
    }
}

int brew_effects(const Cauldron *c, int *type, int *subtype)
{
    unsigned char x[8], y[8], z[8];
    int i, n, sum, k;

    /* The first as many cells as there are ingredients in, whichever those cells are: a cell emptied in the middle leaves its old numbers. */
    n = 0;
    for (i = 0; i < CAULDRON; i++) { n += c->item[i] != 0; }
    mem_zero(x, 8); mem_zero(y, 8); mem_zero(z, 8);
    for (i = 0; i < n; i++) {
        x[i] = c->type[i]; y[i] = c->potency[i]; z[i] = c->subtype[i];
    }
    sort3(x, y, z, n);
    for (i = 0; n - 1 > i; i++) {
        if (x[i] == x[i + 1]) {
            y[i] = (unsigned char)(y[i] + y[i + 1]);
            for (k = i; k < 7; k++) { x[k] = x[k + 1]; z[k] = z[k + 1]; y[k] = y[k + 1]; }
            n--;
        }
    }
    sort3(y, x, z, n);
    sum = 0;
    if (n > 3) {
        for (i = 3; i < n; i++) { sum += y[i]; }
        n = 3;
    }
    switch (n) {
    case 3:
        if ((y[0] >> 1) > y[1]) { n = 1; sum += y[1] + y[2]; }
        else if ((y[1] >> 1) > y[2]) { n = 2; sum += y[2]; }
        break;
    case 2:
        if ((y[0] >> 1) > y[1]) { n = 1; sum += y[1]; }
        break;
    default:
        break;
    }
    for (i = 0; i < n; i++) {
        if (y[i] <= sum) { n = i; break; }
    }
    for (i = 0; i < n; i++) {
        type[i] = x[i];
        subtype[i] = z[i];
    }
    return n;
}

/* The 89-byte spell an unknown brew makes: the effects it was given, element magic, on the caster, and each effect's own settings. */
static void brew_record(int n, const int *type, const int *subtype, df_u8 *r)
{
    int i, flags;

    mem_zero(r, 89);
    for (i = 0; i < 6; i++) { r[i] = 255; }
    r[6] = 4;
    r[7] = 0;
    for (i = 0; i < n; i++) {
        flags = effect_flags(type[i], subtype[i]);
        r[i * 2] = (df_u8)type[i];
        r[i * 2 + 1] = (df_u8)subtype[i];
        if (flags & 1) { r[14 + i * 3] = r[15 + i * 3] = r[16 + i * 3] = 1; }
        if (flags & 2) { r[23 + i * 3] = 70; r[24 + i * 3] = r[25 + i * 3] = 1; }
        /* base_min 10, base_max and plus_min 1; the rest are what the spell record held before, here nothing. */
        if (flags & 4) { r[32 + i * 5] = 10; r[33 + i * 5] = 1; r[34 + i * 5] = 1; }
    }
    for (i = 0; g_maker_unknown[i] && i < 24; i++) { r[47 + i] = (df_u8)g_maker_unknown[i]; }
    r[73] = 255;
}

static int brew_pack(int n, const int *type, const int *subtype)
{
    int i, packed;

    packed = 0;
    for (i = 0; i < n; i++) {
        packed |= ((type[i] + 1) << 4 | (subtype[i] & 15)) << (i * 10);
    }
    return BREW_FLAG | packed;
}

static int brew_unpack(int message, int *type, int *subtype)
{
    int i, n, field;

    n = 0;
    for (i = 0; i < 3; i++) {
        field = (message >> (i * 10)) & 1023;
        if (field >> 4) {
            type[n] = (field >> 4) - 1;
            subtype[n] = field & 15;
            n++;
        }
    }
    return n;
}

static df_u32 make_bottle(int value, int message, const char *name)
{
    df_u32 id;
    ObjectInfo *item;

    id = item_create(game_state()->inventory_id, POTION_GROUP, POTION_INDEX, -1);
    item = object_edit(id);
    if (!item) {
        return 0;
    }
    item->value = value;
    item->message = message;
    df_copy(item->name, name, (int)sizeof(item->name));
    return id;
}

static void use_up_cauldron(void)
{
    int i;

    for (i = 0; i < CAULDRON; i++) {
        if (g_cauldron.item[i]) {
            object_delete(g_cauldron.item[i]);
            g_cauldron.item[i] = 0;
        }
    }
}

int brew_state(int *recipe, df_u32 *potion)
{
    int i, type[3], subtype[3], n;
    char name[32];

    *recipe = -1;
    *potion = 0;
    for (i = 0; i < RECIPES; i++) {
        if (potion_match(i, &g_cauldron)) {
            log_format(name, (int)sizeof(name), "Potion of %s", potion_name(i));
            *potion = make_bottle(potion_value(i), i + 1, name);
            *recipe = i;
            if (*potion) { use_up_cauldron(); }
            return *potion ? BREW_RECIPE : BREW_NOTHING;
        }
    }
    n = brew_effects(&g_cauldron, type, subtype);
    if (!n) {
        return BREW_NOTHING;
    }
    log_format(name, (int)sizeof(name), "Potion of %s", g_maker_unknown);
    *potion = make_bottle(0, brew_pack(n, type, subtype), name);
    if (!*potion) {
        return BREW_NOTHING;
    }
    use_up_cauldron();
    return BREW_UNKNOWN;
}

/* An unknown brew drunk: its spell made from what the bottle remembers and cast on the player. */
static int drink_unknown(df_u32 item)
{
    const ObjectInfo *o;
    int type[3], subtype[3], n;
    df_u8 record[89];

    o = object_at(item);
    if (!o || o->type != RECORD_ITEM || o->group != POTION_GROUP || o->index != POTION_INDEX || !(o->message & BREW_FLAG)) {
        return 0;
    }
    n = brew_unpack(o->message, type, subtype);
    brew_record(n, type, subtype, record);
    spell_replace(record, BREW_SPELL);
    magic_apply(BREW_SPELL, -1, -1, 0, 0);
    object_delete(item);
    return 1;
}

/* ---- Recipes ---- */

df_u32 recipe_create(df_u32 container, int recipe)
{
    df_u32 id;
    ObjectInfo *item;

    if (recipe < 0 || recipe >= RECIPES) {
        return 0;
    }
    id = item_create(container, 27, 4, -1);
    item = object_edit(id);
    if (!item) {
        return 0;
    }
    item->message = recipe + 1;
    return id;
}

int recipe_of(df_u32 item)
{
    const ObjectInfo *o;

    o = object_at(item);
    if (!o || o->type != RECORD_ITEM || o->group != 27 || o->index != 4 || o->message < 1 || o->message > RECIPES) {
        return -1;
    }
    return o->message - 1;
}

int recipes_known(df_u32 *ids, int cap)
{
    int i, n;

    for (i = n = 0; i < OBJECTS; i++) {
        const ObjectInfo *o;

        o = &game_state()->object[i];
        if (o->id && o->parent == game_state()->inventory_id && recipe_of(o->id) >= 0) {
            if (n < cap) { ids[n] = o->id; }
            n++;
        }
    }
    return n < cap ? n : cap;
}

int recipe_load(int recipe)
{
    df_u32 pack[512];
    int group[8], index[8], n, i, k, count, found;

    n = potion_ingredients(recipe, group, index, 8);
    count = ingredient_list(pack, 512);
    /* potion_have_recipe_ingredients: the cauldron's cells emptied (the arrays of keys are not), then each ingredient of the recipe sought in the pack. */
    for (i = 0; i < CAULDRON; i++) { g_cauldron.item[i] = 0; }
    for (i = 0; i < n; i++) {
        found = 0;
        for (k = 0; k < count; k++) {
            const ObjectInfo *o;

            o = object_at(pack[k]);
            if (o && o->group == group[i] && o->index == index[i]) { found = 1; break; }
        }
        if (!found) {
            return 0;
        }
    }
    for (i = 0; i < n; i++) {
        for (k = 0; k < count; k++) {
            const ObjectInfo *o;

            o = object_at(pack[k]);
            if (o && o->group == group[i] && o->index == index[i]) { cauldron_add(pack[k]); break; }
        }
    }
    return 1;
}
