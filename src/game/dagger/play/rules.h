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
/* A swimmer's speed from a walker's (PlayerSpeedChanger.GetSwimSpeed): a quarter of it, and Swimming / 200 more. */
float rules_swim_speed(float base, int swimming);
/* The most breath there is (DaggerfallEntity.MaxBreath): half of Endurance. */
int rules_max_breath(int endurance);
/* Fatigue lost, in its 64ths, each game minute (PlayerEntity): just being about, running, and swimming on a failed roll. A jump costs FATIGUE_JUMP. */
#define RULES_FATIGUE_MINUTE 11
#define RULES_FATIGUE_RUNNING 88
#define RULES_FATIGUE_SWIMMING 44
#define RULES_FATIGUE_JUMP 11
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

/* ---- People (docs/game.md, People): Daggerfall Unity's FormulaHelper and TalkManager. ---- */

/*
 * What a shop asks before bargaining: the item's value by the region's prices
 * (per 1000, at least 1 gold), moved by the shop's quality 0..20 about its
 * middle, doubled (CalculateCost).
 */
int rules_cost(int value, int quality, int prices);
/* That cost bargained: what is paid or got, by the shop's quality and the player's Mercantile and Personality (CalculateTradePrice). */
int rules_trade_price(int cost, int quality, int mercantile, int personality, int selling);
/* A tenth of the value, at least 1, as a cost; 0 for a thing not worn (CalculateItemRepairCost). */
int rules_repair_cost(int value, int quality, int condition, int condition_max, int prices);
/* Chance in 100 of being seen stealing: 5..95 (CalculateShopliftingChance). */
int rules_shoplift_chance(int pickpocket, int quality, int weight_and_count);
/* Chance in 100 of picking a building's lock of this strength: 5..95 (CalculateExteriorLockpickingChance). */
int rules_lock_chance(int lockpicking, int lock);
/* The most a bank lends: a level's 50000 (CalculateMaxBankLoan). */
int rules_max_loan(int level);
/* What a loan costs to pay back: a tenth more (CalculateBankLoanRepayment). */
int rules_loan_repayment(int amount);
/* The cost of a letter of credit of this much: one part in 100 more. */
int rules_letter_cost(int amount);
/*
 * How a person takes a question: Personality / 5, +5 for a place's whereabouts,
 * the player's standing with them, and by the tone the social group's liking for
 * it, with the skill's check (Etiquette if polite, Streetwise if blunt), -10 failed,
 * +5 passed. skill_roll is 0..99.
 */
enum {
    RULES_TONE_POLITE,
    RULES_TONE_NORMAL,
    RULES_TONE_BLUNT,
    RULES_TONES
};
int rules_talk_value(int personality, int is_place, int reaction, int tone, int sgroup, int skill, int skill_roll);
/* The value against a roll of 0..20: 0 they will not, 1 grudging, 2 glad (TalkManager). */
int rules_talk_outcome(int value, int roll);
/* Whether someone of this social group knows where a building is: a roll of 1..20 under TalkManager's table for it. */
int rules_talk_knows(int sgroup, int roll);
/* Eight compass words' index from a way to go (x east, z north): 0 east, on anticlockwise by eighths. */
int rules_compass(float dx, float dz);
/* What a sentence comes to in gold before it is split: within its crime's limits. */
int rules_sentence(int base, int per_point, int low, int high, int legal);

#endif
