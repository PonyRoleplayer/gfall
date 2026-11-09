#ifndef OGFALL_INVENTORY_H
#define OGFALL_INVENTORY_H

/* Original item records, containers and 27 equipment slots. Item IDs remain
 * stable when moved; a spellbook carries its spell children. See docs/ogfall.md. */
#include "game/ogfall/play/state.h"

df_u32 og_item_create(df_u32 parent, int group, int index, int material);
void og_item_name(const OgObject *item, char *out, int capacity);
int og_item_tab(const OgObject *item);
int og_item_equipped(df_u32 id);
int og_item_two_handed(const OgObject *item);
int og_inventory_equip(df_u32 id);
void og_inventory_unequip(df_u32 id);
int og_inventory_transfer(df_u32 id, df_u32 parent);
int og_inventory_weight(df_u32 parent);
int og_inventory_capacity(void);
void og_weight_text(int quarters, char *out, int capacity);
int og_inventory_armor(int part);
int og_inventory_rows(df_u32 parent, int tab, df_u32 *ids, int capacity);
/* The pack list excludes worn items; ownership queries above include them. */
int og_inventory_list(df_u32 parent, int tab, df_u32 *ids, int capacity);
void og_inventory_start_spells(df_u32 book);
df_u32 og_inventory_book(void);
const char *og_inventory_error(void);
/* A world container is keyed once, then retains its children even when empty.
 * table is the original 0..20 loot table; -1 makes an empty dropped pile. */
df_u32 og_loot_container(df_u32 place, df_u32 thing, float x, float y, float z, int table);
int og_inventory_valid(const OgState *state);
/* Fill the additional fields when reading a version 2 save. */
int og_inventory_upgrade(OgState *state);
#endif
