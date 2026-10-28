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
#include "game/dagger/play/game.h"
#include "game/dagger/places/world.h"
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

/*
 * The new game's place, under a held "Loading..." over the menu's last
 * frame: Privateer's Hold, the player at its start, its creatures near
 * the start read and its pictures sharp before it fades in. 0 when it
 * does not load.
 */
static int start_hold(World **world, Level *level, Camera *cam)
{
    gfx_hold("Loading...");
    *world = world_load();
    if (!*world) {
        win_fail(world_error());
        return 0;
    }
    world_as_level(*world, level);
    if (!gfx_prepare(level)) {
        win_fail(gfx_error());
        return 0;
    }
    game_enter(*world, level, cam);
    world_creatures_step(*world, cam, 2048.0f, 0.0f);
    gfx_prime(level, cam);
    gfx_fade_in(0.5f);
    return 1;
}

int gfall_main(void *instance)
{
    World *world;
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
    world = 0;
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
        if (asked == GAME_START && !start_hold(&world, &level, &cam)) {
            break;
        }
        gfx_frame(&cam, game_playing() ? &level : 0);
    }
    input_end();
    game_shutdown();
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
