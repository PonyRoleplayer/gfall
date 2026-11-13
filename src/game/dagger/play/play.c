#include "game/dagger/play/state.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/weapon.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/rest.h"
#include "game/dagger/play/actions.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/play/travel.h"
#include "game/dagger/play/maps.h"
#include "game/dagger/play/dungeon.h"
#include "game/dagger/tables/itemdefs.h"

#include "game/dagger/play/activate.h"
#include "game/dagger/play/chargen.h"
#include "game/dagger/play/hud.h"
#include "game/dagger/play/move.h"
#include "game/dagger/play/msg.h"
#include "game/dagger/play/options.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/controls.h"
#include "game/dagger/play/files.h"
#include "game/dagger/play/save.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/tables/tables.h"

#include "engine/core/df.h"
#include "engine/ui/pic.h"

static int g_pic[PIC_COUNT];
static int g_comp_pic[32];
static int g_comp_w;
static int g_comp_h;
static int g_comp_n;
static int g_preview_pic[6];
static void (*g_sound)(int id);
static void (*g_sound_at)(int id, float x, float y, float z);
static int (*g_words)(int id, char *dst, int cap);
static int (*g_word_variants)(int id, int variant, char *dst, int cap);
static int g_words_id;
static int g_words_variant;
static char g_words_buf[2048];
static int g_cursor_active;
static int g_world;
static int g_mouse_prev;
static int g_wait_buttons;
static int g_attack_motion, g_attack_click;
static int g_px;
static int g_py;
static int g_pointer_x, g_pointer_y, g_on_picture;
static int (*g_capture)(void);

void set_sound_at(void (*fn)(int, float, float, float)) { g_sound_at = fn; }
void sound_at(int id, float x, float y, float z)
{
    if (g_sound_at) { g_sound_at(id,x,y,z); }
    else { play_sound(id); }
}
int play_dead(void) { return g_world && character_state()->health <= 0; }

void play_sound(int id)
{
    if (id > 0 && g_sound) {
        g_sound(id);
    }
}

void gfall_play_startup(void)
{
    int i;

    for (i = 0; i < PIC_COUNT; i++) {
        g_pic[i] = 0;
    }
    g_comp_w = 0;
    g_comp_h = 0;
    g_comp_n = 0;
    for (i = 0; i < 32; i++) {
        g_comp_pic[i] = 0;
    }
    g_sound = 0;
    g_sound_at = 0;
    cheats_startup();
    travel_commands();
    g_capture = 0;
    g_words = 0;
    g_word_variants = 0;
    g_words_id = 0;
    g_words_buf[0] = 0;
    g_cursor_active = 0;
    g_world = 0;
    g_mouse_prev = 0;
    g_wait_buttons = 0;
    g_attack_motion = g_attack_click = 0;
    possessions_reset();
    rest_reset();
    hud_reset();
    options_reset();
    msg_clear();
    chargen_begin();
    move_init();
    weapon_reset(); combat_reset();
}

void gfall_play_shutdown(void)
{
    possessions_reset();
}

void set_save_capture(int (*fn)(void))
{
    g_capture = fn;
}

int pointer_region(void)
{
    int bar;
    bar = !(options_flags() & 1);
    if (play_paused() || options_mouse() || !g_on_picture || (bar && g_pointer_y >= 154)) { return -1; }
    return mouse_region(g_pointer_x, g_pointer_y, 320, bar ? 154 : 200, 0, 0);
}

void set_sound(void (*fn)(int id))
{
    g_sound = fn;
}

void set_words(int (*fn)(int id, char *dst, int cap))
{
    g_words = fn;
    g_words_id = 0;
    g_words_buf[0] = 0;
}

void set_word_variants(int (*fn)(int id, int variant, char *dst, int cap))
{
    g_word_variants = fn; g_words_id = 0;
}

const char *words_variant(int id, int variant)
{
    if (id <= 0) {
        return "";
    }
    if (id == g_words_id && variant == g_words_variant) {
        return g_words_buf;
    }
    g_words_id = id;
    g_words_variant = variant;
    g_words_buf[0] = 0;
    if ((g_word_variants ? g_word_variants(id,variant,g_words_buf,sizeof(g_words_buf)) :
         g_words ? g_words(id,g_words_buf,sizeof(g_words_buf)) : -1) < 0) {
        g_words_buf[0] = 0;
    }
    return g_words_buf;
}

