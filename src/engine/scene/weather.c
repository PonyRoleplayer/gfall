#include "engine/scene/weather.h"

/*
 * See weather.h and docs/weather.md. Runs once a frame on a handful of
 * floats; the casts below are in the rolls, a few a game day.
 */

/*
 * How each kind looks once it has arrived: cloud, fog, rain, snow,
 * storm, sky, and the simulated view's far clip and fog start. Fog is
 * where it goes solid: rain closes it to 2400 units, a storm to 1800,
 * snow to 1500, a fog to 600. The renderer culls at the fog too, so
 * thick weather draws less. A clear day under the simulation sees 4096
 * with a haze over the last 30%: with no fog at all a far roof popped
 * at the clip and the town looked pasted on the sky.
 */
static WeatherLook g_look[WEATHER_KINDS] = {
    { 0.00f, WEATHER_CLEAR, 0.0f, 0.0f, 0.0f, WEATHER_SKY_CLEAR, 4096.0f, 0.70f },
    { 0.25f, WEATHER_CLEAR, 0.0f, 0.0f, 0.0f, WEATHER_SKY_CLEAR, 3500.0f, 0.60f },
    { 0.70f, 4000.0f, 0.0f, 0.0f, 0.0f, WEATHER_SKY_OVERCAST, 2500.0f, 0.45f },
    { 0.80f, 600.0f, 0.0f, 0.0f, 0.0f, WEATHER_SKY_OVERCAST, 700.0f, 0.10f },
    { 0.80f, 2400.0f, 1.0f, 0.0f, 0.0f, WEATHER_SKY_RAIN, 2000.0f, 0.25f },
    { 0.90f, 1800.0f, 1.0f, 0.0f, 1.0f, WEATHER_SKY_STORM, 1500.0f, 0.20f },
    { 0.70f, 1500.0f, 0.0f, 1.0f, 0.0f, WEATHER_SKY_SNOW, 1200.0f, 0.15f },
};

/* Daggerfall's woodlands in summer (Daggerfall Chronicles, p. 47). */
static df_u8 g_odds[WEATHER_KINDS] = { 60, 20, 5, 0, 10, 5, 0 };

static const char *g_names[WEATHER_KINDS] = {
    "sunny", "cloudy", "overcast", "fog", "rain", "thunder", "snow"
};

static int g_on = 1;
static int g_kind = WEATHER_SUNNY;
static int g_from = WEATHER_SUNNY;
static int g_forced = -1;
/* The values when the change began, so a change mid-change does not jump. */
static WeatherLook g_start = { 0.0f, WEATHER_CLEAR, 0.0f, 0.0f, 0.0f, WEATHER_SKY_CLEAR, 4096.0f, 0.70f };
static int g_sim;
static float g_blend = 1.0f;
static float g_next = WEATHER_HOLD_MIN;
static df_u32 g_seed = 0x6d2b79f5u;

static int pick(void);

static df_u32 roll(void)
{
    g_seed = g_seed * 1664525u + 1013904223u;
    return g_seed >> 8;
}

/* 0..1 from the next roll. */
static float roll01(void)
{
    return (float)(roll() & 0xffffu) * (1.0f / 65535.0f);
}

void weather_seed(df_u32 seed)
{
    g_seed = seed ? seed : 1u;
}

void weather_set_on(int on)
{
    g_on = on ? 1 : 0;
}

void weather_set_sim(int on)
{
    g_sim = on ? 1 : 0;
}

int weather_sim(void)
{
    return g_sim;
}

int weather_on(void)
{
    return g_on;
}

const char *weather_name(int kind)
{
    return (kind >= 0 && kind < WEATHER_KINDS) ? g_names[kind] : "?";
}

int weather_by_name(const char *name)
{
    int k;

    for (k = 0; k < WEATHER_KINDS; k++) {
        const char *a;
        const char *b;

        a = name;
        b = g_names[k];
        while (*a && *a == *b) {
            a++;
            b++;
        }
        if (!*a && !*b) {
            return k;
        }
    }
    return WEATHER_KINDS;
}

void weather_set_odds(const df_u8 *odds)
{
    int k;

    for (k = 0; k < WEATHER_KINDS; k++) {
        g_odds[k] = odds ? odds[k] : 0;
    }
    /*
     * Weather the new odds rule out (snow, on arriving in a desert)
     * does not linger: a roll lands at once, with no blend.
     */
    if (g_forced < 0 && g_odds[g_kind] == 0) {
        weather_set(pick());
    }
}

