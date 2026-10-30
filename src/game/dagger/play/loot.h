#ifndef DF_LOOT_H
#define DF_LOOT_H

#include "engine/gfx/camera.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/items.h"

/*
 * What can be searched in a dungeon: a creature's body, a heap of treasure,
 * a pile the player dropped. Read docs/game.md (Loot and the inventory)
 * before editing.
 *
 * The rolls are classic's as Daggerfall Unity has them: LootTables'
 * GenerateRandomLoot, under the table the creature's kind names (ENEM's
 * loot letter) or the dungeon's type does (LDUN), and FormulaHelper's
 * RandomMaterial and RandomArmorMaterial. Only gold, weapons and armour are
 * rolled yet; the tables' other columns wait for their items.
 *
 * A body or a heap of treasure holds no list. What is on it is worked out
 * each time it is opened, from the game's seed and its own key, so a
 * dungeon of them costs no memory until one is searched, and the same one
 * always holds the same things. Its keep row (keep.h) holds the only state
 * there is: the player's level when it died or was first opened, which the
 * rolls depend on, and a bit for each thing already taken. So they cannot
 * be given things: they are only ever emptied. A heap emptied is gone; a
 * body stays where it fell.
 *
 * A pile the player dropped does hold a list (pile.h), and takes things.
 */

/* The most things a body or a heap holds: a bit each in its keep row. */
#define LOOT_MAX 16

/* A new game's seed: every body's contents come from it. */
void loot_new_game(df_u32 seed);
/* That seed, for a save. */
df_u32 loot_seed(void);
/*
 * The material of a random weapon or plate from a roll of 0..255
 * (RandomMaterial): the player's level against 10 moves the roll, higher
 * levels finding better. Plain sums, so tests can walk it.
 */
int loot_metal(int level, int roll);
/* Rolls a body's things into out, emptied first, from seed: the same seed, table and level give the same things. */
void loot_roll(Inventory *out, const LootChance *table, int level, df_u32 seed);
/*
 * What the player is looking at within reach and can search, from the eye
 * along the view, no wall between; 0 when nothing. Over the stream's
 * entities.
 */
Entity *loot_find(const Scene *scene, const Camera *eye);
/*
 * What is left on e, into out, for a player of this level. origin[i] says
 * which of e's rolled things out's item i is, for loot_taken; origin has
 * INVENTORY_MAX entries. How many things are left.
 */
int loot_open(Entity *e, int level, Inventory *out, df_u8 *origin);
/* The thing number origin was taken off e: it stays gone, as the block goes and comes back. A pile keeps its own list instead. */
void loot_taken(const Entity *e, int origin);
/* 1 when e takes things put on it: only a dropped pile. */
int loot_takes_things(const Entity *e);
/* A word for what e is: "Body", "Treasure" or "Pile". */
const char *loot_label(const Entity *e);
/* The screen on e closed with left on it: a pile keeps left, and with nothing left a pile or a heap of treasure goes. */
void loot_closed(Entity *e, const Inventory *left);

#endif