const char *words_state(int id)
{
    return words_variant(id,0);
}

void pic_set(int slot, int id)
{
    if (slot >= 0 && slot < PIC_COUNT) {
        g_pic[slot] = id;
    }
}

int play_pic(int slot)
{
    if (slot < 0 || slot >= PIC_COUNT) {
        return 0;
    }
    return g_pic[slot];
}

void preview_pic_set(int slot, int pic)
{
    if (slot >= 0 && slot < 6) { g_preview_pic[slot] = pic; }
}

static void files_draw_previews(void)
{
    int i;
    if (options_panel() != PANEL_FILES || files_editing() || files_error()[0]) { return; }
    for (i = 0; i < 6; i++) {
        if (g_preview_pic[i]) { blit(g_preview_pic[i], i < 3 ? 40 : 200, (i % 3) * 65 + 4, 80, 50); }
    }
}

void compass_set(int frame, int pic, int w, int h)
{
    if (frame < 0 || frame >= 32) {
        return;
    }
    g_comp_pic[frame] = pic;
    g_comp_w = w;
    g_comp_h = h;
    if (frame + 1 > g_comp_n) {
        g_comp_n = frame + 1;
    }
}

int play_paused(void)
{
    return !g_world || maps_up() || options_up() || possessions_state()->page || rest_state()->open ||
        (g_world && character_state()->health <= 0) || chargen_mode() != UI_WORLD;
}

int cursor_free(void) { return g_cursor_active; }

int wants_pointer(void)
{
    if (play_paused() && chargen_mode() != UI_WORLD) {
        return 1;
    }
    if (maps_up() || options_up() || possessions_state()->page || rest_state()->open || play_dead()) {
        return 1;
    }
    if (options_mouse() == 0) {
        return 1;
    }
    return g_cursor_active;
}

void point_at(int x, int y)
{
    g_px = x;
    g_py = y;
}

static int (*g_status_intro)(char *dst, int cap);
void set_status_intro(int (*fn)(char *dst, int cap)) { g_status_intro = fn; }
int status_intro(char *dst, int cap) { return g_status_intro ? g_status_intro(dst,cap) : 0; }

const char *status_line(void)
{
    if (chargen_mode() != UI_WORLD) {
        return chargen_caption();
    }
    return possessions_state()->page ? "" : message_line();
}

static void mode_line(int step)
{
    char line[MESSAGE_LEN];

    hud_cycle(step, line, MESSAGE_LEN);
    msg_set(line);
}

static void ready_weapon(void)
{
    const ObjectInfo *item;
    weapon_cancel();
    if (hud_weapon_ready()) {
        item = object_at(game_state()->equipped[weapon_hand() ? 21 : 19]);
        if (item && item->group == 3) { play_sound(weapon_sound(item->index)); }
    }
}

/* The parchment's line count, so a click hits the buttons the picture drew. */
static void sync_lines(void)
{
    int mode;
    int id;
    const char *text;

    mode = chargen_mode();
    id = 0;
    if (mode == UI_RACE || mode == UI_CLASS_YES) {
        id = chargen_text_id();
    } else if (mode == UI_GENDER) {
        id = 2200;
    }
    if (id <= 0) {
        return;
    }
    text = words_state(id);
    if (text && text[0]) {
        chargen_set_lines(chargen_count_lines(text));
    }
}

static void screen_keys(const PlayIn *in)
{
    int was;
    int race;

    sync_lines();
    was = chargen_mode();
    if (in->ch) {
        chargen_key(in->ch);
    }
    if ((in->mouse_pressed & 1) && in->on_picture) {
        chargen_click(in->cx, in->cy, in->pixel);
    }
    chargen_scroll_frame(in->cx, in->cy, in->mouse_held, in->mouse_pressed, in->dt);
    sync_lines();
    /* title_menu plays pick + 208 as the confirm opens. pick is race + 1. */
    if (was == UI_MAP && chargen_mode() == UI_RACE) {
        race = character_state()->race;
        if (race >= 0 && race < 8) {
            play_sound(race + 209);
        }
    }
    if (was == UI_CLASS_LIST && chargen_mode() == UI_CLASS_YES) { play_sound(217); }
}

