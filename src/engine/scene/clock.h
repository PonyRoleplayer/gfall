#ifndef ENG_CLOCK_H
#define ENG_CLOCK_H

/*
 * The world's 24-hour clock. Read docs/daylight.md before editing.
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
/* Advance by seconds of real time. Negative or absurd values are ignored. */
void clock_advance(float seconds);

#endif
