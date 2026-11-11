#include "game/ogfall/play/itemmaker.h"
#include "game/ogfall/play/spellbook.h"
#include "game/ogfall/play/magicitem.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/law.h"
#include "game/ogfall/tables/itemdefs.h"
#include "game/ogfall/tables/combatdefs.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/ogfall/tables/itemmakerdata.h"
#include "game/ogfall/tables/enchantdata.h"
#include "game/ogfall/tables/souldata.h"

/* See itemmaker.h. */

static OgItemMaker g;
static int (*g_souls)(int *creatures, int cap);

const OgItemMaker *og_itemmaker(void) { return &g; }

void og_itemmaker_set_souls(int (*list)(int *creatures, int cap)) { g_souls = list; }

/* A slot's type in the item's numbering. */
static int item_type(int s) { return g.kind[s] == 1 ? g.type[s] + 15 : g.type[s]; }

static void clear_pairs(int row)
{
    int k;

    for (k = 0; k < 5; k++) { g.pair[row][k][0] = g.pair[row][k][1] = 255; }
}

void og_itemmaker_reset(void)
{
    int s;

    mem_zero(&g, sizeof(g));
    for (s = 0; s < 10; s++) { g.kind[s] = -1; g.param[s] = -1; }
    for (s = 0; s < 12; s++) { clear_pairs(s); }
    g.pending = -1;
}

/* ---- What an item can take ---- */

int og_itemmaker_capacity(df_u32 id)
{
    const OgObject *item;
    int template, points;

    item = og_object(id);
    if (!item || item->type != OG_RECORD_ITEM) { return 0; }
    template = og_item_template(item->group, item->index);
    if (template < 0) { return 0; }
    points = g_template_enchant[template];
    if (points <= 0) { return 0; }
    /* item_init_from_template: only weapons (not arrows) and plate armour roll a material, which scales the points. */
    if ((item->group == 3 && !df_eq(og_item_def(3, item->index)->name, "Arrow")) || (item->group == 2 && item->armor_type == 2)) {
        if (item->material >= 0 && item->material < 10) { points = (points * g_ench_material[item->material]) >> 2; }
    }
    return points;
}

const OgItemEnchant *og_itemmaker_row(df_u32 id)
{
    int k;

    for (k = 0; k < OG_ENCHANTED_ITEMS; k++) {
        const OgItemEnchant *row;

        row = &og_state()->enchant[k];
        if (row->item == id && id) { return row; }
    }
    return 0;
}

int og_itemmaker_pick(df_u32 id)
{
    if (!og_itemmaker_capacity(id)) { return OG_IM_CANNOT; }
    if (og_itemmaker_row(id) || og_magic_item_def(id) >= 0) { return OG_IM_ENCHANTED; }
    /* Another item than the one in the maker: the slots do not carry over (7.10). */
    if (g.item != id) { og_itemmaker_reset(); }
    g.item = id;
    return 0;
}

int og_itemmaker_free_slots(void)
{
    int s, n;

    for (s = 0, n = 0; s < 10; s++) { if (g.kind[s] == -1) { n++; } }
    return n;
}

static int free_slot(void)
{
    int s;

    for (s = 0; s < 10; s++) { if (g.kind[s] == -1) { return s; } }
    return -1;
}

/* ---- Parameters ---- */

const char *og_itemmaker_name(int side, int kind)
{
    if (kind < 0 || kind >= (side ? 11 : 15)) { return ""; }
    return side ? g_ench_side_name[kind] : g_ench_power_name[kind];
}

int og_itemmaker_param_kind(int side, int kind)
{
    if (kind < 0 || kind >= (side ? 11 : 15)) { return 0; }
    return side ? g_ench_side_kind[kind] : g_ench_power_kind[kind];
}

static int souls_list(int *ids, int cap)
{
    return g_souls ? g_souls(ids, cap < 63 ? cap : 63) : 0;
}

