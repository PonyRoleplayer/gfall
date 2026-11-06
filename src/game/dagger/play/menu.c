#include "game/dagger/play/menu.h"
#include "game/dagger/play/settings.h"
#include "engine/gfx/camera.h"
#include "engine/core/df.h"
#include "engine/gfx/gfx.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/draw.h"
#include "engine/scene/scene.h"
#include "engine/scene/light.h"
#include "engine/scene/weather.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "engine/assets/loader.h"
#include "engine/assets/song.h"
#include "game/dagger/tables/sounds.h"
#include "game/dagger/play/viewopt.h"
#include "engine/core/mem.h"
#include "game/dagger/play/mouse.h"
#include "game/dagger/play/control.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/keys.h"
#include "game/dagger/play/files.h"
#include "game/dagger/play/save.h"
#include "engine/core/prof.h"
#include "game/dagger/formats/img.h"
#include "engine/ui/font.h"
#include "engine/ui/pic.h"

/* See menu.h. */

/* A button: classic's rectangle, and what a click on it does. */
typedef struct Button {
    df_u16 x;
    df_u16 y;
    df_u16 w;
    df_u16 h;
    df_u8 action;
} Button;

enum {
    DO_LOAD,
    DO_START,
    DO_EXIT
};

/* DFU's StartWindow: on PICK03I0.IMG. */
static const Button g_main_buttons[] = {
    { 72, 45, 147, 15, DO_LOAD },
    { 72, 99, 147, 15, DO_START },
    { 125, 145, 41, 15, DO_EXIT }
};

static int g_open;
/* Drafts belong to the open settings session. Apply commits the active
 * section; Back/Escape cancels its uncommitted edits. The game remains paused. */
static SettingsView g_settings;
static int g_settings_pics[UI_ATLASES];
/* The saved games' screen: whether the player is in a game (and may save), who plays, the snapshot's picture and which one it is. */
static int g_files_can_save;
static df_u32 g_files_current;
static int g_thumb_pic;
static df_u32 g_thumb_serial;
static char g_chosen[SAVE_PATH];
static int g_draft_keys[INPUT_ACTIONS];
static int g_saved_keys[INPUT_ACTIONS];
static int g_saved_classic;
static int g_saved_fov;
static SettingsGfxDraft g_gfx_hold;
static int g_gfx_draft_open;
static int g_saved_sfx;
static int g_saved_music;
static int g_saved_bob;
static int g_saved_full_hud;
static int g_saved_crouch_toggle;
static int g_dragging;
static char g_key_names[SETTINGS_BINDINGS][24];
static int g_pic;
static int g_pic_x;
static int g_pic_y;
static int g_pic_w;
static int g_pic_h;
static int g_arrow;
static int g_world_cursor;
static int g_point_x;
static int g_point_y;
static const char *g_note;

/* A menu's picture on the card, from ARENA2, under its own palette or ART_PAL.COL's. */
static void show(const char *file)
{
    ImgPicture pic;
    df_u8 rgb[768];

    if (g_pic) {
        pic_free(g_pic);
        g_pic = 0;
    }
    if (!file || !img_load(file, &pic) || !img_palette(&pic, rgb)) {
        return;
    }
    g_pic = pic_load(pic.pixels, pic.width, pic.height, rgb, 0);
    g_pic_x = pic.x;
    g_pic_y = pic.y;
    g_pic_w = pic.width;
    g_pic_h = pic.height;
}

int menu_setup(void)
{
    ImgPicture arrow;
    df_u8 rgb[768];
    static df_u8 cursors[CONTROL_CURSOR_W * CONTROL_CURSOR_H];

    if (!img_load_raw("ARROW.RAW", 10, 10, &arrow) || !img_art_palette(rgb)) {
        return 0;
    }
    /* Index 0 round the arrow is clear. */
    g_arrow = pic_load(arrow.pixels, arrow.width, arrow.height, rgb, 1);
    /* One startup upload, nine UV rectangles: no texture churn while moving
     * the pointer between arrow regions. Procedural art uses ART_PAL.COL. */
    control_cursor_sheet(cursors);
    g_world_cursor = pic_load(cursors, CONTROL_CURSOR_W, CONTROL_CURSOR_H, rgb, 1);
    return g_arrow != 0 && g_world_cursor != 0;
}

