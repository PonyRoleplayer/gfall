#ifndef DF_PACE_H
#define DF_PACE_H

#include "engine/core/df.h"

/*
 * The frame's pace at the swap: the frames a second the counter shows,
 * and the limit a game may set. Read docs/glide.md, The frame's pace,
 * before editing.
 *
 * glide_swap calls pace_swap just before grBufferSwap, for every swap
 * (a world frame, a menu, the console, a hold), so the count is of
 * pictures shown. With a limit the swap waits for its time: asleep for
 * most of the wait (Sleep, timeBeginPeriod(1) held while a limit is
 * set), the last two milliseconds or less by reading the performance
 * counter. A frame that comes late is not paid back: more than a frame
 * late, the pace starts again from it.
 *
 * With vsync on and a limit at or above the screen's refresh, the
 * retrace already paces the swap and the limit waits for nothing.
 *
 * Sound is refilled from the frame with no thread (docs/sound.md), so
 * the limit never goes below PACE_MIN.
 *
 * The sums (pace_plan, pace_count) are plain C on counter readings and
 * hostcheck runs them; the clock and the sleep are Win32 only.
 */

#define PACE_MIN 20
#define PACE_MAX 240

/* Frames a second at most; 0 is off. Anything else is held to PACE_MIN..PACE_MAX. */
void pace_set_limit(int fps);
int pace_limit(void);
/* Frames a second over the last half second or so; 0 before the first. */
int pace_fps(void);

/* Before the swap: wait for the limit, count the frame. vsync_hz is the refresh when the swap waits for the retrace, else 0. */
void pace_swap(int vsync_hz);
/* The program is closing: the timer's period given back. */
void pace_close(void);

/*
 * The sums, on a counter of freq ticks a second read now. pace_plan
 * answers the ticks to wait before this swap and moves the next one's
 * time; pace_count counts a swap. pace_reset forgets both.
 */
df_u32 pace_plan(df_u32 now, df_u32 freq, int vsync_hz);
void pace_count(df_u32 now, df_u32 freq);
void pace_reset(void);

#endif
