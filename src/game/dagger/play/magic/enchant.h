#ifndef GFALL_ENCHANT_H
#define GFALL_ENCHANT_H

/*
 * What an enchanted item does while it is worn (item_apply_equip_effects, item_remove_equip_effects of the decompilation; docs/game-enchantments.md for the
 * plan and the bugs fixed): Cast when held lasts as an effect on the player with icon 200 + the slot; Absorbs spells sets the condition; Enhances skill adds
 * 15 to the skill. `state_equip` (state.c) is the one place an item goes into or out of a slot, so every path applies and removes. The tick (minute by
 * minute effects), use, combat and carrying are the later slices.
 */
#include "engine/core/df.h"

/*
 * The numbers of the enchantments (item_enchantment_tick, inv_use_item, damage_weapon_strike_effects; FALL.EXE leaves them inline). Enhances Skill adds ENCHANT_SKILL_BONUS. Extra Spell
 * Points' parameter is a season (XSP_WINTER..XSP_AUTUMN, by month), a moon (XSP_FULL_MOON, XSP_HALF_MOON, XSP_NEW_MOON, by the day's phase of MOON_PHASE_MASK + 1, looked MOON_LOOKAHEAD_MINUTES
 * ahead as well) or a creature of a category near (XSP_NEAR_FIRST..XSP_NEAR_LAST, within XSP_NEAR_RANGE); it gives XSP_POINTS, wears the item every XSP_WEAR_EVERY-th minute and, near a
 * creature, restores XSP_MAGICKA_PER_STEP a step. Regenerating and repairing wear an item one time in ENCHANT_WEAR_ONE_IN. Vampiric's parameter is VAMPIRIC_TICK or VAMPIRIC_STRIKE; a strike
 * heals dealt / VAMPIRIC_HEAL_DIV and wears healed / VAMPIRIC_WEAR_DIV + 1. Item Deteriorates by DETERIORATE_DAY or DETERIORATE_TEMPLE; Health Leech is LEECH_WHEN_USED, LEECH_DAILY or
 * LEECH_WEEKLY (a kill within a day or a week prevents it). A cast or a strike wears ENCHANT_CAST_WEAR; a leech's use costs (level >> LEECH_USE_LEVEL_SHIFT) + 1 and wears LEECH_USE_WEAR, its hit
 * (level >> LEECH_HIT_LEVEL_SHIFT) + 1. Artifacts wear ARTIFACT_USE_WEAR (ARTIFACT_SUMMON_WEAR for the summoners). The tick counts a gap over TICK_GAP_MAX minutes as none, then the minutes
 * shifted by TICK_MINUTES_SHIFT; a sun-hurt class takes CLASS_SUN_DAMAGE_PER_STEP a step. Armour is changed by ENCHANT_ARMOR_MODIFIER; carrying by CARRY_BONUS_SMALL or CARRY_BONUS_LARGE over 256;
 * an item weighs FEATHER_WEIGHT, or EXTRA_WEIGHT_MULTIPLIER times, at least EXTRA_WEIGHT_MIN; a reaction moves ENCHANT_REACTION (SOCIAL_GROUP_ALL for every group).
 */
#define ENCHANT_SKILL_BONUS 15
#define XSP_WINTER 0
#define XSP_SPRING 1
#define XSP_SUMMER 2
#define XSP_AUTUMN 3
#define XSP_SEASON_LAST 3
#define XSP_FULL_MOON 4
#define XSP_HALF_MOON 5
#define XSP_NEW_MOON 6
#define XSP_NEAR_FIRST 7
#define XSP_NEAR_LAST 10
#define XSP_NEAR_RANGE 1024
#define XSP_POINTS 75
#define XSP_WEAR_EVERY 4u
#define XSP_MAGICKA_PER_STEP 5
#define MOON_PHASE_MASK 31u
#define MOON_LOOKAHEAD_MINUTES 5760u
#define MOON_FULL 0
#define MOON_HALF_WAX 8
#define MOON_NEW 16
#define MOON_HALF_WANE 24
#define ENCHANT_WEAR_ONE_IN 10
#define VAMPIRIC_TICK 0
#define VAMPIRIC_STRIKE 1
#define VAMPIRIC_HEAL_DIV 2
#define VAMPIRIC_WEAR_DIV 4
#define DETERIORATE_ALWAYS 0
#define DETERIORATE_DAY 1
#define DETERIORATE_TEMPLE 2
#define LEECH_WHEN_USED 0
#define LEECH_DAILY 1
#define LEECH_WEEKLY 2
#define ENCHANT_CAST_WEAR 10
#define LEECH_USE_LEVEL_SHIFT 1
#define LEECH_USE_WEAR 2
#define LEECH_HIT_LEVEL_SHIFT 2
#define ARTIFACT_USE_WEAR 50
/* Mehrunes' Razor (artifact 1, on a hit that deals damage): the target is killed when a resist roll (magic, class bit RAZOR_CLASS_BIT, modifier 0) comes to RESIST_FULL, the item wearing health / RAZOR_WEAR_DIV + 1. (FALL.EXE
 * kills when the roll is not 100, which spares the ones that fail and kills the immune: not kept.) The Mace of Molag Bal (artifact 2, on a hit): 1..MACE_DRAIN_MOST taken from the target's magicka when it has more than MACE_FLOOR, else
 * from its Strength when that is over MACE_FLOOR; wear MACE_WEAR. (The original's 12-minute return of the drain is not kept for a creature, which does not outlive its place.) */
