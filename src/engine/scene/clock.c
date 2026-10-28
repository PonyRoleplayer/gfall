#include "engine/scene/clock.h"
#include "engine/core/df.h"

/* Starts at noon, so an outdoor place opens in daylight. */
static float g_minute = 720.0f;
static int g_scale = CLOCK_SCALE;
/* The 1st of Midyear, 3E 405: Daggerfall Unity's start, on the default's 360-day year. */
static int g_day = 405 * 360 + 5 * 30;

/* Tamriel's calendar, the default. */
static const char *const g_tamriel_months[12] = {
    "Morning Star", "Sun's Dawn", "First Seed", "Rain's Hand", "Second Seed", "Midyear",
    "Sun's Height", "Last Seed", "Hearthfire", "Frostfall", "Sun's Dusk", "Evening Star"
};
static const df_u8 g_tamriel_seasons[12] = {
    CLOCK_WINTER, CLOCK_WINTER, CLOCK_SPRING, CLOCK_SPRING, CLOCK_SPRING, CLOCK_SUMMER,
    CLOCK_SUMMER, CLOCK_SUMMER, CLOCK_FALL, CLOCK_FALL, CLOCK_FALL, CLOCK_WINTER
};
static const char *const g_tamriel_weekdays[7] = {
    "Sundas", "Morndas", "Tirdas", "Middas", "Turdas", "Fredas", "Loredas"
};
static const char *const g_seasons[4] = { "fall", "spring", "summer", "winter" };

/* The calendar in use, filled with the default on first use; its year's days, summed once. */
static Calendar g_cal;
static int g_year_days;
static int g_have_cal;

