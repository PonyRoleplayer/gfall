#include "check_world.h"
#include "game/dagger/play/state.h"
#include "../apps/gfall/seam.h"
/*
 * GFALL slice 1. The sums and the click script do not need the hold.
 * The last part boots the seam and walks one body in Privateer's Hold
 * when ARENA2 is mounted.
 */

#include "check_util.h"
#include "check_state.h"
#include "check_standing.h"
#include "check_trade.h"
#include "check_ui.h"

#include "engine/assets/level.h"
#include <stdio.h>
#include <string.h>
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "engine/platform/input.h"
#include "engine/scene/body.h"
#include "engine/scene/walker.h"
#include "engine/scene/interact.h"
#include "engine/scene/stream.h"
#include "game/dagger/play/magic.h"
#include "engine/ui/pic.h"
#include "engine/sound/sound.h"

#include "game/dagger/places/place.h"
#include "game/dagger/play/chargen.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/hud.h"
#include "game/dagger/play/move.h"
#include "game/dagger/play/options.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/steer.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/vitals.h"
#include "check_combat.h"
#include "game/dagger/tables/tables.h"

static int g_card_quads;
static float g_card_bottom, g_card_top, g_card_left, g_card_right;
static df_u32 g_card_handle;
void check_game_draw_reset(void) { g_card_quads = 0; }
int check_game_draw_count(void) { return g_card_quads; }
static df_u32 card_bytes(int side, int wide) { return side * side * (wide ? 2u : 1u); }
static int card_upload(const df_u8 *p, int side, int fmt, df_u32 addr, df_u32 *handle)
{
    (void)p; (void)side; (void)fmt; (void)addr;
    *handle = ++g_card_handle;
    return 1;
}
static void card_quad(df_u32 handle, float x0, float y0, float x1, float y1,
                      float s0, float t0, float s1, float t1)
{
    (void)handle; (void)x0; (void)y0; (void)x1; (void)y1;
    (void)s0; (void)t0; (void)s1; (void)t1;
    if (!g_card_quads || y1 < g_card_bottom) { g_card_bottom = y1; }
    if (!g_card_quads || y0 > g_card_top) { g_card_top = y0; }
    if (!g_card_quads || x0 < g_card_left) { g_card_left = x0; }
    if (!g_card_quads || x1 > g_card_right) { g_card_right = x1; }
    g_card_quads++;
}
static void card_release(df_u32 handle) { (void)handle; }
static void card_begin(int how, int alpha) { (void)how; (void)alpha; }
static void card_palette(int id, const df_u32 *table) { (void)id; (void)table; }
static void card_end(void) { }
static PicOps g_card = { card_bytes, card_upload, card_release, card_begin, card_palette, card_quad, card_end, 1 };