#define RAZOR_CLASS_BIT 2
#define RAZOR_WEAR_DIV 8
#define MACE_DRAIN_MOST 6
#define MACE_FLOOR 10
#define MACE_WEAR 2
/* The Wabbajack (artifact 6, on a hit): a creature not yet its work becomes one of these kinds (monster_init), and the item wears by the change in most health (the original added condition when that was negative: the size is used). */
#define WABBAJACK_KINDS 17
#define ARTIFACT_SUMMON_WEAR 100
#define TICK_GAP_MAX 100u
#define TICK_MINUTES_SHIFT 2
#define CLASS_SUN_DAMAGE_PER_STEP 12
#define ENCHANT_ARMOR_MODIFIER 25
#define CARRY_BONUS_SMALL 320
#define CARRY_BONUS_LARGE 384
#define FEATHER_WEIGHT 1
#define EXTRA_WEIGHT_MULTIPLIER 4
#define EXTRA_WEIGHT_MIN 20
#define ENCHANT_REACTION 10
#define SOCIAL_GROUP_ALL 5
#define ITEM_NAME_MAX 48
#define TEXT_LINE_MAX 80

/* The player is in daylight (the world's hook): rapid healing and the sun-hurt classes ask it. */
int enchant_daylight(void);
/* What the game tells the tick about the world: daylight (6:00 to 18:00 outdoors), a temple, creatures round the player. Every hook may be 0 (false). */
typedef struct EnchantWorld {
    int (*daylight)(void);
    int (*temple)(void);
    /* A creature of a category (0 undead, 1 Daedra, 2 humanoid, 3 animal) within a distance. */
    int (*category_near)(int category, int distance);
    /* The nearest creature within 128 takes the damage; 1 when there was one. */
    int (*hurt_nearest)(int amount);
    void (*release_soul)(int kind);
    int (*summon_ally)(int kind);
    /* The Wabbajack: the creature (an index of the dungeon's) becomes a new one of a kind where it stands; the change in its most health, or -1 when it could not. */
    int (*transform)(int victim, int kind);
} EnchantWorld;
void enchant_set_world(const EnchantWorld *world);

/* item_damage (equip.c 731): the item's condition goes down by n; one with no more than that breaks (item_break: "%s is broken.", worn effects removed, a soul-bound
 * item says its soul is released, and an enchanted one is gone). */
void item_damage(df_u32 item, int n);

/* effects_tick (disease.c 804): the amount is the minutes since the last step over four (a gap over 100 is thrown away); enchant_minutes runs the steps when
 * minutes have passed, enchant_frame the per-frame part (Extra spell pts raising the maximum magicka). */
void enchant_minutes(void);
void enchant_frame(void);

/* inv_use_item (inven.c 775): every Cast when used of the item fires, each wearing it by 10, and Health leech "whenever used" costs the user (level / 2 + 1) health and wears 2.
 * Returns how many things were used; 0 when the item has nothing to use. */
int item_use(df_u32 item);

/* damage_bonus_vs_target for a weapon that hit a creature of a category (0 undead, 1 Daedra, 2 humanoid, 3 animal): Potent vs adds the wielder's level, Low damage vs takes it off,
 * Health leech "whenever used" costs the wielder (level / 4 + 1) (9.3: they all apply, whatever else is on the weapon). */
int enchant_hit_bonus(df_u32 weapon, int category);
int enchant_hit_bonus_for(df_u32 weapon, int category, int attacker, int level);
/* damage_weapon_strike_effects after a hit that dealt damage to a creature (a dungeon index): Cast when strikes fires each spell through the weapon (wear 10), Vampiric effect "when
 * strikes" heals the wielder half the damage dealt and wears the weapon by a quarter of that and one. */
void enchant_strike(df_u32 weapon, int victim, int dealt);
void enchant_strike_for(df_u32 weapon, int attacker, int victim, int level, int dealt);
/* armor_value_for_part: Strengthens armor takes 25 off every part's armour value, Weakens armor adds 25; only the first of either over the worn items counts. */
int enchant_armor_modifier(void);
void enchant_reflect(int attacker, int category, int dealt);

/* carry_capacity (support.c 1173): the first worn item with Increased Weight Allowance (type 7) makes the capacity a quarter more (param 0) or a half more; later ones are ignored. */
int enchant_capacity(int capacity);
/* object_weight_add (parse.c 1771): the first Feather Weight (11) or Extra weight (23) of an item decides: 1, or four times its weight and at least 20. */
int enchant_weight(df_u32 item, int weight);
/* reaction_mod_item_cb (disease.c 698): over every item carried, worn or not: +10 for Good rep with All or the person's social group, -10 for Bad rep with. */
int enchant_reaction(int social_group);

void enchant_equip(df_u32 item, int slot);
void enchant_unequip(df_u32 item, int slot);

#endif