/* Position within one ninth's row or column; the last cell has spare pixels. */
static int cell_offset(int position, int cell_size)
{
    int offset;

    if (position < cell_size) {
        offset = position;
    } else if (position < cell_size * 2) {
        offset = position - cell_size;
    } else {
        offset = position - cell_size * 2;
    }
    if (offset < 0) {
        return 0;
    }
    return offset > cell_size ? cell_size : offset;
}

static void cursor_weights(MoveIn *move, int x, int y, int bar)
{
    int cell_width, cell_height;
    int offset_x, offset_y;

    cell_width = 320 / 3;
    cell_height = (bar ? 154 : 200) / 3;
    offset_x = cell_offset(x, cell_width);
    offset_y = cell_offset(y, cell_height);
    move->weight_left = (cell_width - offset_x) * 256 / cell_width;
    move->weight_right = offset_x * 256 / cell_width;
    move->weight_up = (cell_height - offset_y) * 256 / cell_height;
    move->weight_down = offset_y * 256 / cell_height;
}

static int modal_frame(const PlayIn *input)
{
    int result, sound, quit;

    if (options_panel() == PANEL_CONTROLS) {
        result = controls_editing();
        if (input->key && controls_key(input->key)) { options_back(); return FRAME_MENU; }
        if (result < 0 && (input->mouse_pressed & 1) && input->on_picture && controls_editing() < 0) {
            play_sound(203);
            if (controls_click(input->cx, input->cy)) { options_back(); }
        }
        controls_drag(input->cx, input->cy, input->mouse_held);
        return FRAME_MENU;
    }
    if (options_panel() == PANEL_FILES) {
        result = files_frame(input);
        if (result == FILES_CANCEL) {
            if (play_dead()) { options_close(); }
            else if (g_world) { options_back(); }
            else { options_close(); chargen_key(27); }
        } else if (result == FILES_SAVE) {
            if (g_capture && !g_capture()) {
                files_failed("Cannot capture the current place.");
                return FRAME_MENU;
            }
            state_capture_motion();
            state_ui(hud_mode(), hud_weapon_ready(), g_cursor_active);
            state_save_label(files_name());
            if (!save_write(files_selected() + 1)) {
                files_failed(save_error());
            } else {
                options_close();
                msg_set("Game saved.");
            }
        } else if (result == FILES_LOAD) {
            if (!save_read(files_selected() + 1)) {
                files_failed(save_error());
                return FRAME_MENU;
            }
            /* Preferences are global, not replaced by an older character's. */
            config_apply();
            options_close();
            chargen_resume();
            g_world = 1;
            g_cursor_active = game_state()->cursor_active;
            hud_set_mode(game_state()->hud_mode);
            if (hud_weapon_ready() != game_state()->weapon_ready) { hud_toggle_ready(); }
            msg_clear();
            return FRAME_LOAD;
        }
        return FRAME_MENU;
    }
    if (input->ch == 27) { options_commit(); return FRAME_MENU; }
    if ((input->mouse_pressed & 1) && input->on_picture) {
        if (options_click(input->cx, input->cy, &sound, &quit)) {
            play_sound(sound);
            if (quit) { return FRAME_QUIT; }
        }
    }
    options_drag(input->cx, input->mouse_held);
    return FRAME_MENU;
}