void menu_shutdown(void)
{
    menu_open(MENU_NONE);
    if (g_world_cursor) {
        pic_free(g_world_cursor);
        g_world_cursor = 0;
    }
    if (g_arrow) {
        pic_free(g_arrow);
        g_arrow = 0;
    }
}

/* Settings textures are present only while the shell is open. Freeing
 * them before opening a save-slot screen keeps the existing 256 KB budget. */
static void settings_leave_tab(int tab);
static void settings_enter_tab(int tab);

static void settings_release(void)
{
    int i;
    if (g_open == MENU_OPTIONS || g_open == MENU_CONTROLS) {
        if (g_settings.tab == SETTINGS_GRAPHICS) settings_leave_tab(SETTINGS_GRAPHICS);
    }
    for (i = 0; i < UI_ATLASES; i++) {
        if (g_settings_pics[i]) pic_free(g_settings_pics[i]);
        g_settings_pics[i] = 0;
    }
    if (g_thumb_pic) pic_free(g_thumb_pic);
    g_thumb_pic = 0;
    g_thumb_serial = 0;
    ui_close();
}

/* The engine's modes into the selector, and the mode open now beside them. */
static void settings_load_modes(void)
{
    int w[SETTINGS_GFX_MODES_MAX];
    int h[SETTINGS_GFX_MODES_MAX];
    int n;
    int ow;
    int oh;

    for (n = 0; n < SETTINGS_GFX_MODES_MAX && gfx_mode(n, &w[n], &h[n]); n++) {
    }
    settings_set_modes(w, h, n);
    gfx_mode_opened(&ow, &oh);
    settings_set_opened(ow, oh);
}

/* Which of the listed modes is w x h; -1 when none is. */
static int mode_index(int w, int h)
{
    int i;

    for (i = 0; i < settings_resolutions(); i++) {
        if (settings_resolution_w(i) == w && settings_resolution_h(i) == h) {
            return i;
        }
    }
    return -1;
}

static void settings_gfx_read_engine(SettingsGfxDraft *draft)
{
    int w;
    int h;
    int at;

    /* The choice kept in the options file; else the mode that opened; else the first listed. */
    settings_load_modes();
    viewopt_mode(&w, &h);
    at = mode_index(w, h);
    if (at < 0) {
        gfx_mode_opened(&w, &h);
        at = mode_index(w, h);
    }
    draft->resolution = at < 0 ? 0 : at;
    draft->fov = df_iround(camera_fov());
    draft->far_clip = df_iround(gfx_far());
    draft->fog = gfx_fog();
    draft->spritepoint = draw_sprite_point();
    draft->vsync = glide_vsync();
    draft->lights = light_on();
    draft->ambient = light_ambient();
    draft->lightrange = light_range();
    draft->lightshadow = light_shadow();
    draft->weathersim = weather_sim();
    draft->flora = scene_flora();
    draft->floradensity = scene_flora_density();
    draft->florasight = scene_flora_sight();
    draft->texdetail = gfx_tex_detail();
    draft->spritedetail = gfx_sprite_detail();
}

void settings_gfx_draft_hold(SettingsGfxDraft *draft)
{
    settings_gfx_read_engine(draft);
    g_gfx_hold = *draft;
}

void settings_gfx_draft_commit(const SettingsGfxDraft *draft)
{
    camera_set_fov((float)draft->fov);
    gfx_set_far((float)draft->far_clip);
    gfx_set_fog(draft->fog);
    draw_set_sprite_point(draft->spritepoint);
    glide_set_vsync(draft->vsync);
    light_set_on(draft->lights);
    light_set_ambient(draft->ambient);
    gfx_set_light_range(draft->lightrange);
    gfx_set_light_shadow(draft->lightshadow);
    weather_set_sim(draft->weathersim);
    scene_set_flora(draft->flora);
    scene_set_flora_density(draft->floradensity);
    scene_set_flora_sight(draft->florasight);
    /* The mode is kept for the next start: the card is open at one now. */
    viewopt_set_mode(settings_resolution_w(draft->resolution), settings_resolution_h(draft->resolution));
    /* A change sends every picture again: only a real one is passed on, and it is kept for the options file. */
    if (gfx_tex_detail() != draft->texdetail) {
        gfx_set_tex_detail(draft->texdetail);
    }
    if (gfx_sprite_detail() != draft->spritedetail) {
        gfx_set_sprite_detail(draft->spritedetail);
    }
    viewopt_set_tex_detail(draft->texdetail);
    viewopt_set_sprite_detail(draft->spritedetail);
}

