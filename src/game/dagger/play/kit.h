#ifndef DF_KIT_H
#define DF_KIT_H

#include "game/dagger/play/inventory.h"

/*
 * What a new character starts with (DFU's ItemHelper.AssignStartingGear):
 * their class's one weapon, in hand, and 100 gold. An Archer has a steel
 * battle axe and 24 arrows too. And clothes, worn: a short shirt and casual
 * pants (a woman's closed short shirt, a man's plain one), the pants in a
 * look of the dice's choosing as DFU's RandomizeClothingVariant; the dye DFU
 * adds is not here yet. The spellbook is magic's (spells.h). Plain C: hostcheck
 * runs it.
 */

/* The kit of class number klass (0..17) for a woman or a man into pack, emptied first. */
void kit_give(int klass, int female, Inventory *pack);

#endif
