#include "game/dagger/play/screens.h"
#include "game/dagger/play/bag.h"
#include "game/dagger/play/create.h"
#include "game/dagger/play/menu.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/sheet.h"
#include "game/dagger/formats/img.h"
#include "engine/ui/font.h"
#include "engine/ui/pic.h"

/* See screens.h. */

enum {
    OPEN_NONE,
    OPEN_BAG,
    OPEN_SHEET,
    OPEN_CREATE
};

static int g_open;
/* The player the bag and the sheet are over: the sheet's Inventory button goes from one to the other. */
static Player *g_player;
/* The composed screen on the card; ART_PAL.COL, the palette all their pieces are in. */
static int g_pic;
static df_u8 g_rgb[768];
static int g_point_x;
static int g_point_y;

/* The open screen's composed picture up on the card, in place of the last one. */
static void send_screen(void)
{
    const df_u8 *pixels;

    if (g_open == OPEN_BAG) {
        bag_compose();
        pixels = bag_pixels();
    } else if (g_open == OPEN_SHEET) {
        sheet_compose();
        pixels = sheet_pixels();
    } else {
        create_compose();
        pixels = create_pixels();
    }
    if (g_pic) {
        pic_free(g_pic);
    }
    g_pic = pic_load(pixels, SCREEN_W, SCREEN_H, g_rgb, 0);
}

void screens_close(void)
{
    if (g_open == OPEN_BAG) {
        bag_close();
    } else if (g_open == OPEN_SHEET) {
        sheet_close();
    } else if (g_open == OPEN_CREATE) {
        create_close();
    }
    g_open = OPEN_NONE;
    if (g_pic) {
        pic_free(g_pic);
        g_pic = 0;
    }
}

/* The screen just opened (or not, opened 0): its first picture goes up. */
static void opened(int which, int ok)
{
    if (ok) {
        g_open = which;
        send_screen();
    }
}

void screens_open_bag(Player *p, Entity *searched)
{
    screens_close();
    g_player = p;
    opened(OPEN_BAG, img_art_palette(g_rgb) && bag_open(p, searched));
}

void screens_open_sheet(Player *p)
{
    screens_close();
    g_player = p;
    opened(OPEN_SHEET, img_art_palette(g_rgb) && sheet_open(p));
}

void screens_open_create(void)
{
    screens_close();
    opened(OPEN_CREATE, img_art_palette(g_rgb) && create_open());
    /* Its first call notes what is down already: the click that opened it is not a key. */
    input_key_pressed();
}

int screens_is_open(void)
{
    return g_open != OPEN_NONE;
}

/* A frame of the inventory. */
static int bag_frame(const Input *in)
{
    bag_point(g_point_x, g_point_y);
    if (in->pressed & (INPUT_BIT(INPUT_MENU) | INPUT_BIT(INPUT_INVENTORY))) {
        return SCREENS_CLOSED;
    }
    if ((in->pressed & INPUT_BIT(INPUT_ATTACK)) && bag_click(g_point_x, g_point_y) == BAG_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (bag_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

/* A frame of the character sheet. */
static int sheet_frame(const Input *in)
{
    if ((in->pressed & (INPUT_BIT(INPUT_MENU) | INPUT_BIT(INPUT_CHARACTER))) && sheet_may_leave()) {
        return SCREENS_CLOSED;
    }
    if (in->pressed & INPUT_BIT(INPUT_ATTACK)) {
        int asked;

        asked = sheet_click(g_point_x, g_point_y);
        if (asked == SHEET_LEAVE) {
            return SCREENS_CLOSED;
        }
        if (asked == SHEET_INVENTORY) {
            screens_open_bag(g_player, 0);
            return SCREENS_STAY;
        }
    }
    if (sheet_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

/* A frame of creation: the keys first (the name), then the click. */
static int create_frame(const Input *in)
{
    int asked;
    int vk;

    asked = CREATE_STAY;
    vk = input_key_pressed();
    if (vk) {
        asked = create_key(vk);
    }
    if (asked == CREATE_STAY && (in->pressed & INPUT_BIT(INPUT_ATTACK))) {
        asked = create_click(g_point_x, g_point_y);
    }
    if (asked == CREATE_DONE) {
        return SCREENS_MADE;
    }
    if (asked == CREATE_SKIPPED) {
        return SCREENS_SKIPPED;
    }
    if (create_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

int screens_frame(const Input *in)
{
    int px;
    int py;
    int asked;

    input_pointer(&px, &py);
    screen_from_pointer(px, py, &g_point_x, &g_point_y);
    if (g_open == OPEN_BAG) {
        asked = bag_frame(in);
    } else if (g_open == OPEN_SHEET) {
        asked = sheet_frame(in);
    } else {
        asked = create_frame(in);
    }
    if (asked == SCREENS_CLOSED) {
        screens_close();
    }
    return asked;
}

void screens_draw(void)
{
    const char *note;

    screen_draw(g_pic, 0, 0, SCREEN_W, SCREEN_H);
    note = g_open == OPEN_BAG ? bag_note() : (g_open == OPEN_SHEET ? sheet_note() : create_note());
    if (note) {
        ui_string((DF_VIEW_W - ui_string_width(note)) / 2, ui_line_height() + 8, note);
    }
    menu_draw_pointer(g_point_x, g_point_y);
}
