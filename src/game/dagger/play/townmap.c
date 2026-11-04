#include "game/dagger/play/townmap.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/places/town.h"
#include "game/dagger/tables/commerce.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See townmap.h. Rectangles are DFU's DaggerfallExteriorAutomapWindow's. */

#define VK_ESCAPE_KEY 0x1B
#define VK_HOME_KEY 0x24
#define VK_LEFT_KEY 0x25
#define VK_UP_KEY 0x26
#define VK_RIGHT_KEY 0x27
#define VK_DOWN_KEY 0x28
#define VK_PRIOR_KEY 0x21
#define VK_NEXT_KEY 0x22
/* The travel map's default key (DFU's V): from the town map, straight to it. */
#define VK_V_KEY 'V'

static const Rect g_area = { 1, 1, 318, 169 };
static const Rect g_grid_rect = { 78, 171, 27, 19 };
static const Rect g_up_rect = { 105, 171, 21, 19 };
static const Rect g_down_rect = { 126, 171, 21, 19 };
static const Rect g_left_rect = { 149, 171, 21, 19 };
static const Rect g_right_rect = { 170, 171, 21, 19 };
static const Rect g_zoom_in_rect = { 237, 171, 21, 19 };
static const Rect g_zoom_out_rect = { 258, 171, 21, 19 };
static const Rect g_exit_rect = { 281, 171, 28, 19 };
static const Rect g_compass_rect = { 3, 172, 76, 17 };
#define CAPTION_Y 190
#define CAPTION_H 10
/* A press of an arrow moves the view this many screen pixels. */
#define PAN_STEP 16
/* Zoom: 2 is half size (two map pixels a screen pixel), 1 whole, -2, -3, -4 that many screen pixels a map pixel. */
#define ZOOM_HALF 2
#define ZOOM_MOST 4

/*
 * The kinds' colours: TOWN00I0.IMG's own caption swatches (guilds and temples,
 * shops, taverns, under ART_PAL.COL), and a house's ours: classic's is not in
 * the caption.
 */
#define COLOUR_GUILD 101
#define COLOUR_SHOP 240
#define COLOUR_TAVERN 199
#define COLOUR_HOUSE 222

enum {
    MODE_ORIGINAL,
    MODE_EXTRA,
    MODE_ALL,
    MODES
};

static void pan(int dx, int dy);

static struct {
    int open;
    Composer composer;
    const df_u8 *back;
    const df_u8 *caption;
    df_u8 *map;
    int w;
    int h;
    /* The map pixel at the view's middle, the zoom, the mode. */
    int cx;
    int cy;
    int zoom;
    int mode;
    int px;
    int py;
    float yaw;
    /* Where on the screen the player's cell was drawn, for the arrow; -100 when off the area. */
    int arrow_x;
    int arrow_y;
    char line[48];
    int changed;
} g;

int townmap_colour(int byte, int mode)
{
    switch (byte) {
    case 0:
        return 0;
    case 12:
    case 15:
        return COLOUR_GUILD;
    case 1:
    case 3:
    case 4:
    case 6:
    case 7:
    case 9:
    case 10:
    case 11:
    case 13:
    case 14:
        return COLOUR_SHOP;
    case 16:
        return COLOUR_TAVERN;
    case 2:
    case 5:
    case 8:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
        return COLOUR_HOUSE;
    case 25:
    case 117:
    case 224:
    case 250:
    case 251:
        /* A ship and the special buildings: shown by DFU's Extra and All. */
        return mode != MODE_ORIGINAL ? COLOUR_HOUSE : 0;
    default:
        /* What else stands on the ground, the props: only in All. */
        return mode == MODE_ALL ? COLOUR_HOUSE : 0;
    }
}

/* The largest whole zoom at which the town fits the area; half size when even whole does not. */
static int fit_zoom(void)
{
    int s;

    for (s = ZOOM_MOST; s > 1; s--) {
        if (g.w * s <= g_area.w && g.h * s <= g_area.h) {
            return -s;
        }
    }
    return g.w <= g_area.w && g.h <= g_area.h ? 1 : ZOOM_HALF;
}