int play_frame(const PlayIn *in, Camera *cam, const Scene *scene)
{
    int bar;
    int region;
    int sound;
    int which;
    int step;
    int blocks;
    int result;
    int casting;
    df_u32 minute;
    MoveIn mv;

    if (!in) {
        return FRAME_MENU;
    }
    god_protect();
    g_pointer_x = in->cx;
    g_pointer_y = in->cy;
    g_on_picture = in->on_picture;
    if (in->reset) {
        g_attack_click = 0;
        g_mouse_prev = in->mouse_held;
        chargen_scroll_frame(0, 0, 0, 0, 0);
        options_cancel_drag();
        return play_paused() ? FRAME_MENU : FRAME_WORLD;
    }
    if (chargen_quit()) {
        return FRAME_QUIT;
    }
    if (maps_up()) {
        maps_frame(in); g_mouse_prev = in->mouse_held;
        if (!maps_up()) { g_wait_buttons = (in->mouse_held & 3) != 0; }
        return FRAME_MENU;
    }
    if (play_travel_asked()) { return FRAME_TRAVEL; }
    if (possessions_state()->page) {
        possessions_frame(in);
        g_mouse_prev = in->mouse_held;
        if (!possessions_state()->page) { g_wait_buttons = (in->mouse_held & 3) != 0; }
        return FRAME_MENU;
    }
    if (options_up()) {
        g_mouse_prev = in->mouse_held;
        result = modal_frame(in);
        if (!options_up()) { g_wait_buttons = (in->mouse_held & 3) != 0; }
        return result;
    }
    if (rest_state()->open) {
        rest_frame(in,cam);
        g_mouse_prev = in->mouse_held;
        if (!rest_state()->open) { g_wait_buttons = (in->mouse_held & 3) != 0; }
        return FRAME_MENU;
    }
    if (play_dead()) {
        weapon_cancel();
        if (in->ch == 'l' || in->ch == 'L' || in->ch == 13 ||
            ((in->mouse_pressed & 1) && in->on_picture && in->cx >= 75 && in->cx <= 245 && in->cy >= 86 && in->cy < 106)) {
            options_open_files(0);
        } else if (in->ch == 27 || in->ch == 'n' || in->ch == 'N' ||
            ((in->mouse_pressed & 1) && in->on_picture && in->cx >= 75 && in->cx <= 245 && in->cy >= 110 && in->cy < 130)) {
            options_close(); possessions_reset();
            chargen_begin(); g_world = 0; g_wait_buttons = 0;
            return FRAME_TITLE;
        } else if (in->ch == 'q' || in->ch == 'Q' ||
            ((in->mouse_pressed & 1) && in->on_picture && in->cx >= 75 && in->cx <= 245 && in->cy >= 134 && in->cy < 154)) {
            return FRAME_QUIT;
        }
        return FRAME_MENU;
    }
    /* A modal's closing click must be released before it can steer or use. */
    if (g_wait_buttons) {
        if (!(in->mouse_held & 3)) { g_wait_buttons = 0; g_mouse_prev = 0; }
        return FRAME_WORLD;
    }
    if (chargen_mode() != UI_WORLD) {
        screen_keys(in);
        if (chargen_mode() == UI_LOAD) { options_open_files(0); }
        if (chargen_quit()) {
            return FRAME_QUIT;
        }
        if (chargen_mode() == UI_WORLD) {
            g_world = 1;
            return FRAME_ENTER;
        }
        g_mouse_prev = in->mouse_held;
        return FRAME_MENU;
    }
    /* The renderer steps triggers after our frame. Apply its pending arrival
     * before this frame's movement, combat, or opening a save/options screen. */
    if (move_take_teleport()) {
        weapon_cancel();
        move_camera(cam);
    }
    actions_step(in->dt);
    if (possessions_state()->page || play_dead()) { return FRAME_MENU; }
    if (progress_state()->active && !dungeon_nearby(cam,1024)) {
        possessions_open(VIEW_SHEET,0);
        return FRAME_MENU;
    }
    if (in->toggle_fullscreen) {
        options_set_flags(options_flags() ^ 1);
        config_write();
    }
    if (in->pressed[CURSOR_KEY] && options_mouse() == 1) {
        g_cursor_active = g_cursor_active ? 0 : 1;
    }
    if (in->pressed[KEY_STEAL]) {
        hud_set_mode(MODE_STEAL);
        mode_line(0);
    } else if (in->pressed[KEY_GRAB]) {
        hud_set_mode(MODE_GRAB);
        mode_line(0);
    } else if (in->pressed[KEY_INFO]) {
        hud_set_mode(MODE_INFO);
        mode_line(0);
    } else if (in->pressed[KEY_TALK]) {
        hud_set_mode(MODE_TALK);
        mode_line(0);
    }
    if (in->pressed[READY]) {
        hud_toggle_ready();
        ready_weapon();
    }
    if (in->pressed[SWITCH]) { weapon_switch(); }
    bar = (options_flags() & 1) == 0;
    blocks = 0;
    if (bar && in->on_picture && in->cy >= HUD_BAR_Y) {
        hud_buttons(in->cx, in->cy, in->mouse_held, g_mouse_prev,
                       options_mouse(), g_cursor_active, &sound, &which, &step);
        if (sound) {
            play_sound(sound);
        }
        if (which == HUD_MODE) {
            mode_line(0);
        } else if (which == HUD_WEAPON) {
            ready_weapon();
        } else if (which == HUD_OPTIONS) {
            options_open();
        } else if (which == HUD_SHEET) { possessions_open(VIEW_SHEET,0); }
        else if (which == HUD_PACK) { possessions_open(VIEW_INVENTORY,0); }
        else if (which == HUD_SPELL) { possessions_cast(); }
        else if (which == HUD_MAP) { maps_open(0); }
        else if (which == HUD_TRAVEL) { maps_open(1); }
        else if (which == HUD_REST) { rest_open(scene,cam); }
        else if (which == HUD_STATUS) { magic_status(); }
        blocks = hud_blocks_mouse(in->cy, 1, key_region(in->held));
    }
    if (in->ch == 27) {
        options_open();
    }
    if (in->pressed[REST]) { rest_open(scene,cam); }
    if (in->pressed[AUTOMAP]) { maps_open(0); }
    if (in->pressed[TRAVEL]) { maps_open(1); }
    if (in->pressed[TRANSPORT] || in->pressed[MAGIC_ITEM]) {
        play_sound(203);
    }
    if (in->pressed[MAGIC_ITEM]) { possessions_magic_items(); }
    if (in->pressed[SHEET]) { possessions_open(VIEW_SHEET,0); }
    else if (in->pressed[INVENTORY]) { possessions_open(VIEW_INVENTORY,0); }
    else if (in->pressed[CAST]) { possessions_cast(); }
    if (in->pressed[RECAST]) { magic_recast(); }
    if (in->pressed[ABORT]) { magic_abort(); }
    if (in->pressed[STATUS_KEY]) { magic_status(); }
    region = control_region(in->on_picture ? in->cx : 160, in->on_picture ? in->cy : 77,
                               bar, options_mouse(), g_cursor_active,
                               in->on_picture && !blocks ? in->mouse_held : 0, in->held);
    /* A newly opened modal consumes this frame too. */
    if (maps_up() || options_up() || possessions_state()->page || rest_state()->open) {
        weapon_cancel();
        g_mouse_prev = in->mouse_held;
        return FRAME_MENU;
    }
    mv.held = in->held;
    mv.pressed = in->pressed;
    mv.region = region;
    mv.view_mode = options_mouse() == 1 && !g_cursor_active;
    mv.mouse_dx = in->mouse_dx;
    mv.mouse_dy = in->mouse_dy;
    /* A held attack gesture owns relative motion in either control scheme. */
    if (hud_weapon_ready() && in->held[SWING_BUTTON]) {
        mv.mouse_dx = 0; mv.mouse_dy = 0;
    }
    mv.dt = in->dt;
    mv.bar_blocks = blocks;
    mv.mouse_steer = key_region(in->held) < 0 && region != 4;
    cursor_weights(&mv, in->cx, in->cy, bar);
    /* The pack's weight walks every object: refreshed every few frames, the walk's speed is not felt to lag by so much. */
    { static int weight_age = 8, weight; if (++weight_age >= 8) { weight = inventory_weight(game_state()->inventory_id); weight_age = 0; } move_set_weight(weight); }
    move_frame(scene, &mv);
    minute = game_state()->minutes;
    state_frame(in->dt);
    survival_minutes((int)(game_state()->minutes-minute),in->held[RUN],move_climbing());
    survival_step(scene,in->dt);
    if (move_splashed()) {
        play_sound(86);
    }
    if (cam) {
        move_camera(cam);
        casting = magic_visible();
        {
            StrikeInfo strike;
            if (!magic_visible() && !(character_state()->conditions & 1)) {
                weapon_frame(in,blocks,&strike);
                dungeon_strike(scene,cam,&strike);
            }
            magic_frame(scene,cam,in,blocks);
            dungeon_step(scene,cam,in->dt);
            guards_tick(in->dt);
            law_calm();
        }
        if (casting || magic_visible()) {
            g_mouse_prev = in->mouse_held;
            return FRAME_WORLD;
        }
    }
    {
        int centre;
        int grab_click;
        int view_click;

        centre = in->pressed[ACTIVATE];
        if (in->mouse_pressed & 2) { g_attack_motion = 0; g_attack_click = 1; }
        if ((in->mouse_held | in->mouse_released) & 2) {
            g_attack_motion += (in->mouse_dx < 0 ? -in->mouse_dx : in->mouse_dx)
                             + (in->mouse_dy < 0 ? -in->mouse_dy : in->mouse_dy);
        }
        grab_click = ((in->mouse_pressed & 2) && !hud_weapon_ready())
            && options_mouse() == 0 && !blocks;
        /* intrface.c steer_center: left-clicking the X uses the pointed
         * object, even with a weapon drawn. The arrow regions still steer. */
        if ((in->mouse_pressed & 1) && in->on_picture && !blocks && options_mouse() == 0 &&
            mouse_region(in->cx,in->cy,320,bar ? 154 : 200,0,0) == 4) { grab_click = 1; }
        /* A stationary click still uses things with a melee weapon drawn.
         * A drag belongs to combat, and a bow's click always releases it. */
        if ((in->mouse_released & 2) && g_attack_click) {
            const ObjectInfo *weapon;
            weapon = object_at(game_state()->equipped[weapon_hand() ? 21 : 19]);
            if (hud_weapon_ready() && options_mouse() == 0 && !blocks &&
                g_attack_motion <= 4 && (!weapon || weapon->group != 3 || weapon->index < 16)) {
                grab_click = 1;
            }
            g_attack_click = 0;
        }
        if (in->pressed[CURSOR_KEY] && options_mouse() == 0 && !blocks) { grab_click = 1; }
        view_click = options_mouse() == 1 && g_cursor_active && (in->mouse_pressed & 1) && !blocks;
        if ((centre || grab_click || view_click) && cam) {
            activate(scene, cam, hud_mode(), options_mouse() == 0, g_cursor_active,
                        centre, in->cx, in->cy);
        }
    }
    g_mouse_prev = in->mouse_held;
    if (options_up() || possessions_state()->page) {
        return FRAME_MENU;
    }
    return FRAME_WORLD;
}

