#include "game/dagger/play/mouse.h"

/* See mouse.h. */

/* 1 cursor, 0 view. Classic starts on the pointer. */
static int g_cursor = 1;

void mouse_defaults(void)
{
    g_cursor = 1;
}

int mouse_cursor(void)
{
    return g_cursor;
}

void mouse_set_cursor(int on)
{
    g_cursor = on ? 1 : 0;
}

int mouse_pointer(int ui_up)
{
    return ui_up || g_cursor;
}
