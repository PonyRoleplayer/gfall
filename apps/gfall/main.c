/*
 * Privateer's Hold. The platform fills an input snapshot, the game flies
 * the camera, and the engine draws the level. Escape quits.
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
#include "game/dagger/play/player.h"
#include "game/dagger/places/world.h"
#include "game/dagger/tables/maindat.h"

int gfall_main(void *instance)
{
    World *world;
    Level level;
    Camera cam;
    Input in;
    float x;
    float y;
    float z;
    float yaw;
    float pitch;

    mem_init();
    if (!win_startup(instance, "/ARENA2", "gfall", 0)) {
        mem_shutdown();
        return 1;
    }
    /* Daggerfall's tables: MAIN.DAT beside the exe, cooked on the first run, and the mods over it (docs/data.md). */
    if (!maindat_start()) {
        win_fail("MAIN.DAT could not be made beside the program.");
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
    world_start(world, &x, &y, &z, &yaw, &pitch);
    camera_set(&cam, x, y, z, yaw, pitch);
    /* The creatures near the start read, then the hold's song and their sharp copies, before the first frame. */
    world_creatures_step(world, &cam, 2048.0f, 0.0f);
    gfx_prime(&level, &cam);
    console_set_camera(&cam);
    input_begin(win_hwnd());
    for (;;) {
        win_pump();
        input_read(&in);
        if (in.quit) {
            break;
        }
        player_tick(&cam, &in);
        /* The rest of the hold's creatures, as they come near, a little a frame (docs/entities.md). */
        if (!gfx_paused()) {
            world_creatures_step(world, &cam, 0.0f, 2.0f);
        }
        gfx_frame(&cam, &level);
    }
    input_end();
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