static void bar_at(int pic, int full, int x, int height)
{
    int y;

    if (pic <= 0 || height <= 0) {
        return;
    }
    if (height > 32) {
        height = 32;
    }
    y = 161 + (32 - height);
    hud_blit_part(pic, 0, 32 - height, 4, height, full ? x - 40 : x, y, 4, height);
}

static void draw_pointer(void)
{
    int region, id, w, h;
    if (!wants_pointer()) { return; }
    region = pointer_region();
    id = region >= 0 ? g_pic[PIC_CURSOR0 + region] : g_pic[PIC_ARROW];
    if (!id) { id = g_pic[PIC_ARROW]; }
    if (pic_size(id, &w, &h)) {
        pic_draw(id, g_px, g_py, w * df_view_w() / 320, h * df_view_w() / 320);
    }
}

void play_draw(void)
{
    int mode;
    int bar;
    int overlay;
    int height;
    int row;
    CharacterState *who;
    /* MAIN01I0's rows are steal, talk, grab, info. */
    static const int mode_row[4] = { 2, 3, 0, 1 };

    mode = chargen_mode();
    if (maps_up()) {
        blit(g_pic[PIC_SCREEN],0,0,320,200); maps_overlay(); draw_pointer(); return;
    }
    if (play_dead() && !options_up()) {
        blit(g_pic[PIC_SCREEN],0,0,320,200);
        draw_pointer();
        return;
    }
    if (possessions_state()->page && possessions_state()->page != VIEW_MESSAGE) {
        blit(g_pic[PIC_SCREEN],0,0,320,200);
        draw_pointer();
        return;
    }
    if (mode == UI_TITLE && g_pic[PIC_TITLE]) {
        blit(g_pic[PIC_TITLE], 0, 0, 320, 200);
    } else if (mode == UI_MAP) {
        /* The composed MAP100I0, or the plain one if the sentence did not fit. */
        blit(g_pic[PIC_MAP] ? g_pic[PIC_MAP] : g_pic[PIC_BACK], 0, 0, 320, 200);
    } else if (mode == UI_CLASS_HOW) {
        /* BUTN01I0.IMG at (68, 28), the two choices title_menu blits. */
        blit(g_pic[PIC_BACK], 0, 0, 320, 200);
        blit(g_pic[PIC_BUTTONS], 68, 28, 184, 144);
    } else if (mode != UI_WORLD) {
        if (g_pic[PIC_SCREEN]) {
            blit(g_pic[PIC_SCREEN], 0, 0, 320, 200);
        } else {
            blit(g_pic[PIC_BACK], 0, 0, 320, 200);
        }
    }
    if (mode != UI_WORLD || options_up()) {
        if (options_up()) {
            if (options_panel() == PANEL_OPTIONS) {
                blit(g_pic[PIC_OPTIONS], 85, 40, 150, 84);
            } else if (options_panel() == PANEL_CONTROLS && controls_mouse()) {
                blit(g_pic[PIC_MOUSE], 66, 22, 188, 146);
            } else {
                blit(g_pic[PIC_SCREEN], 0, 0, 320, 200);
                files_draw_previews();
            }
        }
        draw_pointer();
        return;
    }
    who = character_state();
    bar = (options_flags() & 1) == 0;
    if (!magic_visible()) { weapon_draw(); }
    magic_draw();
    if (bar) {
        hud_blit(g_pic[PIC_BAR], 0, 154, 320, 46);
        row = mode_row[hud_mode() & 3];
        hud_blit_part(g_pic[PIC_MODES], 0, row * 23, 47, 23, 131, 154, 47, 23);
        overlay = magic_indicators();
        overlay = hud_overlay(who->health,who->max_health,overlay & 1,overlay & 2,overlay & 4);
        if (overlay >= 0) {
            hud_blit(g_pic[PIC_OVERLAY+overlay], 5, 159, 36, 35);
        }
        hud_blit(g_pic[PIC_FACE], 7, 160, 32, 32);
        if (g_comp_n > 0) {
            int frame;

            frame = hud_compass_frame(move_yaw());
            if (frame < 0 || frame >= g_comp_n) {
                frame = 0;
            }
            hud_blit(g_comp_pic[frame], 272, 157, g_comp_w, g_comp_h);
        }
    }
    height = hud_bar(0, who->health, who->max_health, who->fatigue, who->attribute[ATTR_STR],
                        who->attribute[ATTR_END], who->magicka, who->spell_points_bonus, who->max_magicka);
    bar_at(g_pic[PIC_HEALTH], !bar, 49, height);
    height = hud_bar(1, who->health, who->max_health, who->fatigue, who->attribute[ATTR_STR],
                        who->attribute[ATTR_END], who->magicka, who->spell_points_bonus, who->max_magicka);
    bar_at(g_pic[PIC_FATIGUE], !bar, 57, height);
    height = hud_bar(2, who->health, who->max_health, who->fatigue, who->attribute[ATTR_STR],
                        who->attribute[ATTR_END], who->magicka, who->spell_points_bonus, who->max_magicka);
    bar_at(g_pic[PIC_MAGICKA], !bar, 65, height);
    if (!bar) {
        hud_blit(g_pic[PIC_COMPBOX], 251, 183, 69, 17);
        hud_blit_part(g_pic[PIC_COMPASS_WIDE], (move_yaw() & 2047) / 8, 0, 65, 13,
                         253, 185, 65, 13);
    }
    if (rest_state()->open || possessions_state()->page == VIEW_MESSAGE) { blit(g_pic[PIC_SCREEN],0,0,320,200); }
    if (survival_state()->breath > 0) {
        int remaining, id;
        remaining = survival_state()->breath;
        id = remaining < character_state()->attribute[ATTR_END]/8+4 ? PIC_BREATH_LOW : PIC_BREATH;
        blit(g_pic[id],310,120-remaining*2,6,remaining*2);
    }
    draw_pointer();
}
