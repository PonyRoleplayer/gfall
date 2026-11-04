#ifndef DF_ILLS_H
#define DF_ILLS_H

#include "game/dagger/play/player.h"
#include "game/dagger/tables/diseases.h"

/*
 * What ails the player beyond their wounds: diseases, and the two curses,
 * lycanthropy (werewolf, wereboar) and vampirism. Read docs/game.md
 * (Afflictions) before editing.
 *
 * Daggerfall Unity's DiseaseEffect, LycanthropyInfection, VampirismInfection,
 * LycanthropyEffect, VampirismEffect and PassiveSpecialsEffect, as plain sums
 * over the sheet:
 *
 * - A disease is caught from a creature's blow (ills_struck), never at level
 *   1, against a saving throw. The day it is caught it incubates; each day
 *   after, one roll of its damage is taken from each attribute, health,
 *   fatigue and spell points its record weighs. What it took from the
 *   attributes it gives back when it passes or is cured.
 * - A curse starts as an infection: a dream the next day, and past three
 *   days the curse itself. Then every disease is gone and none can be
 *   caught, and the curse adds to attributes and skills, given back if it
 *   is ever lifted.
 * - A lycanthrope changes shape once a day at will and every full moon by
 *   force: bare hands, no pack, no talk, healed whole each change. Not having
 *   killed an innocent in a month, their most health shrinks.
 * - A vampire burns in the sun and in temples, 12 health each four minutes;
 *   unfed for a day, they cannot rest; by day, they cannot travel.
 *
 * The state is the game's one, saved as rows: SICK a row per disease had, CURS
 * one row of the rest.
 */

/* An infection or a curse: what it is. */
enum {
    ILL_NONE,
    ILL_WEREWOLF,
    ILL_WEREBOAR,
    ILL_VAMPIRE
};

/* days_left of a disease that never passes by itself. */
#define ILLS_FOREVER 0x7fff
/* The vampire clans' factions (FACTION.TXT): the Lyrezi when a region names none, as classic. */
#define ILLS_CLAN_LYREZI 153
#define ILLS_CLAN_ANTHOTIS 157

typedef struct Ills {
    /* Each disease's days of symptoms left, ILLS_FOREVER for until cured; 0 when not had. */
    int days_left[DISEASE_COUNT];
    /* The day each was caught, then the last day it was felt. */
    int felt_day[DISEASE_COUNT];
    /* What each disease took from each attribute (0..-99), [disease x ATTR_COUNT + attribute]. */
    int drained[DISEASE_COUNT * ATTR_COUNT];
    /* Stage one: ILL_*, the day it was caught, 1 once its dream was had, and where (for a vampire's clan). */
    int infection;
    int infected_day;
    int dreamt;
    int infected_region;
    /* Stage two: ILL_*, a vampire's clan, 1 while in beast form. */
    int curse;
    int clan;
    int beast;
    /* Game minutes (day x 1440 + minute): an innocent last killed, the last change of shape, the last feeding. */
    int last_kill;
    int last_morph;
    int last_fed;
    /* What the curse added to each attribute and skill, as it was given. */
    int boost[ATTR_COUNT];
    int skill_boost[SKILL_COUNT];
} Ills;

/* The game's state; nothing ails a new game. */
Ills *ills(void);
void ills_new_game(void);

/* A save's row of one disease had (SICK): id its number. */
typedef struct IllsSickRow {
    df_u16 id;
    df_i32 days_left;
    df_i32 felt_day;
    df_i32 drained[ATTR_COUNT];
} IllsSickRow;
/* A save's one row of the infection and the curse (CURS), id 0. */
typedef struct IllsCurseRow {
    df_u16 id;
    df_i32 infection;
    df_i32 infected_day;
    df_i32 dreamt;
    df_i32 infected_region;
    df_i32 curse;
    df_i32 clan;
    df_i32 beast;
    df_i32 last_kill;
    df_i32 last_morph;
    df_i32 last_fed;
    df_i32 boost[ATTR_COUNT];
    df_i32 skill_boost[SKILL_COUNT];
} IllsCurseRow;
extern const RecSchema ills_sick_schema;
extern const RecSchema ills_curse_schema;
/* The diseases had as rows into out (DISEASE_COUNT of room): how many. */
int ills_sick_rows(IllsSickRow *out);
void ills_curse_row(IllsCurseRow *out);
/* A saved row over a new game's state. */
void ills_put_sick(const IllsSickRow *row);
void ills_put_curse(const IllsCurseRow *row);

