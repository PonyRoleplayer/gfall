#ifndef GFALL_COMBAT_H
#define GFALL_COMBAT_H

/* FALL.EXE damage_roll_to_hit / damage_resolve_attack. See docs/game.md. */
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
