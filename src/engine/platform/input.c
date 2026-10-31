/*
 * One snapshot per frame. Mouse deltas are from the client centre.
 * The warp that recentres the cursor is ignored, so look does not drift.
 * WASD is yaw-relative later; this file only reports the keys.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/platform/input.h"
#include "engine/core/df.h"
#include "engine/ui/console.h"

static HWND g_hwnd;
static int g_cx;
static int g_cy;
static int g_look_dx;
static int g_look_dy;
static int g_ignore;
static int g_active;
static int g_quit;
/* The actions' keys (input_bind), what was held last read, and whether Escape quits. */
static int g_vk[INPUT_ACTIONS] = {
    VK_LBUTTON, 'E', VK_SHIFT, VK_SPACE, 'C', 'I', 'M', 'R', VK_ESCAPE,
    'W', 'S', 'A', 'D', 'H'
};
/* The scan code of the console's key, the one left of 1 (console.c's CON_SCAN). */
#define CONSOLE_SCAN 0x29
static unsigned int g_held;
static int g_escape_quits = 1;
/*
 * Pointer mode (input_set_pointer): the cursor stays where the mouse
 * puts it, in client pixels, and nothing is looked; the client's size
 * turns it into the screen's 640 x 480.
 */
static int g_pointer;
static int g_px;
static int g_py;
static int g_cw = DF_VIEW_W;
static int g_ch = DF_VIEW_H;

static int key_down(int vk);

/* The console's key as a virtual key on this keyboard's layout. */
static int console_vk(void)
{
    return (int)MapVirtualKeyA(CONSOLE_SCAN, 1);
}

int input_bind(int action, int vk)
{
    if (action < 0 || action >= INPUT_ACTIONS || vk < 1 || vk > 254) {
        return 0;
    }
    if ((vk == VK_ESCAPE && action != INPUT_MENU) || vk == console_vk()) {
        return 0;
    }
    g_vk[action] = vk;
    return 1;
}

int input_binding(int action)
{
    return action >= 0 && action < INPUT_ACTIONS ? g_vk[action] : 0;
}

void input_set_escape_quits(int on)
{
    g_escape_quits = on ? 1 : 0;
}

int input_escape_quits(void)
{
    return g_escape_quits;
}

/*
 * Downs and ups the window saw since the last read (input_note_key).
 * A click that goes down and up between reads never shows in the poll;
 * these still make one press and one release. g_tapped is that read's
 * downs, for input_key_pressed, which runs after input_read.
 */
static unsigned char g_note_down[256];
static unsigned char g_note_up[256];
static unsigned char g_tapped[256];

void input_note_key(int vk, int down)
{
    if (vk < 1 || vk > 254) {
        return;
    }
    if (down) {
        g_note_down[vk] = 1;
    } else {
        g_note_up[vk] = 1;
    }
}

int input_grabs_cursor(void)
{
    return g_active;
}

static void notes_clear(void)
{
    int i;

    for (i = 0; i < 256; i++) {
        g_note_down[i] = 0;
        g_note_up[i] = 0;
        g_tapped[i] = 0;
    }
}

static int key_down(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
}

static void center_cursor(void)
{
    POINT p;

    p.x = g_cx;
    p.y = g_cy;
    ClientToScreen(g_hwnd, &p);
    g_ignore = 1;
    SetCursorPos(p.x, p.y);
    g_ignore = 0;
}

/* Windows' units a wheel notch. A finer wheel sends less: what is short of a notch waits in g_wheel. */
#define WHEEL_NOTCH 120
static int g_wheel;

void input_on_wheel(int delta)
{
    g_wheel += delta;
}

void input_on_mouse(int x, int y)
{
    int dx;
    int dy;

    if (g_pointer) {
        if (g_active && !g_ignore) {
            g_px = x;
            g_py = y;
        }
        return;
    }

    if (!g_active || g_ignore || console_open()) {
        if (g_active && !g_ignore && console_open()) {
            dx = x - g_cx;
            dy = y - g_cy;
            if (dx || dy) {
                center_cursor();
            }
        }
        return;
    }
    dx = x - g_cx;
    dy = y - g_cy;
    if (dx == 0 && dy == 0) {
        return;
    }
    g_look_dx += dx;
    g_look_dy += dy;
    center_cursor();
}