int townmap_open(int region, int index, float x, float z, float yaw)
{
    MemHeap *was;

    townmap_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    {
        df_u32 size;

        g.back = composer_table(&g.composer, "AMAP00I0.IMG", &size);
    }
    g.caption = composer_read(&g.composer, "TOWN00I0.IMG");
    was = mem_heap_use(g.composer.heap);
    g.map = (df_u8 *)mem_alloc(TOWN_MAP_MOST);
    mem_heap_use(was);
    if (!g.back || !g.caption || !g.map || !town_automap(region, index, g.map, &g.w, &g.h)) {
        log_text(LOG_WARN, "town map: its pictures or the town did not read");
        townmap_close();
        return 0;
    }
    /* The player's cell: 64 units a cell, the town's north at the top. */
    g.px = df_ifloor(x / 64.0f);
    g.py = g.h - 1 - df_ifloor(z / 64.0f);
    g.yaw = yaw;
    g.cx = g.px >= 0 && g.px < g.w ? g.px : g.w / 2;
    g.cy = g.py >= 0 && g.py < g.h ? g.py : g.h / 2;
    g.zoom = fit_zoom();
    /* At the fitted zoom the whole town shows: its middle in the middle. */
    if (g.zoom != 1 && g.zoom != ZOOM_HALF) {
        g.cx = g.w / 2;
        g.cy = g.h / 2;
    }
    g.mode = MODE_ORIGINAL;
    df_copy(g.line, town_name(), (int)sizeof(g.line));
    g.changed = 1;
    return 1;
}

void townmap_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

/* The map pixel under a screen pixel of the area; 0 when off the map. */
static int map_at(int sx, int sy, int *mx, int *my)
{
    int dx;
    int dy;

    dx = sx - (g_area.x + g_area.w / 2);
    dy = sy - (g_area.y + g_area.h / 2);
    if (g.zoom == ZOOM_HALF) {
        *mx = g.cx + dx * 2;
        *my = g.cy + dy * 2;
    } else if (g.zoom == 1) {
        *mx = g.cx + dx;
        *my = g.cy + dy;
    } else {
        *mx = g.cx + df_ifloor((float)dx / (float)-g.zoom);
        *my = g.cy + df_ifloor((float)dy / (float)-g.zoom);
    }
    return *mx >= 0 && *mx < g.w && *my >= 0 && *my < g.h;
}

int townmap_marker(float *sx, float *sy, float *angle)
{
    if (g.arrow_x < 0) {
        return 0;
    }
    /* The middle of the player's cell, from classic's pixels to the engine's, whose y counts up from the bottom. */
    *sx = ((float)g.arrow_x + 0.5f) * 2.0f;
    *sy = 480.0f - ((float)g.arrow_y + 0.5f) * 2.4f;
    /* North is up: the facing, clockwise from north, is the arrow's way on the screen. */
    *angle = g.yaw;
    return 1;
}

void townmap_drag(int dx, int dy)
{
    pan(-dx, -dy);
    g.changed = 1;
}

int townmap_repeats(int x, int y)
{
    return rect_has(&g_up_rect, x, y) || rect_has(&g_down_rect, x, y) || rect_has(&g_left_rect, x, y)
           || rect_has(&g_right_rect, x, y);
}

int townmap_on_map(int x, int y)
{
    return rect_has(&g_area, x, y);
}

void townmap_compose(void)
{
    int sx;
    int sy;

    canvas_use(&g.composer);
    canvas_copy_all(g.back);
    canvas_cutout(g.caption, CANVAS_W, CAPTION_H, 0, CAPTION_Y);
    g.arrow_x = -100;
    g.arrow_y = -100;
    for (sy = g_area.y; sy < g_area.y + g_area.h; sy++) {
        for (sx = g_area.x; sx < g_area.x + g_area.w; sx++) {
            int mx;
            int my;
            int colour;

            if (!map_at(sx, sy, &mx, &my)) {
                continue;
            }
            colour = townmap_colour(g.map[my * g.w + mx], g.mode);
            if (colour) {
                g.composer.screen[sy * CANVAS_W + sx] = (df_u8)colour;
            }
            if (mx == g.px && my == g.py && g.arrow_x < 0) {
                g.arrow_x = sx;
                g.arrow_y = sy;
            }
        }
    }
    /* The arrow is drawn over this picture, a picture of its own (marker.h): only its place is found here. */
    canvas_label(g_area.x + 3, g_area.y + 2, g.line);
    g.changed = 0;
}

const unsigned char *townmap_pixels(void)
{
    return g.composer.screen;
}

int townmap_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