static int same(const char *a, const char *b)
{
    int i;

    if (!a || !b) {
        return 0;
    }
    for (i = 0; a[i] || b[i]; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static void hold_down(unsigned char *held, int action)
{
    if (action >= 0 && action < ACTS) {
        held[action] = 1;
    }
}

static void check_steer(void)
{
    unsigned char held[ACTS];
    int i;
    int x;
    int y;

    for (i = 0; i < ACTS; i++) {
        held[i] = 0;
    }
    check_expect(key_region(held) == -1, "no steer key");
    hold_down(held, FORWARD);
    check_expect(key_region(held) == 1, "forward");
    hold_down(held, TURN_LEFT);
    check_expect(key_region(held) == 0, "forward left");
    held[FORWARD] = 0;
    check_expect(key_region(held) == 3, "turn left");
    hold_down(held, STRAFE);
    check_expect(key_region(held) == 6, "turn left plus strafe");
    for (i = 0; i < ACTS; i++) {
        held[i] = 0;
    }
    hold_down(held, FORWARD);
    hold_down(held, TURN_RIGHT);
    check_expect(key_region(held) == 2, "forward right");
    held[FORWARD] = 0;
    check_expect(key_region(held) == 5, "turn right");
    hold_down(held, SLIDE_RIGHT);
    check_expect(key_region(held) == 8, "slide right");
    for (i = 0; i < ACTS; i++) {
        held[i] = 0;
    }
    hold_down(held, BACK);
    check_expect(key_region(held) == 7, "back");
    held[BACK] = 0;
    hold_down(held, SLIDE_LEFT);
    check_expect(key_region(held) == 6, "slide left");

    /* Each ninth of a 320 x 200 view. Thirds are 106 and 66. */
    x = 50;
    y = 20;
    check_expect(mouse_region(x, y, 320, 200, 0, 0) == 0, "cell 0");
    check_expect(mouse_region(160, 20, 320, 200, 0, 0) == 1, "cell 1");
    check_expect(mouse_region(250, 20, 320, 200, 0, 0) == 2, "cell 2");
    check_expect(mouse_region(50, 90, 320, 200, 0, 0) == 3, "cell 3");
    check_expect(mouse_region(160, 90, 320, 200, 0, 0) == 4, "cell 4");
    check_expect(mouse_region(250, 90, 320, 200, 0, 0) == 5, "cell 5");
    check_expect(mouse_region(50, 170, 320, 200, 0, 0) == 6, "cell 6");
    check_expect(mouse_region(160, 170, 320, 200, 0, 0) == 7, "cell 7");
    check_expect(mouse_region(250, 170, 320, 200, 0, 0) == 8, "cell 8");
    for (i = 0; i < ACTS; i++) {
        held[i] = 0;
    }
    check_expect(steer_region(160, 160, 320, 200, 0, 0, held, 1) == -2, "bar suppresses the mouse");
    hold_down(held, FORWARD);
    check_expect(steer_region(160, 160, 320, 200, 0, 0, held, 1) == 1, "a steer key still wins on the bar");
    check_expect(bind_vk(INPUT_FORWARD) == KEY_UP, "forward is Up");
    check_expect(bind_vk(INPUT_FORWARD) != 'W', "forward is not W");
    check_expect(bind_vk(INPUT_TRAVEL_MAP) == 'W', "W is the travel map");
    check_expect(view_slow(11) == 4, "view key 23 halves then subtracts one");
}

static void check_hud(void)
{
    int sound;
    int which;
    int step;

    hud_reset();
    check_expect(hud_hit(5, 170) < 0, "the portrait's left edge misses");
    check_expect(hud_hit(6, 170) == 0, "just inside the portrait hits");
    check_expect(hud_hit(150, 160) == 2, "the mode button");
    hud_buttons(150, 160, 1, 0, 0, 0, &sound, &which, &step);
    check_expect(sound == 0, "a press does not fire");
    hud_buttons(150, 160, 0, 1, 0, 0, &sound, &which, &step);
    check_expect(sound == 203 && which == 2 && step == 1, "release on the same button");
    check_expect(hud_mode() == 1, "mode 0 to 1");
    hud_buttons(200, 160, 1, 0, 0, 0, &sound, &which, &step);
    hud_buttons(150, 160, 0, 1, 0, 0, &sound, &which, &step);
    check_expect(sound == 0 && hud_mode() == 1, "a release on another button does not fire");
    hud_buttons(150, 160, 2, 0, 0, 0, &sound, &which, &step);
    hud_buttons(150, 160, 0, 2, 0, 0, &sound, &which, &step);
    check_expect(step == -1 && hud_mode() == 0, "the right button steps back");
    hud_buttons(150, 160, 1, 0, 1, 0, &sound, &which, &step);
    check_expect(hud_pressed() == 255, "view mode without a cursor ignores the bar");
    check_expect(hud_overlay(5, 100, 1, 1, 1) == 2, "low health wins the overlay order");
    check_expect(hud_overlay(50, 100, 1, 1, 1) == 0, "under a spell is next");
    check_expect(hud_overlay(50, 100, 0, 1, 1) == 3, "poison before disease");
    check_expect(hud_overlay(50, 100, 0, 0, 0) == -1, "no overlay");
    check_expect(hud_compass_frame(0) == 0, "compass north");
    check_expect(hud_compass_frame(64) == 1, "compass one frame");
    check_expect(hud_blocks_mouse(154, 1, -1) == 1, "the bar blocks the mouse");
    check_expect(hud_blocks_mouse(154, 1, 1) == 0, "a steer key is not blocked");
}

static void check_vitals(void)
{
    check_expect(bar_ratio(50, 100) == 16, "half health is 16");
    check_expect(bar_ratio(0, 100) == 0, "no health draws nothing");
    check_expect(bar_ratio(10, 0) == 0, "a zero max does not divide");
    check_expect(bar_fatigue(3200, 50, 50) == 16, "half fatigue");
    check_expect(bar_fatigue(10, 0, 0) == 0, "fatigue with no attributes");
    check_expect(jump_magnitude(50, 50, 0) == 100, "jump magnitude");
    check_expect(jump_impulse(100, 1, 0) == 200, "acrobat doubles");
    check_expect(jump_impulse(100, 0, 1) == 150, "class flag adds half");
    check_expect(jump_impulse(100, 1, 1) == 150, "class flag replaces the double");
    check_expect(jump_ups(100) == 100.0f, "magnitude is world units a second");
    check_expect(fall_damage(0) == 0, "no fall");
    check_expect(fall_damage(256 * 40 * 7) == 0, "seven is the last safe step");
    check_expect(fall_damage(256 * 40 * 15) == 32, "fall damage 8 squared over 2");
    check_expect(fall_v(15.0f * 40.0f) == 256 * 40 * 15, "landing speed back into v");
    check_expect(fatigue_add(100, -11, 50, 50) == 89, "a jump spends 11");
    check_expect(fatigue_add(5, -11, 50, 50) == 0, "fatigue does not go negative");
    check_expect(ground_speed(50, 0, 0, 0, 0, 0) == 200, "standing speed");
    check_expect(ground_speed(50, 1, 0, 0, 0, 0) == 100, "crouch speed");
    check_expect(ground_speed(50, 0, 1, 0, 0, 0) == 250, "run with no skill");
    check_expect(options_mouse() == 0, "cursor mode at the start");
}

static void check_title_and_chargen(void)
{
    int i;
    CharacterState *who;

    chargen_begin();
    chargen_key('L');
    check_expect(chargen_mode() == UI_LOAD, "L opens load");
    chargen_key(27);
    check_expect(chargen_mode() == UI_TITLE, "load cancellation returns to the title");
    chargen_key('E');
    check_expect(chargen_quit(), "E quits");
    chargen_begin();
    chargen_click(90, 105, 0);
    check_expect(chargen_mode() == UI_MAP, "the start rectangle");
    chargen_key(27);
    check_expect(chargen_mode() == UI_TITLE, "escape on the map returns");
    chargen_key('S');
    chargen_click(90, 52, 1);
    who = character_state();
    check_expect(chargen_mode() == UI_RACE, "a province pixel");
    check_expect(who->race == 0 && same(race_name(0), "Breton"), "High Rock is a Breton");
    check_expect(chargen_text_id() == 2003, "the race text is 2000 plus the table byte");
    chargen_key('Y');
    chargen_key('M');
    check_expect(who->female == 0 && chargen_mode() == UI_CLASS_HOW, "male");
    chargen_click(160, 140, 0);
    check_expect(chargen_mode() == UI_QUESTIONS, "the questions button answers");
    chargen_key(27);
    chargen_click(160, 85, 0);
    chargen_click(102, 80, 0);
    chargen_click(102, 80, 0);
    check_expect(chargen_mode() == UI_CLASS_YES, "Battlemage chosen");
    chargen_key('Y');
    check_expect(who->klass == 2, "the class is Battlemage");
    chargen_click(160, 150, 0);
    check_expect(chargen_mode() == UI_BIOG, "biography is reachable");
    chargen_click(80, 80, 0);
    check_expect(chargen_question() == 1, "a biography answer advances");
    chargen_key(27);
    chargen_click(160, 85, 0);
    check_expect(who->gold >= 100 && chargen_mode() == UI_REPUTE, "background before the summary");
    chargen_key(13);
    chargen_key('a');
    chargen_key('l');
    chargen_key('d');
    chargen_key('o');
    chargen_key(13);
    check_expect(same(who->name, "aldo"), "the typed name");
    chargen_click(283, 183, 0);
    chargen_click(18, 32, 0);
    i = chargen_attr_pool();
    while (chargen_attr_pool()) { chargen_click(51, 24, 0); }
    check_expect(chargen_attr_bonus(ATTR_STR) == i, "rolled points in strength");
    chargen_click(283, 183, 0);
    for (i = 0; i < 6; i++) {
        chargen_click(236, 35, 0);
    }
    for (i = 0; i < 6; i++) {
        chargen_click(236, 84, 0);
    }
    for (i = 0; i < 6; i++) {
        chargen_click(236, 133, 0);
    }
    check_expect(chargen_skill_bonus(0) == 6 && chargen_skill_bonus(1) == 6
                 && chargen_skill_bonus(2) == 6, "six points in each skill group");
    chargen_click(283, 183, 0);
    chargen_click(283, 183, 0);
    check_expect(who->reflexes == 2, "reflexes stay average");
    chargen_click(283, 183, 0);
    check_expect(chargen_mode() == UI_WORLD, "the summary enters the hold");
    {
        df_u32 items[OBJECTS];
        int count, found;
        count = inventory_rows(game_state()->inventory_id,-1,items,OBJECTS);
        found = 0;
        for (i = 0; i < count; i++) {
            if (object_at(items[i])->group == 3 && object_at(items[i])->index == 6) { found = 1; }
        }
        check_expect(found && !game_state()->equipped[19], "a battlemage's starting weapon is in the pack, as equip=0 requests");
    }
}

/* Mask bytes 1..8, and a Black Marsh click that leaves the map. */
static void check_races(void)
{
    static const char *const names[] = {
        "Breton", "Redguard", "Nord", "Dark Elf",
        "High Elf", "Wood Elf", "Khajiit", "Argonian"
    };
    static const int texts[] = { 2003, 2002, 2000, 2007, 2006, 2005, 2001, 2004 };
    int i;
    CharacterState *who;

    chargen_begin();
    chargen_key('S');
    chargen_click(10, 10, 0);
    check_expect(chargen_mode() == UI_MAP, "the sea stays on the map");
    chargen_click(10, 10, 9);
    check_expect(chargen_mode() == UI_MAP, "a pixel past the races stays on the map");
    for (i = 0; i < 8; i++) {
        chargen_click(0, 0, i + 1);
        who = character_state();
        check_expect(chargen_mode() == UI_RACE && who->race == i, "each province opens its race");
        check_expect(same(race_name(i), names[i]), "the race name");
        check_expect(chargen_text_id() == texts[i], "the race text id");
        chargen_key('n');
        check_expect(chargen_mode() == UI_MAP, "No returns to the map");
    }
    /* Black Marsh is pixel 8. Yes is the left button, Male the same place. */
    chargen_click(251, 117, 8);
    who = character_state();
    check_expect(chargen_mode() == UI_RACE && who->race == 7, "Black Marsh is an Argonian");
    check_expect(same(race_name(who->race), "Argonian") && chargen_text_id() == 2004,
                 "the Argonian confirm");
    chargen_key(27);
    check_expect(chargen_mode() == UI_MAP, "Escape on the confirm returns to the map");
    chargen_click(251, 117, 8);
    chargen_click(120, chargen_choice_y() + 8, 0);
    check_expect(chargen_mode() == UI_GENDER, "Yes leaves the map");
    chargen_click(120, chargen_choice_y() + 8, 0);
    check_expect(who->female == 0 && chargen_mode() == UI_CLASS_HOW, "Male continues");
    chargen_begin();
    chargen_key('S');
    chargen_click(251, 117, 8);
    chargen_key('Y');
    chargen_click(184, chargen_choice_y() + 8, 0);
    check_expect(character_state()->female == 1 && chargen_mode() == UI_CLASS_HOW,
                 "Female is the right button");
}

/* Black Marsh, the gender parchment, the class list, a class, the background, the name and the face. */
static void check_opening(void)
{
    CharacterState *who;
    PlayIn input;
    int y, on;

    chargen_begin();
    chargen_key('S');
    chargen_click(251, 117, 8);
    who = character_state();
    check_expect(chargen_mode() == UI_RACE && who->race == 7, "Black Marsh opens the confirm");
    y = chargen_choice_y();
    chargen_click(120, y + 8, 0);
    check_expect(chargen_mode() == UI_GENDER, "Yes opens gender");
    y = chargen_choice_y();
    chargen_click(184, y + 8, 0);
    check_expect(who->female == 1 && chargen_mode() == UI_CLASS_HOW, "Female continues");
    chargen_click(160, 85, 0);
    check_expect(chargen_mode() == UI_CLASS_LIST, "the class list is entered");
    chargen_click(102, 80, 0);
    check_expect(chargen_mode() == UI_CLASS_LIST && chargen_list_sel() == 2,
                 "one click highlights Battlemage");
    on = sound_on(); sound_set_on(1); sound_stop_all();
    mem_zero(&input,sizeof(input));
    input.on_picture = 1; input.cx = 102; input.cy = 80; input.mouse_pressed = 1;
    play_frame(&input,0,0);
    check_expect(chargen_mode() == UI_CLASS_YES && chargen_text_id() == 2102,
                 "the class confirm appears");
    check_expect(sound_busy() > 0,"confirming a class plays title_menu's original sound 217");
    sound_stop_all(); sound_set_on(on);
    y = chargen_choice_y();
    chargen_click(120, y + 8, 0);
    check_expect(who->klass == 2 && chargen_mode() == UI_BG_HOW, "Yes accepts the class");
    chargen_click(160, 85, 0);
    check_expect(who->gold >= 100 && chargen_mode() == UI_REPUTE, "the background click");
    chargen_click(40, 40, 0);
    check_expect(chargen_mode() == UI_NAME, "the name screen");
    chargen_key('A');
    chargen_key(13);
    check_expect(chargen_mode() == UI_FACE, "the face screen");
    chargen_click(300, 73, 0);
    check_expect(who->face == 1, "the next face");
    chargen_click(260, 73, 0);
    check_expect(who->face == 0, "the previous face");
}

/* Save the indexed surface that the real app sends to Glide. This catches blank
 * artwork screens as well as producing reviewable images in build/. */
static void screen_shot(const char *name, const char *base, int x0, int y0, int w, int h)
{
    const df_u8 *pixels;
    const df_u8 *palette;
    const df_u8 *background;
    df_u32 size;
    int x, y, changed;
    FILE *file;
    char path[96];

    seam_present();
    g_card_quads = 0;
    pic_draw(play_pic(PIC_SCREEN), 0, 200, 320, 200);
    check_expect(g_card_quads > 0, "the composed screen fits the 384 KB Voodoo picture room");
    pixels = seam_screen(&palette);
    background = vfs_read(base, &size);
    check_expect(pixels && palette && play_pic(PIC_SCREEN), "composed creation screen is published");
    if (!pixels || !palette || !background || size < 64000) { return; }
    changed = 0;
    for (y = y0; y < y0 + h; y++) {
        for (x = x0; x < x0 + w; x++) {
            changed += pixels[y * 320 + x] != background[y * 320 + x];
        }
    }
    check_expect(changed > 8, "character data is drawn over the empty artwork");
    snprintf(path, sizeof(path), "build/gfall-%s.ppm", name);
    file = fopen(path, "wb");
    if (!file) { check_expect(0, "creation screenshot opens"); return; }
    fprintf(file, "P6\n320 200\n255\n");
    for (y = 0; y < 200; y++) {
        for (x = 0; x < 320; x++) {
            fwrite(palette + pixels[y * 320 + x] * 3, 1, 3, file);
        }
    }
    fclose(file);
}

static void standard_background(int manual)
{
    chargen_begin();
    chargen_key('S');
    chargen_click(90, 52, 1);
    chargen_key('Y');
    chargen_key('M');
    chargen_click(160, 85, 0);
    chargen_click(102, 80, 0);
    chargen_click(102, 80, 0);
    chargen_key('Y');
    chargen_click(160, manual ? 150 : 85, 0);
}

static void check_creation_controls(void)
{
    CharacterState *who;
    int attr[8], pool, i, j, y, before, skill;
    int seen[40];
    int counts[3];
    unsigned char old[64000];
    const df_u8 *pixels;

    standard_background(1);
    who = character_state();
    check_expect(who->gold == 100, "gold starts at 100 before biography effects");
    screen_shot("biography", "BIOG00I0.IMG", 30, 20, 260, 30);
    before = who->skill[22];
    chargen_click(80, 80, 0);
    check_expect(chargen_question() == 1, "a real biography answer advances");
    /* Going back and regenerating must not apply the first answer twice. */
    chargen_key(27);
    chargen_click(160, 150, 0);
    check_expect(who->skill[22] == before && who->gold == 100, "returning restores the pre-background character");
    for (i = 0; i < 12; i++) { chargen_key(i == 4 ? '2' : '1'); }
    check_expect(chargen_mode() == UI_REPUTE, "all twelve biography answers finish");
    check_expect(who->gifts > 0, "background gifts survive creation");
    chargen_key(13);
    chargen_key('T'); chargen_key('e'); chargen_key('s'); chargen_key('t');
    screen_shot("name", "CHAR00I0.IMG", 80, 2, 160, 12);
    chargen_click(283, 183, 0);
    check_expect(chargen_mode() == UI_FACE, "Name screen OK accepts the entered name");
    screen_shot("face", "CHAR01I0.IMG", 245, 14, 70, 50);
    chargen_click(283, 183, 0);
    pool = chargen_attr_pool();
    check_expect(pool >= 6 && pool <= 14, "classic rolls 6..14 attribute points");
    for (i = 0; i < 8; i++) {
        attr[i] = who->attribute[i];
        check_expect(attr[i] >= class_at(2)->attribute[i] && attr[i] <= class_at(2)->attribute[i] + 10,
                     "attributes roll from the class minimum through minimum plus ten");
    }
    screen_shot("attributes", "CHAR02I0.IMG", 10, 30, 30, 163);
    chargen_click(283, 183, 0);
    check_expect(chargen_mode() == UI_ATTR && chargen_notice()[0], "OK refuses unspent attributes");
    chargen_key(13);
    chargen_click(180, 165, 0);
    chargen_click(283, 150, 0);
    chargen_click(180, 175, 0);
    check_expect(chargen_attr_pool() == pool, "restore returns the saved point pool");
    for (i = 0; i < 8; i++) { check_expect(who->attribute[i] == attr[i], "restore returns every saved attribute"); }
    for (i = 0; i < 8; i++) {
        y = chargen_attr_y(i);
        chargen_click(18, y + 10, 0);
        chargen_click(51, y + 4, 0);
        check_expect(who->attribute[i] == attr[i] + 1, "every attribute can receive a point");
        chargen_click(51, y + 17, 0);
        chargen_click(51, y + 17, 0);
        check_expect(who->attribute[i] == attr[i], "refund stops at the rolled minimum");
    }
    chargen_click(18, 30, 0);
    while (chargen_attr_pool()) { chargen_click(51, 24, 0); }
    chargen_click(283, 183, 0);
    check_expect(chargen_mode() == UI_SKILL, "spent attributes allow the skill screen");
    screen_shot("skills", "CHAR03I0.IMG", 66, 31, 130, 155);
    chargen_click(283, 183, 0);
    check_expect(chargen_mode() == UI_SKILL && chargen_notice()[0], "OK refuses unspent skills");
    chargen_key(13);
    for (i = 0; i < 12; i++) {
        y = chargen_skill_y(i);
        skill = class_skill(2, i); before = who->skill[skill];
        chargen_click(100, y + 4, 0);
        chargen_click(236, y + 4, 0);
        check_expect(who->skill[skill] == before + 1, "bonus goes to the selected skill");
        chargen_click(208, y + 4, 0);
        chargen_click(208, y + 4, 0);
        check_expect(who->skill[skill] == before, "skill refunds stop at the starting value");
    }
    for (i = 0; i < 3; i++) {
        y = chargen_skill_y(i == 0 ? 2 : (i == 1 ? 5 : 11));
        for (j = 0; j < 7; j++) { chargen_click(236, y + 4, 0); }
        check_expect(chargen_skill_bonus(i) == 6, "each group spends exactly six points");
    }
    chargen_click(283, 183, 0);
    check_expect(chargen_mode() == UI_REFLEX, "spent skills allow reflexes");
    chargen_click(160, 152, 0);
    check_expect(who->reflexes == 0, "very high reflexes can be chosen");
    chargen_click(160, 188, 0);
    check_expect(who->reflexes == 4, "very low reflexes can be chosen");
    screen_shot("reflexes", "CHAR05I0.IMG", 127, 184, 66, 9);
    chargen_click(283, 183, 0);
    check_expect(chargen_mode() == UI_SUMMARY, "summary follows reflexes");
    screen_shot("summary", "CHAR04I0.IMG", 66, 31, 130, 155);
    mem_copy(old, seam_screen(0), sizeof(old));
    seam_present();
    pixels = seam_screen(0);
    check_expect(!memcmp(old, pixels, sizeof(old)), "idle summary pixels are stable");
    chargen_click(279, 117, 0);
    check_expect(who->reflexes == 2, "summary edits reflexes in its shifted rows");
    chargen_click(100, 5, 0);
    chargen_key('X'); chargen_key(13);
    check_expect(chargen_mode() == UI_SUMMARY && same(who->name, "TestX"), "summary rename returns to the summary");
    chargen_click(283, 183, 0);
    check_expect(chargen_mode() == UI_WORLD && who->health == 25 + class_at(2)->hp_per_level,
                 "completed character enters with classic starting health");
    check_expect(who->magicka == magicka_state(who->attribute[ATTR_INT], who->class_flags), "class multiplier supplies starting spell points");
    seam_present();
    check_expect(play_pic(PIC_FACE) > 0, "selected portrait reaches the dungeon HUD");

    chargen_begin(); chargen_key('S'); chargen_click(0, 0, 1); chargen_key('y'); chargen_key('m');
    chargen_click(160, 140, 0);
    screen_shot("question", "CHGN00I0.IMG", 20, 135, 290, 48);
    mem_zero(seen, sizeof(seen)); mem_zero(counts, sizeof(counts));
    for (i = 0; i < 10; i++) {
        j = chargen_question_number();
        check_expect(j >= 1 && j <= 40 && !seen[j - 1], "class questions never repeat");
        seen[j - 1] = 1;
        check_expect(question_text(j)[0], "question text is loaded from TEXT.RSC");
        counts[question_kind(j, i % 3)]++;
        chargen_key('a' + i % 3);
    }
    check_expect(chargen_mode() == UI_CLASS_YES, "ten answers offer a class");
    chargen_key('Y');
    check_expect(character_state()->klass == class_from_answers(counts), "class offer uses the classic answer table");
}

static void check_custom_class(void)
{
    int i;
    const ClassInfo *c;

    chargen_begin(); chargen_key('S'); chargen_click(0, 0, 8); chargen_key('Y'); chargen_key('F');
    chargen_click(160, 85, 0);
    for (i = 0; i < 19; i++) { chargen_key(1002); }
    chargen_key(13);
    check_expect(chargen_mode() == UI_MAKER, "Custom opens its own editor");
    chargen_click(100, 5, 0);
    chargen_key('S'); chargen_key('c'); chargen_key('o'); chargen_key('u'); chargen_key('t'); chargen_key(13);
    for (i = 0; i < 12; i++) {
        chargen_click(100, chargen_skill_y(i) + 4, 0);
        chargen_click(100, 66, 0);
    }
    c = class_at(18);
    if (!c) { check_expect(0, "custom class exists"); return; }
    check_expect(c && same(c->name, "Scout"), "custom class keeps its name");
    for (i = 0; i < 12; i++) { check_expect(class_skill(18, i) == i, "custom skills are distinct selected entries"); }
    chargen_click(255, 50, 0);
    check_expect(c->hp_per_level == 9, "custom hit points can be increased");
    chargen_click(280, 110, 0);
    chargen_click(120, 15, 0);
    chargen_click(100, 82, 0); /* Acute Hearing, kind 2 */
    check_expect(chargen_special_count(0) == 1 && (class_at(18)->flags & 1), "custom advantage sets its real class flag");
    chargen_click(40, 185, 0);
    chargen_click(280, 155, 0);
    chargen_click(44, 56, 0);
    chargen_click(77, 106, 0);
    check_expect(chargen_maker_reputation(0) == 5 && chargen_maker_reputation(1) == -5,
                 "custom reputation bars transfer a balanced five points");
    /* The overlay has a header and dimensions, unlike full-screen CHAR art. */
    seam_present();
    check_expect(play_pic(PIC_SCREEN) > 0, "reputation overlay composes");
    chargen_click(185, 177, 0);
    screen_shot("custom", "CUST00I0.IMG", 66, 31, 110, 155);
    chargen_click(283, 183, 0);
    check_expect(chargen_mode() == UI_BG_HOW && character_state()->klass == 18, "custom class is accepted");
    chargen_click(160, 85, 0);
    check_expect(chargen_mode() == UI_REPUTE && character_state()->gifts > 0, "custom class gets the nearest standard class's background");
}

static void check_place_and_screen(void)
{
    MarkInfo marks[3];
    float x;
    float y;
    float z;
    int cx;
    int cy;

    marks[0].record = 8;
    marks[0].x = 0.0f;
    marks[0].y = 0.0f;
    marks[0].z = 0.0f;
    marks[1].record = 6;
    marks[1].x = 10.0f;
    marks[1].y = 0.0f;
    marks[1].z = 0.0f;
    marks[2].record = 6;
    marks[2].x = 3.0f;
    marks[2].y = 0.0f;
    marks[2].z = 0.0f;
    x = 100.0f;
    y = 100.0f;
    z = 100.0f;
    check_expect(place_marker6(marks, 3, 0.0f, 0.0f, 0.0f, &x, &y, &z) == 1, "a 6 is chosen");
    check_expect(x == 3.0f && y == 0.0f && z == 0.0f, "the nearest 6");
    marks[1].record = 8;
    marks[2].record = 10;
    x = 5.0f;
    check_expect(place_marker6(marks, 3, 0.0f, 0.0f, 0.0f, &x, &y, &z) == 0, "no 6 keeps marker 8");
    check_expect(x == 5.0f, "the point is unchanged");
    check_expect(pointer_classic(320, 240, 640, 480, &cx, &cy) == 1, "the centre is on the picture");
    check_expect(cx == 160 && cy == 100, "640x480 centre is classic 160,100");
    check_expect(pointer_classic(10, 10, 640, 480, &cx, &cy) == 0, "the letterbox is outside");
}

/* A large empty floor isolates controls from Hold walls and spawn geometry. */
static void check_movement_directions(void)
{
    static Scene scene;
    static Mesh mesh;
    static Instance instance;
    static Tri triangle[2];
    static const float x[2][3] = { {-2000, 2000, 2000}, {-2000, 2000, -2000} };
    static const float z[2][3] = { {-2000, -2000, 2000}, {-2000, 2000, 2000} };
    unsigned char held[ACTS];
    unsigned char pressed[ACTS];
    MoveIn in;
    Camera cam;
    Basis basis;
    float left_x, left_z, expected_x, expected_z, distance;
    int i, j, yaw, region;

    mem_zero(&scene, sizeof(scene));
    mem_zero(&mesh, sizeof(mesh));
    mem_zero(&instance, sizeof(instance));
    mem_zero(triangle, sizeof(triangle));
    mem_zero(held, sizeof(held));
    mem_zero(pressed, sizeof(pressed));
    mem_zero(&in, sizeof(in));
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            triangle[i].x[j] = x[i][j];
            triangle[i].z[j] = z[i][j];
        }
        triangle[i].slot = 1;
    }
    mesh.tris = triangle;
    mesh.ntris = 2;
    instance.mesh = &mesh;
    instance.m[0] = instance.m[4] = instance.m[8] = 1;
    scene.models = &instance;
    scene.nmodels = 1;
    scene.version = 1;
    move_init();
    in.held = held;
    in.pressed = pressed;
    in.dt = 0.1f;
    left_x = left_z = 0;
    for (yaw = 0; yaw < 2048; yaw += 512) {
        for (region = 0; region < 9; region++) {
            move_place(0, WALK_EYE, 0, yaw);
            in.region = region;
            move_frame(&scene, &in);
            move_camera(&cam);
            camera_basis(&cam, &basis);
            distance = (float)region_speed(region,
                ground_speed(character_state()->attribute[ATTR_SPD], 0, 0, 0, 0, 0)) * in.dt;
            expected_x = basis.fx * distance;
            expected_z = basis.fz * distance;
            if (region == 6 || region == 8) {
                expected_x = basis.rx * distance * (region == 6 ? -1 : 1);
                expected_z = basis.rz * distance * (region == 6 ? -1 : 1);
            } else if (region == 7) {
                expected_x = -expected_x;
                expected_z = -expected_z;
            }
            check_expect(check_near(cam.x, expected_x, 0.1f) && check_near(cam.z, expected_z, 0.1f),
                         "all nine regions move in their camera-relative direction");
            if (yaw == 0 && region == 0) { left_x = cam.x; left_z = cam.z; }
            if (yaw == 0 && region == 2) {
                check_expect(left_x < 0 && cam.x > 0 && check_near(cam.x, -left_x, 0.01f) &&
                             check_near(cam.z, left_z, 0.01f), "forward-left and forward-right mirror each other");
            }
        }
    }
    move_place(0, WALK_EYE, 0, 0);
    in.region = 4;
    in.view_mode = 1;
    in.mouse_dy = -40;
    move_frame(&scene, &in);
    move_camera(&cam);
    check_expect(cam.pitch > 0, "mouse up looks upward");
    in.mouse_dy = 80;
    move_frame(&scene, &in);
    move_camera(&cam);
    check_expect(cam.pitch < 0, "mouse down looks downward");
    in.mouse_dy = -10000;
    move_frame(&scene, &in);
    move_camera(&cam);
    check_expect(check_near(cam.pitch, DF_TAU / 8, 0.001f), "upward look has the classic pitch limit");
    in.mouse_dy = 0;
    pressed[LOOK_HOME] = 1;
    move_frame(&scene, &in);
    move_camera(&cam);
    check_expect(cam.pitch == 0, "Home centers mouse look");
    pressed[LOOK_HOME] = 0;
    for (i = 0; i < 9; i++) {
        int px, py;
        px = 53 + (i % 3) * 106;
        py = 25 + (i / 3) * 51;
        check_expect(control_region(px, py, 1, 0, 0, 0, held) == 4,
                     "hovering every cursor region leaves the player still");
        check_expect(control_region(px, py, 1, 0, 0, 1, held) == i,
                     "holding left enables the pointed cursor region");
        check_expect(control_region(px, py, 1, 1, 1, 1, held) == 4,
                     "the View-mode UI pointer never steers");
    }
    check_expect(control_region(1, 199, 1, 0, 0, 1, held) == 4, "the HUD cannot steer");
    held[FORWARD] = 1;
    check_expect(control_region(1, 199, 1, 1, 1, 0, held) == 1, "keys still move with the pointer over the HUD");
    move_place(0, WALK_EYE, 0, 0);
    options_set_flags(2);
    in.region = 1;
    in.view_mode = 0;
    in.dt = 0.45f;
    move_frame(&scene, &in);
    move_camera(&cam);
    check_expect(check_near(cam.y - move_body()->feet[1] - move_body()->eye_now, 8, 0.001f),
                 "head bobbing uses the original walking sample at 450 ms");
    options_set_flags(0);
    in.dt = 0.1f;
    move_frame(&scene, &in);
    move_camera(&cam);
    check_expect(check_near(cam.y, move_body()->feet[1] + move_body()->eye_now, 0.001f),
                 "disabling head bobbing returns the camera to the body eye");
    move_init();
}

