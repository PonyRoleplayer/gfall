/*
 * GFALL: Daggerfall. Starts in Privateer's Hold, on foot. The platform
 * fills an input snapshot, the game plays one frame of it (play/game.c),
 * and the engine draws the place. Escape pauses; paused, Q quits.
 * WinMainCRTStartup lives here so renderfall can link the same window
 * code and bring its own entry.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/mem.h"
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

int gfall_main(void *instance)
{
    World *world;
    Level level;
    Camera cam;
    Input in;

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

    world = world_load();
    if (!world) {
        win_fail(world_error());
        win_shutdown();
        mem_shutdown();
        return 1;
    }
    world_as_level(world, &level);

    if (!gfx_startup(win_hwnd())) {
        win_fail(gfx_error());
        win_shutdown();
        mem_shutdown();
        return 1;
    }
    fnt_install();
    /* The game's keys and its pictures' room, before the place takes the card. */
    game_setup();
    if (!gfx_prepare(&level)) {
        win_fail(gfx_error());
        gfx_shutdown();
        win_shutdown();
        mem_shutdown();
        return 1;
    }

    /* The card's sound and its synthesizer; without either the hold is silent. See docs/sound.md. */
    sound_startup(win_hwnd());
    music_startup();
    game_enter(world, &level, &cam);
    /* The creatures near the start read, then the hold's song and their sharp copies, before the first frame. */
    world_creatures_step(world, &cam, 2048.0f, 0.0f);
    gfx_prime(&level, &cam);
    console_set_camera(&cam);
    input_begin(win_hwnd());
    frame_secs();
    for (;;) {
        win_pump();
        input_read(&in);
        if (in.quit || !game_frame(&cam, &in, frame_secs())) {
            break;
        }
        gfx_frame(&cam, &level);
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
