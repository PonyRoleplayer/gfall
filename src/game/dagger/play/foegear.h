#ifndef GFALL_FOEGEAR_H
#define GFALL_FOEGEAR_H

/* monster_give_equipment's tiers, kept as items on the eventual corpse.
 * See docs/gfall.md. Slot annotations are GFALL metadata, not DOS ABI. */
#include "game/dagger/play/combat.h"
void foe_equipment(int kind, df_u32 container, df_u32 *slots, FighterInfo *fighter);
#endif