static int spell_count(int list)
{
    static const int count[3] = {
        (int)sizeof(g_ench_spells0), (int)sizeof(g_ench_spells1), (int)sizeof(g_ench_spells2)
    };

    return count[list];
}

int og_itemmaker_param_count(int side, int kind)
{
    int pk, ids[63];

    pk = og_itemmaker_param_kind(side, kind);
    if (pk == 0) { return 0; }
    if (pk <= 3) { return spell_count(pk - 1); }
    if (pk == 4) { return souls_list(ids, 63); }
    return g_ench_label_count[pk - 100];
}

const char *og_itemmaker_param_name(int side, int kind, int param)
{
    int pk;

    pk = og_itemmaker_param_kind(side, kind);
    if (pk >= 1 && pk <= 3) {
        const OgSpellDef *spell;

        spell = param >= 0 && param < spell_count(pk - 1) ? og_spell_def(g_ench_spells[pk - 1][param]) : 0;
        return spell ? spell->name : "";
    }
    if (pk == 4) {
        const OgFoeDef *foe;

        foe = param >= 0 && param < 43 ? og_foe_def(param) : 0;
        return foe ? foe->name : "";
    }
    if (pk >= 100 && param >= 0 && param < g_ench_label_count[pk - 100]) { return g_ench_labels[pk - 100][param]; }
    return "";
}

static int has_pair(int t, int q)
{
    int row, k;

    for (row = 0; row < 12; row++) {
        for (k = 0; k < 5; k++) { if (g.pair[row][k][0] == t && g.pair[row][k][1] == q) { return 1; } }
    }
    return 0;
}

static int has_kind_pair(int t)
{
    int row, k;

    for (row = 0; row < 10; row++) {
        for (k = 0; k < 5; k++) { if (g.pair[row][k][0] == t && g.pair[row][k][1] == 255) { return 1; } }
    }
    return 0;
}

/* itemmaker_param_excluded: Good and Bad reputation: a group alone or All, and never the same group in both. t is the item numbering, 14 or 25. */
static int rep_excluded(int t, int q)
{
    int s, other;

    other = t == 25 ? 14 : 25;
    for (s = 0; s < 10; s++) {
        if (g.kind[s] == -1 || s == g.pending) { continue; }
        if (item_type(s) == t && g.param[s] != 5 && q == 5) { return 1; }
        if (item_type(s) == t && g.param[s] == 5) { return 1; }
        if (item_type(s) == other && g.param[s] == q) { return 1; }
    }
    return 0;
}

/* The parameters a kind can take now, by index. */
static int allowed(int side, int kind, int *out, int cap)
{
    int pk, count, q, n, ids[63], t;

    pk = og_itemmaker_param_kind(side, kind);
    if (pk == 0) { return 0; }
    t = side ? kind + 15 : kind;
    n = 0;
    if (pk == 4) {
        count = souls_list(ids, 63);
        for (q = 0; q < count && n < cap; q++) { out[n++] = ids[q]; }
        return n;
    }
    count = og_itemmaker_param_count(side, kind);
    for (q = 0; q < count && n < cap; q++) {
        /* A spell's parameter is its index in the list; the soul's rule-outs sit in rows 10 and 11 as (0, index) and (1, index). */
        if (has_pair(t, q)) { continue; }
        if ((t == 14 || t == 25) && rep_excluded(t, q)) { continue; }
        out[n++] = q;
    }
    return n;
}

