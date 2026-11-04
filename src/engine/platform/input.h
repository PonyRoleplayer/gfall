#ifndef DF_INPUT_H
#define DF_INPUT_H

/* One frame. move_f is forward minus back (W, S), move_r right minus left
   (D, A), as bound (INPUT_FORWARD..INPUT_RIGHT); move_y is Q minus E.
   look_dx positive is look right (mouse, or right arrow). look_dy positive
   is look down (mouse, or down arrow). Arrows add a fixed step per frame. */

/*
 * A game's actions (docs/input.md): one bit each in Input's held,
 * pressed (went down this frame) and released (went up this frame).
 * Each is a key or a mouse button, rebound with input_bind; the
 * defaults are attack the left button, use E, run Shift, jump Space,
 * the character C, the inventory I, the map M, rest R, the menu
 * Escape (with input_set_escape_quits(0); else Escape quits), and the
 * walk forward W, back S, left A and right D, which move_f and move_r
 * are made of, swapping the hands H, and the free HUD cursor Tab.
 * New actions go at the end: a game saves them by number.
 */
enum {
    INPUT_ATTACK,
    INPUT_USE,
    INPUT_RUN,
    INPUT_JUMP,
    INPUT_CHARACTER,
    INPUT_INVENTORY,
    INPUT_MAP,
    INPUT_REST,
    INPUT_MENU,
    INPUT_FORWARD,
    INPUT_BACK,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_SWAP_HANDS,
    /*
     * A request to toggle a game's free HUD cursor, default Tab. Reported
     * like every other action: the game decides whether to change pointer
     * mode (input_set_pointer), and whether its world should pause. Merely
     * pressing this action changes neither, so viewers and menus keep
     * their own behavior. Append only: older bindings name actions by id.
     */
    INPUT_CURSOR,
    /*
     * Classic controls, reported even with a free pointer. These are
     * requests only: the game owns gesture recognition, turning speed,
     * pitch, recentering and whether view and movement headings differ.
     * INPUT_SWING is separate from legacy INPUT_ATTACK so either can be
     * rebound without changing a physical HUD click (mouse_pressed).
     */
    INPUT_SWING,          /* Right Button */
    INPUT_TURN_LEFT,      /* Left */
    INPUT_TURN_RIGHT,     /* Right */
    INPUT_LOOK_UP,        /* Page Up */
    INPUT_LOOK_DOWN,      /* Page Down */
    INPUT_LOOK_CENTER,    /* Home */
    INPUT_FREELOOK,       /* Alt */
    /*
     * The travel map, default V as Daggerfall Unity's, apart from
     * INPUT_MAP (M), which a game may give to its automap. Reported
     * only: the game decides what opens.
     */
    INPUT_TRAVEL_MAP,
    /*
     * Mount or dismount the horse or cart, default T as Daggerfall
     * Unity's: the HUD's transport button. Reported only: the game
     * decides what happens.
     */
    INPUT_TRANSPORT,
    INPUT_ACTIONS
};
#define INPUT_BIT(action) (1u << (action))

/* Physical buttons, independent of action bindings and Windows' swap setting. */
#define INPUT_MOUSE_LEFT 1u
#define INPUT_MOUSE_RIGHT 2u

typedef struct Input {
    int move_f;
    int move_r;
    int move_y;
    int look_dx;
    int look_dy;
    int quit;
    /* The actions, by INPUT_BIT. With the console open none is held, and what was is released. */
    unsigned int held;
    unsigned int pressed;
    unsigned int released;
    /* The mouse wheel's notches since the last read: above 0 rolled away from the player, below 0 toward. */
    int wheel;
    /*
     * Physical button snapshot: INPUT_MOUSE_LEFT / INPUT_MOUSE_RIGHT.
     * A HUD uses these, so rebinding an attack or swing never rebinds a
     * menu click. The same edge rule as the actions (input_fold_edges):
     * a click between reads is one press and one release. Polled once
     * per distinct key per input_read, shared with the action snapshot.
     * The console or loss of focus releases what was held, once.
     */
    unsigned int mouse_held;
    unsigned int mouse_pressed;
    unsigned int mouse_released;
    /*
     * Motion since the previous read, right/down positive, scaled to
     * 640x480 logical pixels. Unlike look_dx/dy, there are no keyboard
     * steps and pointer mode does not suppress it or recenter the mouse.
     * Fractional logical pixels carry to the next read. A console,
     * focus/capture transition or pointer toggle flushes pending motion
     * and its fraction: neither a cursor warp nor a stale drag leaks
     * into the next gesture. No allocation or floating point is used.
     */
    int mouse_dx;
    int mouse_dy;
    /*
     * 1 when an input discontinuity happened since the previous read:
     * begin/end, explicit mouse reset, focus/capture change, pointer
     * toggle, or entering/leaving console/focus suppression. Multiple
     * boundaries coalesce. Written and consumed by every input_read,
     * even a suppressed read; otherwise 0. Own cursor warps do not set
     * it. The first read after input_begin is always marked.
     *
     * Check this BEFORE interpreting presses/releases: a focus-loss
     * release must cancel a gesture, not loose a bow. The engine keeps
     * those release bits unchanged; the game cancels its own controller
     * and weapon state, and decides when a new gesture may begin.
     */
    int reset;
} Input;

