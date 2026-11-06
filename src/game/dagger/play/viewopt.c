#include "game/dagger/play/viewopt.h"
#include "game/dagger/play/rules.h"
#include "engine/core/df.h"
#include "engine/gfx/camera.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"

/* See viewopt.h. HeadBobber.cs (MIT, Daggerfall Workshop): PlotPath and
 * PlotRotation. Metres become this world's units. The decompilation is
 * not the source of the wave. */

/* Metres, HeadBobber's walking and running amounts. */
#define WALK_X 0.045f
#define WALK_Y 0.062f
#define RUN_X 0.09f
#define RUN_Y 0.11f
/* Degrees, PlotRotation. */
#define WALK_NOD_X 0.25f
#define WALK_NOD_Y 0.1f
#define RUN_NOD_X 0.6f
#define RUN_NOD_Y 0.15f
#define DEG (DF_TAU / 360.0f)

static int g_full;
static int g_bob;
static int g_crouch_toggle;
static int g_show_fps;
static int g_fps_limit;
static int g_mode_w = 640;
static int g_mode_h = 480;
static int g_tex_detail = 2;
static int g_sprite_detail = 2;

void viewopt_defaults(void)
{
    g_full = 0;
    g_bob = 0;
    g_crouch_toggle = 0;
    g_show_fps = 0;
    g_fps_limit = 0;
    g_tex_detail = 2;
    g_sprite_detail = 2;
    g_mode_w = 640;
    g_mode_h = 480;
    camera_set_fov(75.0f);
    sound_set_volume(80);
    music_set_volume(80);
}

int viewopt_full(void)
{
    return g_full;
}

void viewopt_set_full(int on)
{
    g_full = on ? 1 : 0;
}

int viewopt_bob(void)
{
    return g_bob;
}

void viewopt_set_bob(int on)
{
    g_bob = on ? 1 : 0;
}

int viewopt_crouch_toggle(void)
{
    return g_crouch_toggle;
}

void viewopt_set_crouch_toggle(int on)
{
    g_crouch_toggle = on != 0;
}

int viewopt_show_fps(void)
{
    return g_show_fps;
}

void viewopt_set_show_fps(int on)
{
    g_show_fps = on != 0;
}

int viewopt_fps_limit(void)
{
    return g_fps_limit;
}

void viewopt_set_fps_limit(int fps)
{
    if (fps <= 0) {
        g_fps_limit = 0;
    } else {
        g_fps_limit = fps < VIEWOPT_FPS_LEAST ? VIEWOPT_FPS_LEAST : (fps > VIEWOPT_FPS_MOST ? VIEWOPT_FPS_MOST : fps);
    }
}

void viewopt_eye(float phase, int walking, int running, float *right, float *up, float *pitch, float *yaw)
{
    float s;
    float c;
    float xamt;
    float yamt;
    float nx;
    float ny;

    *right = 0.0f;
    *up = 0.0f;
    *pitch = 0.0f;
    *yaw = 0.0f;
    if (!g_bob || !walking) {
        return;
    }
    s = df_sin(phase);
    c = df_cos(phase);
    if (running) {
        xamt = RUN_X;
        yamt = RUN_Y;
        nx = RUN_NOD_X;
        ny = RUN_NOD_Y;
    } else {
        xamt = WALK_X;
        yamt = WALK_Y;
        nx = WALK_NOD_X;
        ny = WALK_NOD_Y;
    }
    /* PlotPath: lateral cos, vertical |sin| so a step rises and falls. */
    *right = c * xamt * RULES_UNITS_PER_M;
    *up = df_absf(s * yamt) * RULES_UNITS_PER_M;
    /* PlotRotation, degrees, as an offset for this phase rather than a spin. */
    *pitch = df_absf(s * nx) * DEG;
    *yaw = -s * ny * DEG;
}

int viewopt_tex_detail(void)
{
    return g_tex_detail;
}

void viewopt_set_tex_detail(int level)
{
    g_tex_detail = level < 0 ? 0 : (level > 2 ? 2 : level);
}

int viewopt_sprite_detail(void)
{
    return g_sprite_detail;
}

void viewopt_set_sprite_detail(int level)
{
    g_sprite_detail = level < 0 ? 0 : (level > 2 ? 2 : level);
}

void viewopt_mode(int *w, int *h)
{
    *w = g_mode_w;
    *h = g_mode_h;
}

void viewopt_set_mode(int w, int h)
{
    if (w >= 64 && h >= 64) {
        g_mode_w = w;
        g_mode_h = h;
    }
}
