#include "game/ogfall/play/magicitem.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/spellbook.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/potion.h"
#include "game/ogfall/tables/itemdefs.h"
#include "game/ogfall/tables/enchantdata.h"
#include "engine/core/mem.h"

/* See magicitem.h. */

static const df_u8 *g_defs;
static int g_count;

void og_magic_defs_set(const df_u8 *bytes, df_u32 size)
{
    g_defs = 0;
    g_count = 0;
    if (!bytes || size < 4) {
        return;
    }
    g_count = (int)(bytes[0] | bytes[1] << 8 | bytes[2] << 16 | bytes[3] << 24);
    if (g_count < 0 || (df_u32)g_count * OG_MAGIC_DEF_BYTES + 4u > size) {
        g_count = 0;
        return;
    }
    g_defs = bytes + 4;
}

int og_magic_def_count(void)
{
    return g_count;
}

static const df_u8 *def(int i)
{
    return i >= 0 && i < g_count ? g_defs + i * OG_MAGIC_DEF_BYTES : 0;
}

const char *og_magic_def_name(int i)
{
    const df_u8 *d;

    d = def(i);
    return d ? (const char *)d : "";
}

int og_magic_def_artifact(int i)
{
    const df_u8 *d;

    d = def(i);
    return d && d[32] != 0;
}

/* enchant_spell_cost: the standard spell of this id costed for a character with every skill at 50, ten times; 0 for no such spell. */
static int spell_cost_for(int id)
{
    OgCharacter dummy;
    int i;
    const OgSpellDef *s;

    s = og_spell_def(id);
    if (!s || id >= OG_POTION_SPELL) {
        return 0;
    }
    dummy = *og_character();
    for (i = 0; i < OG_SKILLS; i++) {
        dummy.skill[i] = 50;
    }
    return og_spell_cost(s, &dummy) * 10;
}

int og_magic_def_value(int i)
{
    const df_u8 *d;
    int k, type, param, value, code;

    d = def(i);
    if (!d) {
        return 0;
    }
    value = 0;
    for (k = 0; k < 10; k++) {
        type = d[35 + k * 2];
        param = d[36 + k * 2];
        if (type == 255) {
            break;
        }
        /* The enchantment's type is a signed short in the item; 255 is the end here and 16 and over are not costed. */
        if (type >= 16) {
            continue;
        }
        code = g_enchant_code[type];
        if (code == 0) {
            continue;
        }
        if (code < 100) {
            /* enchant_value_slot_cost: 5 to 7 a spell's cost, else the fixed cost of the code. A parameter of 255 (to be chosen) names no spell. */
            value += code >= 5 && code <= 7 ? spell_cost_for(param) : g_enchant_fixed[code];
        } else {
            value += g_enchant_by_param[code - 1000][param < 12 ? param : 11];
        }
    }
    return value;
}

static int pick_base_kind(int group)
{
    int kind;

    kind = 3;
    if (group == 0) {
        kind = g_magic_kind0[og_random_range(0, 6)];
    } else if (group == 1) {
        kind = g_magic_kind1[og_random_range(0, 4)];
    }
    if (kind == 12 || kind == 6) {
        kind = og_character()->female ? 12 : 6;
    }
    return kind;
}

df_u32 og_magic_item_create(df_u32 container, int which)
{
    int n, i, kinds;
    const df_u8 *d;
    df_u32 id;
    OgObject *item;

    n = 0;
    for (i = 0; i < g_count; i++) {
        n += !og_magic_def_artifact(i);
    }
    if (!n) {
        return 0;
    }
    n = which < 0 ? og_random_range(0, n - 1) : which;
    for (i = 0; i < g_count; i++) {
        if (!og_magic_def_artifact(i)) {
            if (n == 0) {
                break;
            }
            n--;
        }
    }
    d = def(i);
    if (!d) {
        return 0;
    }
    /* A base item of the kind, drawn again while it is an arrow stack. */
    do {
        int kind;

        kind = pick_base_kind(d[33]);
        kinds = og_item_kinds(kind);
        if (kinds < 1) {
            return 0;
        }
        id = og_item_create(container, kind, og_random_range(0, kinds - 1), -1);
        item = og_object_edit(id);
        if (!item) {
            return 0;
        }
        if (item->group == 3 && item->index == 18) {
            og_object_delete(id);
            item = 0;
        }
    } while (!item);
    df_copy(item->name, (const char *)d, (int)sizeof(item->name));
    item->condition = item->max_condition = (int)(short)(d[0x37] | d[0x38] << 8);
    item->material = d[0x3d];
    if (item->material == 0 && (item->group == 2 || item->group == 3)) {
        /* item_roll_material: left as the base item rolled it. */
    }
    if (item->group == 2) {
        item->armor_type = 2;
    }
    if (item->group != 1 && item->group != 7 && item->group != 13 && item->group != 27) {
        item->message = i + 1;
    }
    item->value = og_magic_def_value(i);
    return id;
}

int og_magic_item_def(df_u32 item)
{
    const OgObject *o;

    o = og_object(item);
    if (!o || o->type != OG_RECORD_ITEM || o->group == 1 || o->group == 7 || o->group == 13 || o->group == 27 || o->message < 1 || o->message > g_count) {
        return -1;
    }
    return o->message - 1;
}

void og_magic_shop_stock(df_u32 container, int quality, int soul_traps)
{
    int i;

    for (i = 0; i < (quality >> 1) + 1; i++) {
        og_magic_item_create(container, -1);
    }
    og_item_create(container, 27, 0, -1);
    if (soul_traps) {
        og_soul_trap_stock(container, quality);
    }
}