/*
 * Action (INPUT_*) to a Windows virtual key (VK_*, a letter's capital,
 * VK_LBUTTON for the left button). 0, and nothing changed, for Escape
 * on any action but INPUT_MENU, or the console's key (the one left of
 * 1): both belong to the console and the menu.
 */
int input_bind(int action, int vk);
/* The key an action is bound to; 0 for no such action. */
int input_binding(int action);
/*
 * For a screen that binds keys ("press a key"): the virtual key of a key
 * or mouse button that went down since the last call, the lowest if
 * several, 0 for none. Call it every frame while the screen waits: the
 * first call, or one after a frame without a call, only notes what is
 * already down. Never the console's key; 0 while the console is open.
 */
int input_key_pressed(void);
/* A key's name for display, "W", "Space", "Left Button", "Shift", "Escape", in dst (cap bytes). */
void input_key_name(int vk, char *dst, int cap);
/*
 * 1 (default): Escape quits (input_read's quit), as the viewers want.
 * 0: Escape is the INPUT_MENU action and never quits: a game's menu.
 */
void input_set_escape_quits(int on);
/* The setting: the window's own Escape (win32.c) quits only while it is 1. */
int input_escape_quits(void);

/*
 * Pointer mode, for a game's menus or HUD: 1 leaves the cursor where the
 * mouse puts it (still hidden and held to the window; the game draws its
 * own arrow) and nothing is looked, by mouse or arrows; 0 (default) is mouse
 * look, recentred, starting with no jump. Physical clicks and gestures
 * are Input.mouse_*, independent of action bindings. Escape is unchanged.
 * Pointer mode does not pause: that is the game's separate gfx_set_pause.
 */
void input_set_pointer(int on);
int input_pointer_on(void);
/* The pointer in screen pixels, ui_string's frame: from the lower left, 0..639 and 0..479. */
void input_pointer(int *x, int *y);
void input_begin(void *hwnd);
void input_end(void);
void input_read(Input *out);

/* Mouse deltas since the last take. Does not read the keyboard.
   LEVELFALL turns with its own arrow keys and does not call input_read. */
void input_look(int *dx, int *dy);
int input_quit_requested(void);

/*
 * Edges of one key since the last read (docs/input.md). was: it was
 * down then. saw_down and saw_up: the window saw it go down or up.
 * polled: it is down now. A click that finished between reads is one
 * press and one release. A key already held does not press again.
 * *held is down now, including a down the poll has not caught yet.
 */
void input_fold_edges(int was, int saw_down, int saw_up, int polled, int *press, int *release, int *held);

/* Client coordinates. The window calls this from WM_MOUSEMOVE. */
void input_on_mouse(int x, int y);
/* A key or mouse button went down (1) or up (0). The window calls this. */
void input_note_key(int vk, int down);
/* 1 while input_begin has the cursor: the window draws none over the client. */
int input_grabs_cursor(void);
/*
 * Window notifications. A focus change also clears look and suppresses
 * snapshots until focus returns. Mouse reset clears only raw motion
 * and its pointer baseline, and marks the next Input.reset (not a
 * button release's legitimate edges).
 * win_pump resets on console-consumed keys as well, including a toggle
 * opened and closed entirely between two input_read calls.
 */
void input_on_focus(int on);
void input_mouse_reset(void);
/* 1 between input_begin/end while focused: the window keeps session capture
   across button releases. A GUI viewer without a session keeps its drag capture. */
int input_mouse_capture(void);
/* The wheel's movement from WM_MOUSEWHEEL, in Windows' units: 120 a notch. The window calls this. */
void input_on_wheel(int delta);
void input_request_quit(void);
void input_clear_quit(void);

#endif
