#include "game/dagger/play/overlay.h"
#include "game/dagger/formats/img.h"
#include "engine/core/df.h"
#include "engine/ui/font.h"
#include "engine/ui/pic.h"

/* See overlay.h. */

/*
 * The compass (DFU's HUDCompass): the box's 2-pixel outline round a
 * 64-pixel window on the strip, which scrolls 258 pixels for a turn
 * (the strip is 322: the window's 64 past it wrap round to its start).
 */
#define BOX_OUTLINE 2
#define BOX_WINDOW 64
#define STRIP_TURN 258

static int g_strip;
static int g_box;
static int g_box_x;
static int g_box_y;
static int g_box_w;
static int g_box_h;
static int g_strip_h;
static float g_yaw;
static int g_paused;
static int g_crosshair = 1;

/* Classic's screen to the card's: x twice, y 2.4 times, and the card's y counts up from the bottom. */
static int screen_x(int x)
{
    return x * 2;
}

static int screen_h(int h)
{
    return (h * 12 + 2) / 5;
}

static int screen_top(int y)
{
    return DF_VIEW_H - screen_h(y);
}

int overlay_load(void)
{
    ImgPicture strip;
    ImgPicture box;
    df_u8 rgb[768];

    if (!img_art_palette(rgb) || !img_load("COMPASS.IMG", &strip) || !img_load("COMPBOX.IMG", &box)) {
        return 0;
    }
    g_strip = pic_load(strip.pixels, strip.width, strip.height, rgb, 0);
    /* The box is a frame: its middle is index 0, clear, over the strip. */
    g_box = pic_load(box.pixels, box.width, box.height, rgb, 1);
    g_box_x = box.x;
    g_box_y = box.y;
    g_box_w = box.width;
    g_box_h = box.height;
    g_strip_h = strip.height;
    return g_strip != 0 && g_box != 0;
}

void overlay_free(void)
{
    if (g_strip) {
        pic_free(g_strip);
    }
    if (g_box) {
        pic_free(g_box);
    }
    g_strip = 0;
    g_box = 0;
}

void overlay_set_yaw(float yaw)
{
    g_yaw = df_wrap_tau(yaw);
}

void overlay_set_paused(int on)
{
    g_paused = on ? 1 : 0;
}

void overlay_set_crosshair(int on)
{
    g_crosshair = on ? 1 : 0;
}

static void draw_compass(void)
{
    int scroll;

    if (!g_strip || !g_box) {
        return;
    }
    scroll = df_ifloor((float)STRIP_TURN * g_yaw / DF_TAU);
    if (scroll < 0) {
        scroll = 0;
    } else if (scroll > STRIP_TURN) {
        scroll = STRIP_TURN;
    }
    pic_draw_part(g_strip, scroll, 0, BOX_WINDOW, g_strip_h,
                  screen_x(g_box_x + BOX_OUTLINE), screen_top(g_box_y + BOX_OUTLINE),
                  screen_x(g_box_w - 2 * BOX_OUTLINE), screen_h(g_strip_h), 255);
    pic_draw(g_box, screen_x(g_box_x), screen_top(g_box_y), screen_x(g_box_w), screen_h(g_box_h));
}

static void draw_centred(const char *s, int top)
{
    ui_string((DF_VIEW_W - ui_string_width(s)) / 2, top, s);
}

void overlay_draw(void)
{
    draw_compass();
    if (g_crosshair && !g_paused) {
        draw_centred("+", DF_VIEW_H / 2 + ui_line_height() / 2);
    }
    if (g_paused) {
        draw_centred("Paused. Escape goes on, Q quits.", DF_VIEW_H / 2 + ui_line_height() / 2);
    }
}