void settings_gfx_draft_cancel(void)
{
    settings_gfx_draft_commit(&g_gfx_hold);
}

static void settings_gfx_defaults(SettingsGfxDraft *draft)
{
    draft->resolution = mode_index(640, 480) < 0 ? 0 : mode_index(640, 480);
    draft->fov = 75;
    draft->far_clip = (int)DF_FAR;
    draft->fog = 1;
    draft->spritepoint = 1;
    draft->vsync = 1;
    draft->lights = 1;
    draft->ambient = 112;
    draft->lightrange = 3;
    draft->lightshadow = 1;
    draft->weathersim = 0;
    draft->flora = 1;
    draft->floradensity = 40;
    draft->florasight = 0;
    draft->texdetail = 2;
    draft->spritedetail = 2;
}

static int settings_gfx_dirty(void)
{
    SettingsGfxDraft *draft;
    df_u8 *a;
    df_u8 *b;
    int i;
    draft = &g_settings.gfx;
    a = (df_u8 *)draft;
    b = (df_u8 *)&g_gfx_hold;
    for (i = 0; i < (int)sizeof(SettingsGfxDraft); i++) {
        if (a[i] != b[i]) return 1;
    }
    return 0;
}

static void settings_gfx_drag_value(SettingsGfxDraft *draft, int action, int value)
{
    switch (action) {
    case SETTINGS_SLIDER: draft->fov = value; break;
    case SETTINGS_GFX_FAR_SLIDER: draft->far_clip = value; break;
    case SETTINGS_GFX_AMBIENT_SLIDER: draft->ambient = value; break;
    case SETTINGS_GFX_LIGHTRANGE_SLIDER: draft->lightrange = value; break;
    case SETTINGS_GFX_FLORADENSITY_SLIDER: draft->floradensity = value; break;
    case SETTINGS_GFX_FLORASIGHT_SLIDER: draft->florasight = value; break;
    default: break;
    }
}

static void settings_leave_tab(int tab)
{
    if (tab == SETTINGS_FILES && g_thumb_pic) {
        pic_free(g_thumb_pic);
        g_thumb_pic = 0;
        g_thumb_serial = 0;
    }
    if (tab == SETTINGS_GRAPHICS) settings_gfx_draft_cancel();
}

/* The snapshot of the save selected on the card, a new picture only when it changed. */
static void thumb_sync(void)
{
    if (files_thumb_serial() == g_thumb_serial) return;
    g_thumb_serial = files_thumb_serial();
    if (g_thumb_pic) pic_free(g_thumb_pic);
    g_thumb_pic = files_thumb() ? pic_load_565(files_thumb(), SAVE_THUMB_W, SAVE_THUMB_H) : 0;
}

static void settings_enter_tab(int tab)
{
    if (tab == SETTINGS_FILES) {
        files_open(g_files_can_save, g_files_current, 0);
        thumb_sync();
    }
    if (tab == SETTINGS_GRAPHICS && !g_gfx_draft_open) {
        settings_gfx_draft_hold(&g_settings.gfx);
        g_gfx_draft_open = 1;
    }
}

/* The title theme, until a place's song replaces it. Already playing, left alone. */
static void play_title(void)
{
    const char *name;
    Song song;

    name = sounds_title();
    if (!name || df_eq(music_name(), name)) {
        return;
    }
    mem_zero(&song, sizeof(song));
    if (!loader_call("song", name, 0, 0, &song)) {
        return;
    }
    music_play(&song, name);
}

