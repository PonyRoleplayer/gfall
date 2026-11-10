#include "game/ogfall/play/spellbook.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/ogfall/tables/itemdefs.h"

static OgSpellDef g_spell[128];
static int g_count;
/* How many of them came from SPELLS.STD (the rest are the game's own, a potion's). */
static int g_standard;

/* One 89-byte spell record (SPELLS.STD's, or a potion recipe's own) into a definition. */
static void parse_spell(const df_u8 *record, OgSpellDef *spell)
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

int og_spell_standard_count(void)
{
    return g_standard;
}

const OgSpellDef *og_spell_standard(int i)
{
    return i >= 0 && i < g_standard ? &g_spell[i] : 0;
}

int og_spell_register(const df_u8 *record, int id)
{
    OgSpellDef *spell;

    if (g_count >= 128 || og_spell_def(id)) { return 0; }
    spell = &g_spell[g_count++];
    parse_spell(record, spell);
    spell->id = id;
    return 1;
}

int og_spell_replace(const df_u8 *record, int id)
{
    int i;

    for (i = 0; i < g_count; i++) {
        if (g_spell[i].id == id) {
            parse_spell(record, &g_spell[i]);
            g_spell[i].id = id;
            return 1;
        }
    }
    return og_spell_register(record, id);
}

void og_spells_load(void)
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

const OgSpellDef *og_spell_def(int id)
{
    int i;
    for (i = 0; i < g_count; i++) { if (g_spell[i].id == id) { return &g_spell[i]; } }
    return 0;
}
df_u32 og_spell_create(df_u32 book, int definition)
{
    const OgSpellDef *def;
    const OgObject *parent;
    OgObject *spell;
    df_u32 rows[OG_OBJECTS], id;
    int count, i;
    def = og_spell_def(definition);
    parent = og_object(book);
    if (!def || !parent || parent->type != OG_RECORD_ITEM || parent->group != 27 || parent->index != 0) { return 0; }
    count = og_spell_rows(book,rows,OG_OBJECTS);
    if (count >= 128) { return 0; }
    id = og_object_new(OG_RECORD_SPELL,book,definition,100);
    spell = og_object_edit(id);
    if (!spell) { return 0; }
    /* Older books may have gaps after deletion. Compact before appending so
     * a new spell never shares an order value with an existing last entry. */
    for (i = 0; i < count; i++) { og_object_edit(rows[i])->flags = i; }
    df_copy(spell->name,def->name,25);
    spell->spell_icon = def->icon;
    spell->flags = count;
    return id;
}
int og_spell_rename(df_u32 id, const char *name)
{
    OgObject *spell;
    spell = og_object_edit(id);
    if (!spell || spell->type != OG_RECORD_SPELL || !name || !*name) { return 0; }
    df_copy(spell->name,name,25);
    return 1;
}
int og_spell_cycle_icon(df_u32 id, int step)
{
    OgObject *spell;
    spell = og_object_edit(id);
    if (!spell || spell->type != OG_RECORD_SPELL) { return 0; }
    spell->spell_icon = (spell->spell_icon + (step < 0 ? 68 : 1)) % 69;
    return 1;
}
void og_spell_upgrade(OgState *state)
{
    int i;
    for (i = 0; i < OG_OBJECTS; i++) {
        OgObject *spell;
        const OgSpellDef *def;
        spell = &state->object[i];
        if (!spell->id || spell->type != OG_RECORD_SPELL) { continue; }
        def = og_spell_def(spell->index);
        if (!def) { continue; }
        df_copy(spell->name,def->name,25);
        spell->spell_icon = def->icon;
    }
}
int og_spell_rows(df_u32 book, df_u32 *ids, int capacity)
{
    int i, count, at, j;
    const OgObject *spell;
    count = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        spell = &og_state()->object[i];
        if (!spell->id || spell->type != OG_RECORD_SPELL || spell->parent != book) { continue; }
        if (count >= capacity) { break; }
        at = count++;
        /* Spell order is its record flags, independent of allocator slot. */
        while (at > 0 && og_object(ids[at-1])->flags > spell->flags) { ids[at] = ids[at-1]; at--; }
        ids[at] = spell->id;
    }
    for (j = count; j < capacity; j++) { ids[j] = 0; }
    return count;
}
int og_spell_move(df_u32 id, int by)
{
    df_u32 rows[OG_OBJECTS];
    OgObject *spell;
    int count, i, other;
    spell = og_object_edit(id);
    if (!spell || spell->type != OG_RECORD_SPELL) { return 0; }
    count = og_spell_rows(spell->parent, rows, OG_OBJECTS);
    for (i = 0; i < count && rows[i] != id; i++) { }
    other = i + by;
    if (i == count || other < 0 || other >= count) { return 0; }
    rows[i] = rows[other]; rows[other] = id;
    for (i = 0; i < count; i++) { og_object_edit(rows[i])->flags = i; }
    return 1;
}
int og_spell_delete(df_u32 id)
{
    const OgObject *spell;
    spell = og_object(id);
    return spell && spell->type == OG_RECORD_SPELL && og_object_delete(id);
}
const char *og_spell_effect(int group, int sub)
{
    (void)sub;
    return og_effect_name(group);
}