void input_request_quit(void)
{
    g_quit = 1;
}

void input_clear_quit(void)
{
    g_quit = 0;
}

void input_look(int *dx, int *dy)
{
    console_pump();
    if (console_open()) {
        g_look_dx = 0;
        g_look_dy = 0;
    }
    if (dx) {
        *dx = g_look_dx;
    }
    if (dy) {
        *dy = g_look_dy;
    }
    g_look_dx = 0;
    g_look_dy = 0;
}

int input_quit_requested(void)
{
    return g_quit;
}

void input_begin(void *hwnd)
{
    RECT rc;
    RECT clip;
    POINT origin;

    g_hwnd = (HWND)hwnd;
    g_look_dx = 0;
    g_look_dy = 0;
    g_quit = 0;
    g_ignore = 0;
    GetClientRect(g_hwnd, &rc);
    g_cx = (rc.right - rc.left) / 2;
    g_cy = (rc.bottom - rc.top) / 2;
    g_cw = rc.right - rc.left > 0 ? rc.right - rc.left : DF_VIEW_W;
    g_ch = rc.bottom - rc.top > 0 ? rc.bottom - rc.top : DF_VIEW_H;
    g_px = g_cx;
    g_py = g_cy;
    g_active = 1;
    center_cursor();
    g_look_dx = 0;
    g_look_dy = 0;
    while (ShowCursor(FALSE) >= 0) {
    }
    origin.x = 0;
    origin.y = 0;
    ClientToScreen(g_hwnd, &origin);
    clip.left = origin.x;
    clip.top = origin.y;
    clip.right = origin.x + (rc.right - rc.left);
    clip.bottom = origin.y + (rc.bottom - rc.top);
    ClipCursor(&clip);
    SetCapture(g_hwnd);
}

void input_end(void)
{
    g_active = 0;
    g_ignore = 1;
    ReleaseCapture();
    ClipCursor(0);
    while (ShowCursor(TRUE) < 0) {
    }
}

/* Mouse deltas are pixels. Arrows have no pixel count, so each held
   arrow adds this many pixels per frame. At 60 Hz that is about 80
   degrees per second through LOOK_SENS. */
#define KEY_LOOK 8

/* Frames read, so input_key_pressed knows whether it was called last frame. */
static unsigned int g_frame;

void input_read(Input *out)
{
    int escape;
    unsigned int held;

    g_frame++;
    console_pump();
    if (console_open()) {
        g_look_dx = 0;
        g_look_dy = 0;
        out->move_f = 0;
        out->move_r = 0;
        out->move_y = 0;
        out->look_dx = 0;
        out->look_dy = 0;
        out->quit = g_quit;
        /* The console has the keys: nothing is held, and what was is let go. */
        out->held = 0;
        out->pressed = 0;
        out->released = g_held;
        g_held = 0;
        /* Nor the wheel: rolled over the console, it does not scroll a list behind it afterwards. */
        out->wheel = 0;
        g_wheel = 0;
        /* Nor a click on it: that press is not a swing once the console closes. */
        notes_clear();
        return;
    }
    /* Whole notches go out, either way; C's division leaves the part of one, with its sign. */
    out->wheel = g_wheel / WHEEL_NOTCH;
    g_wheel -= out->wheel * WHEEL_NOTCH;
    /* The walk from its actions, so a game's bindings move it. */
    out->move_f = key_down(g_vk[INPUT_FORWARD]) - key_down(g_vk[INPUT_BACK]);
    out->move_r = key_down(g_vk[INPUT_RIGHT]) - key_down(g_vk[INPUT_LEFT]);
    out->move_y = key_down('Q') - key_down('E');
    out->look_dx = g_look_dx + (key_down(VK_RIGHT) - key_down(VK_LEFT)) * KEY_LOOK;
    out->look_dy = g_look_dy + (key_down(VK_DOWN) - key_down(VK_UP)) * KEY_LOOK;
    g_look_dx = 0;
    g_look_dy = 0;
    /* A menu's pointer looks nowhere: not the mouse, not the arrows. */
    if (g_pointer) {
        out->look_dx = 0;
        out->look_dy = 0;
    }
    /*
     * An Escape that just closed the console is still down: it is not a
     * quit and not the menu. A tap between reads is in the notes, so it
     * still counts once when the poll missed it.
     */
    escape = console_take_escape();
    if (escape) {
        g_note_down[VK_ESCAPE] = 0;
        g_note_up[VK_ESCAPE] = 0;
    }
    if (!escape && g_escape_quits && (key_down(VK_ESCAPE) || g_note_down[VK_ESCAPE])) {
        g_quit = 1;
    }
    out->quit = g_quit;
    held = 0;
    {
        unsigned int pressed;
        unsigned int released;
        int a;
        int i;

        pressed = 0;
        released = 0;
        for (i = 1; i < 255; i++) {
            g_tapped[i] = g_note_down[i];
        }
        for (a = 0; a < INPUT_ACTIONS; a++) {
            int vk;
            int was;
            int saw_down;
            int saw_up;
            int polled;
            int press;
            int rel;
            int now;

            vk = g_vk[a];
            was = (g_held & INPUT_BIT(a)) ? 1 : 0;
            /* Escape quits, or the console took it: it is not the menu action. */
            if (vk == VK_ESCAPE && (escape || g_escape_quits)) {
                saw_down = 0;
                saw_up = 0;
                polled = 0;
            } else {
                saw_down = g_note_down[vk];
                saw_up = g_note_up[vk];
                polled = key_down(vk);
            }
            input_fold_edges(was, saw_down, saw_up, polled, &press, &rel, &now);
            if (press) {
                pressed |= INPUT_BIT(a);
            }
            if (rel) {
                released |= INPUT_BIT(a);
            }
            if (now) {
                held |= INPUT_BIT(a);
            }
        }
        out->held = held;
        out->pressed = pressed;
        out->released = released;
        for (i = 0; i < 256; i++) {
            g_note_down[i] = 0;
            g_note_up[i] = 0;
        }
    }
    g_held = held;
}