void menu_open(int which)
{
    static const char *const files[] = { 0, "TITL00I0.IMG", "PICK03I0.IMG", 0, 0, "DIE_00I0.IMG" };
    int i;

    settings_release();
    g_open = which;
    g_note = 0;
    g_dragging = 0;
    show(which > MENU_NONE && which <= MENU_DEATH ? files[which] : 0);
    if (which == MENU_OPTIONS || which == MENU_CONTROLS) {
        mem_zero(&g_settings, sizeof(g_settings));
        g_settings.tab = which == MENU_CONTROLS ? SETTINGS_KEYS : SETTINGS_FILES;
        g_settings.waiting = -1;
        g_settings.hover = SETTINGS_NOTHING;
        g_settings.sfx_volume = g_saved_sfx = sound_volume();
        g_settings.music_volume = g_saved_music = music_volume();
        g_settings.bob = g_saved_bob = viewopt_bob();
        g_settings.crouch_toggle = g_saved_crouch_toggle = viewopt_crouch_toggle();
        g_gfx_draft_open = 0;
        g_saved_fov = df_iround(camera_fov());
        /* Legacy "full screen" hides the bar. The user-facing Full HUD
         * toggle is positive: On shows it. Keep the saved format intact. */
        g_settings.full_hud = g_saved_full_hud = !viewopt_full();
        g_settings.classic = g_saved_classic = control_classic();
        for (i = 0; i < INPUT_ACTIONS; i++) g_draft_keys[i] = g_saved_keys[i] = input_binding(i);
        if (g_settings.tab == SETTINGS_FILES) settings_enter_tab(SETTINGS_FILES);
        if (ui_open()) {
            for (i = 0; i < UI_ATLASES; i++) {
                int side;
                const df_u8 *pixels;
                pixels = ui_atlas(i, &side);
                g_settings_pics[i] = pic_load(pixels, side, side, ui_palette(), i != 0);
            }
        }
        input_key_pressed();
    }
    if (which == MENU_DEATH) input_key_pressed();
    if (which == MENU_TITLE || which == MENU_MAIN) {
        play_title();
    }
}

void menu_files(int can_save, df_u32 current)
{
    g_files_can_save = can_save;
    g_files_current = current;
}

const char *menu_chosen_path(void)
{
    return g_chosen;
}

void menu_files_saved(const char *path)
{
    if (g_open == MENU_OPTIONS || g_open == MENU_CONTROLS) {
        files_open(g_files_can_save, g_files_current, path);
        thumb_sync();
        files_say(path ? "Game saved." : "The game could not be saved.");
    }
}

void menu_say(const char *line)
{
    if ((g_open == MENU_OPTIONS || g_open == MENU_CONTROLS) && g_settings.tab == SETTINGS_FILES) files_say(line);
    else if (g_open == MENU_OPTIONS || g_open == MENU_CONTROLS) g_settings.note = line;
    else g_note = line;
}

int menu_current(void)
{
    return g_open;
}

/* The button under the pointer, of a menu whose buttons sit from left, top. */
static int hit(const Button *b, int n, int left, int top)
{
    int i;

    for (i = 0; i < n; i++) {
        if (screen_in(g_point_x, g_point_y, left + b[i].x, top + b[i].y, b[i].w, b[i].h)) {
            return b[i].action;
        }
    }
    return -1;
}

/* Draw callbacks use native pixels. Classic title/start screens and world
 * pointers retain screen.c's established 320x200 coordinate conversion. */
static void settings_quad(void *ctx, int atlas, int sx, int sy, int sw, int sh,
                          int x, int y, int w, int h)
{
    (void)ctx;
    pic_draw_part(g_settings_pics[atlas], sx, sy, sw, sh,
                  x * DF_VIEW_W / SETTINGS_W, DF_VIEW_H - y * DF_VIEW_H / SETTINGS_H,
                  w * DF_VIEW_W / SETTINGS_W, h * DF_VIEW_H / SETTINGS_H, 255);
}

static int settings_list_scroll(void)
{
    if (g_settings.tab == SETTINGS_GRAPHICS) return g_settings.gfx_scroll;
    return g_settings.scroll;
}

static int settings_dirty(void)
{
    int i;
    if (g_settings.tab == SETTINGS_GRAPHICS) return settings_gfx_dirty();
    if (g_settings.tab == SETTINGS_AUDIO)
        return g_settings.sfx_volume != g_saved_sfx || g_settings.music_volume != g_saved_music;
    if (g_settings.tab == SETTINGS_GAMEPLAY)
        return g_settings.bob != g_saved_bob || g_settings.full_hud != g_saved_full_hud
               || g_settings.crouch_toggle != g_saved_crouch_toggle;
    if (g_settings.tab != SETTINGS_KEYS) return 0;
    if (g_settings.classic != g_saved_classic) return 1;
    for (i = 0; i < INPUT_ACTIONS; i++) if (g_draft_keys[i] != g_saved_keys[i]) return 1;
    return 0;
}

