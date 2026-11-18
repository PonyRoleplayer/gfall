#ifndef GFALL_INVENTORY_H
#define GFALL_INVENTORY_H

/* Original item records, containers and 27 equipment slots. Item IDs remain
 * stable when moved; a spellbook carries its spell children. See docs/game.md. */
#include "game/dagger/play/state.h"

/*
 * Carrying (object_weight_add, carry_capacity, inv_equip_sound, loot_generate). Gold weighs 1 in GOLD_PER_WEIGHT of its value and is held to GOLD_MAX; a stack of arrows to STACK_MAX; a shield adds
 * (its index - SHIELD_INDEX_BASE) * SHIELD_ARMOR_PER_SIZE of armour, a body's rating is held to ARMOR_RATING_CAP and shown in ARMOR_RATING_DIV ths. Ours: weights are kept in quarters
 * (WEIGHT_UNITS_PER_KG). The sounds of putting an item on: metal armour SOUND_EQUIP_METAL, the other armour SOUND_EQUIP_ARMOR + its type, clothing SOUND_EQUIP_CLOTHES, the rest
 * SOUND_EQUIP_OTHER; picking up gold SOUND_GOLD. Loot is rolled for the categories LOOT_FIRST_CATEGORY .. LOOT_CATEGORIES - 1, those up to LOOT_LEVEL_LAST scaled by level, each succeeding
 * again at half the chance; category LOOT_CLOTHING is clothing; ours: LOOT_FILL_TRIES for a pile that is not empty.
 */
#define GOLD_PER_WEIGHT 100
#define GOLD_MAX 100000000
#define STACK_MAX 199
#define SHIELD_INDEX_BASE 6
#define SHIELD_ARMOR_PER_SIZE 5
#define ARMOR_RATING_CAP 200
#define ARMOR_RATING_DIV 5
#define WEIGHT_UNITS_PER_KG 4
#define SOUND_EQUIP_METAL 233
#define SOUND_EQUIP_ARMOR 231
#define SOUND_EQUIP_CLOTHES 234
#define SOUND_EQUIP_OTHER 236
#define SOUND_GOLD 204
#define LOOT_FIRST_CATEGORY 2
#define LOOT_CATEGORIES 15
#define LOOT_LEVEL_LAST 5
#define LOOT_CLOTHING 255
#define LOOT_FILL_TRIES 32
/* A class forbids armour by bit: a shield (index from ARMOR_SHIELD_FIRST) at FORBID_SHIELD_SHIFT + its index, a body piece at FORBID_ARMOR_TYPE_SHIFT + its type. A cuirass-style piece sounds metal when put on: the gauntlets (index 1) and from the helm up, but not the boots (equip sound). */
#define FORBID_SHIELD_SHIFT 2
#define FORBID_ARMOR_TYPE_SHIFT 6

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
df_u32 loot_container_find(df_u32 place, df_u32 thing);
df_u32 loot_container(df_u32 place, df_u32 thing, float x, float y, float z, int table);
int inventory_valid(const GameState *state);
/* Fill the additional fields when reading a version 2 save. */
int inventory_upgrade(GameState *state);
#endif
