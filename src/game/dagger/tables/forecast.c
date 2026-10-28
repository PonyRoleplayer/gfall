#include "game/dagger/tables/forecast.h"
#include "game/dagger/formats/climate.h"
#include "game/dagger/formats/skyfile.h"

/* See forecast.h, docs/weather.md and docs/data.md. */

static const RecField g_odds_fields[] = {
    REC_FIELD("id", REC_UINT, WeatherOdds, id),
    REC_ARRAY("odds", REC_UINT, WeatherOdds, odds)
};

const RecSchema weather_odds_schema = {
    "WTHR", "weather", g_odds_fields, 2, (df_u16)sizeof(WeatherOdds), 0
};

#define L(name, type, member) REC_FIELD(name, type, WeatherKindLook, member)
static const RecField g_look_fields[] = {
    L("id", REC_UINT, id),
    L("cloud", REC_FLOAT, cloud),
    L("fog", REC_FLOAT, fog),
    L("rain", REC_FLOAT, rain),
    L("snow", REC_FLOAT, snow),
    L("storm", REC_FLOAT, storm),
    L("sky", REC_INT, sky),
    L("view", REC_FLOAT, view),
    L("haze", REC_FLOAT, haze)
};
#undef L

const RecSchema weather_look_schema = {
    "WLOK", "look", g_look_fields, 9, (df_u16)sizeof(WeatherKindLook), 0
};

static RecTable g_odds;
static RecTable g_looks;

RecTable *weather_odds_table(void)
{
    if (!g_odds.schema) {
        rec_table_init(&g_odds, &weather_odds_schema, 48);
    }
    return &g_odds;
}

RecTable *weather_look_table(void)
{
    if (!g_looks.schema) {
        rec_table_init(&g_looks, &weather_look_schema, WEATHER_KINDS);
    }
    return &g_looks;
}

int forecast_id(int climate, int season)
{
    return climate * 4 + season;
}

void forecast_odds(int climate, int season, df_u8 *odds)
{
    const WeatherOdds *w;
    int k;

    if (season < 0 || season > 3) {
        season = SEASON_SUMMER;
    }
    w = (const WeatherOdds *)rec_row(weather_odds_table(), forecast_id(climate, season));
    if (!w) {
        w = (const WeatherOdds *)rec_row(weather_odds_table(), forecast_id(CLIMATE_WOODLANDS, season));
    }
    for (k = 0; k < WEATHER_KINDS; k++) {
        odds[k] = w ? w->odds[k] : (df_u8)(k == WEATHER_SUNNY ? 100 : 0);
    }
}

void forecast_set(int climate, int season)
{
    df_u8 odds[WEATHER_KINDS];

    forecast_odds(climate, season, odds);
    weather_set_odds(odds);
}

void forecast_looks(void)
{
    RecTable *t;
    int i;

    t = weather_look_table();
    for (i = 0; i < t->count; i++) {
        const WeatherKindLook *l;
        WeatherLook look;

        l = (const WeatherKindLook *)(t->rows + (df_u32)i * t->schema->row_size);
        if (l->id >= WEATHER_KINDS) {
            continue;
        }
        look.cloud = l->cloud;
        look.fog = l->fog;
        look.rain = l->rain;
        look.snow = l->snow;
        look.storm = l->storm;
        look.sky = (int)l->sky;
        look.view = l->view;
        look.haze = l->haze;
        weather_set_look((int)l->id, &look);
    }
}
