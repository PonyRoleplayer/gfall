#include "game/dagger/play/menu.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/keys.h"
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
    DO_EXIT,
    DO_SAVE,
    DO_CONTINUE,
    DO_CONTROLS,
    DO_DEFAULTS
};

/* DFU's StartWindow: on PICK03I0.IMG. */
static const Button g_main_buttons[] = {
    { 72, 45, 147, 15, DO_LOAD },
    { 72, 99, 147, 15, DO_START },
    { 125, 145, 41, 15, DO_EXIT }
};

/*
 * DFU's PauseOptionsWindow, from OPTN00I0.IMG's top left. Its full
 * screen, head bobbing, sound, music and detail are settings: not yet.
 */
static const Button g_options_buttons[] = {
    { 4, 4, 45, 16, DO_SAVE },
    { 52, 4, 46, 16, DO_LOAD },
    { 101, 4, 45, 16, DO_EXIT },
    { 5, 60, 70, 17, DO_CONTROLS },
    { 76, 60, 70, 17, DO_CONTINUE }
};

/*
 * DFU's ControlsWindow on CNFG00I0.IMG: classic's slot for each of our
 * actions (47 x 7, its column's left and 11 rows a step from its top),
 * and the buttons along the bottom. Classic's other slots (turning,
 * crouching, spells, the log...) stay empty until their actions come.
 */
typedef struct Slot {
    df_u8 action;
    df_u16 x;
    df_u16 y;
} Slot;

static const Slot g_slots[] = {
    { INPUT_FORWARD, 57, 13 },
    { INPUT_BACK, 57, 24 },
    { INPUT_LEFT, 57, 46 },
    { INPUT_RIGHT, 57, 68 },
    { INPUT_JUMP, 164, 35 },
    { INPUT_RUN, 164, 68 },
    { INPUT_REST, 270, 13 },
    { INPUT_ATTACK, 102, 136 },
    { INPUT_CHARACTER, 102, 170 },
    { INPUT_INVENTORY, 102, 181 },
    { INPUT_USE, 270, 80 },
    { INPUT_MAP, 270, 181 }
};
#define SLOTS ((int)(sizeof(g_slots) / sizeof(g_slots[0])))
#define SLOT_W 47
#define SLOT_H 7

static const Button g_controls_buttons[] = {
    { 160, 190, 80, 10, DO_DEFAULTS },
    { 240, 190, 80, 10, DO_CONTINUE }
};

static int g_open;
/* The action waiting for a key on the controls screen, or -1; whether a binding changed since it opened. */
static int g_waiting = -1;
static int g_changed;
/* Exit clicked once in the options. */
static int g_confirm;
static int g_pic;
static int g_pic_x;
static int g_pic_y;
static int g_pic_w;
static int g_pic_h;
static int g_arrow;
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

    if (!img_load_raw("ARROW.RAW", 10, 10, &arrow) || !img_art_palette(rgb)) {
        return 0;
    }
    /* Index 0 round the arrow is clear. */
    g_arrow = pic_load(arrow.pixels, arrow.width, arrow.height, rgb, 1);
    return g_arrow != 0;
}

void menu_shutdown(void)
{
    menu_open(MENU_NONE);
    if (g_arrow) {
        pic_free(g_arrow);
        g_arrow = 0;
    }
}

