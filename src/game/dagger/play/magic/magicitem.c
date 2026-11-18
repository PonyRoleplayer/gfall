#include "game/dagger/play/magic/magicitem.h"
#include "game/dagger/tables/magic.h"
#include "game/dagger/tables/items.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/magic/spellbook.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/magic/potion.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/tables/enchantdata.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"

/* See magicitem.h. */

static const df_u8 *g_defs;
static int g_count;

/* Item record flags are serialized. Older magic items retain their known powers. */
#define MAGIC_UNIDENTIFIED 0x20000
#define MAGIC_DEF_SHIFT 20
#define MAGIC_DEF_MASK 63

void magic_defs_set(const df_u8 *bytes, df_u32 size)
{
    g_defs = 0;
    g_count = 0;
    if (!bytes || size < 4) {
        return;
    }
    g_count = (int)(bytes[0] | bytes[1] << 8 | bytes[2] << 16 | bytes[3] << 24);
    if (g_count < 0 || (df_u32)g_count * MAGIC_DEF_BYTES + 4u > size) {
        g_count = 0;
        return;
    }
    g_defs = bytes + 4;
}

int magic_def_count(void)
{
    return g_count;
}

static const df_u8 *def(int i)
{
    return i >= 0 && i < g_count ? g_defs + i * MAGIC_DEF_BYTES : 0;
}

const char *magic_def_name(int i)
{
    const df_u8 *d;

    d = def(i);
    return d ? (const char *)d : "";
}

int magic_def_artifact(int i)
{
    const df_u8 *d;

    d = def(i);
    return d && d[MAGIC_DEF_ARTIFACT] != 0;
}

/* enchant_spell_cost: the standard spell of this id costed for a character with every skill at 50, ten times; 0 for no such spell. */
int magic_spell_cost(int id)
{
    CharacterState dummy;
    int i;
    const SpellDef *s;

    s = spell_def(id);
    if (!s || id >= POTION_SPELL) {
        return 0;
    }
    dummy = *character_state();
    for (i = 0; i < SKILLS; i++) {
        dummy.skill[i] = ENCHANT_SPELL_SKILL;
    }
    return spell_cost(s, &dummy) * ENCHANT_SPELL_COST_MUL;
}

int magic_def_value(int i)
{
    const df_u8 *d;
    int k, type, param, value, code;

    d = def(i);
    if (!d) {
        return 0;
    }
    value = 0;
    for (k = 0; k < MAGIC_DEF_ENCHANTS_MOST; k++) {
        type = d[MAGIC_DEF_ENCHANTS + k * 2];
        param = d[MAGIC_DEF_ENCHANTS + 1 + k * 2];
        if (type == MAGIC_DEF_END) {
            break;
        }
        /* The enchantment's type is a signed short in the item; 255 is the end here and 16 and over are not costed. */
        if (type >= ENCHANT_TYPES) {
            continue;
        }
        code = g_enchant_code[type];
        if (code == 0) {
            continue;
        }
        if (code < ENCHANT_CODE_PARAM) {
            /* enchant_value_slot_cost: 5 to 7 a spell's cost, else the fixed cost of the code. A parameter of 255 (to be chosen) names no spell. */
            value += code >= ENCHANT_CODE_SPELL_FIRST && code <= ENCHANT_CODE_SPELL_LAST ? magic_spell_cost(param) : g_enchant_fixed[code];
        } else {
            value += g_enchant_by_param[code - ENCHANT_CODE_PARAM][param < ENCHANT_PARAMS ? param : ENCHANT_PARAMS - 1];
        }
    }
    return value;
}

static int pick_base_kind(int group)
{
    int kind;

    kind = 3;
    if (group == 0) {
        kind = g_magic_kind0[random_range(0, (int)sizeof(g_magic_kind0) - 1)];
    } else if (group == 1) {
        kind = g_magic_kind1[random_range(0, (int)sizeof(g_magic_kind1) - 1)];
    }
    if (kind == 12 || kind == 6) {
        kind = character_state()->female ? ITEM_GROUP_WOMENS_CLOTHING : ITEM_GROUP_MENS_CLOTHING;
    }
    return kind;
}