void townmap_point(int x, int y)
{
    char was[48];
    int mx;
    int my;

    df_copy(was, g.line, (int)sizeof(was));
    df_copy(g.line, town_name(), (int)sizeof(g.line));
    if (rect_has(&g_area, x, y) && map_at(x, y, &mx, &my)) {
        int byte;
        const BuildingKind *kind;

        /* A building's byte is its type and one: what BLDG calls it. */
        byte = g.map[my * g.w + mx];
        kind = byte >= 1 && byte <= BUILDING_TYPES ? building_kind(byte - 1) : 0;
        if (kind) {
            df_copy(g.line, kind->name, (int)sizeof(g.line));
        }
    }
    if (!df_eq(was, g.line)) {
        g.changed = 1;
    }
}

/* The view moved by screen pixels: by map pixels as the zoom has them, kept on the map. */
static void pan(int dx, int dy)
{
    if (g.zoom == ZOOM_HALF) {
        dx *= 2;
        dy *= 2;
    } else if (g.zoom < 0) {
        dx /= -g.zoom;
        dy /= -g.zoom;
    }
    g.cx += dx;
    g.cy += dy;
    g.cx = g.cx < 0 ? 0 : (g.cx >= g.w ? g.w - 1 : g.cx);
    g.cy = g.cy < 0 ? 0 : (g.cy >= g.h ? g.h - 1 : g.cy);
}

/* In, out: half, whole, then 2 to 4 screen pixels a map pixel. */
static void zoom_by(int in)
{
    static const int steps[] = { ZOOM_HALF, 1, -2, -3, -4 };
    int i;

    for (i = 0; i < 5 && steps[i] != g.zoom; i++) {
    }
    i += in ? 1 : -1;
    i = i < 0 ? 0 : (i > 4 ? 4 : i);
    g.zoom = steps[i];
}

int townmap_click(int x, int y)
{
    g.changed = 1;
    if (rect_has(&g_exit_rect, x, y)) {
        return TOWNMAP_LEAVE;
    }
    if (rect_has(&g_grid_rect, x, y)) {
        g.mode = (g.mode + 1) % MODES;
    } else if (rect_has(&g_up_rect, x, y)) {
        pan(0, -PAN_STEP);
    } else if (rect_has(&g_down_rect, x, y)) {
        pan(0, PAN_STEP);
    } else if (rect_has(&g_left_rect, x, y)) {
        pan(-PAN_STEP, 0);
    } else if (rect_has(&g_right_rect, x, y)) {
        pan(PAN_STEP, 0);
    } else if (rect_has(&g_zoom_in_rect, x, y)) {
        zoom_by(1);
    } else if (rect_has(&g_zoom_out_rect, x, y)) {
        zoom_by(0);
    } else if (rect_has(&g_compass_rect, x, y)) {
        /* The compass: back to the player. */
        if (g.px >= 0 && g.px < g.w && g.py >= 0 && g.py < g.h) {
            g.cx = g.px;
            g.cy = g.py;
        }
    }
    return TOWNMAP_STAY;
}

void townmap_wheel(int notches)
{
    while (notches > 0) {
        zoom_by(1);
        notches--;
    }
    while (notches < 0) {
        zoom_by(0);
        notches++;
    }
    g.changed = 1;
}

int townmap_key(int vk)
{
    g.changed = 1;
    switch (vk) {
    case VK_ESCAPE_KEY:
        return TOWNMAP_LEAVE;
    case VK_V_KEY:
        return TOWNMAP_TRAVEL;
    case VK_UP_KEY:
        pan(0, -PAN_STEP);
        break;
    case VK_DOWN_KEY:
        pan(0, PAN_STEP);
        break;
    case VK_LEFT_KEY:
        pan(-PAN_STEP, 0);
        break;
    case VK_RIGHT_KEY:
        pan(PAN_STEP, 0);
        break;
    case VK_PRIOR_KEY:
        zoom_by(1);
        break;
    case VK_NEXT_KEY:
        zoom_by(0);
        break;
    case VK_HOME_KEY:
        townmap_click(g_compass_rect.x + 1, g_compass_rect.y + 1);
        break;
    default:
        break;
    }
    return TOWNMAP_STAY;
}

int townmap_scale(void)
{
    return g.zoom;
}

void townmap_centre(int *x, int *y)
{
    *x = g.cx;
    *y = g.cy;
}

const char *townmap_line(void)
{
    return g.line;
}
