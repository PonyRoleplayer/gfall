#ifndef GFALL_COMBAT_H
#define GFALL_COMBAT_H

/* FALL.EXE damage_roll_to_hit / damage_resolve_attack. See docs/game.md. */
#include "game/dagger/play/watcom.h"
#include "game/dagger/play/state.h"
#include "game/dagger/tables/items.h"

/*
 * The sums of damage_roll_to_hit and damage_resolve_attack (qkey.c), which the decomp leaves inline. The names say what each number does; none of them is a tuning knob.
 */
/* A part's armour value starts at ARMOR_BASE and each point of worn armour takes ARMOR_PER_POINT off (the sheet shows fifths), held to ARMOR_LIMIT either way. */
#define ARMOR_BASE 100
#define ARMOR_PER_POINT 5
#define ARMOR_LIMIT 100
/* A class with the adrenaline rush gains ADRENALINE_CHANCE to hit (its enemy loses as much) while its health is under a LOW_HEALTH_DIVISOR-th of its maximum. */
#define LOW_HEALTH_DIVISOR 8
#define ADRENALINE_CHANCE 5
/* The attacker's agility and luck over the target's count a tenth of the difference. */
#define ATTRIBUTE_HIT_DIVISOR 10
/* A creature that is not a class enemy is easier to hit. */
#define MONSTER_HIT_BONUS 40
/* The attacker's Dodging takes a quarter off, and a critical strike roll under its skill adds a tenth of it. */
#define DODGE_DIVISOR 4
#define CRITICAL_HIT_DIVISOR 10
/* The sum is taken down by TO_HIT_OFFSET (the roll is 0..100) and held to TO_HIT_MIN..TO_HIT_MAX. */
#define TO_HIT_OFFSET 50
#define TO_HIT_MIN 3
#define TO_HIT_MAX 97
/* A blow's damage: a skilled class adds a third of its level and one; strength over STRENGTH_BASE adds a fifth; a material's hit a tenth. A Redguard or a Wood Elf gains a third of its level, a Dark Elf a quarter. */
#define EXPERT_LEVEL_DIVISOR 3
#define STRENGTH_BASE 50
#define STRENGTH_DAMAGE_DIVISOR 5
#define MATERIAL_DAMAGE_DIVISOR 10
#define RACE_BONUS_LEVEL_DIVISOR 3
#define DARK_ELF_LEVEL_DIVISOR 4
/* ItemDef.flags: a weapon that is blunt is not halved against a skeletal warrior. */
#define ITEM_FLAG_BLUNT 16
/* damage_player_hurt: the red flash of a hit follows the damage held to HURT_MIN..HURT_MAX, over HURT_MAX, and fades at HURT_FADE_PER_SECOND. */
#define HURT_MIN 4
#define HURT_MAX 32
#define HURT_FADE_PER_SECOND 2
/* item_wear: a blow wears an item by a tenth of its damage rounded, and about one time in five wears it by one when that came to nothing. */
#define WEAR_PERCENT 10
#define WEAR_ROUND 50
#define WEAR_LAST_CHANCE 20

/*
 * The reach and aim of a blow (weapon_melee_strike, func_000733AD.c): a blow lands within MELEE_REACH of the body and MELEE_ARC (2048ths of a turn) of the view, and inside MELEE_ARC_SKIP
 * the arc is not looked at. A creature shown the player's back (more than BACKSTAB_ARC off its facing) is backstabbed, for BACKSTAB_MULTIPLIER times the damage. A blow strikes one of
 * BODY_PART_ROLLS places; without a weapon it does 1 + skill / HAND_DAMAGE_MIN_DIV to 1 + skill / HAND_DAMAGE_MAX_DIV. A creature without a weapon makes up to FOE_ATTACKS attacks,
 * each tried FOE_ATTACK_TRY_BASE percent less FOE_ATTACK_TRY_PER_REFLEX for each point of the player's reflexes above REFLEXES_NORMAL.
 */
#define PERCENT 100
/* A body's middle is BODY_MIDDLE above its feet (aim and hit); a creature aims CREATURE_AIM_HEIGHT above the target's feet. Ours; the original aims at the record's point. */
#define BODY_MIDDLE 40
#define CREATURE_AIM_HEIGHT 50

#define MELEE_REACH 90
#define MELEE_ARC 200
#define MELEE_ARC_SKIP 10
#define BACKSTAB_ARC 640
#define BACKSTAB_MULTIPLIER 3
#define BODY_PART_ROLLS 20
#define HAND_DAMAGE_MIN_DIV 10
#define HAND_DAMAGE_MAX_DIV 5
#define FOE_ATTACKS 5
#define FOE_ATTACK_TRY_BASE 50
#define FOE_ATTACK_TRY_PER_REFLEX 10
#define REFLEXES_NORMAL 2
/* Knockback (damage_knockback): a blow of damage d throws with force d * KNOCK_FORCE_PER_DAMAGE and amount d * KNOCK_AMOUNT_PER_DAMAGE, at least KNOCK_SPEED_MIN, at most KNOCK_SPEED_MAX, a step
 * of at most KNOCK_STEP_MAX, slowing KNOCK_DECAY_PER_TICK a tick. */
#define KNOCK_FORCE_PER_DAMAGE 10
#define KNOCK_AMOUNT_PER_DAMAGE 2
#define KNOCK_SPEED_MIN 15
#define KNOCK_SPEED_MAX 40
#define KNOCK_STEP_MAX 25
#define KNOCK_DECAY_PER_TICK 5
/* A creature thrown against a wall takes half its speed in damage and stops (damage_knockback_move, func_0002F824.c 45); a throw slowed to KNOCK_STOP_SPEED or less ends. A step that gets under
 * KNOCK_WALL_RATIO of its length met a wall (the engine's body gives no collision flag). */
#define KNOCK_STOP_SPEED 5
#define KNOCK_WALL_RATIO 0.5f
/* DAGGER.SND: a blow that hurts is one of SOUND_HIT_COUNT from SOUND_HIT_FIRST; a parry one of SOUND_PARRY_FIRST..SOUND_PARRY_LAST; a creature's blow SOUND_CLAW_HIT_FIRST or the next, or
 * SOUND_CLAW_MISS; a creature dying SOUND_DEATH. Watcom's rand() is 0..RAND_MAX_WATCOM. */
#define SOUND_HIT_FIRST 378
#define SOUND_HIT_COUNT 5
#define SOUND_PARRY_FIRST 291
#define SOUND_PARRY_LAST 299
#define SOUND_CLAW_HIT_FIRST 380
#define SOUND_CLAW_MISS 374
#define SOUND_DEATH 17

typedef struct FighterInfo {
    int race, level, health, maximum, flags, metal, armor[BODY_PARTS];
    int attribute[ATTRS], skill[SKILLS];
    int fatigue, magicka, max_magicka, conditions;
} FighterInfo;

void fighter_player(FighterInfo *out);
/* The caller supplies both rolls so the rule is reproducible. */
int attack_chance(const FighterInfo *attacker, const FighterInfo *target,
                     int skill, int weapon, int material, int part, int swing, int critical);
int attack_damage(const FighterInfo *attacker, const FighterInfo *target,
                     int weapon, int material, int swing, int expert, int base);
void item_wear(df_u32 id, int damage);
void damage_player(int damage, int part);
/* The end of the player (the original's damage_creature_death on them): health nothing, the death cry and line. */
void player_slain(void);
void combat_step(float dt);
void combat_reset(void);
float combat_flash(void);

#endif
