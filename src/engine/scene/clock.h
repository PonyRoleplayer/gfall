#ifndef ENG_CLOCK_H
#define ENG_CLOCK_H

#include "engine/core/df.h"

/*
 * The world's clock and calendar. Read docs/daylight.md before editing.
 *
 * Time is minutes since midnight, 0 up to CLOCK_DAY. It runs only
 * while the world draws: draw_frame advances it by the real time of
 * each frame times the scale, and a frame that spans the console
 * panel is not counted, so the clock stops while the console is open.
 * Plain C with no Glide, so hostcheck runs it.
 */

#define CLOCK_DAY 1440.0f
/* Game seconds per real second, Daggerfall's 12 as the default. */
#define CLOCK_SCALE 12
#define CLOCK_SCALE_MAX 2000

/* Minutes since midnight. Any value is wrapped into the day. */
void clock_set_minute(float minute);
float clock_minute(void);
/* Hour and minute of the current time, for display. */
void clock_hm(int *hour, int *minute);
/* 0 stops the clock. Clamped to 0..CLOCK_SCALE_MAX. */
void clock_set_scale(int scale);
int clock_scale(void);
/*
 * Advance by seconds of real time. Negative or absurd values are ignored.
 * Returns the game minutes it moved, for what runs on game time (weather).
 * Running past midnight turns the day.
 */
float clock_advance(float seconds);

/*
 * The calendar. The engine counts days; a game installs its months,
 * their lengths and seasons, its week and its start date as one
 * Calendar, replaced whole (clock_set_calendar). The default, kept until
 * a game installs its own, is Tamriel's as Daggerfall keeps it
 * (Daggerfall Unity's DaggerfallDateTime): 12 months of 30 days, 360 a
 * year; a week of 7 days that starts again on Sundas each month; the
 * 1st of Midyear, 3E 405, Daggerfall Unity's start. Months, days and
 * weekdays count from 0 here; the names and the "1st" are for display.
 */
#define CLOCK_MONTHS_MAX 16
#define CLOCK_WEEKDAYS_MAX 10
#define CLOCK_MONTH_DAYS_MAX 400
/* A name's bytes, the terminator included. */
#define CLOCK_NAME 24
/*
 * Seasons in Daggerfall's order, which the sky files and the weather
 * odds use. They are the engine's four kinds; a month says which it is.
 */
enum {
    CLOCK_FALL,
    CLOCK_SPRING,
    CLOCK_SUMMER,
    CLOCK_WINTER
};

typedef struct ClockMonth {
    char name[CLOCK_NAME];
    /* 1..CLOCK_MONTH_DAYS_MAX */
    df_u16 days;
    /* CLOCK_FALL..CLOCK_WINTER */
    df_u8 season;
} ClockMonth;

typedef struct Calendar {
    /* 1..CLOCK_MONTHS_MAX, the year's first month first. */
    int nmonths;
    ClockMonth month[CLOCK_MONTHS_MAX];
    /* 1..CLOCK_WEEKDAYS_MAX */
    int nweekdays;
    char weekday[CLOCK_WEEKDAYS_MAX][CLOCK_NAME];
    /* 1: each month's first day is weekday 0 (Daggerfall); 0: the week runs on across months. */
    df_u8 week_restarts;
    /* For display before the year: "3E". */
    char era[8];
    /* The date a new game starts on: year from 0, month and day from 0. */
    int start_year;
    int start_month;
    int start_day;
} Calendar;

/*
 * Install c, copied whole (a name is cut to CLOCK_NAME - 1). The clock
 * goes to its start date, keeping the time of day: a game installs its
 * calendar at start, then a save's day goes in with clock_set_day. 0,
 * and nothing changed, when c breaks a limit above; a null c puts the
 * default back.
 */
int clock_set_calendar(const Calendar *c);
/* The calendar in use. */
void clock_calendar(Calendar *out);
/* Tamriel's, the engine's default: what a game's own data is checked against. */
void clock_calendar_default(Calendar *out);
int clock_months(void);
/* Days in month 0..clock_months()-1; 0 out of range. */
int clock_month_days(int month);
int clock_year_days(void);
int clock_weekdays(void);

/* Year, month from 0 and day from 0. A day past its month is carried into the next. */
void clock_set_date(int year, int month, int day);
void clock_date(int *year, int *month, int *day);
/* Days since the 1st of the first month, year 0: the date as one number. */
int clock_day(void);
void clock_set_day(int day);
/* The same between a day number and a date, without the clock. */
int clock_day_of(int year, int month, int day);
void clock_date_of(int day_number, int *year, int *month, int *day);
/* Today's weekday, 0..clock_weekdays()-1. */
int clock_weekday(void);
/* Today's month's season. */
int clock_season(void);
/*
 * The first day of the year's first month of season (CLOCK_*), keeping
 * the hour: winter is Morning Star in Tamriel's. Nothing when no month
 * has that season.
 */
void clock_set_season(int season);
/* For display: "Midyear", "Morndas", "summer", "3E"; "" out of range. */
const char *clock_month_name(int month);
const char *clock_weekday_name(int weekday);
const char *clock_season_name(int season);
const char *clock_era(void);

#endif