/* Apply only the displayed section. A failed disk write is reported and
 * leaves the previous committed values in force; the draft can be retried. */
static void settings_apply(void)
{
    int i;
    if (g_settings.tab == SETTINGS_GRAPHICS)
        settings_gfx_draft_commit(&g_settings.gfx);
    else if (g_settings.tab == SETTINGS_AUDIO) {
        /* MIDI drivers without device volume rebuild the stream on a
         * volume change. Apply once, never restart music on each drag tick. */
        sound_set_volume(g_settings.sfx_volume);
        music_set_volume(g_settings.music_volume);
    } else if (g_settings.tab == SETTINGS_GAMEPLAY) {
        viewopt_set_bob(g_settings.bob);
        viewopt_set_full(!g_settings.full_hud);
        viewopt_set_crouch_toggle(g_settings.crouch_toggle);
    } else if (g_settings.tab == SETTINGS_KEYS) {
        for (i = 0; i < INPUT_ACTIONS; i++) input_bind(i, g_draft_keys[i]);
        control_set_classic(g_settings.classic);
        mouse_set_cursor(g_settings.classic);
    }
    if (!keys_save()) {
        if (g_settings.tab == SETTINGS_GRAPHICS) {
            settings_gfx_draft_cancel();
        } else if (g_settings.tab == SETTINGS_AUDIO) {
            sound_set_volume(g_saved_sfx);
            music_set_volume(g_saved_music);
        } else if (g_settings.tab == SETTINGS_GAMEPLAY) {
            viewopt_set_bob(g_saved_bob);
            viewopt_set_full(!g_saved_full_hud);
            viewopt_set_crouch_toggle(g_saved_crouch_toggle);
        } else if (g_settings.tab == SETTINGS_KEYS) {
            camera_set_fov((float)g_saved_fov);
            sound_set_volume(g_saved_sfx);
            music_set_volume(g_saved_music);
            viewopt_set_bob(g_saved_bob);
            viewopt_set_full(!g_saved_full_hud);
            viewopt_set_crouch_toggle(g_saved_crouch_toggle);
            for (i = 0; i < INPUT_ACTIONS; i++) input_bind(i, g_saved_keys[i]);
            control_set_classic(g_saved_classic);
            mouse_set_cursor(g_saved_classic);
        } else {
            camera_set_fov((float)g_saved_fov);
            sound_set_volume(g_saved_sfx);
            music_set_volume(g_saved_music);
            viewopt_set_bob(g_saved_bob);
            viewopt_set_full(!g_saved_full_hud);
            viewopt_set_crouch_toggle(g_saved_crouch_toggle);
        }
        g_settings.note = "Could not save settings. Changes were not applied.";
        return;
    }
    if (g_settings.tab == SETTINGS_GRAPHICS) {
        g_gfx_hold = g_settings.gfx;
        g_saved_fov = g_settings.gfx.fov;
    } else if (g_settings.tab == SETTINGS_AUDIO) {
        g_saved_sfx = g_settings.sfx_volume;
        g_saved_music = g_settings.music_volume;
    } else if (g_settings.tab == SETTINGS_GAMEPLAY) {
        g_saved_bob = g_settings.bob;
        g_saved_full_hud = g_settings.full_hud;
        g_saved_crouch_toggle = g_settings.crouch_toggle;
    } else if (g_settings.tab == SETTINGS_KEYS) {
        for (i = 0; i < INPUT_ACTIONS; i++) g_saved_keys[i] = g_draft_keys[i];
        g_saved_classic = g_settings.classic;
    }
    g_settings.note = "Settings applied.";
}

