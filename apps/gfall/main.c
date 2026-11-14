/*
 * GFALL: classic's title and character creation, then Privateer's
 * Hold on foot with FALL.EXE's HUD, cursor and movement. The engine
 * draws and collides. This file is the window and the frame.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/prof.h"
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

#include "game/dagger/play/chargen.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/move.h"
#include "game/dagger/play/options.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/rest.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/steer.h"
#include "game/dagger/play/maps.h"
#include "game/dagger/play/msg.h"

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
    for (i = 0; i < ACTS; i++) {
        g_game_keys.key[i] = (unsigned char)game_config()->key[i];
    }
    g_game_keys.key[ACTS] = 0x79; /* F10: classic full screen toggle. */
    g_game_keys.key[ACTS + 1] = 0x1b;
    g_game_keys.count = ACTS + 2;
}

static void fill_in(PlayIn *in, const Input *keys, float dt)
{
    int x, y, vk;

    mem_zero(in, sizeof(*in));
    mem_copy(in->held, g_game_keys.held, ACTS);
    mem_copy(in->pressed, g_game_keys.pressed, ACTS);
    in->dt = keys->reset ? 0 : dt;
    in->reset = keys->reset;
    in->mouse_dx = keys->mouse_dx;
    in->mouse_dy = keys->mouse_dy;
    in->mouse_held = (int)keys->mouse_held;
    in->mouse_pressed = (int)keys->mouse_pressed;
    in->mouse_released = (int)keys->mouse_released;
    in->wheel = keys->wheel;
    in->toggle_fullscreen = g_game_keys.pressed[ACTS];
    if (g_game_keys.pressed[ACTS + 1]) { in->ch = 27; }
    input_pointer(&x, &y);
    point_at(x, y);
    if (chargen_mode() == UI_WORLD && !maps_up() && !options_up() && !possessions_state()->page && !rest_state()->open && !play_dead()) {
        in->on_picture = world_pointer(x, y, df_view_w(), df_view_h(),
                                         !(options_flags() & 1), &in->cx, &in->cy);
    } else {
        in->on_picture = pointer_classic(x, y, df_view_w(), df_view_h(), &in->cx, &in->cy);
    }
    if (chargen_mode() == UI_MAP && in->on_picture) {
        in->pixel = seam_map_pixel(in->cx, in->cy);
    }
    if (chargen_mode() != UI_WORLD || maps_up() || options_up() || possessions_state()->page || rest_state()->open || play_dead()) {
        vk = input_key_pressed();
        in->key = vk;
        if (!in->ch) { in->ch = typed(vk); }
    }
}

static void extra(void)
{
    const char *line;

    /* The parchment, the list and the name screen, before play_draw blits them. */
    seam_present();
    play_draw();
    /* Chargen captions used to be painted on the map. The pictures carry the words. */
    if (chargen_mode() != UI_WORLD) {
        return;
    }
    line = status_line();
    if (line && line[0]) {
        const char *entry;
        int alpha, row;
        /* ui_string's top counts up from the bottom of the view: the feed is centred along the top, the newest first and each older entry a row lower. */
        for (row = 0; (entry = msg_entry(row, &alpha)) != 0; row++) {
            ui_string_alpha((df_view_w() - ui_string_width(entry)) / 2, df_view_h() - 12 - row * ui_line_height(), entry, alpha);
        }
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

static int gfall_main(void *instance)
{
    Camera cam;
    Input keys;
    PlayIn in;
    int have;
    Config preferences;

    mem_init();
    if (!win_startup(instance, "/ARENA2", "gfall", 0)) {
        mem_shutdown();
        return 1;
    }
    if (!config_read()) { log_text(LOG_WARN, config_error()); }
    preferences = *game_config();
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
    chargen_seed((unsigned)GetTickCount());
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
    config_set(&preferences);
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
        static df_u16 preview[THUMB_PIXELS];

        want = wants_pointer();
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
        was_paused = play_paused();
        /* The game's own frame shows in the profile (docs/profile.md): rules, bodies, motors, the world's step. */
        prof_begin(PROF_PLAY);
        asked = console_open() ? FRAME_MENU
            : play_frame(&in, &cam, have ? &seam_level()->scene : 0);
        prof_end(PROF_PLAY);
        if (asked == FRAME_QUIT) {
            break;
        }
        if (asked == FRAME_TITLE) {
            if (!gfx_prepare(0)) { win_fail(gfx_error()); break; }
            seam_close_hold();
            seam_title(); have = 0;
        }
        if (have && !was_paused && play_paused()) {
            state_preview(gfx_thumbnail(THUMB_W, THUMB_H, preview) ? preview : 0);
        }
        if ((asked == FRAME_ENTER || asked == FRAME_LOAD || asked == FRAME_TRAVEL) &&
            !enter_place(&cam,asked == FRAME_LOAD,asked == FRAME_TRAVEL)) {
            break;
        }
        if (asked == FRAME_ENTER || asked == FRAME_LOAD || asked == FRAME_TRAVEL) {
            have = 1;
        }
        if (have && !play_paused() && !console_open()) {
            prof_begin(PROF_PLAY);
            seam_world_step(&cam,in.dt);
            if (seam_world_needs_reload()) {
                gfx_hold("Loading...");
                seam_world_reload(&cam);
                gfx_prime(seam_level(),&cam);
            }
            seam_creatures(&cam);
            prof_end(PROF_PLAY);
            sound_listen(cam.x, cam.y, cam.z, cam.yaw);
        }
        gfx_set_pause(play_paused());
        /* No world behind a map, or behind a page that covers the whole screen (seam_screen_covers). */
        gfx_frame(&cam,have && !maps_up() && !seam_screen_covers() ? seam_level() : 0);
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
    ExitProcess((UINT)gfall_main(GetModuleHandleA(0)));
}
