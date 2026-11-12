#include "game/dagger/play/spellbook.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/dagger/tables/itemdefs.h"

static SpellDef g_spell[256];
static int g_count;
/* How many of them came from SPELLS.STD (the rest are the game's own, a potion's). */
static int g_standard;

/* One 89-byte spell record (SPELLS.STD's, or a potion recipe's own) into a definition. */
static void parse_spell(const df_u8 *record, SpellDef *spell)
{
    int k;

    spell->id = record[73]; spell->icon = record[72];
    spell->target = record[7]; spell->element = record[6];
    for (k = 0; k < 3; k++) {
        int n;
        spell->group[k] = record[k * 2]; spell->sub[k] = record[k * 2 + 1];
        for (n = 0; n < 3; n++) {
            spell->duration[k][n] = (signed char)record[14+k*3+n]; spell->chance[k][n] = (signed char)record[23+k*3+n];
        }
        for (n = 0; n < 5; n++) { spell->magnitude[k][n] = (signed char)record[32+k*5+n]; }
    }
    mem_copy(spell->name, record + 47, 25); spell->name[25] = 0;
}

int spell_standard_count(void)
{
    return g_standard;
}

const SpellDef *spell_standard(int i)
{
    return i >= 0 && i < g_standard ? &g_spell[i] : 0;
}

int spell_register(const df_u8 *record, int id)
{
    SpellDef *spell;

    if (g_count >= 256 || spell_def(id)) { return 0; }
    spell = &g_spell[g_count++];
    parse_spell(record, spell);
    spell->id = id;
    return 1;
}

int spell_replace(const df_u8 *record, int id)
{
    int i;

    for (i = 0; i < g_count; i++) {
        if (g_spell[i].id == id) {
            parse_spell(record, &g_spell[i]);
            g_spell[i].id = id;
            return 1;
        }
    }
    return spell_register(record, id);
}

void spells_load(void)
{
    const df_u8 *bytes;
    df_u32 size;
    int i;
    g_count = 0;
    bytes = vfs_read("SPELLS.STD", &size);
    if (!bytes || size % 89 || size / 89 > 89) { return; }
    for (i = 0; i < (int)(size / 89); i++) {
        parse_spell(bytes + i * 89, &g_spell[g_count++]);
    }
    g_standard = g_count;
}

const SpellDef *spell_def(int id)
{
    int i;
    for (i = 0; i < g_count; i++) { if (g_spell[i].id == id) { return &g_spell[i]; } }
    return 0;
}
df_u32 spell_create(df_u32 book, int definition)
{
    const SpellDef *def;
    const ObjectInfo *parent;
    ObjectInfo *spell;
    df_u32 rows[OBJECTS], id;
    int count, i;
    def = spell_def(definition);
    parent = object_at(book);
    if (!def || !parent || parent->type != RECORD_ITEM || parent->group != 27 || parent->index != 0) { return 0; }
    count = spell_rows(book,rows,OBJECTS);
    if (count >= 128) { return 0; }
    id = object_new(RECORD_SPELL,book,definition,100);
    spell = object_edit(id);
    if (!spell) { return 0; }
    /* Older books may have gaps after deletion. Compact before appending so
     * a new spell never shares an order value with an existing last entry. */
    for (i = 0; i < count; i++) { object_edit(rows[i])->flags = i; }
    df_copy(spell->name,def->name,25);
    spell->spell_icon = def->icon;
    spell->flags = count;
    return id;
}
int spell_rename(df_u32 id, const char *name)
{
    ObjectInfo *spell;
    spell = object_edit(id);
    if (!spell || spell->type != RECORD_SPELL || !name || !*name) { return 0; }
    df_copy(spell->name,name,25);
    return 1;
}
int spell_cycle_icon(df_u32 id, int step)
{
    ObjectInfo *spell;
    spell = object_edit(id);
    if (!spell || spell->type != RECORD_SPELL) { return 0; }
    spell->spell_icon = (spell->spell_icon + (step < 0 ? 68 : 1)) % 69;
    return 1;
}
void spell_upgrade(GameState *state)
{
    int i;
    for (i = 0; i < OBJECTS; i++) {
        ObjectInfo *spell;
        const SpellDef *def;
        spell = &state->object[i];
        if (!spell->id || spell->type != RECORD_SPELL) { continue; }
        def = spell_def(spell->index);
        if (!def) { continue; }
        df_copy(spell->name,def->name,25);
        spell->spell_icon = def->icon;
    }
}
int spell_rows(df_u32 book, df_u32 *ids, int capacity)
{
    int i, count, at, j;
    const ObjectInfo *spell;
    count = 0;
    for (i = 0; i < OBJECTS; i++) {
        spell = &game_state()->object[i];
        if (!spell->id || spell->type != RECORD_SPELL || spell->parent != book) { continue; }
        if (count >= capacity) { break; }
        at = count++;
        /* Spell order is its record flags, independent of allocator slot. */
        while (at > 0 && object_at(ids[at-1])->flags > spell->flags) { ids[at] = ids[at-1]; at--; }
        ids[at] = spell->id;
    }
    for (j = count; j < capacity; j++) { ids[j] = 0; }
    return count;
}
int spell_move(df_u32 id, int by)
{
    df_u32 rows[OBJECTS];
    ObjectInfo *spell;
    int count, i, other;
    spell = object_edit(id);
    if (!spell || spell->type != RECORD_SPELL) { return 0; }
    count = spell_rows(spell->parent, rows, OBJECTS);
    for (i = 0; i < count && rows[i] != id; i++) { }
    other = i + by;
    if (i == count || other < 0 || other >= count) { return 0; }
    rows[i] = rows[other]; rows[other] = id;
    for (i = 0; i < count; i++) { object_edit(rows[i])->flags = i; }
    return 1;
}
int spell_delete(df_u32 id)
{
    const ObjectInfo *spell;
    spell = object_at(id);
    return spell && spell->type == RECORD_SPELL && object_delete(id);
}
const char *spell_effect(int group, int sub)
{
    (void)sub;
    return effect_name(group);
}

/* The player's own spells (spellmaker_buy) kept in the state and registered under CUSTOM_SPELL_ID + slot; the registry is put to match a state. */
void spells_sync_custom(const GameState *state)
{
    int i, k;

    for (i = 0; i < g_count; ) {
        if (g_spell[i].id >= CUSTOM_SPELL_ID && g_spell[i].id < CUSTOM_SPELL_ID + CUSTOM_SPELLS) {
            for (k = i; k + 1 < g_count; k++) { g_spell[k] = g_spell[k + 1]; }
            g_count--;
        } else { i++; }
    }
    for (k = 0; state && k < CUSTOM_SPELLS; k++) {
        if (state->custom_spell[k].used) { spell_register(state->custom_spell[k].record, CUSTOM_SPELL_ID + k); }
    }
}
