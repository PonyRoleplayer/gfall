#include "engine/scene/clock.h"
#include "engine/core/df.h"

/* Starts at noon, so an outdoor place opens in daylight. */
static float g_minute = 720.0f;
static int g_scale = CLOCK_SCALE;
/* The 1st of Midyear, 3E 405: Daggerfall Unity's start. */
#define START_YEAR 405
#define START_MONTH 5
static int g_day = START_YEAR * CLOCK_YEAR_DAYS + START_MONTH * CLOCK_MONTH_DAYS;

static const char *const g_months[CLOCK_MONTHS] = {
    "Morning Star", "Sun's Dawn", "First Seed", "Rain's Hand", "Second Seed", "Midyear",
    "Sun's Height", "Last Seed", "Hearthfire", "Frostfall", "Sun's Dusk", "Evening Star"
};
static const char *const g_weekdays[7] = {
    "Sundas", "Morndas", "Tirdas", "Middas", "Turdas", "Fredas", "Loredas"
};
static const char *const g_seasons[4] = { "fall", "spring", "summer", "winter" };
/* Each month's season, Morning Star first. */
static const df_u8 g_month_season[CLOCK_MONTHS] = {
    CLOCK_WINTER, CLOCK_WINTER, CLOCK_SPRING, CLOCK_SPRING, CLOCK_SPRING, CLOCK_SUMMER,
    CLOCK_SUMMER, CLOCK_SUMMER, CLOCK_FALL, CLOCK_FALL, CLOCK_FALL, CLOCK_WINTER
};
/* The month each season starts in, in the year: winter's is Morning Star. */
static const df_u8 g_season_start[4] = { 8, 2, 5, 0 };

static float wrap_day(float m)
{
    while (m >= CLOCK_DAY) {
        m -= CLOCK_DAY;
        g_day++;
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
    /* A time of day: the date stays. */
    while (minute >= CLOCK_DAY) {
        minute -= CLOCK_DAY;
    }
    while (minute < 0.0f) {
        minute += CLOCK_DAY;
    }
    g_minute = minute;
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

float clock_advance(float seconds)
{
    float minutes;

    /* A frame of more than a second is a stall or a load: do not jump the sky. */
    if (seconds <= 0.0f || seconds > 1.0f || g_scale == 0) {
        return 0.0f;
    }
    minutes = seconds * (float)g_scale * (1.0f / 60.0f);
    g_minute = wrap_day(g_minute + minutes);
    return minutes;
}

void clock_set_day(int day)
{
    g_day = day < 0 ? 0 : day;
}

int clock_day(void)
{
    return g_day;
}

void clock_set_date(int year, int month, int day)
{
    clock_set_day(year * CLOCK_YEAR_DAYS + month * CLOCK_MONTH_DAYS + day);
}

void clock_date(int *year, int *month, int *day)
{
    int doy;

    doy = g_day % CLOCK_YEAR_DAYS;
    if (year) {
        *year = g_day / CLOCK_YEAR_DAYS;
    }
    if (month) {
        *month = doy / CLOCK_MONTH_DAYS;
    }
    if (day) {
        *day = doy % CLOCK_MONTH_DAYS;
    }
}

int clock_season(void)
{
    int month;

    clock_date(0, &month, 0);
    return (int)g_month_season[month];
}

void clock_set_season(int season)
{
    int year;

    if (season < 0 || season > 3) {
        return;
    }
    clock_date(&year, 0, 0);
    clock_set_date(year, (int)g_season_start[season], 0);
}

const char *clock_month_name(int month)
{
    return month >= 0 && month < CLOCK_MONTHS ? g_months[month] : "";
}

const char *clock_weekday_name(int day)
{
    return day >= 0 ? g_weekdays[day % 7] : "";
}

const char *clock_season_name(int season)
{
    return season >= 0 && season < 4 ? g_seasons[season] : "";
}
