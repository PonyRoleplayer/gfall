#ifndef DF_PILE_H
#define DF_PILE_H

#include "engine/scene/entity.h"
#include "game/dagger/play/inventory.h"

/*
 * What the player drops: piles standing in the dungeon. Read docs/game.md
 * (Loot and the inventory) before editing.
 *
 * A pile in the world is one of its block's spare things (dunstream.h), an
 * entity the stream already moves with the origin and keeps with its block,
 * so a pile stays where it was dropped as the player walks away and back.
 * What is in it is here, in one pool of items, each marked with its pile's
 * key: a block gone from memory loses nothing, and a save only has to write
 * the pool.
 *
 * Dropping lands at the player's feet. Near a pile already there, it joins
 * it; with no spare thing left in the block, it joins the nearest pile there.
 *
 * Plain C: hostcheck runs it.
 */

/* Every dropped item in the game at once. */
#define PILE_ITEMS 256

/* A new game: nothing dropped anywhere. */
void pile_new_game(void);
/* 1 when count things can lie in pile e (0: a new pile) with every other pile's still there. */
int pile_room(const Entity *e, int count);
/* Puts things down at feet, as a new pile or on one there. 0, with nothing done, when there is nowhere to. */
int pile_drop(const Inventory *things, const float *feet);
/* What is in pile e, into out. */
void pile_read(const Entity *e, Inventory *out);
/* Pile e now holds just these. With nothing in it, it goes from the world and its spare thing is free again. */
void pile_write(Entity *e, const Inventory *things);

#endif
