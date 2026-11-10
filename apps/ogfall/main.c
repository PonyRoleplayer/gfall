/*
 * OGFALL: classic's title and character creation, then Privateer's
 * Hold on foot with FALL.EXE's HUD, cursor and movement. The engine
 * draws and collides. This file is the window and the frame.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/gfx/camera.h"
#include "engine/gfx/gfx.h"
#include "engine/platform/input.h"
#include "engine/platform/win32.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "engine/ui/font.h"
#include "engine/ui/console.h"
#include "engine/ui/hud.h"
#include "engine/ui/pic.h"

#include "game/ogfall/play/chargen.h"
#include "game/ogfall/play/config.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/move.h"
#include "game/ogfall/play/options.h"
#include "game/ogfall/play/play.h"
#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/rest.h"
#include "game/ogfall/play/screen.h"
#include "game/ogfall/play/steer.h"
#include "game/ogfall/play/maps.h"

#include "seam.h"

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

static int typed(int vk)
{
    if (vk == VK_TAB) { return 9; }
    if (vk == VK_UP) { return 1001; }
    if (vk == VK_DOWN) { return 1002; }
    if (vk == VK_ESCAPE) {
        return 27;
    }
    if (vk == VK_RETURN) {
        return 13;
    }
    if (vk == VK_BACK) {
        return 8;
    }
    if (vk >= 'A' && vk <= 'Z') {
        return (GetKeyState(VK_SHIFT) < 0) != ((GetKeyState(VK_CAPITAL) & 1) != 0) ? vk : vk - 'A' + 'a';
    }
    if (vk == VK_SPACE || (vk >= '0' && vk <= '9')) { return vk; }
    return 0;
}

static InputKeys g_game_keys;

static void update_bindings(void)
{
    int i;
    for (i = 0; i < OG_ACTS; i++) {
        g_game_keys.key[i] = (unsigned char)og_config()->key[i];
    }
    g_game_keys.key[OG_ACTS] = 0x79; /* F10: classic full screen toggle. */
    g_game_keys.key[OG_ACTS + 1] = 0x1b;
    g_game_keys.count = OG_ACTS + 2;
}

static void fill_in(OgIn *in, const Input *keys, float dt)
{
    int x, y, vk;

    mem_zero(in, sizeof(*in));
    mem_copy(in->held, g_game_keys.held, OG_ACTS);
    mem_copy(in->pressed, g_game_keys.pressed, OG_ACTS);
    in->dt = keys->reset ? 0 : dt;
    in->reset = keys->reset;
    in->mouse_dx = keys->mouse_dx;
    in->mouse_dy = keys->mouse_dy;
    in->mouse_held = (int)keys->mouse_held;
    in->mouse_pressed = (int)keys->mouse_pressed;
    in->mouse_released = (int)keys->mouse_released;
    in->wheel = keys->wheel;
    in->toggle_fullscreen = g_game_keys.pressed[OG_ACTS];
    if (g_game_keys.pressed[OG_ACTS + 1]) { in->ch = 27; }
    input_pointer(&x, &y);
    og_point_at(x, y);
    if (og_chargen_mode() == OG_UI_WORLD && !og_maps_up() && !og_options_up() && !og_possessions()->page && !og_rest()->open && !og_dead()) {
        in->on_picture = og_world_pointer(x, y, df_view_w(), df_view_h(),
                                         !(og_options_flags() & 1), &in->cx, &in->cy);
    } else {
        in->on_picture = og_pointer_classic(x, y, df_view_w(), df_view_h(), &in->cx, &in->cy);
    }
    if (og_chargen_mode() == OG_UI_MAP && in->on_picture) {
        in->pixel = seam_map_pixel(in->cx, in->cy);
    }
    if (og_chargen_mode() != OG_UI_WORLD || og_maps_up() || og_options_up() || og_possessions()->page || og_rest()->open || og_dead()) {
        vk = input_key_pressed();
        in->key = vk;
        if (!in->ch) { in->ch = typed(vk); }
    }
}

static void extra(void)
{
    const char *line;

    /* The parchment, the list and the name screen, before og_draw blits them. */
    seam_present();
    og_draw();
    /* Chargen captions used to be painted on the map. The pictures carry the words. */
    if (og_chargen_mode() != OG_UI_WORLD) {
        return;
    }
    line = og_status_line();
    if (line && line[0]) {
        ui_string(8, 24, line);
    }
}

static void under(void)
{
    const AutomapView *view;
    view = seam_automap_view();
    if (view && seam_level()) { gfx_automap(seam_level(),view,0x141418u); }
}

