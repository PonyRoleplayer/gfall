#ifndef DF_FORECAST_H
#define DF_FORECAST_H

#include "engine/assets/recfile.h"
#include "engine/scene/weather.h"

/*
 * Daggerfall's weather, as records (docs/data.md, docs/weather.md):
 *
 * WTHR, the odds by climate and season: the table in the Daggerfall
 * Chronicles, p. 47, as Daggerfall Unity carries it (WeatherTable.json),
 * one record a climate id (223..232) and season (SEASON_*). Percent for
 * each engine weather kind, in the order of engine/scene/weather.h.
 *
 * WLOK, what each kind looks like once it has arrived: the engine's
 * WeatherLook, which the game hands it (weather_set_look). The first
 * run cooks the engine's own defaults, so a mod can change them.
 */

typedef struct WeatherOdds {
    /* climate * 4 + season: forecast_id. */
    df_u16 id;
    df_u8 odds[WEATHER_KINDS];
} WeatherOdds;

typedef struct WeatherKindLook {
    /* WEATHER_SUNNY..WEATHER_SNOW. */
    df_u16 id;
    float cloud;
    float fog;
    float rain;
    float snow;
    float storm;
    df_i32 sky;
    float view;
    float haze;
} WeatherKindLook;

extern const RecSchema weather_odds_schema;
extern const RecSchema weather_look_schema;
RecTable *weather_odds_table(void);
RecTable *weather_look_table(void);
int forecast_id(int climate, int season);

/*
 * odds receives WEATHER_KINDS percents for a climate id (223..232) and
 * season (SEASON_*): its record, or the woodlands' in that season for a
 * climate with none, or all sun when no record is merged.
 */
void forecast_odds(int climate, int season, df_u8 *odds);
/* Hands those odds to the engine's weather. */
void forecast_set(int climate, int season);
/* Hands every merged look to the engine (weather_set_look). */
void forecast_looks(void);

#endif
