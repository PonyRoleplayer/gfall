#ifndef GFALL_MAGICITEM_H
#define GFALL_MAGICITEM_H

#include "engine/core/df.h"

/*
 * Magic items: the ones MAGIC.DEF describes, as a shop makes them (item_make_magic, shop_stock_magic, guild_buy_magic_items: src/hand/run_0006077F.c, equip.c 450,
 * src/lifted/guilds.c 162) and prices them (enchant_item_value, itemmakr.c 607). Slice 10 of people and law.
 *
 * MAGIC.DEF is a count and 62-byte entries: a name, whether it is an artifact, the group of base item and its index, ten enchantments (a type, 255 ending the list,
 * and a parameter), a condition, a value and a material. A magic item is one of the entries that are not artifacts, made on a random item of the kinds its group
 * allows. Its enchantments are the entry's, so the item keeps only which entry it is (one more, in its message field); what the enchantments do when it is worn or struck is not built.
 */
#define MAGIC_DEF_BYTES 62
/* An entry's offsets: the name at 0, then the artifact byte, the base item's group and index, MAGIC_DEF_ENCHANTS_MOST (type, parameter) pairs ended by MAGIC_DEF_END, a condition, a value and a material. */
#define MAGIC_DEF_ARTIFACT 32
#define MAGIC_DEF_GROUP 33
#define MAGIC_DEF_INDEX 34
#define MAGIC_DEF_ENCHANTS 35
#define MAGIC_DEF_ENCHANTS_MOST 10
#define MAGIC_DEF_END 255
#define MAGIC_DEF_CONDITION 0x37
#define MAGIC_DEF_VALUE 0x39
#define MAGIC_DEF_MATERIAL 0x3D
/* enchant_spell_cost: a cast-spell power costs the spell's cost at every skill ENCHANT_SPELL_SKILL, times ENCHANT_SPELL_COST_MUL. Enchantment type ENCHANT_SPECIAL_ADVANTAGE holds the artifact's number. */
#define ENCHANT_SPELL_SKILL 50
#define ENCHANT_SPELL_COST_MUL 10
/* The enchantment codes below ENCHANT_CODE_PARAM are fixed costs, ENCHANT_CODE_SPELL_FIRST..LAST cost the spell's; from ENCHANT_CODE_PARAM the cost is by parameter, ENCHANT_PARAMS of them. A type from ENCHANT_TYPES up is not costed. */
#define ENCHANT_CODE_PARAM 1000
#define ENCHANT_CODE_SPELL_FIRST 5
#define ENCHANT_CODE_SPELL_LAST 7
#define ENCHANT_PARAMS 12
#define ENCHANT_TYPES 16
/* The artifacts' pictures are in image archive ARTIFACT_IMAGE_ARCHIVE (the next one for a woman); identifying costs value * IDENTIFY_PRICE_NUM / 256; an artifact's lore is TEXT_ARTIFACT_LORE plus its number. A curio index CURIO_ARTIFACT_EXEMPT is no magic item. */
#define ARTIFACT_IMAGE_ARCHIVE 432
#define IDENTIFY_PRICE_NUM 25
#define TEXT_ARTIFACT_LORE 8700
#define CURIO_ARTIFACT_EXEMPT 5

/* MAGIC.DEF's bytes, kept by the caller. */
void magic_defs_set(const df_u8 *bytes, df_u32 size);
int magic_def_count(void);
/* An entry's name; and whether it is an artifact. */
const char *magic_def_name(int def);
int magic_def_artifact(int def);

/* enchant_item_value: what an entry's enchantments are worth (the item's price; its base price is not kept). */
int magic_def_value(int def);

/* item_make_magic(-1) or (which): an item of a random non-artifact entry (or the which-th) made into container; 0 when there is none or no room. */
df_u32 magic_item_create(df_u32 container, int which);
df_u32 magic_artifact_create(df_u32 container, int ordinal);
/* The k-th enchantment of an entry: its type (15 and over a side effect) and parameter (-1 for none). 0 past the last. */
int magic_def_enchant(int def, int k, int *type, int *param);
/* The entry a magic item is of, -1 for another item. */
int magic_item_def(df_u32 item);
int magic_item_unknown(df_u32 item);
int magic_item_identify_price(df_u32 item);
int magic_item_identify(df_u32 item);
int magic_artifact_text(df_u32 item);
int magic_artifact_power(df_u32 item);
df_u32 magic_artifact_worn(int power);

/* enchant_spell_cost: a standard spell's cost for a character with every skill at 50, ten times; 0 when there is no such spell. */
int magic_spell_cost(int id);

/* shop_stock_magic: quality / 2 + 1 magic items and a spellbook, and, if asked (a Mages Guild's rank 4), as many soul traps (a quarter holding a soul). */
void magic_shop_stock(df_u32 container, int quality, int soul_traps);

#endif