static void copy_name(char *dst, const char *src, int cap)
{
    int i;

    for (i = 0; i < cap - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    for (; i < cap; i++) {
        dst[i] = 0;
    }
}

void clock_calendar_default(Calendar *out)
{
    int i;

    if (!out) {
        return;
    }
    for (i = 0; i < (int)sizeof(*out); i++) {
        ((df_u8 *)out)[i] = 0;
    }
    out->nmonths = 12;
    for (i = 0; i < 12; i++) {
        copy_name(out->month[i].name, g_tamriel_months[i], CLOCK_NAME);
        out->month[i].days = 30;
        out->month[i].season = g_tamriel_seasons[i];
    }
    out->nweekdays = 7;
    for (i = 0; i < 7; i++) {
        copy_name(out->weekday[i], g_tamriel_weekdays[i], CLOCK_NAME);
    }
    out->week_restarts = 1;
    copy_name(out->era, "3E", (int)sizeof(out->era));
    out->start_year = 405;
    out->start_month = 5;
    out->start_day = 0;
}

static const Calendar *cal(void)
{
    int i;

    if (!g_have_cal) {
        clock_calendar_default(&g_cal);
        g_year_days = 0;
        for (i = 0; i < g_cal.nmonths; i++) {
            g_year_days += g_cal.month[i].days;
        }
        g_have_cal = 1;
    }
    return &g_cal;
}

int clock_set_calendar(const Calendar *c)
{
    Calendar d;
    int i;
    int days;

    if (!c) {
        clock_calendar_default(&d);
        c = &d;
    }
    if (c->nmonths < 1 || c->nmonths > CLOCK_MONTHS_MAX
        || c->nweekdays < 1 || c->nweekdays > CLOCK_WEEKDAYS_MAX
        || c->start_year < 0 || c->start_month < 0 || c->start_month >= c->nmonths) {
        return 0;
    }
    days = 0;
    for (i = 0; i < c->nmonths; i++) {
        if (c->month[i].days < 1 || c->month[i].days > CLOCK_MONTH_DAYS_MAX
            || c->month[i].season > CLOCK_WINTER) {
            return 0;
        }
        days += c->month[i].days;
    }
    if (c->start_day < 0 || c->start_day >= c->month[c->start_month].days) {
        return 0;
    }
    /* Field by field, so every name ends inside its array whatever the caller sent. */
    for (i = 0; i < (int)sizeof(g_cal); i++) {
        ((df_u8 *)&g_cal)[i] = 0;
    }
    g_cal.nmonths = c->nmonths;
    for (i = 0; i < c->nmonths; i++) {
        copy_name(g_cal.month[i].name, c->month[i].name, CLOCK_NAME);
        g_cal.month[i].days = c->month[i].days;
        g_cal.month[i].season = c->month[i].season;
    }
    g_cal.nweekdays = c->nweekdays;
    for (i = 0; i < c->nweekdays; i++) {
        copy_name(g_cal.weekday[i], c->weekday[i], CLOCK_NAME);
    }
    g_cal.week_restarts = c->week_restarts ? 1 : 0;
    copy_name(g_cal.era, c->era, (int)sizeof(g_cal.era));
    g_cal.start_year = c->start_year;
    g_cal.start_month = c->start_month;
    g_cal.start_day = c->start_day;
    g_year_days = days;
    g_have_cal = 1;
    clock_set_date(g_cal.start_year, g_cal.start_month, g_cal.start_day);
    return 1;
}

void clock_calendar(Calendar *out)
{
    if (out) {
        *out = *cal();
    }
}

int clock_months(void)
{
    return cal()->nmonths;
}

int clock_month_days(int month)
{
    const Calendar *c;

    c = cal();
    return month >= 0 && month < c->nmonths ? (int)c->month[month].days : 0;
}

int clock_year_days(void)
{
    cal();
    return g_year_days;
}

int clock_weekdays(void)
{
    return cal()->nweekdays;
}

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

int clock_day_of(int year, int month, int day)
{
    const Calendar *c;
    int n;
    int i;

    c = cal();
    /* Months past the year carry into years; days past the month carry on through the count. */
    if (month < 0) {
        month = 0;
    }
    year += month / c->nmonths;
    month %= c->nmonths;
    n = year * g_year_days;
    for (i = 0; i < month; i++) {
        n += c->month[i].days;
    }
    return n + day;
}

void clock_date_of(int day_number, int *year, int *month, int *day)
{
    const Calendar *c;
    int doy;
    int m;

    c = cal();
    if (day_number < 0) {
        day_number = 0;
    }
    doy = day_number % g_year_days;
    for (m = 0; m < c->nmonths - 1 && doy >= (int)c->month[m].days; m++) {
        doy -= c->month[m].days;
    }
    if (year) {
        *year = day_number / g_year_days;
    }
    if (month) {
        *month = m;
    }
    if (day) {
        *day = doy;
    }
}

void clock_set_date(int year, int month, int day)
{
    clock_set_day(clock_day_of(year, month, day));
}

void clock_date(int *year, int *month, int *day)
{
    clock_date_of(g_day, year, month, day);
}

int clock_weekday(void)
{
    const Calendar *c;
    int d;

    c = cal();
    if (c->week_restarts) {
        clock_date(0, 0, &d);
        return d % c->nweekdays;
    }
    return g_day % c->nweekdays;
}

int clock_season(void)
{
    int month;

    clock_date(0, &month, 0);
    return (int)cal()->month[month].season;
}

void clock_set_season(int season)
{
    const Calendar *c;
    int year;
    int m;

    c = cal();
    if (season < 0 || season > 3) {
        return;
    }
    for (m = 0; m < c->nmonths && c->month[m].season != season; m++) {
    }
    if (m == c->nmonths) {
        return;
    }
    clock_date(&year, 0, 0);
    clock_set_date(year, m, 0);
}

const char *clock_month_name(int month)
{
    const Calendar *c;

    c = cal();
    return month >= 0 && month < c->nmonths ? c->month[month].name : "";
}

const char *clock_weekday_name(int weekday)
{
    const Calendar *c;

    c = cal();
    return weekday >= 0 && weekday < c->nweekdays ? c->weekday[weekday] : "";
}

const char *clock_season_name(int season)
{
    return season >= 0 && season < 4 ? g_seasons[season] : "";
}

const char *clock_era(void)
{
    return cal()->era;
}
