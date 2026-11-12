#ifndef OGFALL_KIT_H
#define OGFALL_KIT_H

#include "game/dagger/tables/tables.h"

/*
 * chargen_give_starting_equipment records the gifts; state_begin makes real
 * items, equips clothing and attaches starting spells to the book. Weapons
 * begin in the pack, as starting_equipment_give passes equip=0.
 * The weapon byte is FALL.EXE's D_0017BABC pair for that class.
 */

void og_kit_give(OgCharacter *who);

#endif
