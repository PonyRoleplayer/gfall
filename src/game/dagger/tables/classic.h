#ifndef DF_CLASSIC_H
#define DF_CLASSIC_H

#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/forecast.h"
#include "game/dagger/tables/calendar.h"

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

#endif