static int settings_frame(const Input *in, int px, int py)
{
    int click, hit, vk, i;
    vk = input_key_pressed();
    click = (in->mouse_pressed & INPUT_MOUSE_LEFT) != 0;
    /* Wheel updates list offset before hit testing so hover matches layout. */
    if (g_settings.tab == SETTINGS_KEYS && in->wheel) {
        g_settings.scroll -= in->wheel;
        if (g_settings.scroll < 0) g_settings.scroll = 0;
        if (g_settings.scroll > SETTINGS_BINDINGS-SETTINGS_ROWS) g_settings.scroll = SETTINGS_BINDINGS-SETTINGS_ROWS;
    }
    if (g_settings.tab == SETTINGS_GRAPHICS && in->wheel) {
        g_settings.gfx_scroll -= in->wheel;
        if (g_settings.gfx_scroll < 0) g_settings.gfx_scroll = 0;
        if (g_settings.gfx_scroll > SETTINGS_GFX_ITEM_COUNT - SETTINGS_GFX_ROWS)
            g_settings.gfx_scroll = SETTINGS_GFX_ITEM_COUNT - SETTINGS_GFX_ROWS;
    }
    if (g_settings.tab == SETTINGS_FILES && in->wheel) files_wheel(in->wheel);
    hit = settings_hit(g_settings.tab, settings_list_scroll(), px, py);
    g_settings.hover = hit;
    /* Focus loss cancels a drag or key capture BEFORE interpreting releases
     * or a newly polled held key. A resumed frame needs a fresh click. */
    if (in->reset) {
        g_dragging = 0;
        g_settings.waiting = -1;
        return MENU_STAY;
    }
    if (g_settings.waiting >= 0) {
        if (vk) {
            if (vk != 0x1b) {
                int action, old;
                action = settings_binding_action(g_settings.waiting, g_settings.classic);
                old = g_draft_keys[action];
                /* input_bind owns validation. Restore immediately: drafting
                 * must not change gameplay bindings until Apply succeeds. */
                if (input_bind(action, vk)) {
                    input_bind(action, g_saved_keys[action]);
                    for (i = 0; i < SETTINGS_BINDINGS; i++) {
                        int other;
                        other = settings_binding_action(i, g_settings.classic);
                        if (other != action && g_draft_keys[other] == vk) g_draft_keys[other] = old;
                    }
                    g_draft_keys[action] = vk;
                    g_settings.note = 0;
                } else g_settings.note = "That key belongs to the menu or console.";
            } else g_settings.note = 0;
            g_settings.waiting = -1;
        }
        return MENU_STAY;
    }
    if (in->pressed & INPUT_BIT(INPUT_MENU)) {
        menu_open(MENU_NONE);
        return MENU_CLOSED;
    }
    /* The saved games' keys: Up and Down, Enter to load, Delete. */
    if (g_settings.tab == SETTINGS_FILES && vk) {
        int asked;

        asked = files_key(vk);
        thumb_sync();
        if (asked == FILES_LOAD) {
            df_copy(g_chosen, files_chosen(), SAVE_PATH);
            return MENU_LOAD;
        }
    }
    /* Store the slider action, not just a drag flag: moving vertically
     * across the other audio row must not transfer ownership to it. */
    if (click && settings_slider_at(hit, px) >= 0) g_dragging = hit + 1;
    if (g_dragging) {
        int value;
        value = settings_slider_at(g_dragging - 1, px);
        if (g_settings.tab == SETTINGS_GRAPHICS)
            settings_gfx_drag_value(&g_settings.gfx, g_dragging - 1, value);
        else if (g_dragging - 1 == SETTINGS_SFX_SLIDER) g_settings.sfx_volume = value;
        else if (g_dragging - 1 == SETTINGS_MUSIC_SLIDER) g_settings.music_volume = value;
        g_settings.note = 0;
        if (!(in->mouse_held & INPUT_MOUSE_LEFT)) g_dragging = 0;
        return MENU_STAY;
    }
    if (!click) return MENU_STAY;
    g_settings.note = 0;
    if (hit != SETTINGS_EXIT) g_settings.confirm_exit = 0;
    if (hit >= SETTINGS_TAB_FILES && hit <= SETTINGS_TAB_GAMEPLAY) {
        int new_tab;
        new_tab = hit - SETTINGS_TAB_FILES;
        if (new_tab != g_settings.tab) {
            settings_leave_tab(g_settings.tab);
            g_settings.tab = new_tab;
            settings_enter_tab(g_settings.tab);
        }
        g_open = g_settings.tab == SETTINGS_KEYS ? MENU_CONTROLS : MENU_OPTIONS;
    } else if (g_settings.tab == SETTINGS_FILES && hit >= FILES_ID_LOAD) {
        /* A control of the saved games' screen: its own to say what a click does. */
        int asked;

        asked = files_click(hit, prof_now(), prof_per_ms());
        thumb_sync();
        if (asked == FILES_LOAD) {
            df_copy(g_chosen, files_chosen(), SAVE_PATH);
            return MENU_LOAD;
        }
        if (asked == FILES_SAVE) return MENU_SAVE;
    } else if (hit >= SETTINGS_BIND && hit < SETTINGS_BIND + SETTINGS_BINDINGS) {
        g_settings.waiting = hit-SETTINGS_BIND;
    } else switch (hit) {
    case SETTINGS_EXIT:
        if (g_settings.confirm_exit) return MENU_QUIT;
        g_settings.confirm_exit = 1;
        g_settings.note = "Click Confirm Exit to leave the game.";
        break;
    case SETTINGS_BACK:
        menu_open(MENU_NONE);
        return MENU_CLOSED;
    case SETTINGS_APPLY: settings_apply(); break;
    case SETTINGS_DEFAULT:
        if (g_settings.tab == SETTINGS_GRAPHICS) settings_gfx_defaults(&g_settings.gfx);
        else if (g_settings.tab == SETTINGS_AUDIO) {
            g_settings.sfx_volume = 80;
            g_settings.music_volume = 80;
        } else if (g_settings.tab == SETTINGS_GAMEPLAY) {
            g_settings.bob = 0;
            g_settings.full_hud = 1;
            g_settings.crouch_toggle = 0;
        } else if (g_settings.tab == SETTINGS_KEYS) {
            /* Borrow the one canonical default table, then restore live
             * preferences. No duplicate binding defaults can drift here. */
            keys_defaults();
            for (i = 0; i < INPUT_ACTIONS; i++) {
                g_draft_keys[i] = input_binding(i);
                input_bind(i, g_saved_keys[i]);
            }
            control_set_classic(g_saved_classic);
            mouse_set_cursor(g_saved_classic);
            g_settings.classic = 1;
        }
        break;
    case SETTINGS_GFX_TEXDETAIL: g_settings.gfx.texdetail = (g_settings.gfx.texdetail + 1) % 3; break;
    case SETTINGS_GFX_SPRITEDETAIL: g_settings.gfx.spritedetail = (g_settings.gfx.spritedetail + 1) % 3; break;
    case SETTINGS_FULL_HUD: g_settings.full_hud = !g_settings.full_hud; break;
    case SETTINGS_BOB: g_settings.bob = !g_settings.bob; break;
    case SETTINGS_CROUCH_TOGGLE: g_settings.crouch_toggle = !g_settings.crouch_toggle; break;
    case SETTINGS_MOUSE: g_settings.classic = !g_settings.classic; break;
    case SETTINGS_UP:
        if (g_settings.tab == SETTINGS_GRAPHICS) {
            if (g_settings.gfx_scroll > 0) g_settings.gfx_scroll--;
        } else if (g_settings.scroll > 0) g_settings.scroll--;
        break;
    case SETTINGS_DOWN:
        if (g_settings.tab == SETTINGS_GRAPHICS) {
            if (g_settings.gfx_scroll < SETTINGS_GFX_ITEM_COUNT - SETTINGS_GFX_ROWS) g_settings.gfx_scroll++;
        } else if (g_settings.scroll < SETTINGS_BINDINGS - SETTINGS_ROWS) g_settings.scroll++;
        break;
    case SETTINGS_GFX_RESOLUTION: g_settings.gfx.resolution = (g_settings.gfx.resolution + 1) % settings_resolutions(); break;
    case SETTINGS_GFX_FOG: g_settings.gfx.fog = !g_settings.gfx.fog; break;
    case SETTINGS_GFX_SPRITEPOINT: g_settings.gfx.spritepoint = !g_settings.gfx.spritepoint; break;
    case SETTINGS_GFX_VSYNC: g_settings.gfx.vsync = !g_settings.gfx.vsync; break;
    case SETTINGS_GFX_LIGHTS: g_settings.gfx.lights = !g_settings.gfx.lights; break;
    case SETTINGS_GFX_LIGHTSHADOW: g_settings.gfx.lightshadow = !g_settings.gfx.lightshadow; break;
    case SETTINGS_GFX_WEATHERSIM: g_settings.gfx.weathersim = !g_settings.gfx.weathersim; break;
    case SETTINGS_GFX_FLORA: g_settings.gfx.flora = !g_settings.gfx.flora; break;
    default: break;
    }
    return MENU_STAY;
}

