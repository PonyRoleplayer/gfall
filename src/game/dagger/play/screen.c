#include "game/dagger/play/screen.h"
#include "engine/core/df.h"
#include "engine/ui/font.h"
#include "engine/ui/pic.h"

/* See screen.h. */

int screen_x(int x)
{
    return x * DF_VIEW_W / SCREEN_W;
}

int screen_h(int h)
{
    /* 2.4 a row, rounded. */
    return (h * DF_VIEW_H * 5 / SCREEN_H + 2) / 5;
}

int screen_top(int y)
{
    return DF_VIEW_H - screen_h(y);
}

void screen_from_pointer(int px, int py, int *x, int *y)
{
    *x = px * SCREEN_W / DF_VIEW_W;
    *y = (DF_VIEW_H - 1 - py) * SCREEN_H / DF_VIEW_H;
}

int screen_in(int x, int y, int left, int top, int w, int h)
{
    return x >= left && x < left + w && y >= top && y < top + h;
}

void screen_draw(int pic, int left, int top, int w, int h)
{
    if (pic) {
        pic_draw(pic, screen_x(left), screen_top(top), screen_x(w), screen_h(top + h) - screen_h(top));
    }
}

void screen_text(int left, int top, const char *s)
{
    ui_string(screen_x(left), screen_top(top), s);
}
