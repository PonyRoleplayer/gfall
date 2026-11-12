#ifndef OGFALL_FOEGEAR_H
#define OGFALL_FOEGEAR_H

/* monster_give_equipment's tiers, kept as items on the eventual corpse.
 * See docs/ogfall.md. Slot annotations are OGFALL metadata, not DOS ABI. */
#include "game/dagger/play/combat.h"
void og_foe_equipment(int kind, df_u32 container, df_u32 *slots, OgFighter *fighter);
#endif