int menu_frame(const Input *in)
{
    int click;
    int px;
    int py;
    int what;

    input_pointer(&px, &py);
    screen_from_pointer(px, py, &g_point_x, &g_point_y);
    click = !in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT) != 0;
    switch (g_open) {
    case MENU_TITLE:
        /* Any key or click goes on to the menu, bound to an action or not. */
        if (input_key_pressed() || in->pressed) {
            menu_open(MENU_MAIN);
        }
        return MENU_STAY;
    case MENU_MAIN:
        what = click ? hit(g_main_buttons, 3, 0, 0) : -1;
        if (what == DO_START) {
            menu_open(MENU_NONE);
            return MENU_START;
        }
        if (what == DO_EXIT) {
            return MENU_QUIT;
        }
        if (what == DO_LOAD) {
            return MENU_LOAD;
        }
        return MENU_STAY;
    case MENU_OPTIONS:
    case MENU_CONTROLS:
        return settings_frame(in, px * SETTINGS_W / DF_VIEW_W,
                              (DF_VIEW_H - 1 - py) * SETTINGS_H / DF_VIEW_H);
    case MENU_DEATH:
        if (input_key_pressed() || in->pressed) {
            return MENU_DEATH_OVER;
        }
        return MENU_STAY;
    default:
        return MENU_STAY;
    }
}