static void check_hud_geometry(void)
{
    static const int dimensions[4][2] = { {640,480}, {800,600}, {512,384}, {320,240} };
    int i, x, y, sx, sy, height;

    for (i = 0; i < 4; i++) {
        int w, h;
        w = dimensions[i][0];
        h = dimensions[i][1];
        hud_screen_rect(w, h, 0, &height);
        check_expect(height == w * 46 / 320, "HUD height follows the full screen width");
        check_expect(world_pointer(0, 0, w, h, 1, &x, &y) && x == 0 && y == 199,
                     "bottom pixel hits the bottom HUD row without a letterbox");
        check_expect(world_pointer(w-1, height-1, w, h, 1, &x, &y) && x == 319 && y == 154,
                     "the HUD top edge agrees with the hit geometry");
        check_expect(world_pointer(0, height, w, h, 1, &x, &y) && y == 153,
                     "the pixel above the HUD belongs to the world");
        world_engine(200, 70, w, h, 1, &sx, &sy);
        check_expect(world_pointer(sx, sy, w, h, 1, &x, &y) &&
                     x >= 199 && x <= 200 && y >= 69 && y <= 70,
                     "pointed activation shares the world pointer transform");
        check_expect(world_pointer(w-1, 0, w, h, 0, &x, &y) && y == 199,
                     "fullscreen mode uses the entire screen for the world");
    }
}

