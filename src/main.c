/*
 * GFALL frame. Platform fills one input snapshot, the player flies the
 * camera, and the renderer draws the block. Nothing else is in this step.
 */

#include "game/player.h"
#include "game/world.h"
#include "platform/mem.h"
#include "platform/win32.h"
#include "render/camera.h"
#include "render/draw.h"
#include "render/glide.h"
#include "render/texcache.h"

int gfall_main(void *instance)
{
    World *world;
    Camera cam;
    Input in;
    float x;
    float y;
    float z;
    float yaw;
    float pitch;

    mem_init();
    if (!win_startup(instance)) {
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

    if (!glide_open(win_hwnd())) {
        win_fail(glide_error());
        win_shutdown();
        mem_shutdown();
        return 1;
    }
    if (!texcache_upload(world)) {
        win_fail(texcache_error());
        glide_close();
        win_shutdown();
        mem_shutdown();
        return 1;
    }

    world_start(world, &x, &y, &z, &yaw, &pitch);
    camera_set(&cam, x, y, z, yaw, pitch);
    input_begin(win_hwnd());
    for (;;) {
        win_pump();
        input_read(&in);
        if (in.quit) {
            break;
        }
        player_tick(&cam, &in);
        draw_frame(&cam, world);
    }
    input_end();
    glide_close();
    win_shutdown();
    mem_shutdown();
    return 0;
}
