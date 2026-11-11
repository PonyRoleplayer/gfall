#ifndef OGFALL_ENCHANT_H
#define OGFALL_ENCHANT_H

/*
 * What an enchanted item does while it is worn (item_apply_equip_effects, item_remove_equip_effects of the decompilation; docs/ogfall-enchantments.md for the
 * plan and the bugs fixed): Cast when held lasts as an effect on the player with icon 200 + the slot; Absorbs spells sets the condition; Enhances skill adds
 * 15 to the skill. `og_state_equip` (state.c) is the one place an item goes into or out of a slot, so every path applies and removes. The tick (minute by
 * minute effects), use, combat and carrying are the later slices.
 */
#include "engine/core/df.h"

/* What the game tells the tick about the world: daylight (6:00 to 18:00 outdoors), a temple, creatures round the player. Every hook may be 0 (false). */
typedef struct OgEnchantWorld {
    int (*daylight)(void);
    int (*temple)(void);
    /* A creature of a category (0 undead, 1 Daedra, 2 humanoid, 3 animal) within a distance. */
    int (*category_near)(int category, int distance);
    /* The nearest creature within 128 takes the damage; 1 when there was one. */
    int (*hurt_nearest)(int amount);
} OgEnchantWorld;
void og_enchant_set_world(const OgEnchantWorld *world);

/* item_damage (equip.c 731): the item's condition goes down by n; one with no more than that breaks (item_break: "%s is broken.", worn effects removed, a soul-bound
 * item says its soul is released, and an enchanted one is gone). */
void og_item_damage(df_u32 item, int n);

/* effects_tick (disease.c 804): the amount is the minutes since the last step over four (a gap over 100 is thrown away); og_enchant_minutes runs the steps when
 * minutes have passed, og_enchant_frame the per-frame part (Extra spell pts raising the maximum magicka). */
void og_enchant_minutes(void);
void og_enchant_frame(void);

void og_enchant_equip(df_u32 item, int slot);
void og_enchant_unequip(df_u32 item, int slot);

#endif
