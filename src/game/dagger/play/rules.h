#ifndef DF_RULES_H
#define DF_RULES_H

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
 * A new character (DFU's StatsRollout and SkillsRollout, classic's
 * character creation): an attribute is its class's base and a roll of
 * 0..10; a primary skill 28, a major 18, a minor 13, each and a roll of
 * 0..3; any other 3..6. Until creation (P6) a character takes the
 * rolls' middles, below.
 */
#define RULES_ATTRIBUTE_ROLL 5
#define RULES_PRIMARY_SKILL 30
#define RULES_MAJOR_SKILL 20
#define RULES_MINOR_SKILL 15
#define RULES_OTHER_SKILL 4

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
