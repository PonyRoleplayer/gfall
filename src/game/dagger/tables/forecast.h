#ifndef DF_FORECAST_H
#define DF_FORECAST_H

#include "engine/core/df.h"

/*
 * Daggerfall's weather odds, by climate and season: the table in the
 * Daggerfall Chronicles, p. 47, as Daggerfall Unity carries it
 * (WeatherTable.json). Percent for each engine weather kind, in the
 * order of engine/scene/weather.h. See docs/weather.md.
 */

/* odds receives WEATHER_KINDS percents for a climate id (223..232) and season (SEASON_*). */
void forecast_odds(int climate, int season, df_u8 *odds);
/* Hands those odds to the engine's weather. */
void forecast_set(int climate, int season);

#endif
