#ifndef DF_LOOT_H
#define DF_LOOT_H

#include "engine/gfx/camera.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/items.h"

/*
 * What a creature leaves on its body. Read docs/game.md (Loot and the
 * inventory) before editing.
 *
 * The rolls are classic's as Daggerfall Unity has them: LootTables'
 * GenerateRandomLoot, under the table the creature's kind names (ENEM's
 * loot letter), and FormulaHelper's RandomMaterial and RandomArmorMaterial.
 * Only gold, weapons and armour are rolled yet; the tables' other columns
 * wait for their items.
 *
 * A body holds no list. What is on it is worked out each time it is opened,
 * from the game's seed and the body's own key, so a dungeon of dead
 * creatures costs no memory until one is searched, and the same body
 * always holds the same things. Its keep row (keep.h) holds the only state
 * there is: the player's level when it died, which the rolls depend on,
 * and a bit for each thing already taken. So a body cannot be given
 * things: it is only ever emptied.
 */

/* The most things a body holds: a bit each in its keep row. */
#define LOOT_MAX 16

/* A new game's seed: every body's contents come from it. */
void loot_new_game(df_u32 seed);
/*
 * The material of a random weapon or plate from a roll of 0..255
 * (RandomMaterial): the player's level against 10 moves the roll, higher
 * levels finding better. Plain sums, so tests can walk it.
 */
int loot_metal(int level, int roll);
/* Rolls a body's things into out, emptied first, from seed: the same seed, table and level give the same things. */
void loot_roll(Inventory *out, const LootChance *table, int level, df_u32 seed);
/*
 * The dead creature the player is looking at within reach, from the eye
 * along the view, no wall between; 0 when none. Searched over the
 * stream's creatures.
 */
const Entity *loot_find(const Scene *scene, const Camera *eye);
/*
 * What is left on body e, into out. origin[i] says which of its rolled
 * things out's item i is, for loot_taken. origin has INVENTORY_MAX entries.
 * How many things are left.
 */
int loot_open(const Entity *e, Inventory *out, df_u8 *origin);
/* The rolled thing number origin was taken off body e: it stays gone, as the block goes and comes back. */
void loot_taken(const Entity *e, int origin);

#endif