void menu_draw_pointer(int x, int y)
{
    screen_draw(g_arrow, x, y, 10, 10);
}

void menu_draw_world_pointer(int shape, int x, int y)
{
    if (!g_world_cursor || shape < CONTROL_TARGET || shape > CONTROL_NW) {
        menu_draw_pointer(x, y);
        return;
    }
    /* Source coordinates are top-down like the startup sheet. The 8,8
     * hotspot stays at the activation ray, including near screen edges. */
    pic_draw_part(g_world_cursor, (shape % 4) * 16, (shape / 4) * 16, 16, 16,
                  screen_x(x - 8), screen_top(y - 8), screen_x(16), screen_h(16), 255);
}

void menu_draw(void)
{
    if (g_open == MENU_NONE) {
        return;
    }
    screen_draw(g_pic, g_pic_x, g_pic_y, g_pic_w, g_pic_h);
    if (g_open == MENU_OPTIONS || g_open == MENU_CONTROLS) {
        int i;
        for (i = 0; i < SETTINGS_BINDINGS; i++) {
            input_key_name(g_draft_keys[settings_binding_action(i, g_settings.classic)], g_key_names[i], 24);
            g_settings.bindings[i] = g_key_names[i];
        }
        g_settings.dirty = settings_dirty();
        settings_draw(&g_settings, settings_quad, 0);
        /* The snapshot of the save selected, over its frame (the same 640 x 480 the shell's quads use). */
        if (g_settings.tab == SETTINGS_FILES && g_thumb_pic) {
            int tx;
            int ty;
            int tw;
            int th;

            files_thumb_rect(&tx, &ty, &tw, &th);
            pic_draw(g_thumb_pic, tx * DF_VIEW_W / SETTINGS_W, DF_VIEW_H - ty * DF_VIEW_H / SETTINGS_H,
                     tw * DF_VIEW_W / SETTINGS_W, th * DF_VIEW_H / SETTINGS_H);
        }
    }
    if (g_note) {
        ui_string((DF_VIEW_W - ui_string_width(g_note)) / 2,
                  g_open == MENU_CONTROLS ? screen_h(10) + ui_line_height() + 2 : ui_line_height() + 8, g_note);
    }
    if (g_open != MENU_TITLE && g_open != MENU_DEATH) {
        screen_draw(g_arrow, g_point_x, g_point_y, 10, 10);
    }
}