/* "day x 1440 + minute", the clock's minute as the state counts it. */
int ills_now(int day, float minute);

/*
 * The saving throw against disease (FormulaHelper.SavingThrow): 50, the
 * race's resistance +30, low tolerance -25, weakness -50, the class's
 * immunity +50, resistance +25, low tolerance -25, weakness -50, then
 * Willpower / 10, held to 5..95. 100: immune, a race's immunity or a curse.
 */
int ills_disease_save(const Character *c);
/* 1 when a roll of 1..100 throws it off: within the save and more than 20 under it (else the disease still takes). */
int ills_saved(int save, int roll);

/* Caught on day: 0 when already had or not caught (a curse keeps every disease off). Rolls its days. */
int ills_catch(const Character *c, int disease, int day);
/* An infection begun on day in region: 0 when one is there already, or a curse. */
int ills_infect(int kind, int day, int region);

/* What a creature's blow passed on: nothing, a disease, or an infection. */
enum {
    ILLS_STRUCK_NOTHING = -1,
    ILLS_STRUCK_INFECTION = 100
};
/*
 * A creature of kind (ENEMY_*) has hurt the player (FormulaHelper.OnMonsterHit):
 * a rat 5% and a giant bat 2%, plague, stomach rot or brain fever; a zombie 2%
 * and a mummy 5%, one of eleven; a werewolf or a wereboar 0.6% its own
 * lycanthropy; a vampire 0.6% vampirism, else 2% plague. The disease number,
 * ILLS_STRUCK_INFECTION + ILL_*, or ILLS_STRUCK_NOTHING.
 */
int ills_struck(const Character *c, int kind, int day, int region);

/* What a day's look at the state did, as bits. */
#define ILLS_FELT_BAD 0x01
#define ILLS_DIED 0x02
#define ILLS_DREAM 0x04
#define ILLS_TURNED 0x08
#define ILLS_PASSED 0x10
/*
 * The day changed to day: each disease's symptoms for each day since it was
 * last felt, and an infection's dream (past a day) or its curse (past three).
 * A vampire's turning leaves the time and the place to the game.
 */
int ills_day(Player *p, int day);

/* What a minute's look did, as bits. */
#define ILLS_BURNT 0x01
#define ILLS_FORCED 0x02
#define ILLS_HUNGER 0x04
/*
 * A game minute, now (ills_now): a vampire in the sun or a holy place burns,
 * each fourth minute; a lycanthrope on a full moon changes; one who has not
 * killed an innocent for a month has their health held under a limit that
 * falls a health point an hour, to 4. ILLS_DIED with the rest.
 */
int ills_minute(Player *p, int now, int in_sun, int in_holy, int full_moon);

/*
 * Changes shape: into the beast once a day at most (forced, any time), out of
 * it any time. Into it, nothing in either hand. Healed whole each change, as
 * classic. 0 when refused: not a lycanthrope, or not yet a day.
 */
int ills_morph(Player *p, int now, int forced);
/* 1 in beast form. */
int ills_beast(void);
/* The curse, ILL_*. */
int ills_curse(void);

/* A vampire's blow that struck: fed. An innocent killed: a lycanthrope sated. */
void ills_fed(int now);
void ills_killed_innocent(int now);
/* 1 when a vampire unfed a day may not rest (VampirismEffect.CheckStartRest). */
int ills_rest_refused(int now);
/* 1 when a vampire may not travel by day (CheckFastTravel). */
int ills_travel_refused(int daytime);

/* Diseases had, and an infection: what a temple cures, each counting once. */
int ills_curable(void);
/* Every disease gone, its drain given back, and an infection with it. */
void ills_cure(Character *c);
/*
 * A temple's price (DaggerfallGuildServiceCureDisease): 250 a disease, by
 * quality and the region's prices (rules_cost), half on a holiday of
 * HOLIDAY_TEMPLE_HALF, nothing on one of HOLIDAY_TEMPLE_CURE.
 */
int ills_cure_cost(int count, int quality, int prices, int holiday);
/* The curse lifted (a quest's end, or the console): what it added taken back. */
void ills_lift_curse(Player *p);

/*
 * 1 when either moon is full (DaggerfallDateTime's lunar phase): Masser on
 * (day of year + year x 360 + 3) mod 32 == 0, Secunda on (... - 1). Day of
 * year from 1.
 */
int ills_full_moon(int year, int day_of_year);
/* What the sheet calls the race: the curse's name, or the race's own. */
const char *ills_race_name(const Character *c);

#endif
