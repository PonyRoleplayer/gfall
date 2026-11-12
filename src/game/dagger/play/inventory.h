#ifndef GFALL_INVENTORY_H
#define GFALL_INVENTORY_H

/* Original item records, containers and 27 equipment slots. Item IDs remain
 * stable when moved; a spellbook carries its spell children. See docs/gfall.md. */
#include "game/dagger/play/state.h"

df_u32 item_create(df_u32 parent, int group, int index, int material);
void item_name(const ObjectInfo *item, char *out, int capacity);
int item_tab(const ObjectInfo *item);
int item_equipped(df_u32 id);
int item_two_handed(const ObjectInfo *item);
int inventory_equip(df_u32 id);
void inventory_unequip(df_u32 id);
int inventory_transfer(df_u32 id, df_u32 parent);
int inventory_weight(df_u32 parent);
int inventory_capacity(void);
void weight_text(int quarters, char *out, int capacity);
int inventory_armor(int part);
int inventory_rows(df_u32 parent, int tab, df_u32 *ids, int capacity);
/* The pack list excludes worn items; ownership queries above include them. */
int inventory_list(df_u32 parent, int tab, df_u32 *ids, int capacity);
void inventory_start_spells(df_u32 book);
df_u32 inventory_book(void);
const char *inventory_error(void);
/* A world container is keyed once, then retains its children even when empty.
 * table is the original 0..20 loot table; -1 makes an empty dropped pile. */
df_u32 loot_container(df_u32 place, df_u32 thing, float x, float y, float z, int table);
int inventory_valid(const GameState *state);
/* Fill the additional fields when reading a version 2 save. */
int inventory_upgrade(GameState *state);
#endif
