#include "engine/core/prof.h"

/* See prof.h and docs/profile.md. */

/* A window closes after this many cycles at the calibrated rate, about half a second. */
#define WINDOW_MS 500u
/* Calibrated over this many performance-counter ticks' worth of frames, at least. */
#define CALIBRATE_MS 250u

static const char *const g_names[PROF_STAGES] = {
    "frame", "sim", "sound", "sky", "cull", "models", "decals", "windows",
    "sprites", "precip", "overlay", "swap", "stream"
};

static df_u32 g_start[PROF_STAGES];
static df_u32 g_frame[PROF_STAGES];
static df_u32 g_sum[PROF_STAGES];
static df_u32 g_worst[PROF_STAGES];
/* The last closed frame's stages, as they were. */
static df_u32 g_last[PROF_STAGES];
static float g_ms[PROF_STAGES];
static float g_peak[PROF_STAGES];
static int g_frames;
static int g_done;
/* Cycles a millisecond, once calibrated; and the calibration's running sums. */
static df_u32 g_per_ms;
static df_u32 g_cal_cycles;
static df_u32 g_cal_ticks;
static df_u32 g_last_cycles;
static df_u32 g_last_ticks;
static int g_have_last;
static df_u32 g_window_cycles;

/* The low half of the cycle counter: a frame's differences fit, wrapping or not. */
static df_u32 cycles(void)
{
    df_u32 lo;
    df_u32 hi;

    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    (void)hi;
    return lo;
}

void prof_begin(int stage)
{
    if (stage >= 0 && stage < PROF_STAGES) {
        g_start[stage] = cycles();
    }
}

void prof_end(int stage)
{
    if (stage >= 0 && stage < PROF_STAGES) {
        g_frame[stage] += cycles() - g_start[stage];
    }
}

/* Add this frame's stages to the window, and close the window when it is long enough. */
static void close_frame(void)
{
    int i;

    for (i = 0; i < PROF_STAGES; i++) {
        g_last[i] = g_frame[i];
        g_sum[i] += g_frame[i];
        if (g_frame[i] > g_worst[i]) {
            g_worst[i] = g_frame[i];
        }
        g_window_cycles += i == PROF_FRAME ? g_frame[i] : 0u;
        g_frame[i] = 0;
    }
    g_frames++;
    if (g_per_ms == 0 || g_window_cycles < WINDOW_MS * g_per_ms) {
        return;
    }
    for (i = 0; i < PROF_STAGES; i++) {
        g_ms[i] = (float)g_sum[i] / ((float)g_per_ms * (float)g_frames);
        g_peak[i] = (float)g_worst[i] / (float)g_per_ms;
        g_sum[i] = 0;
        g_worst[i] = 0;
    }
    g_done = g_frames;
    g_frames = 0;
    g_window_cycles = 0;
}

void prof_frame(df_u32 ticks, df_u32 freq)
{
    df_u32 now;

    now = cycles();
    if (g_have_last && freq >= 1000u) {
        df_u32 dc;
        df_u32 dt;

        dc = now - g_last_cycles;
        dt = ticks - g_last_ticks;
        /* A long gap (a load, the console) is not a frame: it would swamp the sums. */
        if (g_per_ms == 0 && dt > 0 && dt < freq) {
            g_cal_cycles += dc;
            g_cal_ticks += dt;
            /* Calibrated over a quarter second, then kept: the clock does not change. */
            if (g_per_ms == 0 && g_cal_ticks >= freq / 1000u * CALIBRATE_MS) {
                /* Cycles a millisecond: the cycles over the milliseconds the ticks were. */
                g_per_ms = (df_u32)df_iround((float)g_cal_cycles
                                             / ((float)g_cal_ticks * 1000.0f / (float)freq));
            }
        }
    }
    g_last_cycles = now;
    g_last_ticks = ticks;
    g_have_last = 1;
    close_frame();
}

void prof_feed(const df_u32 *stage_cycles, int mhz)
{
    int i;

    g_per_ms = mhz > 0 ? (df_u32)mhz * 1000u : 0u;
    for (i = 0; i < PROF_STAGES; i++) {
        g_frame[i] = stage_cycles[i];
    }
    close_frame();
}

float prof_ms(int stage)
{
    return stage >= 0 && stage < PROF_STAGES ? g_ms[stage] : 0.0f;
}

float prof_peak_ms(int stage)
{
    return stage >= 0 && stage < PROF_STAGES ? g_peak[stage] : 0.0f;
}

const char *prof_name(int stage)
{
    return stage >= 0 && stage < PROF_STAGES ? g_names[stage] : "";
}

int prof_mhz(void)
{
    return (int)((g_per_ms + 500u) / 1000u);
}

int prof_frames(void)
{
    return g_done;
}

df_u32 prof_now(void)
{
    return cycles();
}

df_u32 prof_per_ms(void)
{
    return g_per_ms;
}

float prof_last_ms(int stage)
{
    if (stage < 0 || stage >= PROF_STAGES || g_per_ms == 0) {
        return 0.0f;
    }
    return (float)g_last[stage] / (float)g_per_ms;
}
