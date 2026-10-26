#ifndef ENG_CLOCK_H
#define ENG_CLOCK_H

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
 * The calendar, Tamriel's as Daggerfall keeps it (Daggerfall Unity's
 * DaggerfallDateTime): 12 months of 30 days, 360 a year; a week of 7
 * days that starts again on Sundas each month. It starts on the 1st of
 * Midyear, 3E 405, as Daggerfall Unity's does. Months and days count
 * from 0 here; the names and the "1st" are for display.
 */
#define CLOCK_MONTHS 12
#define CLOCK_MONTH_DAYS 30
#define CLOCK_YEAR_DAYS (CLOCK_MONTHS * CLOCK_MONTH_DAYS)
/* Seasons in Daggerfall's order, which the sky files and the weather odds use. */
enum {
    CLOCK_FALL,
    CLOCK_SPRING,
    CLOCK_SUMMER,
    CLOCK_WINTER
};
/* Year, month 0..11 and day 0..29. Out of range values are carried into the next unit up. */
void clock_set_date(int year, int month, int day);
void clock_date(int *year, int *month, int *day);
/* Days since the 1st of Morning Star, 3E 0: the date as one number. */
int clock_day(void);
void clock_set_day(int day);
/*
 * Winter is Evening Star, Morning Star and Sun's Dawn; spring First
 * Seed to Second Seed; summer Midyear to Last Seed; fall Hearthfire to
 * Sun's Dusk, on whole months, as Daggerfall Unity has it.
 */
int clock_season(void);
/* The first day of season (CLOCK_*) in the current year, keeping the hour. */
void clock_set_season(int season);
/* For display: "Midyear", "Morndas", "summer". */
const char *clock_month_name(int month);
const char *clock_weekday_name(int day);
const char *clock_season_name(int season);

#endif
