#include "game/dagger/play/steer.h"

#include "engine/platform/input.h"

/*
 * Held flags are classic actions. The tests are intrface_key_region's,
 * in its order: a slide key before a turn, a turn-plus-strafe before
 * a plain turn, forward-plus-turn before plain forward.
 */
int key_region(const unsigned char *held)
{
    if (!held) {
        return -1;
    }
    if (held[SLIDE_LEFT]) {
        return 6;
    }
    if (held[TURN_LEFT] && held[STRAFE]) {
        return 6;
    }
    if (held[SLIDE_RIGHT]) {
        return 8;
    }
    if (held[TURN_RIGHT] && held[STRAFE]) {
        return 8;
    }
    if (held[FORWARD] && held[TURN_RIGHT]) {
        return 2;
    }
    if (held[FORWARD] && held[TURN_LEFT]) {
        return 0;
    }
    if (held[FORWARD]) {
        return 1;
    }
    if (held[BACK]) {
        return 7;
    }
    if (held[TURN_RIGHT] && !held[STRAFE]) {
        return 5;
    }
    if (held[TURN_LEFT] && !held[STRAFE]) {
        return 3;
    }
    return -1;
}

int mouse_region(int x, int y, int vw, int vh, int ox, int oy)
{
    int x1;
    int x2;
    int y1;
    int y2;
    int region;

    if (vw < 3) {
        vw = 3;
    }
    if (vh < 3) {
        vh = 3;
    }
    y1 = vh / 3 + oy;
    y2 = (vh / 3) * 2 + oy;
    x1 = vw / 3 + ox;
    x2 = (vw / 3) * 2 + ox;
    if (y1 > y) {
        region = 0;
    } else if (y < y2) {
        region = 3;
    } else {
        region = 6;
    }
    if (x1 > x) {
        return region;
    }
    if (x < x2) {
        return region + 1;
    }
    return region + 2;
}

int steer_region(int x, int y, int vw, int vh, int ox, int oy,
                    const unsigned char *held, int bar_up)
{
    int key;

    key = key_region(held);
    if (bar_up && y >= 154 && key < 0) {
        return -2;
    }
    if (key >= 0) {
        return key;
    }
    return mouse_region(x, y, vw, vh, ox, oy);
}

/*
 * Engine action -> classic key. Spare engine actions are F1..F4,
 * because the list is shorter than key_map. F5, F6 and S are not
 * bound; the sheet, the inventory and the off hand are HUD clicks.
 * W is the travel map, so it cannot also be forward.
 */
int bind_vk(int engine_action)
{
    switch (engine_action) {
    case INPUT_FORWARD:
        return KEY_UP;
    case INPUT_BACK:
        return KEY_DOWN;
    case INPUT_TURN_LEFT:
        return KEY_LEFT;
    case INPUT_TURN_RIGHT:
        return KEY_RIGHT;
    case INPUT_LEFT:
        return KEY_LSHIFT;
    case INPUT_RIGHT:
        return KEY_RSHIFT;
    case INPUT_JUMP:
        return 'J';
    case INPUT_CROUCH:
        return 'D';
    case INPUT_RUN:
        return 'P';
    case INPUT_LOOK_UP:
        return KEY_PRIOR;
    case INPUT_LOOK_DOWN:
        return KEY_NEXT;
    case INPUT_USE:
        return KEY_SPACE;
    case INPUT_CURSOR:
        return KEY_RETURN;
    case INPUT_FREELOOK:
        return KEY_MENU;
    case INPUT_LOOK_CENTER:
        return KEY_HOME;
    case INPUT_ATTACK:
        return 'A';
    case INPUT_RECAST:
        return KEY_CONTROL;
    case INPUT_MENU:
        return KEY_ESCAPE;
    case INPUT_TRAVEL_MAP:
        return 'W';
    case INPUT_MAP:
        return 'M';
    case INPUT_REST:
        return 'R';
    case INPUT_TRANSPORT:
        return 'T';
    case INPUT_SPELLBOOK:
        return KEY_F1;
    case INPUT_SWAP_HANDS:
        return KEY_F2;
    case INPUT_CHARACTER:
        return KEY_F3;
    case INPUT_INVENTORY:
        return KEY_F4;
    default:
        return 0;
    }
}

int view_slow(int speed)
{
    speed >>= 1;
    speed--;
    return speed;
}

int control_region(int x, int y, int bar, int view_mode, int cursor_active, int buttons,
                      const unsigned char *held)
{
    int key;
    key = key_region(held);
    if (key >= 0) {
        return key;
    }
    if (view_mode || cursor_active || !(buttons & 1) || (bar && y >= 154)) {
        return 4;
    }
    return mouse_region(x, y, 320, bar ? 154 : 200, 0, 0);
}
