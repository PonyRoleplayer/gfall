#ifndef DF_PROF_H
#define DF_PROF_H

#include "engine/core/df.h"

/*
 * Where a frame's time goes, by the Pentium's cycle counter (rdtsc,
 * about 11 cycles a read). Read docs/profile.md before editing.
 *
 * A stage is timed between prof_begin and prof_end; a stage timed twice
 * in a frame adds up. prof_frame closes the frame: each stage's time is
 * summed over a window of about half a second, then its mean and its
 * worst frame are what prof_ms and prof_peak_ms answer. Always on: the
 * reads cost a few hundred cycles a frame.
 *
 * The counter is in cycles; the milliseconds come from calibrating it
 * against the performance counter that prof_frame is handed each frame,
 * so no clock speed is assumed.
 *
 * The Voodoo draws in parallel from its command FIFO: a stage's time is
 * the CPU's work, plus any wait when the FIFO is full. The card's own
 * fill shows mostly in PROF_SWAP, where the CPU waits for it to finish.
 */

enum {
    /* The whole of draw_frame, the others inside it. */
    PROF_FRAME,
    /* The clock, the weather, the walker, the use key and the movers. */
    PROF_SIM,
    /* ambience_frame: the soundscape, the voices' gains and the card's refill. */
    PROF_SOUND,
    PROF_SKY,
    /* scene_visible_models: the frustum and the occlusion cells. */
    PROF_CULL,
    PROF_MODELS,
    PROF_DECALS,
    PROF_WINDOWS,
    /* scene_visible_sprites and drawing them. */
    PROF_SPRITES,
    PROF_PRECIP,
    /* Debug views, the HUD, the viewer's panels, notes. */
    PROF_OVERLAY,
    PROF_SWAP,
    /* The world stream's loading and the card's uploads (docs/streaming.md). */
    PROF_STREAM,
    /*
     * The game's own frame, outside draw_frame: its rules, its bodies and
     * motors, its HUD's sums. The game brackets it (prof_begin, prof_end).
     */
    PROF_PLAY,
    PROF_STAGES
};

void prof_begin(int stage);
void prof_end(int stage);
/*
 * Close a frame. ticks is a performance counter reading now, freq its
 * ticks a second: the cycle counter is calibrated against them.
 */
void prof_frame(df_u32 ticks, df_u32 freq);
/* The window's mean and worst frame for stage, in milliseconds; 0 before the first window. */
float prof_ms(int stage);
float prof_peak_ms(int stage);
/* The stage's short name ("models"). */
const char *prof_name(int stage);
/* The last frame's time in stage, in milliseconds; 0 before calibration. For a budget fitted to the frame. */
float prof_last_ms(int stage);
/* The cycle counter's rate as calibrated, in MHz; 0 until it is. */
int prof_mhz(void);
/* Frames in the last window. */
int prof_frames(void);
/* The cycle counter now (its low half), and its cycles a millisecond (0 until calibrated), for a budget. */
df_u32 prof_now(void);
df_u32 prof_per_ms(void);

/*
 * Milliseconds the next frame can spend loading without missing a retrace
 * it would have made: what the last frame left before that 60 Hz retrace,
 * less a margin, held to 1..8 ms so loading never stops or stalls a frame.
 * fallback_ms when that cannot be known: vsync (1 if the swap waits for the
 * retrace) is off, the counter is not calibrated, or no frame was timed.
 * docs/profile.md. A program calls gfx_stream_budget, which knows the vsync.
 */
float prof_spare_ms(int vsync, float fallback_ms);

/* For hostcheck: feed a frame's stage times in cycles, at mhz, without the counter. */
void prof_feed(const df_u32 *cycles, int mhz);

#endif
