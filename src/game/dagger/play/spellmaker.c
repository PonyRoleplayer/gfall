#include "game/dagger/play/spellmaker.h"
#include "game/dagger/play/spellbook.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/play.h"
#include "game/dagger/tables/magicdefs.h"
#include "game/dagger/tables/itemdefs.h"
#include "engine/core/mem.h"

/* See spellmaker.h. */

static OgMaker g;

const OgMaker *og_maker(void) { return &g; }

static int type_of(int slot) { return g.spell[slot * 2]; }
static int sub_of(int slot) { return g.spell[slot * 2 + 1]; }
static int cost_of(int slot) { return (short)(g.spell[8 + slot * 2] | (g.spell[9 + slot * 2] << 8)); }
static void store_cost(int slot, int value)
{
    g.spell[8 + slot * 2] = (df_u8)(value & 255); g.spell[9 + slot * 2] = (df_u8)((value >> 8) & 255);
}

void og_maker_new(void)
{
    int k;

    mem_zero(&g, sizeof(g));
    for (k = 0; k < 89; k++) { g.spell[k] = 1; }
    for (k = OG_MAKER_NAME_AT; k < OG_MAKER_NAME_AT + 24; k++) { g.spell[k] = 0; }
    g.spell[7] = 0; g.spell[6] = 4;
    for (k = 0; k < 6; k++) { g.spell[k] = 255; }
    for (k = 8; k < 14; k++) { g.spell[k] = 0; }
    g.slot = 0; g.kind = 0;
}

int og_maker_targets(void)
{
    int mode, k, class;

    mode = 2;
    for (k = 0; k < 3; k++) {
        if (type_of(k) == 255) { continue; }
        class = og_effect_target_class(type_of(k));
        if (class == 2) { continue; }
        mode = class;
    }
    return mode;
}

int og_maker_elements(void)
{
    int best, v, k;

    best = 0;
    for (k = 0; k < 3; k++) {
        v = og_effect_element_level(type_of(k));
        if (v > best) { best = v; }
    }
    return best;
}

int og_maker_free_slot(void)
{
    int k;

    for (k = 0; k < 3; k++) { if (type_of(k) == 255) { return k; } }
    return -1;
}

int og_maker_effect_list(int *types, int cap)
{
    int mode, i, n, class;

    mode = og_maker_targets();
    n = 0;
    for (i = 0; i < 51 && n < cap; i++) {
        if (!og_effect_name(i)[0]) { continue; }
        class = og_effect_target_class(i);
        if (!(class == 2 || mode == 2 || class == mode)) { continue; }
        types[n++] = i;
    }
    return n;
}

int og_maker_subtype_count(int type)
{
    int n;

    for (n = 0; n < 12 && og_effect_sub_name(type, n); n++) { }
    return n;
}

int og_maker_pick_effect(int type)
{
    int slot, mode;

    slot = og_maker_free_slot();
    if (slot < 0) { return -1; }
    g.slot = slot;
    g.spell[slot * 2] = (df_u8)type;
    if (og_maker_elements() == 0) { g.spell[6] = 4; }
    mode = og_maker_targets();
    if (mode != 2) {
        if (mode == 0 && g.spell[7] != 0) { g.spell[7] = 0; }
        if (mode == 1 && g.spell[7] == 0) { g.spell[7]++; }
    }
    /* With no subtypes the effect's numbers are chosen at once; else after the subtype. */
    if (!og_maker_subtype_count(type)) { g.kind = og_effect_flags(type, 0); }
    return slot;
}

void og_maker_pick_subtype(int sub)
{
    int type;

    type = type_of(g.slot);
    g.spell[g.slot * 2 + 1] = (df_u8)sub;
    g.kind = og_effect_flags(type, sub);
}

void og_maker_edit(int slot)
{
    int sub;

    if (slot < 0 || slot > 2 || type_of(slot) == 255) { return; }
    g.slot = slot;
    sub = sub_of(slot);
    if (sub == 255) { sub = 0; }
    g.kind = og_effect_flags(type_of(slot), sub);
    if (!g.kind) { og_maker_recompute(); }
}

void og_maker_delete(int slot)
{
    int k;

    if (slot < 0 || slot > 2) { return; }
    g.spell[slot * 2] = g.spell[slot * 2 + 1] = 255;
    for (k = 0; k < 3; k++) { g.spell[14 + slot * 3 + k] = 1; g.spell[23 + slot * 3 + k] = 1; }
    for (k = 0; k < 5; k++) { g.spell[32 + slot * 5 + k] = 1; }
    store_cost(slot, 0);
}

void og_maker_select(int slot) { if (slot >= 0 && slot < 3) { g.slot = slot; } }

void og_maker_close_panel(void) { g.kind = 0; }

/* Handler index -> the byte, its limit and its pair offset (spellmaker_duration_base .. spellmaker_magnitude_per_level). */
static int spin_at(int index, int *limit, int *pair)
{
    static const int base[11] = {14, 14, 14, 23, 23, 23, 32, 32, 32, 32, 32};
    static const int step[11] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 3, 4};
    static const int top[11] = {60, 60, 20, 100, 100, 20, 100, 100, 100, 100, 20};
    static const int offset[11] = {0, 0, 0, 0, 0, 0, 1, -1, 1, -1, 0};

    *limit = top[index]; *pair = offset[index];
    return base[index] + (index < 3 ? g.slot * 3 : index < 6 ? g.slot * 3 : g.slot * 5) + step[index];
}