static void check_list_scroll(void)
{
    int y, height;

    chargen_scroll_thumb(0, 0, &y, &height);
    check_expect(y == 59 && height == 81, "an empty list has a full thumb without division");
    chargen_scroll_thumb(0, 9, &y, &height);
    check_expect(height == 81, "nine entries fit the track");
    chargen_scroll_thumb(10, 19, &y, &height);
    check_expect(y == 102 && height == 37, "overflowing lists use the original thumb dimensions");
    chargen_begin();
    chargen_key('S');
    chargen_click(90, 52, 1);
    chargen_key('Y');
    chargen_key('M');
    chargen_click(160, 85, 0);
    chargen_click(243, 139, 0);
    check_expect(chargen_list_top() == 10 && chargen_list_sel() == 18,
                 "the scrollbar can reach the Custom class by mouse");
    chargen_scroll_frame(243, 139, 1, 1, 0);
    chargen_scroll_frame(40, -20, 1, 0, 0.1f);
    check_expect(chargen_list_top() == 0 && chargen_list_sel() == 0,
                 "a captured thumb drag clamps above the track");
    chargen_scroll_frame(243, 139, 0, 0, 0);
    chargen_click(243, 150, 0);
    chargen_scroll_frame(243, 150, 1, 1, 0);
    chargen_scroll_frame(243, 150, 1, 0, 0.23f);
    check_expect(chargen_list_sel() == 2, "holding a scroll arrow repeats the selection");
    chargen_scroll_frame(243, 150, 0, 0, 0);
    screen_shot("class-list", "MAP100I0.IMG", 241, 59, 5, 37);
}

