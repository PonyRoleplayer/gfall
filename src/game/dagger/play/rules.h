#ifndef DF_RULES_H
#define DF_RULES_H

#include "engine/core/df.h"

/*
 * Classic's rules, as sums: each a pure function of a character's
 * numbers, citing where it comes from. Read docs/game.md before
 * editing. A later mode of our own swaps these, never the code that
 * calls them; a deliberate change from classic says so here.
 *
 * Plain C: hostcheck runs it.
 */

/* Daggerfall Unity measures in metres; the engine in 40 units a metre (walker.h). */
#define RULES_UNITS_PER_M 40.0f

/*
 * Walking, in metres a second, from the Speed attribute (0..100):
 * PlayerSpeedChanger.GetWalkSpeed, (speed + 150 - drag) / 39.5, drag
 * half of how far speed is under 100, counted from no less than 30.
 */
float rules_walk_speed(int speed);
/* Running: (speed + 150) / 39.5 times 1.35 and the Running skill (0..100) over 200. */
float rules_run_speed(int speed, int running);
/* The player's gravity, in metres a second squared: AcrobatMotor's 20. */
float rules_gravity(void);
/* A jump's speed up, in metres a second: AcrobatMotor's 4.5, raised by the Jumping skill (0..100) over 200. */
float rules_jump_speed(int jumping);
/* The height that jump rises under rules_gravity, in metres. */
float rules_jump_height(int jumping);
/* The most a character can carry, kilograms: Strength x 1.5, as DFU has it (UESP's Strength). */
float rules_max_carry(int strength);
/*
 * What an attribute gives on classic's screens (FormulaHelper): a tenth of
 * it less 5 for hit points, healing and to hit; a tenth of it for magic
 * resistance.
 */
int rules_attribute_modifier(int attribute);
int rules_magic_resist(int willpower);

/*
 * Levelling (FormulaHelper, DFCareer, UESP's Leveling and Skills).
 * The uses a skill needs for its next point: its value x how hard the
 * skill is x how hard the class is x 1.04 ^ level x 0.4, down, plus 1.
 */
int rules_skill_uses_needed(int value, int skill_advance, float class_advance, int level);
/*
 * How hard a class is from its CFG's raw word. The word is 16.16 fixed
 * point by its values: a Mage's 0x10A00 is 1.04, a Warrior's 0xBF00 0.75.
 * DFU's reader keeps it raw and its conversion was not read: inferred.
 */
float rules_class_advance(df_u32 raw);
/* The level two skill sums make: the gain since creation, plus 28, over 15 (CalculatePlayerLevel). */
int rules_level_from_sums(int start_sum, int now_sum);
/* Health a level gives: half the class's hit points plus roll (0..the other half), and endurance's modifier; at least 1. */
int rules_level_health(int hp_per_level, int roll, int endurance);
/*
 * What an hour of rest gives back (FormulaHelper): health, endurance's
 * modifier and (Medical + 60) x its most / 1000, at least 1; fatigue an eighth
 * of its most, at least 1; spell points the same, or none with none to have.
 */
int rules_rest_health(int endurance, int medical, int health_max);
int rules_rest_fatigue(int fatigue_max);
int rules_rest_magicka(int magicka_max);
/* Reflexes (0 very high .. 4 very low): a skill's uses count this many times over, an eighth a step from average. */
float rules_reflex_uses(int reflexes);

/*
 * A new character (DFU's StatsRollout and SkillsRollout, classic's
 * character creation): an attribute is its class's base and a roll of
 * 0..10, with 6..14 points more to place; a primary skill 28, a major 18, a
 * minor 13, each and a roll of 0..3, with 6 points more a group; any other
 * 3..6. create.c rolls them. A character made without creation takes the
 * rolls' middles, below.
 */
#define RULES_ATTRIBUTE_ROLL 5
#define RULES_PRIMARY_SKILL 30
#define RULES_MAJOR_SKILL 20
#define RULES_MINOR_SKILL 15
#define RULES_OTHER_SKILL 4
/* The rolls' bases, for creation. */
#define RULES_PRIMARY_BASE 28
#define RULES_MAJOR_BASE 18
#define RULES_MINOR_BASE 13
#define RULES_OTHER_BASE 3
#define RULES_SKILL_ROLLS 4
#define RULES_ATTRIBUTE_ROLLS 11
#define RULES_ATTRIBUTE_MAX 100
#define RULES_POOL_LOW 6
#define RULES_POOL_ROLLS 9
#define RULES_SKILL_POOL 6
/* Points to place at a level (FormulaHelper.BonusPool): 4 and a roll of 0..2. */
#define RULES_LEVEL_POOL_LOW 4
#define RULES_LEVEL_POOL_ROLLS 3
#define RULES_REFLEX_AVERAGE 2

/* Health at a level: 25, its class's hit points a level, and a roll each level past the first (FormulaHelper.RollMaxHealth). */
int rules_max_health(int level, int hp_per_level);
/* Fatigue: strength and endurance (DaggerfallEntity.MaxFatigue, shown; kept in 64ths). */
int rules_max_fatigue(int strength, int endurance);
/* Spell points: intelligence times its class's multiplier, rounded down (FormulaHelper.SpellPoints). */
int rules_max_magicka(int intelligence, float multiplier);
/* A fall's damage: 5 health a metre past the first 5 (PlayerHealth.ApplyPlayerFallDamage). */
int rules_fall_damage(float metres);
/*
 * Seconds a frame of a melee swing shows: 3 (115 - Speed) / 980, classic's
 * frame updates (FormulaHelper.GetMeleeWeaponAnimTime). Every weapon
 * and bare hands alike; a Speed of 50 swings 5 frames in about a second.
 */
float rules_swing_frame_time(int speed);

#endif
