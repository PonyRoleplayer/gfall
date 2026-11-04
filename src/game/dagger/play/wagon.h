#ifndef DF_WAGON_H
#define DF_WAGON_H

#include "game/dagger/play/inventory.h"

/*
 * The small cart's wagon: things kept beside the pack, as DFU's wagon. Read
 * docs/game.md (The wider world, Riding).
 *
 * It holds WAGON_KG kilograms (ItemHelper.WagonKgLimit), gold as a stack of
 * coins among the things. The player reaches it only owning a cart, and where
 * the cart can stand: anywhere outdoors or in a building, in a dungeon only at
 * its way out (WAGON_EXIT_REACH, DFU's 5 m). The game says whether it is in reach
 * as the inventory opens; the inventory moves things in and out of it.
 *
 * The game's own block, kept across places and in a save's WAGN chunk.
 *
 * Plain C: hostcheck runs it.
 */

#define WAGON_KG 750.0f
#define WAGON_EXIT_REACH 200.0f

/* What is in it. */
Inventory *wagon_contents(void);
/* A new game's: empty. */
void wagon_new_game(void);
/* The game's word, as the inventory opens: 1 when the wagon is there to use. */
void wagon_set_reachable(int reachable);
int wagon_reachable(void);
/* 1 when count of item fits under WAGON_KG with what is in it already. */
int wagon_fits(const Item *it);
/* 1 for what can never go in: a horse, a cart. */
int wagon_refuses(const Item *it);

#endif
