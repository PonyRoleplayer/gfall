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
#include "engine/ui/console.h"

static HWND g_hwnd;
static int g_cx;
static int g_cy;
static int g_look_dx;
static int g_look_dy;
static int g_ignore;
static int g_active;
static int g_quit;

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
        return;
    }
    out->move_f = key_down('W') - key_down('S');
    out->move_r = key_down('D') - key_down('A');
    out->move_y = key_down('Q') - key_down('E');
    out->look_dx = g_look_dx + (key_down(VK_RIGHT) - key_down(VK_LEFT)) * KEY_LOOK;
    out->look_dy = g_look_dy + (key_down(VK_DOWN) - key_down(VK_UP)) * KEY_LOOK;
    g_look_dx = 0;
    g_look_dy = 0;
    /* An Escape that just closed the console is still down. */
    if (key_down(VK_ESCAPE) && !console_take_escape()) {
        g_quit = 1;
    }
    out->quit = g_quit;
}