int og_itemmaker_kinds(int side, int *kinds, int cap)
{
    const OgObject *item;
    int kind, t, n, count, s, scratch[40];

    item = og_object(g.item);
    n = 0;
    for (kind = 0; kind < (side ? 11 : 15) && n < cap; kind++) {
        t = side ? kind + 15 : kind;
        if (has_kind_pair(t)) { continue; }
        if (t == 20 && (!item || item->group != 3)) { continue; }
        if (t == 18 || t == 19) { continue; }
        /* itemmaker_power_excluded: a reputation kind is gone once All of the other, or of itself, is on the item. */
        if (t == 25 || t == 14) {
            for (s = 0; s < 10; s++) { if (g.kind[s] != -1 && item_type(s) == t && g.param[s] == 5) { break; } }
            if (s < 10) { continue; }
        }
        if (t == 15) { for (s = 0; s < 10; s++) { if (g.kind[s] == 1 && g.type[s] == 0) { break; } } if (s < 10) { continue; } }
        if (t == 21) { for (s = 0; s < 10; s++) { if (g.kind[s] == 1 && g.type[s] == 6) { break; } } if (s < 10) { continue; } }
        /* A kind with every parameter taken is not offered (7.3). */
        if (og_itemmaker_param_kind(side, kind)) {
            count = allowed(side, kind, scratch, 40);
            if (!count) { continue; }
        }
        kinds[n++] = kind;
    }
    return n;
}

/* ---- Adding ---- */

static void set_pair(int s, int k, int t, int p)
{
    g.pair[s][k][0] = (df_u8)(t < 0 ? 255 : t); g.pair[s][k][1] = (df_u8)(p < 0 ? 255 : p);
}

int og_itemmaker_begin(int side, int kind)
{
    int s;

    if (!g.item) { return -OG_IM_NO_ITEM; }
    if (kind < 0 || kind >= (side ? 11 : 15)) { return -OG_IM_NO_ITEM; }
    s = free_slot();
    if (s < 0) { return side ? -OG_IM_NO_SIDE_SLOT : -OG_IM_NO_POWER_SLOT; }
    g.kind[s] = side ? 1 : 0; g.type[s] = kind; g.param[s] = -1; g.soul[s] = 0;
    clear_pairs(s);
    if (og_itemmaker_param_kind(side, kind)) {
        g.pending = s;
        return s;
    }
    /* A kind with no parameter writes its own pairs: Feather Weight and Extra weight rule each other out, as do Strengthens and Weakens armor; a power that can be had once rules itself out (7.7). */
    if (!side && kind == 11) { set_pair(s, 0, 23, 255); set_pair(s, 1, 11, 255); }
    else if (!side && kind == 12) { set_pair(s, 0, 24, 255); set_pair(s, 1, 12, 255); }
    else if (!side && kind == 8) { set_pair(s, 0, 8, 255); }
    else if (!side && kind == 9) { set_pair(s, 0, 9, 255); }
    else if (side && kind == 8) { set_pair(s, 0, 11, 255); set_pair(s, 1, 23, 255); }
    else if (side && kind == 9) { set_pair(s, 0, 12, 255); set_pair(s, 1, 24, 255); }
    return s;
}

int og_itemmaker_params(int *params, int cap)
{
    int s;

    s = g.pending;
    if (s < 0) { return 0; }
    return allowed(g.kind[s], g.type[s], params, cap);
}

/* The two pairs a power or side effect's row holds for a parameter (func_00057147), or none: a row has five parameters a row, and past them nothing is read (7.4). */
static void row_pairs(int s, int side, int kind, int param)
{
    const df_u8 *rows, *row_of, *pairs;
    int row;

    row_of = side ? g_ench_side_row : g_ench_power_row;
    pairs = side ? g_ench_side_pairs : g_ench_power_pairs;
    rows = row_of;
    row = rows[kind];
    if (!row || param < 0 || param >= 5) { return; }
    row--;
    set_pair(s, 1, pairs[row * 20 + param * 4], pairs[row * 20 + param * 4 + 1]);
    set_pair(s, 2, pairs[row * 20 + param * 4 + 2], pairs[row * 20 + param * 4 + 3]);
}

