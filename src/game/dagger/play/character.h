#ifndef DF_CHARACTER_H
#define DF_CHARACTER_H

#include "game/dagger/tables/careers.h"

/*
 * The player's character sheet: who they are and what they have left.
 * Read docs/game.md (The player) before editing.
 *
 * Built from MAIN.DAT's CLAS, RACE and SKIL records by classic's rules
 * (rules.c). Until character creation (P8) the game starts with a fixed
 * one, CHARACTER_START_*: a Breton warrior, the rolls at their middles.
 * Fatigue is kept in 64ths as classic keeps it; shown, it is whole.
 */

#define CHARACTER_START_RACE RACE_BRETON
#define CHARACTER_START_CLASS CLASS_WARRIOR
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
} Character;

/* A new level-1 character of race and class, its vitals full. 0 when MAIN.DAT has no such race or class. */
int character_new(Character *c, int race, int klass, int female, const char *name);
/* Health, fatigue and spell points back to their most. */
void character_rest_full(Character *c);
/* Takes health; 1 when it reaches 0. */
int character_hurt(Character *c, int amount);

#endif
