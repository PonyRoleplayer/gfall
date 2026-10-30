/*
 * GFALL: Daggerfall. Opens on classic's title and start menu; a new game
 * starts in Privateer's Hold, on foot. The platform fills an input
 * snapshot, the game plays one frame of it (play/game.c), and the engine
 * draws the place, or the menu's screen before one. Escape opens the
 * options.
 * WinMainCRTStartup lives here so renderfall can link the same window
 * code and bring its own entry.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/mem.h"
#include "engine/platform/input.h"
#include "engine/gfx/camera.h"
#include "engine/gfx/gfx.h"
#include "engine/platform/win32.h"
#include "engine/sound/music.h"
#include "engine/sound/sound.h"
#include "engine/ui/console.h"
#include "game/dagger/formats/fnt.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/play/game.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/places/dunstream.h"
#include "game/dagger/tables/maindat.h"

/* A frame's real seconds by the performance counter (GetTickCount is too coarse on Win9x); a stall counts as a tenth. */
static float frame_secs(void)
{
    static LARGE_INTEGER last;
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    float secs;

    if (!freq.QuadPart && !QueryPerformanceFrequency(&freq)) {
        freq.QuadPart = 1;
    }
    QueryPerformanceCounter(&now);
    secs = last.QuadPart ? (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart : 0.0f;
    last = now;
    return secs > 0.1f ? 0.1f : secs;
}

/* Privateer's Hold's record: the stream reads its blocks as the player walks, so it outlives the load. */
static Dungeon g_hold;

/* 1 while a place's temp heap is open: what it read and built dies with it. */
static int g_place_open;

/* The last place gone, its heap with it, as LEVELFALL leaves one: the stream, the file cache's pointers into it, then the heap. */
static void leave_place(void)
{
    dunstream_close();
    if (g_place_open) {
        texture_cache_clear();
        mem_temp_end();
        g_place_open = 0;
    }
}

/*
 * A game's place, new or loaded (the game set up who and from where), under
 * a held "Loading..." over the last frame: Privateer's Hold streamed by
 * block (dunstream.c), the player at its start or the save's place, the
 * blocks round it loaded and their lights cut, its pictures sharp before it
 * fades in. Each in a temp heap of its own, or every load would leak the
 * stream's arrays. 0 when it does not load.
 */
static int start_hold(Level *level, Camera *cam)
{
    float x;
    float y;
    float z;
    float yaw;
    float pitch;

    gfx_hold("Loading...");
    leave_place();
    if (!mem_temp_begin()) {
        win_fail("No memory for the place.");
        return 0;
    }
    g_place_open = 1;
    if (!dungeon_load(&g_hold) || !dunstream_open(&g_hold, level, &x, &y, &z, &yaw, &pitch)) {
        win_fail("Privateer's Hold could not be opened.");
        return 0;
    }
    if (!gfx_prepare(level)) {
        win_fail(gfx_error());
        return 0;
    }
    game_enter(level, cam, x, y, z, yaw, pitch);
    dunstream_fill(cam);
    gfx_prime(level, cam);
    gfx_fade_in(0.5f);
    return 1;
}

int gfall_main(void *instance)
{
    Level level;
    Camera cam;
    Input in;
    int asked;

    mem_init();
    if (!win_startup(instance, "/ARENA2", "gfall", 0)) {
        mem_shutdown();
        return 1;
    }
    /* Daggerfall's tables: DATA\MAIN.DAT beside the exe, cooked on the first run, and the mods over it (docs/data.md). */
    if (!maindat_start()) {
        win_fail(maindat_error());
        win_shutdown();
        mem_shutdown();
        return 1;
    }
    if (!gfx_startup(win_hwnd())) {
        win_fail(gfx_error());
        win_shutdown();
        mem_shutdown();
        return 1;
    }
    fnt_install();
    input_begin(win_hwnd());
    /* The game's keys, its pictures and their room, the title open; then the card with no place, for the menus. */
    game_setup();
    if (!gfx_prepare(0)) {
        win_fail(gfx_error());
        input_end();
        gfx_shutdown();
        win_shutdown();
        mem_shutdown();
        return 1;
    }
    /* The card's sound and its synthesizer; without either the hold is silent. See docs/sound.md. */
    sound_startup(win_hwnd());
    music_startup();
    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    console_set_camera(&cam);
    frame_secs();
    for (;;) {
        win_pump();
        input_read(&in);
        if (in.quit) {
            break;
        }
        asked = game_frame(&cam, &in, frame_secs());
        if (asked == GAME_QUIT) {
            break;
        }
        if (asked == GAME_START && !start_hold(&level, &cam)) {
            break;
        }
        /* A screen that covers everything is drawn with no world under it: the card does nothing it would then hide. */
        gfx_frame(&cam, game_world_shows() ? &level : 0);
    }
    input_end();
    game_shutdown();
    leave_place();
    music_shutdown();
    sound_shutdown();
    gfx_shutdown();
    win_shutdown();
    mem_shutdown();
    return 0;
}

void WinMainCRTStartup(void)
{
    ExitProcess((UINT)gfall_main(GetModuleHandleA(0)));
}
