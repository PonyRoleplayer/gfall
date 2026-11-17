#ifndef GFALL_STEER_H
#define GFALL_STEER_H

/*
 * intrface_key_region and intrface_region_at. The view is three by
 * three. The bar is not part of the grid; hud_update drops the mouse
 * region when the pointer is on the bar and no steer key is held.
 *
 * Classic actions are key_map's numbers (docs/play.md, default_key_map
 * in FALL.EXE). The engine's own actions are a different list. The
 * virtual-key numbers below are Win32's, so this file does not include
 * windows.h. Forward is Up, not W.
 */

#define ACTS 38

enum {
    FORWARD,
    BACK,
    TURN_LEFT,
    SLIDE_LEFT,
    TURN_RIGHT,
    SLIDE_RIGHT,
    FLOAT_UP,
    FLOAT_DOWN,
    JUMP,
    CROUCH,
    STRAFE,
    RUN,
    REST,
    TRANSPORT,
    KEY_STEAL,
    KEY_GRAB,
    KEY_INFO,
    KEY_TALK,
    ACTIVATE,
    CURSOR_KEY,
    LOOK_UP,
    LOOK_DOWN,
    LOOK_HOME,
    SLOW,
    LOG,
    NOTE,
    AUTOMAP,
    TRAVEL,
    CAST,
    RECAST,
    ABORT,
    MAGIC_ITEM,
    READY,
    SWING_BUTTON,
    SWITCH,
    STATUS_KEY,
    SHEET,
    INVENTORY
};

/* Win32 virtual keys, numeric so the game stays free of windows.h. */
#define KEY_ESCAPE 0x1B
#define KEY_RETURN 0x0D
#define KEY_SPACE 0x20
#define KEY_PRIOR 0x21
#define KEY_NEXT 0x22
#define KEY_HOME 0x24
#define KEY_LEFT 0x25
#define KEY_UP 0x26
#define KEY_RIGHT 0x27
#define KEY_DOWN 0x28
#define KEY_CONTROL 0x11
#define KEY_MENU 0x12
#define KEY_LSHIFT 0xA0
#define KEY_RSHIFT 0xA1
#define KEY_F1 0x70
#define KEY_F2 0x71
#define KEY_F3 0x72
#define KEY_F4 0x73

/* -1 when no steer key is held. Regions 0..8 otherwise. */
int key_region(const unsigned char *held);

/*
 * The cell under a point in a view of vw by vh whose origin is ox, oy
 * (classic pixels, top-left). intrface_set_regions: width/3, height/3.
 */
int mouse_region(int x, int y, int vw, int vh, int ox, int oy);

/*
 * The region movement should use. A steer key wins. With none, the
 * mouse cell. On the bar (y >= 154) with the bar up and no steer key,
 * returns -2: hud_update returned 1 and the mouse does not steer.
 * -1 is the centre stop from a key function that found nothing; the
 * mouse still supplies a cell, so a quiet keyboard returns the cell
 * or -2.
 */
int steer_region(int x, int y, int vw, int vh, int ox, int oy,
                    const unsigned char *held, int bar_up);

/* The virtual key slice 1 binds for an engine INPUT_* action, or 0. */
int bind_vk(int engine_action);

/*
 * View mode, on the ground, key 23 held: halve, then subtract one.
 * Integer, as the decomp writes it.
 */
/* intrface_steer dispatches the pointer only in Cursor mode with left held. */
int control_region(int x, int y, int bar, int view_mode, int cursor_active, int buttons,
                      const unsigned char *held);
int view_slow(int speed);

#endif