/* One slot filled with a soul's power or side effect: the same pairs a manual pick writes. */
static void fill_pairs(int s)
{
    int side, kind, param;

    side = g.kind[s]; kind = g.type[s]; param = g.param[s];
    set_pair(s, 0, side ? kind + 15 : kind, param);
    if (!side && param < 0) {
        if (kind == 11) { set_pair(s, 0, 23, 255); set_pair(s, 1, 11, 255); }
        else if (kind == 12) { set_pair(s, 0, 24, 255); set_pair(s, 1, 12, 255); }
    } else if (side && param < 0) {
        if (kind == 8) { set_pair(s, 0, 11, 255); set_pair(s, 1, 23, 255); }
        else if (kind == 9) { set_pair(s, 0, 12, 255); set_pair(s, 1, 24, 255); }
    } else {
        row_pairs(s, side, kind, param);
    }
}

/* itemmaker_add_soul_powers: the powers the creature's soul grants fill the free slots (none at all when they do not fit), and its rule-outs go to the pairs. Returns 0 or -1 for no room. */
static int add_soul_powers(int soul_slot, int creature)
{
    int r, k, j, n, want, free_count, s, spell, idx;
    short type, param;

    for (r = 0; r < 12 && g_ench_soul_creature[r] != creature; r++) { }
    if (r == 12) { return 0; }
    for (k = 0; k < 2; k++) {
        int kind, arg;

        kind = g_ench_soul_rule[r * 4 + k * 2]; arg = g_ench_soul_rule[r * 4 + k * 2 + 1];
        if (kind == 255) { continue; }
        if (kind == 128 || kind == 129) {
            int list, row, slot_out;

            list = kind - 128; row = 10 + list; slot_out = 0;
            for (j = 0; j < 5 && slot_out < 5; j++) {
                spell = g_ench_soul_spells[arg * 5 + j];
                if (spell == 255) { continue; }
                for (idx = 0; idx < spell_count(list); idx++) {
                    if (g_ench_spells[list][idx] == spell) { set_pair(row, slot_out++, list, idx); break; }
                }
            }
        }
        /* The rule-out is carried by the soul's own slot, in its pairs 3 and 4. */
        set_pair(soul_slot, 3 + k, kind, arg);
    }
    want = 0;
    while (want < 5 && g_ench_soul_power[r][want][0] != -1) { want++; }
    free_count = og_itemmaker_free_slots();
    if (free_count < want) { return -1; }
    for (n = 0; n < want; n++) {
        type = g_ench_soul_power[r][n][0]; param = g_ench_soul_power[r][n][1];
        s = free_slot();
        g.soul[s] = 1;
        if (type < 15) { g.kind[s] = 0; g.type[s] = type; }
        else { g.kind[s] = 1; g.type[s] = type - 15; }
        g.param[s] = param;
        clear_pairs(s);
        fill_pairs(s);
        for (k = 0; k < 2; k++) {
            int kind;

            kind = g_ench_soul_rule[r * 4 + k * 2];
            if (kind != 255) { set_pair(s, 3 + k, kind, g_ench_soul_rule[r * 4 + k * 2 + 1]); }
        }
    }
    return 0;
}

int og_itemmaker_choose(int param)
{
    int s, side, kind, result;

    s = g.pending;
    if (s < 0) { return 0; }
    side = g.kind[s]; kind = g.type[s];
    g.pending = -1;
    result = 0;
    if (side && kind == 0) {
        /* Soul bound: the parameter is the creature; its powers come first, and the soul slot's own pairs after. */
        g.param[s] = param;
        result = add_soul_powers(s, param);
        set_pair(s, 0, 15, 255);
        return result;
    }
    g.param[s] = param;
    set_pair(s, 0, side ? kind + 15 : kind, param);
    row_pairs(s, side, kind, param);
    return result;
}

void og_itemmaker_cancel(void)
{
    int s;

    s = g.pending;
    if (s < 0) { return; }
    g.kind[s] = -1; g.type[s] = 0; g.param[s] = -1; g.soul[s] = 0;
    clear_pairs(s);
    g.pending = -1;
}

