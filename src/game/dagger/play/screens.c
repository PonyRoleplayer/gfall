#include "game/dagger/play/screens.h"
#include "game/dagger/play/logbook.h"
#include "game/dagger/play/spellbook.h"
#include "game/dagger/play/maker.h"
#include "game/dagger/play/ills.h"
#include "engine/scene/clock.h"
#include "game/dagger/play/msgbox.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/court.h"
#include "game/dagger/play/service.h"
#include "game/dagger/play/travelmap.h"
#include "game/dagger/play/townmap.h"
#include "game/dagger/play/dunmap.h"
#include "game/dagger/play/marker.h"
#include "game/dagger/play/bag.h"
#include "game/dagger/play/create.h"
#include "game/dagger/play/menu.h"
#include "game/dagger/play/rest.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/save.h"
#include "game/dagger/play/sheet.h"
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
    OPEN_TALK,
    OPEN_SERVICE,
    OPEN_BANK,
    OPEN_COURT,
    OPEN_TRAVEL,
    OPEN_TOWNMAP,
    OPEN_DUNMAP,
    OPEN_MSGBOX,
    OPEN_LOGBOOK,
    OPEN_SPELLBOOK,
    OPEN_MAKER
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
/*
 * A map's mouse: a drag under way (its last point), a right-button turn under
 * way, how long the left button has been held over a button that repeats, and
 * the player's arrow as its own picture (marker.h) with the step it was made at.
 */