static int enter_place(Camera *cam, int restoring, int changing)
{
    float x;
    float y;
    float z;
    Level *level;

    gfx_hold("Loading...");
    /* Release graphics references before the old place's heap is destroyed. */
    if (!gfx_prepare(0)) {
        win_fail(gfx_error());
        return 0;
    }
    if (!seam_open_place(restoring,changing,&x,&y,&z)) {
        win_fail(seam_error());
        return 0;
    }
    level = seam_level();
    /* One ring of 4096-unit chunks covers this distance from every point. */
    gfx_set_far(level->streamed ? 4096 : DF_FAR);
    if (!gfx_prepare(level)) {
        win_fail(gfx_error());
        return 0;
    }
    seam_land(cam,restoring,changing);
    seam_creatures(cam);
    gfx_prime(level, cam);
    gfx_fade_in(0.5f);
    return 1;
}

static int ogfall_main(void *instance)
{
    Camera cam;
    Input keys;
    OgIn in;
    int have;
    OgConfig preferences;

    mem_init();
    if (!win_startup(instance, "/ARENA2", "ogfall", 0)) {
        mem_shutdown();
        return 1;
    }
    if (!og_config_read()) { log_text(LOG_WARN, og_config_error()); }
    preferences = *og_config();
    gfx_set_mode(preferences.width, preferences.height);
    if (!gfx_startup(win_hwnd())) {
        win_fail(gfx_error());
        win_shutdown();
        mem_shutdown();
        return 1;
    }
    sound_startup(win_hwnd());
    music_startup();
    input_begin(win_hwnd());
    input_set_escape_quits(0);
    update_bindings();
    input_keys_attach(&g_game_keys);
    og_chargen_seed((unsigned)GetTickCount());
    pic_set_room(384);
    if (!seam_boot()) {
        win_fail(seam_error());
        input_end();
        music_shutdown(); sound_shutdown();
        gfx_shutdown();
        win_shutdown();
        mem_shutdown();
        return 1;
    }
    og_config_set(&preferences);
    update_bindings();
    hud_set(0);
    gfx_set_use_key(0);
    gfx_set_extra(extra);
    gfx_set_under(under);
    if (!gfx_prepare(0)) {
        win_fail(gfx_error());
        input_end();
        music_shutdown(); sound_shutdown();
        gfx_shutdown();
        win_shutdown();
        mem_shutdown();
        return 1;
    }
    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    have = 0;
    frame_secs();
    for (;;) {
        int want;
        int asked;
        int was_paused;
        static df_u16 preview[OG_THUMB_PIXELS];

        want = og_wants_pointer();
        if (want != input_pointer_on()) {
            input_set_pointer(want);
        }
        update_bindings();
        win_pump();
        console_pump();
        input_read(&keys);
        if (keys.quit) {
            break;
        }
        fill_in(&in, &keys, frame_secs());
        was_paused = og_paused();
        asked = console_open() ? OG_FRAME_MENU
            : og_frame(&in, &cam, have ? &seam_level()->scene : 0);
        if (asked == OG_FRAME_QUIT) {
            break;
        }
        if (asked == OG_FRAME_TITLE) {
            if (!gfx_prepare(0)) { win_fail(gfx_error()); break; }
            seam_close_hold();
            seam_title(); have = 0;
        }
        if (have && !was_paused && og_paused()) {
            og_state_preview(gfx_thumbnail(OG_THUMB_W, OG_THUMB_H, preview) ? preview : 0);
        }
        if ((asked == OG_FRAME_ENTER || asked == OG_FRAME_LOAD || asked == OG_FRAME_TRAVEL) &&
            !enter_place(&cam,asked == OG_FRAME_LOAD,asked == OG_FRAME_TRAVEL)) {
            break;
        }
        if (asked == OG_FRAME_ENTER || asked == OG_FRAME_LOAD || asked == OG_FRAME_TRAVEL) {
            have = 1;
        }
        if (have && !og_paused() && !console_open()) {
            seam_world_step(&cam,in.dt);
            if (seam_world_needs_reload()) {
                gfx_hold("Loading...");
                seam_world_reload(&cam);
                gfx_prime(seam_level(),&cam);
            }
            seam_creatures(&cam);
            sound_listen(cam.x, cam.y, cam.z, cam.yaw);
        }
        gfx_set_pause(og_paused());
        gfx_frame(&cam,have && !og_maps_up() ? seam_level() : 0);
    }
    input_keys_attach(0);
    input_end();
    music_shutdown(); sound_shutdown();
    gfx_shutdown();
    seam_close_hold();
    win_shutdown();
    mem_shutdown();
    return 0;
}

void WinMainCRTStartup(void)
{
    ExitProcess((UINT)ogfall_main(GetModuleHandleA(0)));
}
