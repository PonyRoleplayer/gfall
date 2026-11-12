#ifndef OGFALL_MAGICITEM_H
#define OGFALL_MAGICITEM_H

#include "engine/core/df.h"

/*
 * Magic items: the ones MAGIC.DEF describes, as a shop makes them (item_make_magic, shop_stock_magic, guild_buy_magic_items: src/hand/run_0006077F.c, equip.c 450,
 * src/lifted/guilds.c 162) and prices them (enchant_item_value, itemmakr.c 607). Slice 10 of people and law.
 *
 * MAGIC.DEF is a count and 62-byte entries: a name, whether it is an artifact, the group of base item and its index, ten enchantments (a type, 255 ending the list,
 * and a parameter), a condition, a value and a material. A magic item is one of the entries that are not artifacts, made on a random item of the kinds its group
 * allows. Its enchantments are the entry's, so the item keeps only which entry it is (one more, in its message field); what the enchantments do when it is worn or struck is not built.
 */
#define OG_MAGIC_DEF_BYTES 62

/* MAGIC.DEF's bytes, kept by the caller. */
void og_magic_defs_set(const df_u8 *bytes, df_u32 size);
int og_magic_def_count(void);
/* An entry's name; and whether it is an artifact. */
const char *og_magic_def_name(int def);
int og_magic_def_artifact(int def);

/* enchant_item_value: what an entry's enchantments are worth (the item's price; its base price is not kept). */
int og_magic_def_value(int def);

/* item_make_magic(-1) or (which): an item of a random non-artifact entry (or the which-th) made into container; 0 when there is none or no room. */
df_u32 og_magic_item_create(df_u32 container, int which);
/* The k-th enchantment of an entry: its type (15 and over a side effect) and parameter (-1 for none). 0 past the last. */
int og_magic_def_enchant(int def, int k, int *type, int *param);
/* The entry a magic item is of, -1 for another item. */
int og_magic_item_def(df_u32 item);

/* enchant_spell_cost: a standard spell's cost for a character with every skill at 50, ten times; 0 when there is no such spell. */
int og_magic_spell_cost(int id);

/* shop_stock_magic: quality / 2 + 1 magic items and a spellbook, and, if asked (a Mages Guild's rank 4), as many soul traps (a quarter holding a soul). */
void og_magic_shop_stock(df_u32 container, int quality, int soul_traps);

#endif