void weather_set_look(int kind, const WeatherLook *look)
{
    if (kind >= 0 && kind < WEATHER_KINDS && look) {
        g_look[kind] = *look;
    }
}

void weather_look(int kind, WeatherLook *out)
{
    if (kind >= 0 && kind < WEATHER_KINDS) {
        *out = g_look[kind];
    }
}

/* Fog blends as visibility, 1 / distance, so clearing is gradual. */
static float mix_fog(float a, float b, float t)
{
    float ia;
    float ib;

    ia = 1.0f / a;
    ib = 1.0f / b;
    return 1.0f / (ia + (ib - ia) * t);
}

/* The values now: from g_start toward the kind's look. */
static void blended(WeatherLook *out)
{
    const WeatherLook *to;
    float t;

    to = &g_look[g_kind];
    /* Smoothstep, so a change eases in and out. */
    t = g_blend * g_blend * (3.0f - 2.0f * g_blend);
    out->cloud = g_start.cloud + (to->cloud - g_start.cloud) * t;
    out->fog = mix_fog(g_start.fog, to->fog, t);
    out->rain = g_start.rain + (to->rain - g_start.rain) * t;
    out->snow = g_start.snow + (to->snow - g_start.snow) * t;
    out->storm = g_start.storm + (to->storm - g_start.storm) * t;
    out->sky = g_blend < 0.5f ? g_start.sky : to->sky;
    /* The view blends as visibility too. */
    out->view = mix_fog(g_start.view, to->view, t);
    out->haze = g_start.haze + (to->haze - g_start.haze) * t;
}

static void begin(int kind)
{
    WeatherLook now;

    blended(&now);
    g_start = now;
    g_from = g_kind;
    g_kind = kind;
    g_blend = kind == g_from ? 1.0f : 0.0f;
}

static int pick(void)
{
    int total;
    int k;
    int r;

    total = 0;
    for (k = 0; k < WEATHER_KINDS; k++) {
        total += g_odds[k];
    }
    if (total <= 0) {
        return WEATHER_SUNNY;
    }
    r = (int)(roll() % (df_u32)total);
    for (k = 0; k < WEATHER_KINDS; k++) {
        r -= g_odds[k];
        if (r < 0) {
            return k;
        }
    }
    return WEATHER_SUNNY;
}

static void hold(void)
{
    g_next = WEATHER_HOLD_MIN + (WEATHER_HOLD_MAX - WEATHER_HOLD_MIN) * roll01();
}

void weather_force(int kind)
{
    if (kind < 0 || kind >= WEATHER_KINDS) {
        g_forced = -1;
        hold();
        return;
    }
    g_forced = kind;
    if (kind != g_kind) {
        begin(kind);
    }
}

int weather_forced(void)
{
    return g_forced;
}

void weather_set(int kind)
{
    if (kind < 0 || kind >= WEATHER_KINDS) {
        kind = WEATHER_SUNNY;
    }
    g_kind = kind;
    g_from = kind;
    g_start = g_look[kind];
    g_blend = 1.0f;
    hold();
}

void weather_advance(float minutes)
{
    /* A jump longer than a change (a sleep, a journey) lands it whole. */
    if (minutes <= 0.0f) {
        return;
    }
    if (g_blend < 1.0f) {
        g_blend += minutes / WEATHER_BLEND;
        if (g_blend > 1.0f) {
            g_blend = 1.0f;
        }
    }
    if (g_forced >= 0) {
        return;
    }
    g_next -= minutes;
    if (g_next <= 0.0f) {
        int kind;

        hold();
        kind = pick();
        if (kind != g_kind) {
            begin(kind);
        }
    }
}

void weather_state(Weather *out)
{
    WeatherLook now;

    if (!g_on) {
        now = g_look[WEATHER_SUNNY];
        out->kind = WEATHER_SUNNY;
        out->from = WEATHER_SUNNY;
        out->blend = 1.0f;
    } else {
        blended(&now);
        out->kind = g_kind;
        out->from = g_from;
        out->blend = g_blend;
    }
    out->cloud = now.cloud;
    out->fog = now.fog;
    out->rain = now.rain;
    out->snow = now.snow;
    out->storm = now.storm;
    out->sky = now.sky;
    out->view = now.view;
    out->haze = now.haze;
    out->next = g_forced >= 0 ? 0.0f : g_next;
}