void input_set_pointer(int on)
{
    on = on ? 1 : 0;
    if (on == g_pointer) {
        return;
    }
    g_pointer = on;
    g_look_dx = 0;
    g_look_dy = 0;
    if (!g_active) {
        return;
    }
    /* Either way from the middle: the pointer starts there, and look starts with no jump. */
    center_cursor();
    g_px = g_cx;
    g_py = g_cy;
}

int input_pointer_on(void)
{
    return g_pointer;
}

void input_pointer(int *x, int *y)
{
    int sx;
    int sy;

    /* Client pixels, row 0 at the top, to the screen's, from the lower left (ui_string's). */
    sx = g_px * DF_VIEW_W / g_cw;
    sy = DF_VIEW_H - 1 - g_py * DF_VIEW_H / g_ch;
    if (sx < 0) {
        sx = 0;
    }
    if (sx > DF_VIEW_W - 1) {
        sx = DF_VIEW_W - 1;
    }
    if (sy < 0) {
        sy = 0;
    }
    if (sy > DF_VIEW_H - 1) {
        sy = DF_VIEW_H - 1;
    }
    if (x) {
        *x = sx;
    }
    if (y) {
        *y = sy;
    }
}

/*
 * The bind screen's scan: every virtual key but the left/right copies of
 * Shift, Ctrl and Alt (Windows 98 reports the plain ones) against what
 * was down at the last call. 254 GetAsyncKeyState a frame, only while a
 * screen waits for a key.
 */
static df_u8 g_down[256];
static unsigned int g_scan_frame;
static int g_scan_primed;

int input_key_pressed(void)
{
    int vk;
    int found;
    int fresh;
    int con;

    fresh = !g_scan_primed || g_frame - g_scan_frame > 1u;
    g_scan_primed = 1;
    g_scan_frame = g_frame;
    con = console_vk();
    found = 0;
    for (vk = 1; vk < 255; vk++) {
        int down;

        if (vk >= VK_LSHIFT && vk <= VK_RMENU) {
            continue;
        }
        down = key_down(vk) || g_tapped[vk];
        if (down && !g_down[vk] && !found && !fresh && vk != con) {
            found = vk;
        }
        /* The poll, not the tap: a click already up is not stuck down. */
        g_down[vk] = (df_u8)(key_down(vk) ? 1 : 0);
        g_tapped[vk] = 0;
    }
    if (console_open()) {
        return 0;
    }
    return found;
}
