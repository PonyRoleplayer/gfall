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
    VK_LBUTTON, 'E', VK_SHIFT, VK_SPACE, 'C', 'I', 'M', 'R', VK_ESCAPE
};
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

void input_bind(int action, int vk)
{
    if (action >= 0 && action < INPUT_ACTIONS) {
        g_vk[action] = vk;
    }
}

void input_set_escape_quits(int on)
{
    g_escape_quits = on ? 1 : 0;
}

/* The actions held now; escape is whether Escape counts (not the console's own). */
static unsigned int actions_held(int escape)
{
    unsigned int held;
    int a;

    held = 0;
    for (a = 0; a < INPUT_ACTIONS; a++) {
        int down;

        down = g_vk[a] == VK_ESCAPE ? escape : key_down(g_vk[a]);
        if (down) {
            held |= INPUT_BIT(a);
        }
    }
    return held;
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

void input_read(Input *out)
{
    int escape;
    unsigned int held;

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
        return;
    }
    out->move_f = key_down('W') - key_down('S');
    out->move_r = key_down('D') - key_down('A');
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
    /* An Escape that just closed the console is still down. */
    escape = key_down(VK_ESCAPE) && !console_take_escape();
    if (escape && g_escape_quits) {
        g_quit = 1;
    }
    out->quit = g_quit;
    held = actions_held(escape && !g_escape_quits);
    out->held = held;
    out->pressed = held & ~g_held;
    out->released = g_held & ~held;
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
