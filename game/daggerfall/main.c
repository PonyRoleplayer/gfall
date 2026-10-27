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
#include "game/dagger/fnt.h"
#include "game/dagger/player.h"
#include "game/dagger/world.h"

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
    /* The hold's song and its creatures' sharp copies before the first frame. */
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
