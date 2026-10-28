#ifndef DF_INPUT_H
#define DF_INPUT_H

/* One frame. move_f is W minus S, move_r is D minus A, move_y is Q minus E.
   look_dx positive is look right (mouse, or right arrow). look_dy positive
   is look down (mouse, or down arrow). Arrows add a fixed step per frame. */

/*
 * A game's actions (docs/input.md): one bit each in Input's held,
 * pressed (went down this frame) and released (went up this frame).
 * Each is a key or a mouse button, rebound with input_bind; the
 * defaults are attack the left button, use E, run Shift, jump Space,
 * the character C, the inventory I, the map M, rest R, and the menu
 * Escape (with input_set_escape_quits(0); else Escape quits).
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
    INPUT_ACTIONS
};
#define INPUT_BIT(action) (1u << (action))

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
} Input;

/* Action (INPUT_*) to a Windows virtual key (VK_*, a letter's capital, VK_LBUTTON for the left button). */
void input_bind(int action, int vk);
/*
 * 1 (default): Escape quits (input_read's quit), as the viewers want.
 * 0: Escape is the INPUT_MENU action and never quits: a game's menu.
 */
void input_set_escape_quits(int on);

/*
 * Pointer mode, for a game's menus: 1 leaves the cursor where the mouse
 * puts it (still hidden and held to the window; the game draws its own
 * arrow) and nothing is looked, by mouse or arrows; 0 (default) is mouse
 * look, recentred, starting with no jump. A click is an action's press,
 * INPUT_ATTACK's (the left button). Escape is unchanged.
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

/* Client coordinates. The window calls this from WM_MOUSEMOVE. */
void input_on_mouse(int x, int y);
void input_request_quit(void);
void input_clear_quit(void);

#endif
