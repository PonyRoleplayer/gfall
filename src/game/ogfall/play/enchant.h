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

/* inv_use_item (inven.c 775): every Cast when used of the item fires, each wearing it by 10, and Health leech "whenever used" costs the user (level / 2 + 1) health and wears 2.
 * Returns how many things were used; 0 when the item has nothing to use. */
int og_item_use(df_u32 item);

/* damage_bonus_vs_target for a weapon that hit a creature of a category (0 undead, 1 Daedra, 2 humanoid, 3 animal): Potent vs adds the wielder's level, Low damage vs takes it off,
 * Health leech "whenever used" costs the wielder (level / 4 + 1) (9.3: they all apply, whatever else is on the weapon). */
int og_enchant_hit_bonus(df_u32 weapon, int category);
/* damage_weapon_strike_effects after a hit that dealt damage to a creature (a dungeon index): Cast when strikes fires each spell through the weapon (wear 10), Vampiric effect "when
 * strikes" heals the wielder half the damage dealt and wears the weapon by a quarter of that and one. */
void og_enchant_strike(df_u32 weapon, int victim, int dealt);
/* armor_value_for_part: Strengthens armor takes 25 off every part's armour value, Weakens armor adds 25; only the first of either over the worn items counts. */
int og_enchant_armor_modifier(void);

/* carry_capacity (support.c 1173): the first worn item with Increased Weight Allowance (type 7) makes the capacity a quarter more (param 0) or a half more; later ones are ignored. */
int og_enchant_capacity(int capacity);
/* object_weight_add (parse.c 1771): the first Feather Weight (11) or Extra weight (23) of an item decides: 1, or four times its weight and at least 20. */
int og_enchant_weight(df_u32 item, int weight);
/* reaction_mod_item_cb (disease.c 698): over every item carried, worn or not: +10 for Good rep with All or the person's social group, -10 for Bad rep with. */
int og_enchant_reaction(int social_group);

void og_enchant_equip(df_u32 item, int slot);
void og_enchant_unequip(df_u32 item, int slot);

#endif