static void check_hold(const char *root)
{
    float x;
    float y;
    float z;
    float x0;
    float z0;
    int i;
    int grounded;
    CharacterState *who;
    MoveIn mv;
    unsigned char held[ACTS];
    unsigned char pressed[ACTS];
    Body *body;

    (void)root;
    check_expect(play_pic(PIC_TITLE) != 0, "PICK03I0 loaded");
    check_expect(play_pic(PIC_MAP) != 0, "MAP100I0 loaded");
    check_expect(seam_map_pixel(90, 52) == 1, "High Rock's pixel is 1");
    check_expect(seam_map_pixel(251, 117) == 8, "Black Marsh's pixel is 8");
    check_expect(seam_map_pixel(109, 55) == 2, "Hammerfell's pixel is 2");
    check_expect(seam_map_pixel(130, 30) == 3, "Skyrim's pixel is 3");
    check_expect(seam_map_pixel(249, 32) == 4, "Morrowind's pixel is 4");
    check_expect(seam_map_pixel(75, 132) == 5, "Summerset's pixel is 5");
    check_expect(seam_map_pixel(134, 119) == 6, "Valenwood's pixel is 6");
    check_expect(seam_map_pixel(156, 120) == 7, "Elsweyr's pixel is 7");
    chargen_begin();
    chargen_key('S');
    chargen_click(90, 52, seam_map_pixel(90, 52));
    check_expect(chargen_mode() == UI_RACE && character_state()->race == 0,
                 "the map's pixel selects a Breton");
    standard_background(0);
    chargen_key(13); chargen_key('A'); chargen_key(13);
    chargen_click(283, 183, 0);
    while (chargen_attr_pool()) { chargen_click(51, 24, 0); }
    chargen_click(283, 183, 0);
    for (i = 0; i < 6; i++) {
        chargen_click(236, 35, 0); chargen_click(236, 84, 0); chargen_click(236, 133, 0);
    }
    chargen_click(283, 183, 0); chargen_click(283, 183, 0);
    {
        PlayIn in;
        Camera cam;
        mem_zero(&in, sizeof(in));
        in.cx = 283; in.cy = 183; in.on_picture = 1; in.mouse_pressed = 1;
        check_expect(play_frame(&in, &cam, 0) == FRAME_ENTER, "summary OK asks the app to load Privateer's Hold");
    }
    if (!seam_open_hold(&x, &y, &z)) {
        check_expect(0, "the hold loads");
        return;
    }
    who = character_state();
    g_card_quads = 0;
    hud_blit(play_pic(PIC_BAR), 0, 154, 320, 46);
    check_expect(g_card_quads && g_card_bottom == 0 && g_card_top == 92 &&
                 g_card_left == 0 && g_card_right == 640,
                 "the actual HUD quads meet the screen bottom across its full width");
    move_place(x, y, z, 0);
    for (i = 0; i < ACTS; i++) {
        held[i] = 0;
        pressed[i] = 0;
    }
    mem_zero(&mv, sizeof(mv));
    mv.held = held;
    mv.pressed = pressed;
    mv.region = 4;
    mv.view_mode = 0;
    mv.mouse_dx = 0;
    mv.mouse_dy = 0;
    mv.dt = 0.05f;
    mv.bar_blocks = 0;
    grounded = 0;
    for (i = 0; i < 40; i++) {
        move_frame(seam_level() ? &seam_level()->scene : 0, &mv);
        if (move_body()->grounded) {
            grounded = 1;
            break;
        }
    }
    check_expect(grounded, "the start stands on the floor");
    body = move_body();
    x0 = body->feet[0];
    z0 = body->feet[2];
    mv.region = 1;
    mv.dt = 0.25f;
    move_frame(&seam_level()->scene, &mv);
    {
        float dx;
        float dz;

        dx = body->feet[0] - x0;
        dz = body->feet[2] - z0;
        check_expect(dx * dx + dz * dz < 400.0f * 400.0f, "a short walk does not cross the hold");
    }
    pressed[JUMP] = 1;
    mv.region = 4;
    mv.dt = 0.05f;
    move_frame(&seam_level()->scene, &mv);
    check_expect(move_airborne() || !body->grounded, "a jump leaves the ground");
    check_expect(who->fatigue == ((who->attribute[ATTR_STR] + who->attribute[ATTR_END]) << 6) - 11, "the jump spent 11 fatigue");
    pressed[JUMP] = 0;
    grounded = 0;
    for (i = 0; i < 80; i++) {
        move_frame(&seam_level()->scene, &mv);
        if (body->grounded) {
            grounded = 1;
            break;
        }
    }
    check_expect(grounded, "the jump lands");
}

