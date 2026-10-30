#ifndef DF_CHARACTER_H
#define DF_CHARACTER_H

#include "game/dagger/tables/careers.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/items.h"

/*
 * The player's character sheet: who they are and what they have left.
 * Read docs/game.md (The player) before editing.
 *
 * Built from MAIN.DAT's CLAS, RACE and SKIL records by classic's rules
 * (rules.c). Until character creation (P6) the game starts with a fixed
 * one, CHARACTER_START_*: an Argonian woman, a warrior, the rolls at
 * their middles.
 * Fatigue is kept in 64ths as classic keeps it; shown, it is whole.
 */

#define CHARACTER_START_RACE RACE_ARGONIAN
#define CHARACTER_START_CLASS CLASS_WARRIOR
#define CHARACTER_START_FEMALE 1
#define CHARACTER_FATIGUE_UNIT 64

typedef struct Character {
    char name[24];
    int race;
    int klass;
    int female;
    int level;
    int attribute[ATTR_COUNT];
    int skill[SKILL_COUNT];
    int health;
    int health_max;
    /* In 64ths. */
    int fatigue;
    int fatigue_max;
    int magicka;
    int magicka_max;
    /* Which of the race's ten faces. */
    int face;
    /* 0 very high .. 4 very low, 2 average: the game's speed against them, and how fast skills come. */
    int reflexes;
    /* Each skill's uses since it last rose (character_tally). */
    int skill_uses[SKILL_COUNT];
    /* The level sum at creation: a level is earned by what it has grown. */
    int level_sum_start;
    /* The game minute skills were last looked at, and 1 once a new level is due. */
    float skills_checked;
    int level_due;
    /* Each body part's armour value, BODY_*: 100 bare, lower is better. What is worn sets it (inventory_armor). */
    int armor[BODY_PARTS];
} Character;

/* A new level-1 character of race and class, its vitals full. 0 when MAIN.DAT has no such race or class. */
int character_new(Character *c, int race, int klass, int female, const char *name);
/* Health, fatigue and spell points back to their most. */
void character_rest_full(Character *c);
/* Fatigue's and spell points' most worked out again from the attributes, after they change. */
void character_attributes_changed(Character *c);

/*
 * Skills and levels (PlayerEntity.RaiseSkills, CheckForLevelUp; UESP's
 * Leveling and Skills). A skill used counts up; at most once in six game
 * hours each skill with enough uses rises a point and starts counting
 * again. Past 95 only the first primary skill to get there goes on, to 100.
 * A level is earned by the sum of the three primary skills, the two
 * highest major and the highest minor.
 */
#define CHARACTER_SKILLS_EVERY 360.0f
#define CHARACTER_SKILL_CAP 95
#define CHARACTER_SKILL_MASTERED 100
/* Used n times more. */
void character_tally(Character *c, int skill, int n);
/* The sum a level is earned by. */
int character_level_sum(const Character *c);
/* Starts the level's count from what it is now: the end of creation. */
void character_start_levelling(Character *c);
/* Skills rise if six hours have passed since the last look, at game minute now. 1 when any rose. */
int character_raise_skills(Character *c, float now);
/* The new level taken: level, and health by rules_level_health with roll. The screen places its points. */
void character_level_up(Character *c, int roll);
/*
 * What a class allows (DFCareer's bits in its CFG, the weapons word and the
 * forbidden materials). 0 when it may use it: else a word for why, a
 * thing broken or forbidden its class.
 */
const char *character_refuses(const Character *c, const Item *it);
/* 1 when its class is expert with this weapon skill: a blow with it gains the level to hit, and level / 3 + 1 damage. */
int character_expert(const Character *c, int skill);
/* Damage its class's bonus (+ the level) or phobia (- the level) adds against a group, ENEMY_GROUP_*. */
int character_group_damage(const Character *c, int group);
/* Takes health; 1 when it reaches 0. */
int character_hurt(Character *c, int amount);

#endif