df_u32 magic_item_create(df_u32 container, int which)
{
    int n, i, kinds;
    const df_u8 *d;
    df_u32 id;
    ObjectInfo *item;

    n = 0;
    for (i = 0; i < g_count; i++) {
        n += !magic_def_artifact(i);
    }
    if (!n) {
        return 0;
    }
    n = which < 0 ? random_range(0, n - 1) : which;
    for (i = 0; i < g_count; i++) {
        if (!magic_def_artifact(i)) {
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

        kind = pick_base_kind(d[MAGIC_DEF_GROUP]);
        kinds = item_kinds(kind);
        if (kinds < 1) {
            return 0;
        }
        id = item_create(container, kind, random_range(0, kinds - 1), -1);
        item = object_edit(id);
        if (!item) {
            return 0;
        }
        if (item->group == ITEM_GROUP_WEAPON && item->index == WEAPON_ARROW) {
            object_delete(id);
            item = 0;
        }
    } while (!item);
    df_copy(item->name, (const char *)d, (int)sizeof(item->name));
    item->condition = item->max_condition = (int)(short)(d[MAGIC_DEF_CONDITION] | d[MAGIC_DEF_CONDITION + 1] << 8);
    item->material = d[MAGIC_DEF_MATERIAL];
    if (item->material == MATERIAL_IRON && (item->group == ITEM_GROUP_ARMOR || item->group == ITEM_GROUP_WEAPON)) {
        /* item_roll_material: left as the base item rolled it. */
    }
    if (item->group == ITEM_GROUP_ARMOR) {
        item->armor_type = ARMOR_PLATE;
    }
    if (item->group != ITEM_GROUP_USELESS && item->group != ITEM_GROUP_BOOK && item->group != ITEM_GROUP_PAINTING && item->group != ITEM_GROUP_MISC) {
        item->message = i + 1;
    }
    item->value = magic_def_value(i);
    if (i < MAGIC_DEF_MASK) { item->flags |= (i + 1) << MAGIC_DEF_SHIFT; }
    item->flags |= MAGIC_UNIDENTIFIED;
    return id;
}

/* Artifact ordinals count only the artifact entries. Their original artwork
 * comes from TEXTURE.432/433, using the record for the MAGIC.DEF entry. */
df_u32 magic_artifact_create(df_u32 container, int ordinal)
{
    static const df_u8 picture[] = {12,13,10,8,19,16,25,18,21,2,24,26,0,15,3,9,23,17,7,1,22,20,5};
    ObjectInfo *item;
    const df_u8 *d;
    df_u32 id;
    int i, n, count;

    count = 0;
    for (i = 0; i < g_count; i++) { count += magic_def_artifact(i); }
    if (!count) { return 0; }
    if (ordinal < 0) { ordinal = random_range(0,count-1); }
    for (i = n = 0; i < g_count; i++) {
        if (magic_def_artifact(i) && n++ == ordinal) { break; }
    }
    d = def(i);
    if (!d || i >= MAGIC_DEF_MASK) { return 0; }
    id = item_create(container,d[MAGIC_DEF_GROUP],d[MAGIC_DEF_INDEX],d[MAGIC_DEF_MATERIAL] < MATERIALS ? d[MAGIC_DEF_MATERIAL] : -1);
    item = object_edit(id);
    if (!item) { return 0; }
    df_copy(item->name,(const char *)d,sizeof(item->name));
    item->condition = item->max_condition = df_ru16(d+MAGIC_DEF_CONDITION);
    item->value = df_ru32(d+MAGIC_DEF_VALUE);
    item->flags |= (i + 1) << MAGIC_DEF_SHIFT;
    if (ordinal < (int)sizeof(picture)) { item->inventory_image = (ARTIFACT_IMAGE_ARCHIVE + character_state()->female)*IMAGE_ARCHIVE_STRIDE + picture[ordinal]; }
    return id;
}

/* The k-th enchantment of an entry (a type, 15 and over a side effect, and a parameter, -1 for none): 0 past the last. */
int magic_def_enchant(int i, int k, int *type, int *param)
{
    const df_u8 *d;

    d = def(i);
    if (!d || k < 0 || k >= MAGIC_DEF_ENCHANTS_MOST || d[MAGIC_DEF_ENCHANTS + k * 2] == MAGIC_DEF_END) { return 0; }
    *type = d[MAGIC_DEF_ENCHANTS + k * 2]; *param = d[MAGIC_DEF_ENCHANTS + 1 + k * 2] == MAGIC_DEF_END ? -1 : d[MAGIC_DEF_ENCHANTS + 1 + k * 2];
    return 1;
}

int magic_item_def(df_u32 item)
{
    const ObjectInfo *o;
    int entry;

    o = object_at(item);
    if (!o || o->type != RECORD_ITEM) { return -1; }
    entry = (o->flags >> MAGIC_DEF_SHIFT) & MAGIC_DEF_MASK;
    if (entry) { return def(entry-1) ? entry-1 : -1; }
    /* The message field held MAGIC.DEF indices before item flags did. */
    if (o->group == ITEM_GROUP_USELESS || o->group == ITEM_GROUP_BOOK || o->group == ITEM_GROUP_PAINTING || o->group == ITEM_GROUP_MISC ||
        (o->group == ITEM_GROUP_CURIO && o->index == CURIO_ARTIFACT_EXEMPT) || o->message < 1 || o->message > g_count) {
        return -1;
    }
    return o->message - 1;
}

int magic_item_unknown(df_u32 id)
{
    const ObjectInfo *item;

    item = object_at(id);
    return item && (item->flags & MAGIC_UNIDENTIFIED) && magic_item_def(id) >= 0;
}

int magic_item_identify_price(df_u32 id)
{
    return magic_item_unknown(id) ? (int)(((df_u32)object_at(id)->value * IDENTIFY_PRICE_NUM) >> 8) : 0;
}

int magic_item_identify(df_u32 id)
{
    if (!magic_item_unknown(id)) { return 0; }
    object_edit(id)->flags &= ~MAGIC_UNIDENTIFIED;
    return 1;
}

int magic_artifact_power(df_u32 id)
{
    int entry, type, param;

    entry = magic_item_def(id);
    return magic_def_artifact(entry) && magic_def_enchant(entry,0,&type,&param) && type == ENCHANT_SPECIAL_ADVANTAGE ? param : -1;
}

df_u32 magic_artifact_worn(int power)
{
    int slot;
    df_u32 id;

    for (slot = 0; slot < EQUIP_SLOTS; slot++) {
        id = game_state()->equipped[slot];
        if (id && magic_artifact_power(id) == power) { return id; }
    }
    return 0;
}

int magic_artifact_text(df_u32 id)
{
    int index, i, ordinal;

    index = magic_item_def(id);
    if (!magic_def_artifact(index)) { return 0; }
    for (i = ordinal = 0; i < index; i++) { ordinal += magic_def_artifact(i); }
    return TEXT_ARTIFACT_LORE + ordinal;
}

void magic_shop_stock(df_u32 container, int quality, int soul_traps)
{
    int i;

    for (i = 0; i < (quality >> 1) + 1; i++) {
        magic_item_create(container, -1);
    }
    item_create(container, ITEM_GROUP_MISC, MISC_SPELLBOOK, -1);
    if (soul_traps) {
        soul_trap_stock(container, quality);
    }
}