int og_maker_spin_allowed(int index)
{
    int r;

    r = index % 11;
    /* The original's own test: the chance bit also admits the duration spinners, the magnitude bit every one. */
    return (r < 3 && (g.kind & 1)) || (r < 6 && (g.kind & 2)) || (g.kind & 4);
}

void og_maker_adjust(int index, int delta)
{
    int at, limit, pair;
    df_u8 *value;

    if (index < 0 || index > 10 || !g.kind) { return; }
    at = spin_at(index, &limit, &pair);
    value = &g.spell[at];
    *value = (df_u8)(*value + delta);
    if (*value < 1) { *value = 1; }
    if (*value > limit) { *value = (df_u8)limit; }
    if (pair < 0 && value[pair] > *value) { value[pair] = *value; }
    if (pair > 0 && value[pair] < *value) { value[pair] = *value; }
}

int og_maker_value(int index)
{
    int limit, pair;

    if (index < 0 || index > 10) { return 0; }
    return g.spell[spin_at(index, &limit, &pair)];
}

void og_maker_recompute(void)
{
    int last, k, d[3], ch[3], m[5];
    const df_i16 *record;

    last = -1;
    for (k = 0; k < 3; k++) { if (type_of(k) != 255) { last = k; } }
    if (last < 0 || type_of(g.slot) == 255) { return; }
    record = og_effect_costs(type_of(last), sub_of(last));
    for (k = 0; k < 3; k++) { d[k] = g.spell[14 + g.slot * 3 + k]; ch[k] = g.spell[23 + g.slot * 3 + k]; }
    for (k = 0; k < 5; k++) { m[k] = g.spell[32 + g.slot * 5 + k]; }
    store_cost(g.slot, og_effect_cost_raw(type_of(g.slot), record, d, ch, m));
}

void og_maker_set_element(int element)
{
    if (element < 0 || element > 4) { return; }
    if (element == 4 || og_maker_elements() > 0) { g.spell[6] = (df_u8)element; }
}

void og_maker_set_target(int target)
{
    int mode;

    if (target < 0 || target > 4) { return; }
    mode = og_maker_targets();
    if (target == 0 ? mode != 1 : mode != 0) { g.spell[7] = (df_u8)target; }
}

void og_maker_icon(int step)
{
    g.spell[72] = (df_u8)((g.spell[72] + OG_MAKER_ICONS + step) % OG_MAKER_ICONS);
}

void og_maker_set_name(const char *name)
{
    int i;

    for (i = 0; i < OG_MAKER_NAME_LEN && name && name[i]; i++) { g.spell[OG_MAKER_NAME_AT + i] = (df_u8)name[i]; }
    for (; i < 25; i++) { g.spell[OG_MAKER_NAME_AT + i] = 0; }
}

int og_maker_price(void)
{
    return (cost_of(0) + cost_of(1) + cost_of(2)) * og_target_factor(g.spell[7]) * 2;
}

int og_maker_shown(void)
{
    return (((cost_of(0) + cost_of(1) + cost_of(2)) * og_target_factor(g.spell[7])) >> 1) << 2;
}

int og_maker_spell_points(void)
{
    OgSpellDef def;
    int k, n;

    mem_zero(&def, sizeof(def));
    def.target = g.spell[7]; def.element = g.spell[6];
    for (k = 0; k < 3; k++) {
        def.group[k] = g.spell[k * 2]; def.sub[k] = g.spell[k * 2 + 1];
        for (n = 0; n < 3; n++) { def.duration[k][n] = g.spell[14 + k * 3 + n]; def.chance[k][n] = g.spell[23 + k * 3 + n]; }
        for (n = 0; n < 5; n++) { def.magnitude[k][n] = g.spell[32 + k * 5 + n]; }
    }
    return og_spell_cost(&def, og_character());
}

int og_maker_buy(void)
{
    OgCustomSpell *custom;
    df_u32 book;
    int price, k, slot, count;
    df_u32 rows[OG_OBJECTS];

    if (type_of(0) == 255 && type_of(1) == 255 && type_of(2) == 255) { return 0; }
    if (!g.spell[OG_MAKER_NAME_AT]) { return 1704; }
    price = og_maker_price();
    if (!og_gold_can_afford(price)) { return 1702; }
    book = og_inventory_book();
    if (!book) { return -1; }
    count = og_spell_rows(book, rows, OG_OBJECTS);
    slot = -1;
    for (k = 0; k < OG_CUSTOM_SPELLS; k++) { if (!og_custom_spell_edit(k)->used) { slot = k; break; } }
    if (count > 128 || slot < 0) { return 1709; }
    og_gold_spend(price);
    custom = og_custom_spell_edit(slot);
    custom->used = 1;
    mem_copy(custom->record, g.spell, 89);
    og_spells_sync_custom(og_state());
    og_spell_create(book, OG_CUSTOM_SPELL_ID + slot);
    og_sound(206);
    og_maker_new();
    return 1705;
}
