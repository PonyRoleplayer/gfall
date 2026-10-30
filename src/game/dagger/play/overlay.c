#include "game/dagger/play/overlay.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/formats/cif.h"
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
/* The vitals' bars: health, fatigue, spell points; each a 4 x 32 picture and where classic puts it. */
#define BARS 3
static const char *const g_bar_files[BARS] = { "MAIN03I0.IMG", "MAIN04I0.IMG", "MAIN05I0.IMG" };
static int g_bar[BARS];
static int g_bar_x[BARS];
static int g_bar_y[BARS];
static int g_bar_w[BARS];
static int g_bar_h[BARS];
static int g_now[BARS];
static int g_most[BARS];
static float g_yaw;
static int g_crosshair = 1;
/* The bottom panel: its picture, on or off, and the face in it. */
static int g_panel;
static int g_panel_on;
static int g_face;
/* The panel's place on classic's screen, and the face's place on the panel, stretched to fill it. */
#define PANEL_X 0
#define PANEL_Y 154
#define PANEL_W 320
#define PANEL_H 46
#define FACE_X 7
#define FACE_Y 8
#define FACE_W 33
#define FACE_H 30

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
    {
        ImgPicture bar;
        int i;

        for (i = 0; i < BARS; i++) {
            if (img_load(g_bar_files[i], &bar)) {
                g_bar[i] = pic_load(bar.pixels, bar.width, bar.height, rgb, 0);
                g_bar_x[i] = bar.x;
                g_bar_y[i] = bar.y;
                g_bar_w[i] = bar.width;
                g_bar_h[i] = bar.height;
            }
        }
    }
    {
        ImgPicture panel;

        if (img_load("MAIN00I0.IMG", &panel)) {
            g_panel = pic_load(panel.pixels, panel.width, panel.height, rgb, 0);
        }
    }
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
    {
        int i;

        for (i = 0; i < BARS; i++) {
            if (g_bar[i]) {
                pic_free(g_bar[i]);
            }
            g_bar[i] = 0;
        }
    }
}

void overlay_set_vitals(int health, int health_max, int fatigue, int fatigue_max, int magicka, int magicka_max)
{
    g_now[0] = health;
    g_most[0] = health_max;
    g_now[1] = fatigue;
    g_most[1] = fatigue_max;
    g_now[2] = magicka;
    g_most[2] = magicka_max;
}

void overlay_set_yaw(float yaw)
{
    g_yaw = df_wrap_tau(yaw);
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

/* Each bar filled from the bottom to its share, the picture's own rows from that height down. */
static void draw_vitals(void)
{
    int i;

    for (i = 0; i < BARS; i++) {
        int rows;

        if (!g_bar[i] || g_most[i] <= 0) {
            continue;
        }
        rows = g_bar_h[i] * (g_now[i] > 0 ? g_now[i] : 0) / g_most[i];
        if (rows > g_bar_h[i]) {
            rows = g_bar_h[i];
        }
        if (rows <= 0) {
            continue;
        }
        pic_draw_part(g_bar[i], 0, g_bar_h[i] - rows, g_bar_w[i], rows,
                      screen_x(g_bar_x[i]), screen_top(g_bar_y[i] + g_bar_h[i] - rows),
                      screen_x(g_bar_w[i]), screen_h(g_bar_y[i] + g_bar_h[i]) - screen_h(g_bar_y[i] + g_bar_h[i] - rows),
                      255);
    }
}

static void draw_centred(const char *s, int top)
{
    ui_string((DF_VIEW_W - ui_string_width(s)) / 2, top, s);
}

void overlay_set_panel(int on)
{
    g_panel_on = on ? 1 : 0;
}

int overlay_panel(void)
{
    return g_panel_on;
}

void overlay_set_face(const char *cif, int which)
{
    static CifSet faces;
    const CifFrame *face;
    df_u8 rgb[768];

    if (g_face) {
        pic_free(g_face);
        g_face = 0;
    }
    if (!img_art_palette(rgb) || !cif_open(cif, &faces)) {
        return;
    }
    face = cif_frame(&faces, which);
    if (face) {
        g_face = pic_load(face->pixels, face->w, face->h, rgb, 1);
    }
    cif_close(&faces);
}

void overlay_draw(void)
{
    if (g_panel_on) {
        screen_draw(g_panel, PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
        screen_draw(g_face, PANEL_X + FACE_X, PANEL_Y + FACE_Y, FACE_W, FACE_H);
    }
    draw_compass();
    draw_vitals();
    if (g_crosshair) {
        draw_centred("+", DF_VIEW_H / 2 + ui_line_height() / 2);
    }
}
