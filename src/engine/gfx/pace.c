#include "engine/gfx/pace.h"

#ifndef DF_HOST
#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "engine/core/log.h"

/*
 * See pace.h and docs/glide.md, The frame's pace. Times are counter
 * ticks, compared by their signed difference so the low half's wrap
 * (an hour at the 1.19 MHz timer, seconds at a cycle counter's rate)
 * does no harm.
 */

/* The counter's half second: how long the frames a second are averaged over. */
#define COUNT_DIV 2
/* The last part of a wait, in milliseconds, read off the counter rather than slept: Sleep comes back up to a millisecond late. */
#define SPIN_MS 2

static int g_limit;
/* The earliest the next swap may go; g_have 0 until a paced swap set it. */
static df_u32 g_due;
static int g_have;
/* The count: where its window began, swaps in it, the last window's rate. */
static df_u32 g_count_from;
static int g_counting;
static int g_frames;
static int g_fps;

void pace_reset(void)
{
    g_have = 0;
    g_counting = 0;
    g_frames = 0;
    g_fps = 0;
}

df_u32 pace_plan(df_u32 now, df_u32 freq, int vsync_hz)
{
    df_u32 period;
    df_i32 early;

    /* Off, or the retrace already holds the swap to the limit or below: start afresh when it comes back. */
    if (g_limit <= 0 || freq == 0 || (vsync_hz > 0 && g_limit >= vsync_hz)) {
        g_have = 0;
        return 0;
    }
    period = freq / (df_u32)g_limit;
    if (!g_have) {
        g_have = 1;
        g_due = now + period;
        return 0;
    }
    early = (df_i32)(g_due - now);
    if (early > 0) {
        /* Waits until g_due, and the next follows a period after: a steady beat. */
        g_due += period;
        return (df_u32)early;
    }
    /* Late. Less than a frame keeps the beat; more starts it again from now, nothing paid back. */
    if ((df_u32)(-early) < period) {
        g_due += period;
    } else {
        g_due = now + period;
    }
    return 0;
}

void pace_count(df_u32 now, df_u32 freq)
{
    df_u32 dt;

    if (freq == 0) {
        return;
    }
    if (!g_counting) {
        g_counting = 1;
        g_count_from = now;
        g_frames = 0;
        return;
    }
    g_frames++;
    dt = now - g_count_from;
    if (dt >= freq / COUNT_DIV) {
        /* Twice a second: the float sum keeps frames x freq from overflowing at a cycle counter's rate. */
        g_fps = df_iround((float)g_frames * (float)freq / (float)dt);
        g_frames = 0;
        g_count_from = now;
    }
}

int pace_fps(void)
{
    return g_fps;
}

int pace_limit(void)
{
    return g_limit;
}

/* 0 for off; else PACE_MIN..PACE_MAX. */
static int held(int fps)
{
    if (fps <= 0) {
        return 0;
    }
    return fps < PACE_MIN ? PACE_MIN : (fps > PACE_MAX ? PACE_MAX : fps);
}

#ifdef DF_HOST

void pace_set_limit(int fps)
{
    g_limit = held(fps);
    g_have = 0;
}

void pace_swap(int vsync_hz)
{
    (void)vsync_hz;
}

void pace_close(void)
{
}

#else

typedef UINT (WINAPI *time_period_fn)(UINT);

static df_u32 g_freq;
/* WINMM's timer period calls, found on first use; the period is held while a limit is set. */
static time_period_fn g_begin;
static time_period_fn g_end;
static int g_bound;
static int g_period;

static df_u32 read_counter(void)
{
    LARGE_INTEGER t;

    if (g_freq == 0) {
        LARGE_INTEGER f;

        /* Every Windows 98 PC has the counter; a rate past 32 bits (none on a 233) is not used. */
        if (QueryPerformanceFrequency(&f) && f.HighPart == 0 && f.LowPart >= 1000u) {
            g_freq = f.LowPart;
        } else {
            g_freq = 1000u;
        }
    }
    if (g_freq == 1000u || !QueryPerformanceCounter(&t)) {
        return GetTickCount();
    }
    return t.LowPart;
}

/* Sleep(1) sleeps a millisecond, not the default tick, while a limit is set. */
static void hold_period(int on)
{
    if (!g_bound) {
        HMODULE dll;

        g_bound = 1;
        dll = LoadLibraryA("WINMM.DLL");
        if (dll) {
            g_begin = (time_period_fn)GetProcAddress(dll, "timeBeginPeriod");
            g_end = (time_period_fn)GetProcAddress(dll, "timeEndPeriod");
        }
        if (!g_begin || !g_end) {
            log_text(LOG_WARN, "pace: no timeBeginPeriod, the limit sleeps by the default tick");
            g_begin = 0;
            g_end = 0;
        }
    }
    if (on && !g_period && g_begin) {
        g_begin(1);
        g_period = 1;
    } else if (!on && g_period && g_end) {
        g_end(1);
        g_period = 0;
    }
}

void pace_set_limit(int fps)
{
    g_limit = held(fps);
    g_have = 0;
    hold_period(g_limit > 0);
}

void pace_swap(int vsync_hz)
{
    df_u32 now;
    df_u32 wait;

    now = read_counter();
    wait = pace_plan(now, g_freq, vsync_hz);
    if (wait) {
        df_u32 due;
        df_u32 per_ms;

        due = now + wait;
        per_ms = g_freq / 1000u;
        /* Asleep for all but the last SPIN_MS, then the counter read to the tick. */
        if (wait > per_ms * SPIN_MS) {
            Sleep((DWORD)((wait - per_ms * SPIN_MS) / per_ms));
        }
        while ((df_i32)(due - read_counter()) > 0) {
        }
    }
    pace_count(read_counter(), g_freq);
}

void pace_close(void)
{
    hold_period(0);
}

#endif