void menu_open(int which)
{
    static const char *const files[] = { 0, "TITL00I0.IMG", "PICK03I0.IMG", "OPTN00I0.IMG", "CNFG00I0.IMG" };

    /* Leaving the controls with a binding changed: kept for the next start. */
    if (g_open == MENU_CONTROLS && which != MENU_CONTROLS && g_changed) {
        keys_save();
    }
    g_open = which;
    g_note = 0;
    g_waiting = -1;
    g_changed = 0;
    g_confirm = 0;
    if (which == MENU_CONTROLS) {
        /* Its first call notes what is down already: the click that opened the screen is not a key. */
        input_key_pressed();
    }
    show(which > MENU_NONE && which <= MENU_CONTROLS ? files[which] : 0);
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

/* The controls screen: a click on a slot waits for a key; the key, or Escape to leave it be. */
static int controls_frame(const Input *in, int click)
{
    int vk;
    int i;

    vk = input_key_pressed();
    if (g_waiting >= 0) {
        if (!vk) {
            return MENU_STAY;
        }
        /* Escape (0x1B) cancels. A key another action has goes to them both swapped. */
        if (vk != 0x1B) {
            int was;
            int other;

            was = input_binding(g_waiting);
            other = -1;
            for (i = 0; i < SLOTS; i++) {
                if (g_slots[i].action != g_waiting && input_binding(g_slots[i].action) == vk) {
                    other = g_slots[i].action;
                }
            }
            if (input_bind(g_waiting, vk)) {
                if (other >= 0 && was) {
                    input_bind(other, was);
                }
                g_changed = 1;
                g_note = 0;
            } else {
                g_note = "That key is the menu's or the console's.";
            }
        } else {
            g_note = 0;
        }
        g_waiting = -1;
        return MENU_STAY;
    }
    if (in->pressed & INPUT_BIT(INPUT_MENU)) {
        menu_open(MENU_OPTIONS);
        return MENU_STAY;
    }
    if (!click) {
        return MENU_STAY;
    }
    for (i = 0; i < SLOTS; i++) {
        if (screen_in(g_point_x, g_point_y, g_slots[i].x, g_slots[i].y, SLOT_W, SLOT_H)) {
            g_waiting = g_slots[i].action;
            g_note = "Press a key for it, or Escape to leave it.";
            return MENU_STAY;
        }
    }
    switch (hit(g_controls_buttons, 2, 0, 0)) {
    case DO_DEFAULTS:
        keys_defaults();
        g_changed = 1;
        break;
    case DO_CONTINUE:
        menu_open(MENU_OPTIONS);
        break;
    default:
        break;
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
    click = (in->pressed & INPUT_BIT(INPUT_ATTACK)) != 0;
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
            g_note = "There are no saved games yet.";
        }
        return MENU_STAY;
    case MENU_OPTIONS:
        if (in->pressed & INPUT_BIT(INPUT_MENU)) {
            menu_open(MENU_NONE);
            return MENU_CLOSED;
        }
        what = click ? hit(g_options_buttons, 5, g_pic_x, g_pic_y) : -1;
        if (what == DO_CONTINUE) {
            menu_open(MENU_NONE);
            return MENU_CLOSED;
        }
        /* In play, Exit asks once: a stray click does not end the game. */
        if (what == DO_EXIT) {
            if (g_confirm) {
                return MENU_QUIT;
            }
            g_confirm = 1;
            g_note = "Click Exit again to leave the game.";
            return MENU_STAY;
        }
        /* Another button clicked takes the question back; a frame with no click keeps it. */
        if (what >= 0) {
            g_confirm = 0;
        }
        if (what == DO_SAVE || what == DO_LOAD) {
            g_note = "Saving and loading come later.";
        } else if (what == DO_CONTROLS) {
            menu_open(MENU_CONTROLS);
        }
        return MENU_STAY;
    case MENU_CONTROLS:
        return controls_frame(in, click);
    default:
        return MENU_STAY;
    }
}

void menu_draw(void)
{
    if (g_open == MENU_NONE) {
        return;
    }
    screen_draw(g_pic, g_pic_x, g_pic_y, g_pic_w, g_pic_h);
    if (g_open == MENU_CONTROLS) {
        int i;

        for (i = 0; i < SLOTS; i++) {
            char name[24];

            if (g_slots[i].action == g_waiting) {
                screen_text(g_slots[i].x + 1, g_slots[i].y, "...");
                continue;
            }
            input_key_name(input_binding(g_slots[i].action), name, (int)sizeof(name));
            screen_text(g_slots[i].x + 1, g_slots[i].y, name);
        }
    }
    if (g_note) {
        ui_string((DF_VIEW_W - ui_string_width(g_note)) / 2, ui_line_height() + 8, g_note);
    }
    if (g_open != MENU_TITLE) {
        screen_draw(g_arrow, g_point_x, g_point_y, 10, 10);
    }
}
