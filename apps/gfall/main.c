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

#include "engine/core/df.h"
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

/* The far clip each kind of place draws to: a dungeon's own, the world's far enough to see a town across a field. */
#define WORLD_FAR 8192.0f

/*
 * Where the game asked to go, under a held "Loading..." over the last frame:
 * the old place left (travel.h), the new one read onto a temp heap of its
 * own, or every load would leak its arrays; what is round the player loaded
 * and lit, its pictures sharp before it fades in. arriving 0: a new or a
 * loaded game; 1: through a passage, the player going on as they are.
 * 0 when it does not load.
 */
static int enter_place(Level *level, Camera *cam, int arriving)
{
    TravelPose at;

    gfx_hold("Loading...");
    travel_leave();
    /* What a place may see depends on what it is: decided before the world's radius is. */
    gfx_set_far(travel_next_is_world() ? WORLD_FAR : DF_FAR);
    if (!travel_open(level, travel_radius(gfx_far()))) {
        win_fail(travel_error());
        return 0;
    }
    /* A house that would not read has put the player back in the street: the world's clip again. */
    gfx_set_far(travel_next_is_world() ? WORLD_FAR : DF_FAR);
    if (!gfx_prepare(level)) {
        win_fail(gfx_error());
        return 0;
    }
    travel_land(level, cam, game_eye(), &at);
    if (arriving) {
        game_arrive(level, cam, &at);
    } else {
        game_enter(level, cam, &at);
    }
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
        if ((asked == GAME_START || asked == GAME_TRAVEL) && !enter_place(&level, &cam, asked == GAME_TRAVEL)) {
            break;
        }
        /* The season turned, or the world's meshes filled up: behind "Loading...", sharp again before the next frame. */
        if (travel_must_reload()) {
            gfx_hold("Loading...");
            travel_reload(&cam);
            gfx_prime(&level, &cam);
            gfx_fade_in(0.5f);
        }
        /* A screen that covers everything is drawn with no world under it: the card does nothing it would then hide. */
        gfx_frame(&cam, game_world_shows() ? &level : 0);
    }
    input_end();
    game_shutdown();
    travel_leave();
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
