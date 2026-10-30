#ifndef DF_CLASSIC_H
#define DF_CLASSIC_H

#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/forecast.h"
#include "game/dagger/tables/calendar.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/items.h"

/*
 * Classic's tables that live inside FALL.EXE, not in an ARENA2 file:
 * what the first run cooks into MAIN.DAT (maindat.c, docs/data.md).
 * Only the importer reads them. The game reads the records it made.
 */

/* EnemyBasics' kinds, by mobile id. */
const EnemyKind *classic_enemies(int *n);
/* RandomEncounters' tables, by dungeon type, then the underwater one. */
const Encounter *classic_encounters(int *n);
/* The weather odds for every climate id and season, into out (cap rows); returns the rows. */
int classic_weather_odds(WeatherOdds *out, int cap);
/* Tamriel's calendar: its months, its weekdays, and its one CALE record. */
const CalMonth *classic_months(int *n);
const CalWeekday *classic_weekdays(int *n);
const CalInfo *classic_calendar(void);
/* The races (RaceTemplate) and the skills (DFCareer.Skills, DaggerfallSkills); the classes come from ARENA2. */
const CareerRace *classic_races(int *n);
const CareerSkill *classic_skills(int *n);
/* The item templates, what weapons and armour add to them, the materials, and the loot tables. */
const ItemTemplate *classic_items(int *n);
const WeaponTemplate *classic_weapons(int *n);
const ArmorTemplate *classic_armors(int *n);
const LootChance *classic_loots(int *n);
const DungeonLoot *classic_dungeon_loots(int *n);
const Material *classic_materials(int *n);

#endif
