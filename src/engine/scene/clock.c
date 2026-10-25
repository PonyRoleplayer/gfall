#include "engine/scene/clock.h"
#include "engine/core/df.h"

/* Starts at noon, so an outdoor place opens in daylight. */
static float g_minute = 720.0f;
static int g_scale = CLOCK_SCALE;

static float wrap_day(float m)
{
    while (m >= CLOCK_DAY) {
        m -= CLOCK_DAY;
    }
    while (m < 0.0f) {
        m += CLOCK_DAY;
    }
    return m;
}

void clock_set_minute(float minute)
{
    /* A huge value would loop for ever below; keep it in a few days. */
    if (minute > CLOCK_DAY * 64.0f || minute < -CLOCK_DAY * 64.0f) {
        minute = 0.0f;
    }
    g_minute = wrap_day(minute);
}

float clock_minute(void)
{
    return g_minute;
}

void clock_hm(int *hour, int *minute)
{
    int m;

    m = df_ifloor(g_minute);
    if (m < 0) {
        m = 0;
    }
    if (m > 1439) {
        m = 1439;
    }
    if (hour) {
        *hour = m / 60;
    }
    if (minute) {
        *minute = m % 60;
    }
}

void clock_set_scale(int scale)
{
    if (scale < 0) {
        scale = 0;
    }
    if (scale > CLOCK_SCALE_MAX) {
        scale = CLOCK_SCALE_MAX;
    }
    g_scale = scale;
}

int clock_scale(void)
{
    return g_scale;
}

void clock_advance(float seconds)
{
    /* A frame of more than a second is a stall or a load: do not jump the sky. */
    if (seconds <= 0.0f || seconds > 1.0f || g_scale == 0) {
        return;
    }
    g_minute = wrap_day(g_minute + seconds * (float)g_scale * (1.0f / 60.0f));
}
