#ifndef OGFALL_STEER_H
#define OGFALL_STEER_H

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

#define OG_ACTS 38

enum {
    OG_FORWARD,
    OG_BACK,
    OG_TURN_LEFT,
    OG_SLIDE_LEFT,
    OG_TURN_RIGHT,
    OG_SLIDE_RIGHT,
    OG_FLOAT_UP,
    OG_FLOAT_DOWN,
    OG_JUMP,
    OG_CROUCH,
    OG_STRAFE,
    OG_RUN,
    OG_REST,
    OG_TRANSPORT,
    OG_KEY_STEAL,
    OG_KEY_GRAB,
    OG_KEY_INFO,
    OG_KEY_TALK,
    OG_ACTIVATE,
    OG_CURSOR_KEY,
    OG_LOOK_UP,
    OG_LOOK_DOWN,
    OG_LOOK_HOME,
    OG_SLOW,
    OG_LOG,
    OG_NOTE,
    OG_AUTOMAP,
    OG_TRAVEL,
    OG_CAST,
    OG_RECAST,
    OG_ABORT,
    OG_MAGIC_ITEM,
    OG_READY,
    OG_SWING_BUTTON,
    OG_SWITCH,
    OG_STATUS_KEY,
    OG_SHEET,
    OG_INVENTORY
};

/* Win32 virtual keys, numeric so the game stays free of windows.h. */
#define OG_VK_ESCAPE 0x1B
#define OG_VK_RETURN 0x0D
#define OG_VK_SPACE 0x20
#define OG_VK_PRIOR 0x21
#define OG_VK_NEXT 0x22
#define OG_VK_HOME 0x24
#define OG_VK_LEFT 0x25
#define OG_VK_UP 0x26
#define OG_VK_RIGHT 0x27
#define OG_VK_DOWN 0x28
#define OG_VK_CONTROL 0x11
#define OG_VK_MENU 0x12
#define OG_VK_LSHIFT 0xA0
#define OG_VK_RSHIFT 0xA1
#define OG_VK_F1 0x70
#define OG_VK_F2 0x71
#define OG_VK_F3 0x72
#define OG_VK_F4 0x73

/* -1 when no steer key is held. Regions 0..8 otherwise. */
int og_key_region(const unsigned char *held);

/*
 * The cell under a point in a view of vw by vh whose origin is ox, oy
 * (classic pixels, top-left). intrface_set_regions: width/3, height/3.
 */
int og_mouse_region(int x, int y, int vw, int vh, int ox, int oy);

/*
 * The region movement should use. A steer key wins. With none, the
 * mouse cell. On the bar (y >= 154) with the bar up and no steer key,
 * returns -2: hud_update returned 1 and the mouse does not steer.
 * -1 is the centre stop from a key function that found nothing; the
 * mouse still supplies a cell, so a quiet keyboard returns the cell
 * or -2.
 */
int og_steer_region(int x, int y, int vw, int vh, int ox, int oy,
                    const unsigned char *held, int bar_up);

/* The virtual key slice 1 binds for an engine INPUT_* action, or 0. */
int og_bind_vk(int engine_action);

/*
 * View mode, on the ground, key 23 held: halve, then subtract one.
 * Integer, as the decomp writes it.
 */
/* intrface_steer dispatches the pointer only in Cursor mode with left held. */
int og_control_region(int x, int y, int bar, int view_mode, int cursor_active, int buttons,
                      const unsigned char *held);
int og_view_slow(int speed);

#endif