static int g_drag;
static int g_drag_x;
static int g_drag_y;
static int g_turning;
static float g_hold;
static int g_marker_pic;
static int g_marker_step = -1;
/* A held button repeats after this, then each this. */
#define REPEAT_AFTER 0.3f
#define REPEAT_EVERY 0.05f
/* The arrow's picture's size on the screen, the engine's pixels. */
#define MARKER_DRAWN 18
/* The merchant's panel's answer, and a message box's, kept past their closing. */
static int g_service_choice;
static int g_msgbox_answer;

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
    } else if (g_open == OPEN_TOWNMAP) {
        townmap_compose();
        pixels = townmap_pixels();
    } else if (g_open == OPEN_DUNMAP) {
        dunmap_compose();
        pixels = dunmap_pixels();
    } else if (g_open == OPEN_MSGBOX) {
        msgbox_compose();
        pixels = msgbox_pixels();
    } else if (g_open == OPEN_LOGBOOK) {
        logbook_compose();
        pixels = logbook_pixels();
    } else if (g_open == OPEN_SPELLBOOK) {
        spellbook_compose();
        pixels = spellbook_pixels();
    } else if (g_open == OPEN_MAKER) {
        maker_compose();
        pixels = maker_pixels();
    } else if (g_open == OPEN_TRAVEL) {
        travelmap_compose();
        pixels = travelmap_pixels();
        /* The maps' own palette, FMAP_PAL.COL. */
        travelmap_palette(g_rgb);
    } else {
        create_compose();
        pixels = create_pixels();
        /* The class questions' picture has a palette of its own. */
        create_palette(g_rgb);
    }
    if (g_pic) {
        pic_free(g_pic);
    }
    /* The indoor automap's chrome has a clear hole the engine's map shows through. */
    g_pic = pic_load(pixels, SCREEN_W, SCREEN_H, g_rgb, g_open == OPEN_DUNMAP);
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
    } else if (g_open == OPEN_TALK) {
        talk_close();
    } else if (g_open == OPEN_SERVICE) {
        service_close();
    } else if (g_open == OPEN_BANK) {
        bank_close();
    } else if (g_open == OPEN_COURT) {
        court_close();
    } else if (g_open == OPEN_TRAVEL) {
        travelmap_close();
    } else if (g_open == OPEN_TOWNMAP) {
        townmap_close();
    } else if (g_open == OPEN_DUNMAP) {
        dunmap_close();
    } else if (g_open == OPEN_MSGBOX) {
        msgbox_close();
    } else if (g_open == OPEN_LOGBOOK) {
        logbook_close();
    } else if (g_open == OPEN_SPELLBOOK) {
        spellbook_close();
    } else if (g_open == OPEN_MAKER) {
        maker_close();
    }
    g_drag = 0;
    g_turning = 0;
    g_hold = 0.0f;
    if (g_marker_pic) {
        pic_free(g_marker_pic);
        g_marker_pic = 0;
        g_marker_step = -1;
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

void screens_open_logbook(int now)
{
    screens_close();
    opened(OPEN_LOGBOOK, img_art_palette(g_rgb) && logbook_open(now));
    input_key_pressed();
}

void screens_open_spellbook(Spellbook *book, const Character *pc)
{
    screens_close();
    opened(OPEN_SPELLBOOK, img_art_palette(g_rgb) && spellbook_open(book, pc));
    input_key_pressed();
}

void screens_open_maker(Spellbook *book, Player *player)
{
    screens_close();
    opened(OPEN_MAKER, img_art_palette(g_rgb) && maker_open(book, player));
    input_key_pressed();
}

/* A frame of the spell maker: keys (a name is typed) and clicks. */
static int maker_frame(const Input *in)
{
    int vk;
    int asked;

    asked = MAKER_STAY;
    vk = input_key_pressed();
    if (vk) {
        asked = maker_key(vk);
    } else if (!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT)) {
        asked = maker_click(g_point_x, g_point_y);
    }
    if (asked == MAKER_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (maker_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

void screens_open_spellbook_buy(Spellbook *book, Player *player, int quality)
{
    screens_close();
    opened(OPEN_SPELLBOOK, img_art_palette(g_rgb) && spellbook_open_buy(book, player, quality));
    input_key_pressed();
}

/* The spell the book chose, kept past the screen's close for the game. */
static int g_spell_chosen = -1;

int screens_spell(void)
{
    return g_spell_chosen;
}

/* A frame of the spellbook: keys, clicks and the wheel. */
static int spellbook_frame(const Input *in)
{
    int vk;
    int asked;

    asked = SPELLBOOK_STAY;
    vk = input_key_pressed();
    if (vk) {
        asked = spellbook_key(vk);
    } else if (in->pressed & INPUT_BIT(INPUT_MENU)) {
        asked = SPELLBOOK_LEAVE;
    } else if (!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT)) {
        asked = spellbook_click(g_point_x, g_point_y);
    }
    if (in->wheel) {
        spellbook_wheel(in->wheel);
    }
    if (asked == SPELLBOOK_READY) {
        g_spell_chosen = spellbook_chosen();
        return SCREENS_SPELL;
    }
    if (asked == SPELLBOOK_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (spellbook_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

/* A frame of the logbook: keys and clicks. */
static int logbook_frame(const Input *in)
{
    int vk;
    int asked;

    asked = LOGBOOK_STAY;
    vk = input_key_pressed();
    if (vk) {
        asked = logbook_key(vk);
    } else if (in->pressed & INPUT_BIT(INPUT_MENU)) {
        asked = LOGBOOK_LEAVE;
    } else if (!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT)) {
        asked = logbook_click(g_point_x, g_point_y);
    }
    if (asked == LOGBOOK_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (logbook_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

void screens_open_msgbox(const char *text, int yes_no)
{
    screens_close();
    g_msgbox_answer = MSGBOX_STAY;
    opened(OPEN_MSGBOX, img_art_palette(g_rgb) && msgbox_open(text, yes_no));
    input_key_pressed();
}

int screens_msgbox_answer(void)
{
    return g_msgbox_answer;
}

/* A frame of a message box: a click or a key answers it. */
static int msgbox_frame(const Input *in)
{
    int vk;

    vk = input_key_pressed();
    if (vk) {
        g_msgbox_answer = msgbox_key(vk);
    } else if (!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT)) {
        g_msgbox_answer = msgbox_click(g_point_x, g_point_y);
    }
    if (g_msgbox_answer != MSGBOX_STAY) {
        return SCREENS_ANSWERED;
    }
    if (msgbox_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
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

void screens_open_dunmap(const float *feet, float yaw)
{
    screens_close();
    opened(OPEN_DUNMAP, img_art_palette(g_rgb) && dunmap_open(feet, yaw));
    input_key_pressed();
}

int screens_dunmap_open(void)
{
    return g_open == OPEN_DUNMAP;
}

void screens_open_townmap(int region, int index, float x, float z, float yaw)
{
    screens_close();
    opened(OPEN_TOWNMAP, img_art_palette(g_rgb) && townmap_open(region, index, x, z, yaw));
    input_key_pressed();
}

void screens_open_travel(Player *p, int px, int py, int region)
{
    screens_close();
    g_player = p;
    opened(OPEN_TRAVEL, travelmap_open(p, px, py, region));
    input_key_pressed();
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

    if (in->wheel) {
        talk_wheel(g_point_x, g_point_y, in->wheel);
    }

    vk = input_key_pressed();
    if (vk && talk_key(vk) == TALK_LEAVE) {
        return SCREENS_CLOSED;
    }
    if ((!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT)) && talk_click(g_point_x, g_point_y) == TALK_LEAVE) {
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
    int vk;

    if (in->pressed & INPUT_BIT(INPUT_MENU)) {
        return SCREENS_CLOSED;
    }
    /* A tavern's days are typed. */
    vk = input_key_pressed();
    if (vk) {
        g_service_choice = service_key(vk);
        if (g_service_choice != SERVICE_STAY) {
            return SCREENS_SERVICE;
        }
    }
    if (!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT)) {
        g_service_choice = service_click(g_point_x, g_point_y);
        if (g_service_choice == SERVICE_LEAVE) {
            return SCREENS_CLOSED;
        }
        if (g_service_choice != SERVICE_STAY) {
            return SCREENS_SERVICE;
        }
    }
    /* A tavern's question or menu shown, or the days typed. */
    if (service_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

/* A frame of the travel map: its pointer, keys and clicks, and a journey's countdown. */
static int travel_frame(const Input *in, float dt)
{
    int vk;

    travelmap_point(g_point_x, g_point_y);
    vk = input_key_pressed();
    if (vk && travelmap_key(vk) == TRAVELMAP_LEAVE) {
        return SCREENS_CLOSED;
    }
    if ((in->pressed & INPUT_BIT(INPUT_ATTACK)) && travelmap_click(g_point_x, g_point_y) == TRAVELMAP_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (travelmap_step(dt) == TRAVELMAP_GO) {
        return SCREENS_JOURNEY;
    }
    if (travelmap_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

static void map_mouse(const Input *in, float dt, int (*on_map)(int, int), void (*drag)(int, int),
                      void (*turn)(int), int (*repeats)(int, int), int (*click)(int, int));

/* A frame of the town map: the pointer names what it is over, the wheel zooms. */
static int townmap_frame(const Input *in, float dt)
{
    int vk;
    int asked;

    townmap_point(g_point_x, g_point_y);
    if (in->wheel) {
        townmap_wheel(in->wheel);
    }
    map_mouse(in, dt, townmap_on_map, townmap_drag, 0, townmap_repeats, townmap_click);
    vk = input_key_pressed();
    asked = vk ? townmap_key(vk) : TOWNMAP_STAY;
    if (asked == TOWNMAP_STAY && (in->pressed & INPUT_BIT(INPUT_ATTACK))) {
        asked = townmap_click(g_point_x, g_point_y);
    }
    if (asked == TOWNMAP_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (asked == TOWNMAP_TRAVEL) {
        return SCREENS_TO_TRAVEL;
    }
    if (townmap_changed()) {
        send_screen();
    }
    return SCREENS_STAY;
}

/*
 * A map's mouse, either map's: a left press on the map starts a drag and each
 * frame moves it by the pointer's move; a right-button drag turns (on_map's and
 * turn 0 for a map that does not turn); a left button held on a button that
 * repeats acts again after a moment, then often. The click itself is the frame's.
 */
static void map_mouse(const Input *in, float dt, int (*on_map)(int, int), void (*drag)(int, int),
                      void (*turn)(int), int (*repeats)(int, int), int (*click)(int, int))
{
    int held;
    int right;

    held = (in->held & INPUT_BIT(INPUT_ATTACK)) != 0;
    right = (in->held & INPUT_BIT(INPUT_SWING)) != 0;
    if ((in->pressed & INPUT_BIT(INPUT_ATTACK)) && on_map(g_point_x, g_point_y)) {
        g_drag = 1;
        g_drag_x = g_point_x;
        g_drag_y = g_point_y;
    }
    if (turn && (in->pressed & INPUT_BIT(INPUT_SWING)) && on_map(g_point_x, g_point_y)) {
        g_turning = 1;
        g_drag_x = g_point_x;
    }
    if (g_drag && held && (g_point_x != g_drag_x || g_point_y != g_drag_y)) {
        drag(g_point_x - g_drag_x, g_point_y - g_drag_y);
        g_drag_x = g_point_x;
        g_drag_y = g_point_y;
    }
    if (g_turning && right && g_point_x != g_drag_x) {
        turn(g_point_x - g_drag_x);
        g_drag_x = g_point_x;
    }
    g_drag = g_drag && held;
    g_turning = g_turning && right;
    /* A button held: once by the click, then again and again. */
    if (held && !g_drag && repeats(g_point_x, g_point_y)) {
        g_hold += dt;
        while (g_hold >= REPEAT_AFTER + REPEAT_EVERY) {
            g_hold -= REPEAT_EVERY;
            click(g_point_x, g_point_y);
        }
    } else {
        g_hold = 0.0f;
    }
}

/* A frame of the indoor automap: its keys, clicks and the wheel; the engine draws the map each frame. */
static int dunmap_frame(const Input *in, float dt)
{
    int vk;

    if (in->wheel) {
        dunmap_wheel(in->wheel);
    }
    map_mouse(in, dt, dunmap_on_map, dunmap_drag, dunmap_turn_by, dunmap_repeats, dunmap_click);
    vk = input_key_pressed();
    if (vk && dunmap_key(vk) == DUNMAP_LEAVE) {
        return SCREENS_CLOSED;
    }
    if ((in->pressed & INPUT_BIT(INPUT_ATTACK)) && dunmap_click(g_point_x, g_point_y) == DUNMAP_LEAVE) {
        return SCREENS_CLOSED;
    }
    if (dunmap_changed()) {
        send_screen();
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
    if ((!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT)) && bank_click(g_point_x, g_point_y) == BANK_LEAVE) {
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
    if (!over && (!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT))) {
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
static int bag_frame(const Input *in, float dt)
{
    int vk;

    bag_point(g_point_x, g_point_y);
    bag_step(dt);
    if (in->wheel) {
        bag_wheel(g_point_x, g_point_y, in->wheel);
    }
    /* The Gold button's question has the keys while it is up. */
    vk = input_key_pressed();
    if (bag_asking()) {
        if (vk) {
            bag_key(vk);
        }
    } else if (in->pressed & (INPUT_BIT(INPUT_MENU) | INPUT_BIT(INPUT_INVENTORY))) {
        return SCREENS_CLOSED;
    }
    if ((!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT)) && bag_click(g_point_x, g_point_y) == BAG_LEAVE) {
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
    if (!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT)) {
        int asked;

        asked = sheet_click(g_point_x, g_point_y);
        if (asked == SHEET_LEAVE) {
            return SCREENS_CLOSED;
        }
        if (asked == SHEET_INVENTORY) {
            screens_open_bag(g_player, 0);
            return SCREENS_STAY;
        }
        if (asked == SHEET_LOGBOOK) {
            screens_open_logbook(ills_now(clock_day(), clock_minute()));
            return SCREENS_STAY;
        }
        if (asked == SHEET_SPELLBOOK) {
            return SCREENS_BOOK;
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
    if (asked == CREATE_STAY && (!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT))) {
        asked = create_click_bulk(g_point_x, g_point_y, (in->held & INPUT_BIT(INPUT_RUN)) != 0);
    }
    if (asked == CREATE_DONE) {
        return SCREENS_MADE;
    }
    /* Escape on the race map: the screen closes and the game puts the start menu back. */
    if (asked == CREATE_CANCELLED) {
        return SCREENS_CLOSED;
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
    if ((!in->reset && (in->mouse_pressed & INPUT_MOUSE_LEFT)) && rest_click(g_point_x, g_point_y) == REST_LEAVE) {
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

int screens_frame(const Input *in, float dt)
{
    int px;
    int py;
    int asked;

    input_pointer(&px, &py);
    screen_from_pointer(px, py, &g_point_x, &g_point_y);
    if (g_open == OPEN_BAG) {
        asked = bag_frame(in, dt);
    } else if (g_open == OPEN_SHEET) {
        asked = sheet_frame(in);
    } else if (g_open == OPEN_REST) {
        asked = rest_frame(in, dt);
    } else if (g_open == OPEN_TALK) {
        asked = talk_frame(in);
    } else if (g_open == OPEN_SERVICE) {
        asked = service_frame(in);
    } else if (g_open == OPEN_BANK) {
        asked = bank_frame(in);
    } else if (g_open == OPEN_COURT) {
        asked = court_frame(in, dt);
    } else if (g_open == OPEN_TRAVEL) {
        asked = travel_frame(in, dt);
    } else if (g_open == OPEN_TOWNMAP) {
        asked = townmap_frame(in, dt);
    } else if (g_open == OPEN_DUNMAP) {
        asked = dunmap_frame(in, dt);
    } else if (g_open == OPEN_MSGBOX) {
        asked = msgbox_frame(in);
    } else if (g_open == OPEN_LOGBOOK) {
        asked = logbook_frame(in);
    } else if (g_open == OPEN_SPELLBOOK) {
        asked = spellbook_frame(in);
    } else if (g_open == OPEN_MAKER) {
        asked = maker_frame(in);
    } else {
        asked = create_frame(in);
    }
    if (asked == SCREENS_CLOSED || asked == SCREENS_SERVICE || asked == SCREENS_SEEN || asked == SCREENS_COURT_OVER || asked == SCREENS_JOURNEY
        || asked == SCREENS_TO_TRAVEL || asked == SCREENS_ANSWERED || asked == SCREENS_SPELL || asked == SCREENS_BOOK) {
        screens_close();
    }
    return asked;
}

/* The player's arrow over a map, its own picture composed again only when its step changes. */
static void draw_marker(void)
{
    static unsigned char texels[MARKER_SIDE * MARKER_SIDE];
    float sx;
    float sy;
    float angle;
    int shown;
    int step;

    shown = g_open == OPEN_DUNMAP ? dunmap_marker(&sx, &sy, &angle)
                                  : (g_open == OPEN_TOWNMAP ? townmap_marker(&sx, &sy, &angle) : 0);
    if (!shown) {
        return;
    }
    step = marker_step(angle);
    if (step != g_marker_step || !g_marker_pic) {
        if (g_marker_pic) {
            pic_free(g_marker_pic);
        }
        marker_compose(texels, step);
        g_marker_pic = pic_load(texels, MARKER_SIDE, MARKER_SIDE, g_rgb, 1);
        g_marker_step = step;
    }
    /* pic_draw's top counts from the bottom, as the engine's y does. */
    pic_draw(g_marker_pic, df_iround(sx) - MARKER_DRAWN / 2, df_iround(sy) + MARKER_DRAWN / 2, MARKER_DRAWN, MARKER_DRAWN);
}

void screens_draw(void)
{
    const char *note;

    screen_draw(g_pic, 0, 0, SCREEN_W, SCREEN_H);
    draw_marker();
    if (g_open == OPEN_BAG) {
        note = bag_note();
    } else if (g_open == OPEN_SHEET) {
        note = sheet_note();
    } else if (g_open == OPEN_REST) {
        note = rest_note();
    } else if (g_open == OPEN_TALK || g_open == OPEN_SERVICE || g_open == OPEN_COURT || g_open == OPEN_TRAVEL || g_open == OPEN_TOWNMAP || g_open == OPEN_DUNMAP || g_open == OPEN_MSGBOX || g_open == OPEN_LOGBOOK) {
        note = 0;
    } else if (g_open == OPEN_SPELLBOOK) {
        note = spellbook_note();
    } else if (g_open == OPEN_MAKER) {
        note = maker_note();
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
