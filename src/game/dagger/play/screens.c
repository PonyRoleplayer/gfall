#include "game/dagger/play/screens.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/court.h"
#include "game/dagger/play/service.h"
#include "game/dagger/play/bag.h"
#include "game/dagger/play/create.h"
#include "game/dagger/play/menu.h"
#include "game/dagger/play/rest.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/save.h"
#include "game/dagger/play/sheet.h"
#include "game/dagger/play/slots.h"
#include "game/dagger/formats/img.h"
#include "engine/ui/font.h"
#include "engine/ui/pic.h"

/* See screens.h. */

enum {
    OPEN_NONE,
    OPEN_BAG,
    OPEN_SHEET,
    OPEN_CREATE,
    OPEN_REST,
    OPEN_SLOTS,
    OPEN_TALK,
    OPEN_SERVICE,
    OPEN_BANK,
    OPEN_COURT
};

static int g_open;
/* The player the bag and the sheet are over: the sheet's Inventory button goes from one to the other. */
static Player *g_player;
/* The composed screen on the card; ART_PAL.COL, the palette all their pieces are in. */
static int g_pic;
static df_u8 g_rgb[768];
static int g_point_x;
static int g_point_y;
static const char *g_said;
/* The save slot chosen, kept past the screen's closing. */
static int g_slot;
static int g_slot_saving;
static char g_slot_name[SAVE_NAME];
/* The merchant's panel's answer, kept past its closing. */
static int g_service_choice;

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
    } else if (g_open == OPEN_REST) {
        rest_compose();
        pixels = rest_pixels();
    } else if (g_open == OPEN_SLOTS) {
        slots_compose();
        pixels = slots_pixels();
    } else if (g_open == OPEN_TALK) {
        talk_compose();
        pixels = talk_pixels();
    } else if (g_open == OPEN_SERVICE) {
        service_compose();
        pixels = service_pixels();
    } else if (g_open == OPEN_BANK) {
        bank_compose();
        pixels = bank_pixels();
    } else if (g_open == OPEN_COURT) {
        court_compose();
        pixels = court_pixels();
    } else {
        create_compose();
        pixels = create_pixels();
        /* The class questions' picture has a palette of its own. */
        create_palette(g_rgb);
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
    } else if (g_open == OPEN_REST) {
        g_said = rest_said();
        rest_close();
    } else if (g_open == OPEN_SLOTS) {
        slots_close();
    } else if (g_open == OPEN_TALK) {
        talk_close();
    } else if (g_open == OPEN_SERVICE) {
        service_close();
    } else if (g_open == OPEN_BANK) {
        bank_close();
    } else if (g_open == OPEN_COURT) {
        court_close();
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

void screens_open_rest(Player *p)
{
    screens_close();
    g_player = p;
    opened(OPEN_REST, img_art_palette(g_rgb) && rest_open(p));
    input_key_pressed();
}

void screens_open_talk(Player *p, const TalkWho *who)
{
    screens_close();
    g_player = p;
    opened(OPEN_TALK, img_art_palette(g_rgb) && talk_open(p, who));
    input_key_pressed();
}

void screens_open_service(int panel)
{
    screens_close();
    g_service_choice = SERVICE_STAY;
    opened(OPEN_SERVICE, img_art_palette(g_rgb) && service_open(panel));
    input_key_pressed();
}

int screens_service_choice(void)
{
    return g_service_choice;
}

void screens_open_trade(Player *p, const Shop *shop, int trade, int day)
{
    screens_close();
    g_player = p;
    opened(OPEN_BAG, img_art_palette(g_rgb) && bag_open_trade(p, shop, trade, day));
    input_key_pressed();
}

void screens_open_bank(Player *p, int region)
{
    screens_close();
    g_player = p;
    opened(OPEN_BANK, img_art_palette(g_rgb) && bank_open(p, region));
    input_key_pressed();
}

void screens_open_court(Player *p)
{
    screens_close();
    g_player = p;
    opened(OPEN_COURT, img_art_palette(g_rgb) && court_open(p));
    input_key_pressed();
}

void screens_open_slots(int saving)
{
    screens_close();
    g_slot_saving = saving;
    opened(OPEN_SLOTS, img_art_palette(g_rgb) && slots_open(saving));
    input_key_pressed();
}

int screens_slot(void)
{
    return g_slot;
}

const char *screens_slot_name(void)
{
    return g_slot_name;
}

int screens_slot_saving(void)
{
    return g_slot_saving;
}

const char *screens_said(void)
{
    const char *said;

    said = g_said;
    g_said = 0;
    return said;
}

int screens_is_open(void)
{
    return g_open != OPEN_NONE;
}

/* A frame of the talk window. */
static int talk_frame(const Input *in)
{
    int vk;

    vk = input_key_pressed();
    if (vk && talk_key(vk) == TALK_LEAVE) {
        return SCREENS_CLOSED;
    }
    if ((in->pressed & INPUT_BIT(INPUT_ATTACK)) && talk_click(g_point_x, g_point_y) == TALK_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (talk_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

/* A frame of the merchant's panel: its choice kept, as it closes. */
static int service_frame(const Input *in)
{
    if (in->pressed & INPUT_BIT(INPUT_MENU)) {
        return SCREENS_CLOSED;
    }
    if (in->pressed & INPUT_BIT(INPUT_ATTACK)) {
        g_service_choice = service_click(g_point_x, g_point_y);
        if (g_service_choice == SERVICE_LEAVE) {
            return SCREENS_CLOSED;
        }
        if (g_service_choice != SERVICE_STAY) {
            return SCREENS_SERVICE;
        }
    }
    return SCREENS_STAY;
}

/* A frame of the bank. */
static int bank_frame(const Input *in)
{
    int vk;

    vk = input_key_pressed();
    if (vk && bank_key(vk) == BANK_LEAVE) {
        return SCREENS_CLOSED;
    }
    if ((in->pressed & INPUT_BIT(INPUT_ATTACK)) && bank_click(g_point_x, g_point_y) == BANK_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (bank_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

/* A frame of the court: no leaving it but by its end. */
static int court_frame(const Input *in, float dt)
{
    int over;

    over = court_step(dt) == COURT_LEAVE;
    if (!over && (in->pressed & INPUT_BIT(INPUT_ATTACK))) {
        over = court_click(g_point_x, g_point_y) == COURT_LEAVE;
    }
    if (over) {
        return SCREENS_COURT_OVER;
    }
    if (court_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

/* A frame of the inventory. */
static int bag_frame(const Input *in)
{
    int vk;

    bag_point(g_point_x, g_point_y);
    /* The Gold button's question has the keys while it is up. */
    vk = input_key_pressed();
    if (bag_asking()) {
        if (vk) {
            bag_key(vk);
        }
    } else if (in->pressed & (INPUT_BIT(INPUT_MENU) | INPUT_BIT(INPUT_INVENTORY))) {
        return SCREENS_CLOSED;
    }
    if ((in->pressed & INPUT_BIT(INPUT_ATTACK)) && bag_click(g_point_x, g_point_y) == BAG_LEAVE) {
        return bag_stole() ? SCREENS_SEEN : SCREENS_CLOSED;
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

/* A frame of rest: the keys, the click, then the time that passes. */
static int rest_frame(const Input *in, float dt)
{
    int vk;

    vk = input_key_pressed();
    if (vk && rest_key(vk) == REST_LEAVE) {
        return SCREENS_CLOSED;
    }
    if ((in->pressed & INPUT_BIT(INPUT_ATTACK)) && rest_click(g_point_x, g_point_y) == REST_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (rest_step(dt) == REST_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (rest_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

/* A frame of the save slots: the keys, the click; what is chosen is kept before it closes. */
static int slots_frame(const Input *in)
{
    int asked;
    int vk;

    asked = SLOTS_STAY;
    vk = input_key_pressed();
    if (vk) {
        asked = slots_key(vk);
    }
    if (asked == SLOTS_STAY && (in->pressed & INPUT_BIT(INPUT_ATTACK))) {
        asked = slots_click(g_point_x, g_point_y);
    }
    if (asked == SLOTS_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (asked == SLOTS_CHOSEN) {
        g_slot = slots_chosen();
        df_copy(g_slot_name, slots_chosen_name(), SAVE_NAME);
        screens_close();
        return SCREENS_SLOT;
    }
    if (slots_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

int screens_frame(const Input *in, float dt)
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
    } else if (g_open == OPEN_REST) {
        asked = rest_frame(in, dt);
    } else if (g_open == OPEN_SLOTS) {
        asked = slots_frame(in);
    } else if (g_open == OPEN_TALK) {
        asked = talk_frame(in);
    } else if (g_open == OPEN_SERVICE) {
        asked = service_frame(in);
    } else if (g_open == OPEN_BANK) {
        asked = bank_frame(in);
    } else if (g_open == OPEN_COURT) {
        asked = court_frame(in, dt);
    } else {
        asked = create_frame(in);
    }
    if (asked == SCREENS_CLOSED || asked == SCREENS_SERVICE || asked == SCREENS_SEEN || asked == SCREENS_COURT_OVER) {
        screens_close();
    }
    return asked;
}

void screens_draw(void)
{
    const char *note;

    screen_draw(g_pic, 0, 0, SCREEN_W, SCREEN_H);
    if (g_open == OPEN_BAG) {
        note = bag_note();
    } else if (g_open == OPEN_SHEET) {
        note = sheet_note();
    } else if (g_open == OPEN_REST) {
        note = rest_note();
    } else if (g_open == OPEN_SLOTS) {
        note = slots_note();
    } else if (g_open == OPEN_TALK || g_open == OPEN_SERVICE || g_open == OPEN_COURT) {
        note = 0;
    } else if (g_open == OPEN_BANK) {
        note = bank_note();
    } else {
        note = create_note();
    }
    if (note) {
        ui_string((DF_VIEW_W - ui_string_width(note)) / 2, ui_line_height() + 8, note);
    }
    menu_draw_pointer(g_point_x, g_point_y);
}
