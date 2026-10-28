#include "game/dagger/tables/forecast.h"
#include "game/dagger/formats/climate.h"
#include "game/dagger/formats/skyfile.h"
#include "engine/scene/weather.h"

/*
 * Six climate groups, four seasons each in SEASON_* order (fall,
 * spring, summer, winter), seven kinds each in the engine's order:
 * sunny, cloudy, overcast, fog, rain, thunder, snow.
 */
enum { G_DESERT, G_MOUNTAINS, G_JUNGLE, G_SWAMP, G_SUBTROPICAL, G_WOODLANDS };

static const df_u8 g_table[6][4][WEATHER_KINDS] = {
    /* Desert */
    { { 80, 15, 0, 0, 3, 2, 0 }, { 75, 15, 0, 0, 5, 5, 0 },
      { 85, 15, 0, 0, 0, 0, 0 }, { 75, 15, 0, 3, 5, 2, 0 } },
    /* Mountains */
    { { 30, 18, 20, 2, 20, 10, 0 }, { 30, 23, 15, 2, 20, 10, 0 },
      { 45, 25, 15, 0, 10, 5, 0 }, { 18, 20, 25, 2, 0, 0, 35 } },
    /* Jungle (rainforest) */
    { { 20, 20, 20, 0, 25, 15, 0 }, { 20, 15, 10, 3, 37, 15, 0 },
      { 35, 20, 10, 0, 25, 10, 0 }, { 15, 20, 25, 3, 25, 12, 0 } },
    /* Swamp, and the sea */
    { { 15, 15, 15, 20, 20, 15, 0 }, { 10, 10, 20, 20, 25, 15, 0 },
      { 25, 15, 15, 15, 20, 10, 0 }, { 15, 20, 25, 25, 0, 0, 15 } },
    /* Subtropical */
    { { 25, 20, 15, 0, 25, 15, 0 }, { 30, 15, 10, 3, 27, 15, 0 },
      { 40, 15, 10, 0, 20, 15, 0 }, { 20, 20, 20, 5, 25, 10, 0 } },
    /* Woodlands */
    { { 25, 15, 20, 10, 20, 10, 0 }, { 35, 15, 10, 5, 25, 10, 0 },
      { 60, 20, 5, 0, 10, 5, 0 }, { 25, 15, 20, 5, 10, 0, 25 } },
};

static int group(int climate)
{
    switch (climate) {
    case CLIMATE_DESERT:
    case CLIMATE_DESERT2:
        return G_DESERT;
    case CLIMATE_MOUNTAIN:
    case CLIMATE_MOUNTAIN_WOODS:
        return G_MOUNTAINS;
    case CLIMATE_RAINFOREST:
        return G_JUNGLE;
    case CLIMATE_OCEAN:
    case CLIMATE_SWAMP:
        return G_SWAMP;
    case CLIMATE_SUBTROPICAL:
        return G_SUBTROPICAL;
    default:
        return G_WOODLANDS;
    }
}

void forecast_odds(int climate, int season, df_u8 *odds)
{
    int k;

    if (season < 0 || season > 3) {
        season = SEASON_SUMMER;
    }
    for (k = 0; k < WEATHER_KINDS; k++) {
        odds[k] = g_table[group(climate)][season][k];
    }
}

void forecast_set(int climate, int season)
{
    df_u8 odds[WEATHER_KINDS];

    forecast_odds(climate, season, odds);
    weather_set_odds(odds);
}
