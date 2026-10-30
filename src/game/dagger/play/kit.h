#ifndef DF_KIT_H
#define DF_KIT_H

#include "game/dagger/play/inventory.h"

/*
 * What a new character starts with (DFU's ItemHelper.AssignStartingGear):
 * their class's one weapon, in hand, and 100 gold. An Archer has a steel
 * battle axe and 24 arrows too. Clothing and the spellbook come with
 * clothing and magic. Plain C: hostcheck runs it.
 */

/* The kit of class number klass (0..17) into pack, emptied first. */
void kit_give(int klass, Inventory *pack);

#endif
