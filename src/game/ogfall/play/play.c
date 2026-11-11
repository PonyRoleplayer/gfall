#include "game/ogfall/play/state.h"
#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/play.h"
#include "game/ogfall/play/law.h"
#include "game/ogfall/play/weapon.h"
#include "game/ogfall/play/combat.h"
#include "game/ogfall/play/cheats.h"
#include "game/ogfall/play/rest.h"
#include "game/ogfall/play/actions.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/travel.h"
#include "game/ogfall/play/maps.h"
#include "game/ogfall/play/dungeon.h"
#include "game/ogfall/tables/itemdefs.h"

#include "game/ogfall/play/activate.h"
#include "game/ogfall/play/chargen.h"
#include "game/ogfall/play/hud.h"
#include "game/ogfall/play/move.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/play/options.h"
#include "game/ogfall/play/config.h"
#include "game/ogfall/play/controls.h"
#include "game/ogfall/play/files.h"
#include "game/ogfall/play/save.h"
#include "game/ogfall/play/screen.h"
#include "game/ogfall/tables/tables.h"

#include "engine/core/df.h"
#include "engine/ui/pic.h"

static int g_pic[OG_PIC_COUNT];
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

void og_set_sound_at(void (*fn)(int, float, float, float)) { g_sound_at = fn; }
void og_sound_at(int id, float x, float y, float z)
{
    if (g_sound_at) { g_sound_at(id,x,y,z); }
    else { og_sound(id); }
}
int og_dead(void) { return g_world && og_character()->health <= 0; }

void og_sound(int id)
{
    if (id > 0 && g_sound) {
        g_sound(id);
    }
}

