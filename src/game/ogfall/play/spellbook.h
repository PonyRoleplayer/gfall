#ifndef OGFALL_SPELLBOOK_H
#define OGFALL_SPELLBOOK_H

/* Readable books and the spellbook's owned spell list. No casting simulation
 * here: this slice exposes the real contents and descriptions. See docs/ogfall.md. */
#include "game/ogfall/formats/book.h"
#include "game/ogfall/play/state.h"

typedef struct OgSpellDef {
    int id, icon, element, target;
    int group[3], sub[3];
    int duration[3][3], chance[3][3], magnitude[3][5];
    char name[26];
} OgSpellDef;
void og_spells_load(void);
const OgSpellDef *og_spell_def(int id);
/* A spell of the game's own beyond SPELLS.STD (a potion's): its 89-byte record under an id of its own. 0 when full or taken. */
/* The standard spells in SPELLS.STD's order (what the original keeps as spell_records). */
int og_spell_standard_count(void);
const OgSpellDef *og_spell_standard(int i);
int og_spell_register(const df_u8 *record, int id);
/* The same, put over the spell of that id if there is one (an unknown brew is made fresh each time it is drunk). */
int og_spell_replace(const df_u8 *record, int id);
df_u32 og_spell_create(df_u32 book, int definition);
int og_spell_rename(df_u32 id, const char *name);
int og_spell_cycle_icon(df_u32 id, int step);
void og_spell_upgrade(OgState *state);
/* The registry made to hold the state's own spells (and none left from another game). */
void og_spells_sync_custom(const OgState *state);
int og_spell_rows(df_u32 book, df_u32 *ids, int capacity);
int og_spell_move(df_u32 id, int by);
int og_spell_delete(df_u32 id);
const char *og_spell_effect(int group, int sub);
#endif