void check_game(const char *root)
{
    int id;
    vfs_mount(root);
    mem_init();
    pic_end();
    for (id = 1; id <= PIC_MAX; id++) { pic_free(id); }
    pic_begin(0, 384u * 1024u, &g_card);
    check_journey();
    /* The rules on a small table first: the boot below defines the real one. */
    check_standing();
    check_trade();
    if (!seam_boot()) {
        check_expect(0, "GFALL assets and classes load");
        return;
    }
    check_standing_real();
    chargen_seed(1);
    check_steer();
    check_hud();
    options_reset();
    check_vitals();
    check_title_and_chargen();
    check_races();
    check_opening();
    check_creation_controls();
    check_custom_class();
    check_place_and_screen();
    check_hud_geometry();
    check_list_scroll();
    check_movement_directions();
    check_hold(root);
    check_ui(root);
    check_combat(root);
    check_world(root);
    check_state(root);
    pic_end();
}

int check_game_resume(const char *root, int slot)
{
    PlayIn input;
    Camera camera;
    float x, y, z;
    int i, opened;
    double saved_x, saved_z, world_x, world_z;

    mem_init();
    vfs_mount(root);
    pic_begin(0, 384u * 1024u, &g_card);
    if (!seam_boot()) { return 1; }
    config_read();
    mem_zero(&input, sizeof(input));
    input.ch = 'L';
    play_frame(&input, &camera, 0);
    check_expect(options_panel() == PANEL_FILES, "cold title L opens the real load browser");
    input.ch = 1002; input.key = KEY_DOWN;
    /* Scenario 7 exercises an outdoor save in the browser's sixth slot. */
    for (i = 1; i < (slot == 7 ? 6 : slot); i++) { play_frame(&input,&camera,0); }
    input.ch = 13; input.key = 0;
    check_expect(play_frame(&input, &camera, 0) == FRAME_LOAD, "cold title Enter loads the selected save");
    check_expect(slot == 7 ? !strcmp(game_state()->save_label,"World round trip") : slot == 5 ? !strcmp(game_state()->save_label,"Combat round trip") :
                 character_state()->gold == (slot == 1 ? 987 : slot == 4 ? 1087 : 1024) && !strcmp(game_state()->save_label, "Host round trip"),
                 "fresh process recovered the actual saved character");
    saved_x = game_state()->x; saved_z = game_state()->z;
    if (slot == 7) {
        saved_x += (double)world_where()->where.origin_x*STREAM_CHUNK;
        saved_z += (double)world_where()->where.origin_z*STREAM_CHUNK;
    }
    if (!seam_open_place(1,0,&x, &y, &z)) { return 1; }
    seam_land(&camera,1,0);
    world_x = camera.x; world_z = camera.z;
    if (slot == 7) { stream_world(camera.x,camera.z,&world_x,&world_z); }
    check_expect(check_near((float)(world_x-saved_x),0,0.1f) &&
                 check_near((float)(world_z-saved_z),0,0.1f), "cold load stands at the saved position");
    if (slot == 7) {
        check_expect(seam_level()->streamed && world_where()->where.kind == WHERE_WORLD && automap_count() > 0,
                     "cold load reopens the streamed world and restores dungeon exploration");
        check_expect(check_near(magic_state()->missile[0].x-camera.x,20,0.1f) &&
                     check_near(magic_state()->missile[0].z-camera.z,30,0.1f),
                     "cold load keeps in-flight magic beside the player after the stream origin changes");
        pic_end(); return check_failed() ? 1 : 0;
    }
    opened = 0;
    for (i = 0; i < seam_level()->scene.nmovers; i++) {
        opened += mover_at_end(&seam_level()->scene.movers[i]) != 0;
    }
    check_expect(slot == 5 || opened > 0, "cold load restores the changed door");
    if (slot == 5) { check_combat_loaded(); }
    if (slot == 6) {
        df_u32 spells[OBJECTS];
        const ObjectInfo *item;
        int dagger;
        dagger = 0;
        for (i = 0; i < 27; i++) {
            item = object_at(game_state()->equipped[i]);
            dagger += item && item->group == 3 && item->index == 0 && item->material == 1;
        }
        check_expect(dagger && spell_rows(inventory_book(),spells,OBJECTS),
                     "cold load restores actual equipment and spells owned by the book");
        check_expect(possessions_open(VIEW_INVENTORY,0), "cold-loaded inventory opens on restored item records");
        seam_present(); play_draw(); possessions_close();
    }
    if (slot == 4) {
        const ObjectInfo *item;
        int edited, arrows;
        edited = arrows = 0;
        for (i = 0; i < OBJECTS; i++) {
            item = &game_state()->object[i];
            if (!item->id) { continue; }
            if (item->type == RECORD_SPELL && item->parent == inventory_book() && item->index == 5) {
                edited += !strcmp(item->name,"My Lantern") && item->spell_icon == spell_def(5)->icon;
            }
            if (item->type == RECORD_ITEM && item->parent == game_state()->inventory_id && item->group == 3 && item->index == 18) {
                arrows += item->count;
            }
        }
        check_expect(edited == 1 && arrows == 199,"cold loading preserves the spell's editable name/icon and the merged arrow stack");
    }
    pic_end();
    return check_failed() ? 1 : 0;
}

#ifdef GFALL_HOST_MAIN
#include <stdio.h>

int main(int argc, char **argv)
{
    const char *root;

    if (argc == 3 && !strcmp(argv[1], "--gfall-resume")) { return check_game_resume(argv[2],1); }
    if (argc == 3 && !strcmp(argv[1], "--gfall-resume-items")) { return check_game_resume(argv[2],6); }
    if (argc == 3 && !strcmp(argv[1], "--gfall-resume-inventory")) { return check_game_resume(argv[2],4); }
    if (argc == 3 && !strcmp(argv[1], "--gfall-resume-combat")) { return check_game_resume(argv[2],5); }
    if (argc == 3 && !strcmp(argv[1], "--gfall-resume-world")) { return check_game_resume(argv[2],7); }
    root = argc > 1 ? argv[1] : "daggerfall/DAGGER/ARENA2";
    check_game(root);
    if (check_failed()) {
        fprintf(stderr, "gfall host failed\n");
        return 1;
    }
    printf("gfall host ok\n");
    return 0;
}
#endif
