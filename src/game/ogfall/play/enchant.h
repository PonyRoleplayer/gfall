#ifndef OGFALL_ENCHANT_H
#define OGFALL_ENCHANT_H

/*
 * What an enchanted item does while it is worn (item_apply_equip_effects, item_remove_equip_effects of the decompilation; docs/ogfall-enchantments.md for the
 * plan and the bugs fixed): Cast when held lasts as an effect on the player with icon 200 + the slot; Absorbs spells sets the condition; Enhances skill adds
 * 15 to the skill. `og_state_equip` (state.c) is the one place an item goes into or out of a slot, so every path applies and removes. The tick (minute by
 * minute effects), use, combat and carrying are the later slices.
 */
#include "engine/core/df.h"

void og_enchant_equip(df_u32 item, int slot);
void og_enchant_unequip(df_u32 item, int slot);

#endif