void og_itemmaker_remove(int slot)
{
    int s, k;

    if (slot < 0 || slot > 9 || g.kind[slot] == -1 || g.soul[slot]) { return; }
    if (g.kind[slot] == 1 && g.type[slot] == 0) {
        /* A soul going takes everything it granted and its rule-outs with it. */
        for (s = 0; s < 10; s++) {
            if (!g.soul[s]) { continue; }
            g.soul[s] = 0; g.kind[s] = -1; g.type[s] = 0; g.param[s] = -1;
            clear_pairs(s);
        }
        clear_pairs(10); clear_pairs(11);
    }
    g.kind[slot] = -1; g.param[slot] = -1; g.type[slot] = 0;
    clear_pairs(slot);
    for (k = 0; k < 10; k++) { if (g.pending == slot) { g.pending = -1; } }
}

/* ---- Cost ---- */

/* enchant_slot_cost: a power or a side effect, by its type and parameter. */
static int slot_cost(int side, int type, int param, int as_points)
{
    int code;

    if (!side) {
        code = g_enchant_code[type];
        if (code == 0) { return 0; }
        if (code >= 1000) { return g_enchant_by_param[code - 1000][param < 0 ? 0 : param < 12 ? param : 11]; }
        if (code >= 5 && code <= 7) { return param >= 0 && param < spell_count(code - 5) ? og_magic_spell_cost(g_ench_spells[code - 5][param]) : 0; }
        return g_enchant_fixed[code];
    }
    code = g_ench_side_code[type];
    if (code == 99) {
        int v;

        v = param >= 0 && param < 43 ? g_soul_value[param] : 0;
        if (as_points) { v /= 100; }
        return -v;
    }
    if (code >= 1000) { return g_ench_side_by_param[type][param < 0 ? 0 : param < 6 ? param : 5]; }
    return g_enchant_fixed[code];
}

int og_itemmaker_points_used(void)
{
    int s, points;

    points = 0;
    for (s = 0; s < 10; s++) {
        if (g.kind[s] == -1 || g.soul[s] || s == g.pending) { continue; }
        points += slot_cost(g.kind[s], g.type[s], g.param[s], 1);
    }
    return points;
}

int og_itemmaker_gold(void)
{
    int s, cost;

    cost = 0;
    for (s = 0; s < 10; s++) {
        if (g.kind[s] != 0 || s == g.pending) { continue; }
        cost += slot_cost(0, g.type[s], g.param[s], 0);
    }
    return cost * 10;
}

int og_itemmaker_enchant(void (*consume)(int creature))
{
    OgItemEnchant *row;
    int s, n, k, gold, free_row, soul_creature, any;

    if (!g.item) { return OG_IM_NO_ITEM; }
    any = 0;
    for (s = 0; s < 10; s++) { if (g.kind[s] != -1 && s != g.pending) { any = 1; } }
    if (!any) { return 0; }
    if (og_itemmaker_points_used() > og_itemmaker_capacity(g.item)) { return OG_IM_NO_POINTS; }
    gold = og_itemmaker_gold();
    if (!og_gold_can_afford(gold)) { return OG_IM_NO_GOLD; }
    free_row = -1;
    for (k = 0; k < OG_ENCHANTED_ITEMS; k++) {
        OgItemEnchant *candidate;

        candidate = og_item_enchant_edit(k);
        if (candidate->item == g.item) { free_row = k; break; }
        if (free_row < 0 && (!candidate->item || !og_object(candidate->item))) { free_row = k; }
    }
    if (free_row < 0) { return OG_IM_NO_GOLD; }
    og_gold_spend(gold);
    row = og_item_enchant_edit(free_row);
    mem_zero(row, sizeof(*row));
    row->item = g.item; row->identified = 1;
    for (k = 0; k < 10; k++) { row->type[k] = -1; row->param[k] = -1; }
    soul_creature = -1;
    for (s = 0, n = 0; s < 10; s++) {
        if (g.kind[s] == -1 || s == g.pending) { continue; }
        row->type[n] = g.kind[s] == 1 ? g.type[s] + 15 : g.type[s];
        row->param[n] = g.param[s];
        /* The first three powers store the spell they name, not its place in the list. */
        if (g.kind[s] == 0 && g.type[s] <= 2 && g.param[s] >= 0) { row->param[n] = g_ench_spells[g.type[s]][g.param[s]]; }
        if (g.kind[s] == 1 && g.type[s] == 0) { soul_creature = g.param[s]; }
        n++;
    }
    if (soul_creature >= 0 && consume) { consume(soul_creature); }
    og_itemmaker_reset();
    return OG_IM_DONE;
}