void ogfall_play_startup(void)
{
    int i;

    for (i = 0; i < OG_PIC_COUNT; i++) {
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
    og_cheats_startup();
    og_travel_commands();
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
    og_possessions_reset();
    og_rest_reset();
    og_hud_reset();
    og_options_reset();
    og_msg_clear();
    og_chargen_begin();
    og_move_init();
    og_weapon_reset(); og_combat_reset();
}

void ogfall_play_shutdown(void)
{
    og_possessions_reset();
}

void og_set_save_capture(int (*fn)(void))
{
    g_capture = fn;
}

int og_pointer_region(void)
{
    int bar;
    bar = !(og_options_flags() & 1);
    if (og_paused() || og_options_mouse() || !g_on_picture || (bar && g_pointer_y >= 154)) { return -1; }
    return og_mouse_region(g_pointer_x, g_pointer_y, 320, bar ? 154 : 200, 0, 0);
}

void og_set_sound(void (*fn)(int id))
{
    g_sound = fn;
}

void og_set_words(int (*fn)(int id, char *dst, int cap))
{
    g_words = fn;
    g_words_id = 0;
    g_words_buf[0] = 0;
}

void og_set_word_variants(int (*fn)(int id, int variant, char *dst, int cap))
{
    g_word_variants = fn; g_words_id = 0;
}

const char *og_words_variant(int id, int variant)
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

const char *og_words(int id)
{
    return og_words_variant(id,0);
}

void og_pic_set(int slot, int id)
{
    if (slot >= 0 && slot < OG_PIC_COUNT) {
        g_pic[slot] = id;
    }
}

int og_pic(int slot)
{
    if (slot < 0 || slot >= OG_PIC_COUNT) {
        return 0;
    }
    return g_pic[slot];
}

void og_preview_pic_set(int slot, int pic)
{
    if (slot >= 0 && slot < 6) { g_preview_pic[slot] = pic; }
}

static void og_files_draw_previews(void)
{
    int i;
    if (og_options_panel() != OG_PANEL_FILES || og_files_editing() || og_files_error()[0]) { return; }
    for (i = 0; i < 6; i++) {
        if (g_preview_pic[i]) { og_blit(g_preview_pic[i], i < 3 ? 40 : 200, (i % 3) * 65 + 4, 80, 50); }
    }
}

void og_compass_set(int frame, int pic, int w, int h)
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

int og_paused(void)
{
    return !g_world || og_maps_up() || og_options_up() || og_possessions()->page || og_rest()->open ||
        (g_world && og_character()->health <= 0) || og_chargen_mode() != OG_UI_WORLD;
}

int og_cursor_free(void) { return g_cursor_active; }

int og_wants_pointer(void)
{
    if (og_paused() && og_chargen_mode() != OG_UI_WORLD) {
        return 1;
    }
    if (og_maps_up() || og_options_up() || og_possessions()->page || og_rest()->open || og_dead()) {
        return 1;
    }
    if (og_options_mouse() == 0) {
        return 1;
    }
    return g_cursor_active;
}

void og_point_at(int x, int y)
{
    g_px = x;
    g_py = y;
}

static int (*g_status_intro)(char *dst, int cap);
void og_set_status_intro(int (*fn)(char *dst, int cap)) { g_status_intro = fn; }
int og_status_intro(char *dst, int cap) { return g_status_intro ? g_status_intro(dst,cap) : 0; }

const char *og_status_line(void)
{
    if (og_chargen_mode() != OG_UI_WORLD) {
        return og_chargen_caption();
    }
    return og_possessions()->page ? "" : og_msg();
}

static void mode_line(int step)
{
    char line[OG_MSG];

    og_hud_cycle(step, line, OG_MSG);
    og_msg_set(line);
}

static void ready_weapon(void)
{
    const OgObject *item;
    og_weapon_cancel();
    if (og_hud_weapon_ready()) {
        item = og_object(og_state()->equipped[og_weapon_hand() ? 21 : 19]);
        if (item && item->group == 3) { og_sound(og_weapon_sound(item->index)); }
    }
}

/* The parchment's line count, so a click hits the buttons the picture drew. */
static void sync_lines(void)
{
    int mode;
    int id;
    const char *text;

    mode = og_chargen_mode();
    id = 0;
    if (mode == OG_UI_RACE || mode == OG_UI_CLASS_YES) {
        id = og_chargen_text_id();
    } else if (mode == OG_UI_GENDER) {
        id = 2200;
    }
    if (id <= 0) {
        return;
    }
    text = og_words(id);
    if (text && text[0]) {
        og_chargen_set_lines(og_chargen_count_lines(text));
    }
}

static void screen_keys(const OgIn *in)
{
    int was;
    int race;

    sync_lines();
    was = og_chargen_mode();
    if (in->ch) {
        og_chargen_key(in->ch);
    }
    if ((in->mouse_pressed & 1) && in->on_picture) {
        og_chargen_click(in->cx, in->cy, in->pixel);
    }
    og_chargen_scroll_frame(in->cx, in->cy, in->mouse_held, in->mouse_pressed, in->dt);
    sync_lines();
    /* title_menu plays pick + 208 as the confirm opens. pick is race + 1. */
    if (was == OG_UI_MAP && og_chargen_mode() == OG_UI_RACE) {
        race = og_character()->race;
        if (race >= 0 && race < 8) {
            og_sound(race + 209);
        }
    }
    if (was == OG_UI_CLASS_LIST && og_chargen_mode() == OG_UI_CLASS_YES) { og_sound(217); }
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

static void cursor_weights(OgMoveIn *move, int x, int y, int bar)
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

static int modal_frame(const OgIn *input)
{
    int result, sound, quit;

    if (og_options_panel() == OG_PANEL_CONTROLS) {
        result = og_controls_editing();
        if (input->key && og_controls_key(input->key)) { og_options_back(); return OG_FRAME_MENU; }
        if (result < 0 && (input->mouse_pressed & 1) && input->on_picture && og_controls_editing() < 0) {
            og_sound(203);
            if (og_controls_click(input->cx, input->cy)) { og_options_back(); }
        }
        og_controls_drag(input->cx, input->cy, input->mouse_held);
        return OG_FRAME_MENU;
    }
    if (og_options_panel() == OG_PANEL_FILES) {
        result = og_files_frame(input);
        if (result == OG_FILES_CANCEL) {
            if (og_dead()) { og_options_close(); }
            else if (g_world) { og_options_back(); }
            else { og_options_close(); og_chargen_key(27); }
        } else if (result == OG_FILES_SAVE) {
            if (g_capture && !g_capture()) {
                og_files_failed("Cannot capture the current place.");
                return OG_FRAME_MENU;
            }
            og_state_capture_motion();
            og_state_ui(og_hud_mode(), og_hud_weapon_ready(), g_cursor_active);
            og_state_save_label(og_files_name());
            if (!og_save_write(og_files_selected() + 1)) {
                og_files_failed(og_save_error());
            } else {
                og_options_close();
                og_msg_set("Game saved.");
            }
        } else if (result == OG_FILES_LOAD) {
            if (!og_save_read(og_files_selected() + 1)) {
                og_files_failed(og_save_error());
                return OG_FRAME_MENU;
            }
            /* Preferences are global, not replaced by an older character's. */
            og_config_apply();
            og_options_close();
            og_chargen_resume();
            g_world = 1;
            g_cursor_active = og_state()->cursor_active;
            og_hud_set_mode(og_state()->hud_mode);
            if (og_hud_weapon_ready() != og_state()->weapon_ready) { og_hud_toggle_ready(); }
            og_msg_clear();
            return OG_FRAME_LOAD;
        }
        return OG_FRAME_MENU;
    }
    if (input->ch == 27) { og_options_commit(); return OG_FRAME_MENU; }
    if ((input->mouse_pressed & 1) && input->on_picture) {
        if (og_options_click(input->cx, input->cy, &sound, &quit)) {
            og_sound(sound);
            if (quit) { return OG_FRAME_QUIT; }
        }
    }
    og_options_drag(input->cx, input->mouse_held);
    return OG_FRAME_MENU;
}

int og_frame(const OgIn *in, Camera *cam, const Scene *scene)
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
    OgMoveIn mv;

    if (!in) {
        return OG_FRAME_MENU;
    }
    og_god_protect();
    g_pointer_x = in->cx;
    g_pointer_y = in->cy;
    g_on_picture = in->on_picture;
    if (in->reset) {
        g_attack_click = 0;
        g_mouse_prev = in->mouse_held;
        og_chargen_scroll_frame(0, 0, 0, 0, 0);
        og_options_cancel_drag();
        return og_paused() ? OG_FRAME_MENU : OG_FRAME_WORLD;
    }
    if (og_chargen_quit()) {
        return OG_FRAME_QUIT;
    }
    if (og_maps_up()) {
        og_maps_frame(in); g_mouse_prev = in->mouse_held;
        if (!og_maps_up()) { g_wait_buttons = (in->mouse_held & 3) != 0; }
        return OG_FRAME_MENU;
    }
    if (og_travel_asked()) { return OG_FRAME_TRAVEL; }
    if (og_possessions()->page) {
        og_possessions_frame(in);
        g_mouse_prev = in->mouse_held;
        if (!og_possessions()->page) { g_wait_buttons = (in->mouse_held & 3) != 0; }
        return OG_FRAME_MENU;
    }
    if (og_options_up()) {
        g_mouse_prev = in->mouse_held;
        result = modal_frame(in);
        if (!og_options_up()) { g_wait_buttons = (in->mouse_held & 3) != 0; }
        return result;
    }
    if (og_rest()->open) {
        og_rest_frame(in,cam);
        g_mouse_prev = in->mouse_held;
        if (!og_rest()->open) { g_wait_buttons = (in->mouse_held & 3) != 0; }
        return OG_FRAME_MENU;
    }
    if (og_dead()) {
        og_weapon_cancel();
        if (in->ch == 'l' || in->ch == 'L' || in->ch == 13 ||
            ((in->mouse_pressed & 1) && in->on_picture && in->cx >= 75 && in->cx <= 245 && in->cy >= 86 && in->cy < 106)) {
            og_options_open_files(0);
        } else if (in->ch == 27 || in->ch == 'n' || in->ch == 'N' ||
            ((in->mouse_pressed & 1) && in->on_picture && in->cx >= 75 && in->cx <= 245 && in->cy >= 110 && in->cy < 130)) {
            og_options_close(); og_possessions_reset();
            og_chargen_begin(); g_world = 0; g_wait_buttons = 0;
            return OG_FRAME_TITLE;
        } else if (in->ch == 'q' || in->ch == 'Q' ||
            ((in->mouse_pressed & 1) && in->on_picture && in->cx >= 75 && in->cx <= 245 && in->cy >= 134 && in->cy < 154)) {
            return OG_FRAME_QUIT;
        }
        return OG_FRAME_MENU;
    }
    /* A modal's closing click must be released before it can steer or use. */
    if (g_wait_buttons) {
        if (!(in->mouse_held & 3)) { g_wait_buttons = 0; g_mouse_prev = 0; }
        return OG_FRAME_WORLD;
    }
    if (og_chargen_mode() != OG_UI_WORLD) {
        screen_keys(in);
        if (og_chargen_mode() == OG_UI_LOAD) { og_options_open_files(0); }
        if (og_chargen_quit()) {
            return OG_FRAME_QUIT;
        }
        if (og_chargen_mode() == OG_UI_WORLD) {
            g_world = 1;
            return OG_FRAME_ENTER;
        }
        g_mouse_prev = in->mouse_held;
        return OG_FRAME_MENU;
    }
    /* The renderer steps triggers after our frame. Apply its pending arrival
     * before this frame's movement, combat, or opening a save/options screen. */
    if (og_move_take_teleport()) {
        og_weapon_cancel();
        og_move_camera(cam);
    }
    og_actions_step(in->dt);
    if (og_possessions()->page || og_dead()) { return OG_FRAME_MENU; }
    if (og_progress()->active && !og_dungeon_nearby(cam,1024)) {
        og_possessions_open(OG_VIEW_SHEET,0);
        return OG_FRAME_MENU;
    }
    if (in->toggle_fullscreen) {
        og_options_set_flags(og_options_flags() ^ 1);
        og_config_write();
    }
    if (in->pressed[OG_CURSOR_KEY] && og_options_mouse() == 1) {
        g_cursor_active = g_cursor_active ? 0 : 1;
    }
    if (in->pressed[OG_KEY_STEAL]) {
        og_hud_set_mode(OG_MODE_STEAL);
        mode_line(0);
    } else if (in->pressed[OG_KEY_GRAB]) {
        og_hud_set_mode(OG_MODE_GRAB);
        mode_line(0);
    } else if (in->pressed[OG_KEY_INFO]) {
        og_hud_set_mode(OG_MODE_INFO);
        mode_line(0);
    } else if (in->pressed[OG_KEY_TALK]) {
        og_hud_set_mode(OG_MODE_TALK);
        mode_line(0);
    }
    if (in->pressed[OG_READY]) {
        og_hud_toggle_ready();
        ready_weapon();
    }
    if (in->pressed[OG_SWITCH]) { og_weapon_switch(); }
    bar = (og_options_flags() & 1) == 0;
    blocks = 0;
    if (bar && in->on_picture && in->cy >= OG_BAR_Y) {
        og_hud_buttons(in->cx, in->cy, in->mouse_held, g_mouse_prev,
                       og_options_mouse(), g_cursor_active, &sound, &which, &step);
        if (sound) {
            og_sound(sound);
        }
        if (which == OG_HUD_MODE) {
            mode_line(0);
        } else if (which == OG_HUD_WEAPON) {
            ready_weapon();
        } else if (which == OG_HUD_OPTIONS) {
            og_options_open();
        } else if (which == OG_HUD_SHEET) { og_possessions_open(OG_VIEW_SHEET,0); }
        else if (which == OG_HUD_PACK) { og_possessions_open(OG_VIEW_INVENTORY,0); }
        else if (which == OG_HUD_SPELL) { og_possessions_cast(); }
        else if (which == OG_HUD_MAP) { og_maps_open(0); }
        else if (which == OG_HUD_TRAVEL) { og_maps_open(1); }
        else if (which == OG_HUD_REST) { og_rest_open(scene,cam); }
        else if (which == OG_HUD_STATUS) { og_magic_status(); }
        blocks = og_hud_blocks_mouse(in->cy, 1, og_key_region(in->held));
    }
    if (in->ch == 27) {
        og_options_open();
    }
    if (in->pressed[OG_REST]) { og_rest_open(scene,cam); }
    if (in->pressed[OG_AUTOMAP]) { og_maps_open(0); }
    if (in->pressed[OG_TRAVEL]) { og_maps_open(1); }
    if (in->pressed[OG_TRANSPORT] || in->pressed[OG_MAGIC_ITEM]) {
        og_sound(203);
    }
    if (in->pressed[OG_SHEET]) { og_possessions_open(OG_VIEW_SHEET,0); }
    else if (in->pressed[OG_INVENTORY]) { og_possessions_open(OG_VIEW_INVENTORY,0); }
    else if (in->pressed[OG_CAST]) { og_possessions_cast(); }
    if (in->pressed[OG_RECAST]) { og_magic_recast(); }
    if (in->pressed[OG_ABORT]) { og_magic_abort(); }
    if (in->pressed[OG_STATUS_KEY]) { og_magic_status(); }
    region = og_control_region(in->on_picture ? in->cx : 160, in->on_picture ? in->cy : 77,
                               bar, og_options_mouse(), g_cursor_active,
                               in->on_picture && !blocks ? in->mouse_held : 0, in->held);
    /* A newly opened modal consumes this frame too. */
    if (og_maps_up() || og_options_up() || og_possessions()->page || og_rest()->open) {
        og_weapon_cancel();
        g_mouse_prev = in->mouse_held;
        return OG_FRAME_MENU;
    }
    mv.held = in->held;
    mv.pressed = in->pressed;
    mv.region = region;
    mv.view_mode = og_options_mouse() == 1 && !g_cursor_active;
    mv.mouse_dx = in->mouse_dx;
    mv.mouse_dy = in->mouse_dy;
    /* A held attack gesture owns relative motion in either control scheme. */
    if (og_hud_weapon_ready() && in->held[OG_SWING_BUTTON]) {
        mv.mouse_dx = 0; mv.mouse_dy = 0;
    }
    mv.dt = in->dt;
    mv.bar_blocks = blocks;
    mv.mouse_steer = og_key_region(in->held) < 0 && region != 4;
    cursor_weights(&mv, in->cx, in->cy, bar);
    og_move_set_weight(og_inventory_weight(og_state()->inventory_id));
    og_move_frame(scene, &mv);
    minute = og_state()->minutes;
    og_state_frame(in->dt);
    og_survival_minutes((int)(og_state()->minutes-minute),in->held[OG_RUN],og_move_climbing());
    og_survival_step(scene,in->dt);
    if (og_move_splashed()) {
        og_sound(86);
    }
    if (cam) {
        og_move_camera(cam);
        casting = og_magic_visible();
        {
            OgStrike strike;
            if (!og_magic_visible() && !(og_character()->conditions & 1)) {
                og_weapon_frame(in,blocks,&strike);
                og_dungeon_strike(scene,cam,&strike);
            }
            og_magic_frame(scene,cam,in,blocks);
            og_dungeon_step(scene,cam,in->dt);
            og_guards_tick(in->dt);
            og_law_calm();
        }
        if (casting || og_magic_visible()) {
            g_mouse_prev = in->mouse_held;
            return OG_FRAME_WORLD;
        }
    }
    {
        int centre;
        int grab_click;
        int view_click;

        centre = in->pressed[OG_ACTIVATE];
        if (in->mouse_pressed & 2) { g_attack_motion = 0; g_attack_click = 1; }
        if ((in->mouse_held | in->mouse_released) & 2) {
            g_attack_motion += (in->mouse_dx < 0 ? -in->mouse_dx : in->mouse_dx)
                             + (in->mouse_dy < 0 ? -in->mouse_dy : in->mouse_dy);
        }
        grab_click = ((in->mouse_pressed & 2) && !og_hud_weapon_ready())
            && og_options_mouse() == 0 && !blocks;
        /* intrface.c steer_center: left-clicking the X uses the pointed
         * object, even with a weapon drawn. The arrow regions still steer. */
        if ((in->mouse_pressed & 1) && in->on_picture && !blocks && og_options_mouse() == 0 &&
            og_mouse_region(in->cx,in->cy,320,bar ? 154 : 200,0,0) == 4) { grab_click = 1; }
        /* A stationary click still uses things with a melee weapon drawn.
         * A drag belongs to combat, and a bow's click always releases it. */
        if ((in->mouse_released & 2) && g_attack_click) {
            const OgObject *weapon;
            weapon = og_object(og_state()->equipped[og_weapon_hand() ? 21 : 19]);
            if (og_hud_weapon_ready() && og_options_mouse() == 0 && !blocks &&
                g_attack_motion <= 4 && (!weapon || weapon->group != 3 || weapon->index < 16)) {
                grab_click = 1;
            }
            g_attack_click = 0;
        }
        if (in->pressed[OG_CURSOR_KEY] && og_options_mouse() == 0 && !blocks) { grab_click = 1; }
        view_click = og_options_mouse() == 1 && g_cursor_active && (in->mouse_pressed & 1) && !blocks;
        if ((centre || grab_click || view_click) && cam) {
            og_activate(scene, cam, og_hud_mode(), og_options_mouse() == 0, g_cursor_active,
                        centre, in->cx, in->cy);
        }
    }
    g_mouse_prev = in->mouse_held;
    if (og_options_up() || og_possessions()->page) {
        return OG_FRAME_MENU;
    }
    return OG_FRAME_WORLD;
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
    og_hud_blit_part(pic, 0, 32 - height, 4, height, full ? x - 40 : x, y, 4, height);
}

static void draw_pointer(void)
{
    int region, id, w, h;
    if (!og_wants_pointer()) { return; }
    region = og_pointer_region();
    id = region >= 0 ? g_pic[OG_PIC_CURSOR0 + region] : g_pic[OG_PIC_ARROW];
    if (!id) { id = g_pic[OG_PIC_ARROW]; }
    if (pic_size(id, &w, &h)) {
        pic_draw(id, g_px, g_py, w * df_view_w() / 320, h * df_view_w() / 320);
    }
}

void og_draw(void)
{
    int mode;
    int bar;
    int overlay;
    int height;
    int row;
    OgCharacter *who;
    /* MAIN01I0's rows are steal, talk, grab, info. */
    static const int mode_row[4] = { 2, 3, 0, 1 };

    mode = og_chargen_mode();
    if (og_maps_up()) {
        og_blit(g_pic[OG_PIC_SCREEN],0,0,320,200); og_maps_overlay(); draw_pointer(); return;
    }
    if (og_dead() && !og_options_up()) {
        og_blit(g_pic[OG_PIC_SCREEN],0,0,320,200);
        draw_pointer();
        return;
    }
    if (og_possessions()->page && og_possessions()->page != OG_VIEW_MESSAGE) {
        og_blit(g_pic[OG_PIC_SCREEN],0,0,320,200);
        draw_pointer();
        return;
    }
    if (mode == OG_UI_TITLE && g_pic[OG_PIC_TITLE]) {
        og_blit(g_pic[OG_PIC_TITLE], 0, 0, 320, 200);
    } else if (mode == OG_UI_MAP) {
        /* The composed MAP100I0, or the plain one if the sentence did not fit. */
        og_blit(g_pic[OG_PIC_MAP] ? g_pic[OG_PIC_MAP] : g_pic[OG_PIC_BACK], 0, 0, 320, 200);
    } else if (mode == OG_UI_CLASS_HOW) {
        /* BUTN01I0.IMG at (68, 28), the two choices title_menu blits. */
        og_blit(g_pic[OG_PIC_BACK], 0, 0, 320, 200);
        og_blit(g_pic[OG_PIC_BUTTONS], 68, 28, 184, 144);
    } else if (mode != OG_UI_WORLD) {
        if (g_pic[OG_PIC_SCREEN]) {
            og_blit(g_pic[OG_PIC_SCREEN], 0, 0, 320, 200);
        } else {
            og_blit(g_pic[OG_PIC_BACK], 0, 0, 320, 200);
        }
    }
    if (mode != OG_UI_WORLD || og_options_up()) {
        if (og_options_up()) {
            if (og_options_panel() == OG_PANEL_OPTIONS) {
                og_blit(g_pic[OG_PIC_OPTIONS], 85, 40, 150, 84);
            } else if (og_options_panel() == OG_PANEL_CONTROLS && og_controls_mouse()) {
                og_blit(g_pic[OG_PIC_MOUSE], 66, 22, 188, 146);
            } else {
                og_blit(g_pic[OG_PIC_SCREEN], 0, 0, 320, 200);
                og_files_draw_previews();
            }
        }
        draw_pointer();
        return;
    }
    who = og_character();
    bar = (og_options_flags() & 1) == 0;
    if (!og_magic_visible()) { og_weapon_draw(); }
    og_magic_draw();
    if (bar) {
        og_hud_blit(g_pic[OG_PIC_BAR], 0, 154, 320, 46);
        row = mode_row[og_hud_mode() & 3];
        og_hud_blit_part(g_pic[OG_PIC_MODES], 0, row * 23, 47, 23, 131, 154, 47, 23);
        overlay = og_magic_indicators();
        overlay = og_hud_overlay(who->health,who->max_health,overlay & 1,overlay & 2,overlay & 4);
        if (overlay >= 0) {
            og_hud_blit(g_pic[OG_PIC_OVERLAY+overlay], 5, 159, 36, 35);
        }
        og_hud_blit(g_pic[OG_PIC_FACE], 7, 160, 32, 32);
        if (g_comp_n > 0) {
            int frame;

            frame = og_hud_compass_frame(og_move_yaw());
            if (frame < 0 || frame >= g_comp_n) {
                frame = 0;
            }
            og_hud_blit(g_comp_pic[frame], 272, 157, g_comp_w, g_comp_h);
        }
    }
    height = og_hud_bar(0, who->health, who->max_health, who->fatigue, who->attribute[OG_STR],
                        who->attribute[OG_END], who->magicka, who->spell_points_bonus, who->max_magicka);
    bar_at(g_pic[OG_PIC_HEALTH], !bar, 49, height);
    height = og_hud_bar(1, who->health, who->max_health, who->fatigue, who->attribute[OG_STR],
                        who->attribute[OG_END], who->magicka, who->spell_points_bonus, who->max_magicka);
    bar_at(g_pic[OG_PIC_FATIGUE], !bar, 57, height);
    height = og_hud_bar(2, who->health, who->max_health, who->fatigue, who->attribute[OG_STR],
                        who->attribute[OG_END], who->magicka, who->spell_points_bonus, who->max_magicka);
    bar_at(g_pic[OG_PIC_MAGICKA], !bar, 65, height);
    if (!bar) {
        og_hud_blit(g_pic[OG_PIC_COMPBOX], 251, 183, 69, 17);
        og_hud_blit_part(g_pic[OG_PIC_COMPASS_WIDE], (og_move_yaw() & 2047) / 8, 0, 65, 13,
                         253, 185, 65, 13);
    }
    if (og_rest()->open || og_possessions()->page == OG_VIEW_MESSAGE) { og_blit(g_pic[OG_PIC_SCREEN],0,0,320,200); }
    if (og_survival()->breath > 0) {
        int remaining, id;
        remaining = og_survival()->breath;
        id = remaining < og_character()->attribute[OG_END]/8+4 ? OG_PIC_BREATH_LOW : OG_PIC_BREATH;
        og_blit(g_pic[id],310,120-remaining*2,6,remaining*2);
    }
    draw_pointer();
}