/* ---- What an item holds ---- */

static int strlen_of(const char *s) { int n; for (n = 0; s[n]; n++) { } return n; }

int og_itemmaker_value(df_u32 id)
{
    const OgItemEnchant *row;
    int k, value, code, param;

    row = og_itemmaker_row(id);
    if (!row) { return 0; }
    value = 0;
    for (k = 0; k < 10; k++) {
        if (row->type[k] < 0 || row->type[k] >= 15) { continue; }
        param = row->param[k];
        code = g_enchant_code[row->type[k]];
        if (code == 0) { continue; }
        if (code >= 1000) { value += g_enchant_by_param[code - 1000][param < 0 ? 0 : param < 12 ? param : 11]; }
        else if (code >= 5 && code <= 7) { value += og_magic_spell_cost(param); }
        else { value += g_enchant_fixed[code]; }
    }
    return value;
}

static int enchant_lines(const OgItemEnchant *row, char *out, int cap)
{
    int k, n;

    n = 0;
    for (k = 0; k < 10; k++) {
        int type, param;
        const char *name, *what;

        type = row->type[k]; param = row->param[k];
        if (type < 0) { continue; }
        name = type < 15 ? g_ench_power_name[type] : g_ench_side_name[type - 15];
        what = "";
        if (param >= 0) {
            if (type < 3) { const OgSpellDef *spell; spell = og_spell_def(param); what = spell ? spell->name : ""; }
            else if (type == 15) { const OgFoeDef *foe; foe = param < 43 ? og_foe_def(param) : 0; what = foe ? foe->name : ""; }
            else { what = og_itemmaker_param_name(type < 15 ? 0 : 1, type < 15 ? type : type - 15, param); }
        }
        /* The names of the first three powers end with ": "; the others are followed by a space. */
        n += log_format(out + n, cap - n, what[0] && name[0] && name[strlen_of(name) - 1] != ' ' ? "%s %s\n" : "%s%s\n", name, what);
    }
    return n;
}

int og_itemmaker_text(df_u32 id, char *out, int cap)
{
    const OgItemEnchant *row;

    row = og_itemmaker_row(id);
    if (!row || !row->identified) { return log_format(out, cap, "Powers unknown."); }
    return enchant_lines(row, out, cap);
}

int og_item_enchanted(df_u32 id)
{
    return og_itemmaker_row(id) != 0 || og_magic_item_def(id) >= 0;
}

int og_item_powers_text(df_u32 id, char *out, int cap)
{
    OgItemEnchant row;
    int def, k, type, param;

    if (og_itemmaker_row(id)) { return og_itemmaker_text(id, out, cap); }
    def = og_magic_item_def(id);
    if (def < 0) { out[0] = 0; return 0; }
    mem_zero(&row, sizeof(row));
    for (k = 0; k < 10; k++) { row.type[k] = -1; row.param[k] = -1; }
    for (k = 0; og_magic_def_enchant(def, k, &type, &param); k++) { row.type[k] = type; row.param[k] = param; }
    return enchant_lines(&row, out, cap);
}

void og_itemmaker_refresh_values(void)
{
    int k;

    for (k = 0; k < OG_ENCHANTED_ITEMS; k++) {
        const OgItemEnchant *row;
        OgObject *item;

        row = &og_state()->enchant[k];
        item = row->item ? og_object_edit(row->item) : 0;
        if (item) { item->value = og_itemmaker_value(row->item); }
    }
}
