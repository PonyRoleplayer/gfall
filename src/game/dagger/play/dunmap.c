#include "game/dagger/play/dunmap.h"
#include "game/dagger/play/canvas.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See dunmap.h. Rectangles are DFU's DaggerfallAutomapWindow's, as the town map's. */

#define VK_ESCAPE_KEY 0x1B
#define VK_HOME_KEY 0x24
#define VK_LEFT_KEY 0x25
#define VK_UP_KEY 0x26
#define VK_RIGHT_KEY 0x27
#define VK_DOWN_KEY 0x28
#define VK_PRIOR_KEY 0x21
#define VK_NEXT_KEY 0x22
#define VK_M_KEY 'M'

static const Rect g_area = { 1, 1, 318, 169 };
static const Rect g_grid_rect = { 78, 171, 27, 19 };
static const Rect g_up_rect = { 105, 171, 21, 19 };
static const Rect g_down_rect = { 126, 171, 21, 19 };
static const Rect g_left_rect = { 149, 171, 21, 19 };
static const Rect g_right_rect = { 170, 171, 21, 19 };
static const Rect g_turn_left_rect = { 193, 171, 21, 19 };
static const Rect g_turn_right_rect = { 214, 171, 21, 19 };
static const Rect g_upstairs_rect = { 237, 171, 21, 19 };
static const Rect g_downstairs_rect = { 258, 171, 21, 19 };
static const Rect g_exit_rect = { 281, 171, 28, 19 };
static const Rect g_compass_rect = { 3, 172, 76, 17 };
/* AMAP01I0.IMG: the grid button as the 3D view shows it, where its header puts it. */
#define GRID3D_W 27
#define GRID3D_H 19

/* Classic's 320 x 200 on the engine's 640 x 480, whose y counts from the bottom. */
#define SCREEN_X_PER 2.0f
#define SCREEN_Y_PER 2.4f
#define SCREEN_H_PX 480.0f

/* From above: pixels a unit, the start and its limits; a pan step in screen pixels; a turn's step. */
#define SCALE_START 0.15f
#define SCALE_LEAST 0.01f
#define SCALE_MOST 0.5f
#define PAN_PIXELS 24.0f
/* A pixel of a right-button drag turns the view this far; the 3D view's half field of view's tangent; how slanted its ground is. */
#define DRAG_TURN 0.012f
#define TAN_HALF_FOV 0.4142f
#define SLANT 0.84f
#define TURN_STEP (DF_TAU / 16.0f)
/*
 * The slice: from a storey under the player's feet to just over their head, a
 * storey moved by the stairs. A storey is ours, about a dungeon's level: classic
 * and DFU move a camera, not a slice of fixed height.
 */
#define STOREY 384.0f
#define SLICE_BELOW 64.0f
#define SLICE_ABOVE 160.0f
/*
 * From behind: how far back and up the camera sits, its look down, its field of
 * view. High and steep, above where the slice cuts the ceilings away, so it looks
 * down into the rooms as DFU's 3D view does, not at a room's walls from within it.
 */
#define BACK_DIST 700.0f
#define BACK_RISE 1100.0f
#define BACK_PITCH -1.0f
#define BACK_FOV 45.0f
/* The ground under the map: near classic's dark, ours. */
#define GROUND_RGB 0x141418ul
/* What the chrome's own index 0 becomes, so that 0 is clear only in the hole: a near black under ART_PAL.COL. */
#define CHROME_DARK 1

static struct {
    int open;
    Composer composer;
    const df_u8 *back;
    const df_u8 *grid3d;
    float feet[3];
    float player_yaw;
    AutomapView view;
    int storey;
    int changed;
} g;

/* The view's rectangle and slice from the state: what the engine and the arrow both read. */
static void set_slice(void)
{
    g.view.bottom = g.feet[1] - SLICE_BELOW + (float)g.storey * STOREY;
    g.view.top = g.feet[1] + SLICE_ABOVE + (float)g.storey * STOREY;
}

/* From behind: the camera back along the facing and up, looking down at the view's centre. */
static void set_camera(void)
{
    float fx;
    float fz;
    float far;

    /* The zoom from above zooms this too: the camera nearer as the scale grows. */
    far = SCALE_START / g.view.scale;
    fx = df_sin(g.view.yaw);
    fz = df_cos(g.view.yaw);
    camera_set(&g.view.cam, g.view.cx - fx * BACK_DIST * far, g.feet[1] + BACK_RISE * far + (float)g.storey * STOREY,
               g.view.cz - fz * BACK_DIST * far, g.view.yaw, BACK_PITCH);
    g.view.fov = BACK_FOV;
}

int dunmap_open(const float *feet, float yaw)
{
    dunmap_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    {
        df_u32 size;

        g.back = composer_table(&g.composer, "AMAP00I0.IMG", &size);
    }
    g.grid3d = composer_read(&g.composer, "AMAP01I0.IMG");
    if (!g.back || !g.grid3d) {
        log_text(LOG_WARN, "automap: its pictures did not read");
        dunmap_close();
        return 0;
    }
    g.feet[0] = feet[0];
    g.feet[1] = feet[1];
    g.feet[2] = feet[2];
    g.player_yaw = yaw;
    g.view.mode = AUTOMAP_TOP;
    g.view.x0 = (float)g_area.x * SCREEN_X_PER;
    g.view.x1 = (float)(g_area.x + g_area.w) * SCREEN_X_PER;
    g.view.y1 = SCREEN_H_PX - (float)g_area.y * SCREEN_Y_PER;
    g.view.y0 = SCREEN_H_PX - (float)(g_area.y + g_area.h) * SCREEN_Y_PER;
    g.view.cx = feet[0];
    g.view.cz = feet[2];
    g.view.yaw = yaw;
    g.view.scale = SCALE_START;
    set_slice();
    set_camera();
    g.changed = 1;
    return 1;
}

void dunmap_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

const AutomapView *dunmap_view(void)
{
    return &g.view;
}

unsigned long dunmap_ground(void)
{
    return GROUND_RGB;
}

int dunmap_storey(void)
{
    return g.storey;
}

int dunmap_marker(float *sx, float *sy, float *angle)
{
    float ax;
    float ay;

    if (!automap_to_screen(&g.view, g.feet[0], g.feet[1], g.feet[2], sx, sy)) {
        return 0;
    }
    /* Its way on the screen, from where a point a little ahead of the player falls: right in both views. */
    if (!automap_to_screen(&g.view, g.feet[0] + df_sin(g.player_yaw) * 64.0f, g.feet[1], g.feet[2] + df_cos(g.player_yaw) * 64.0f,
                           &ax, &ay)) {
        *angle = g.player_yaw - g.view.yaw;
        return 1;
    }
    /* Clockwise from up: x right, the engine's y up the screen. */
    *angle = df_atan2(ax - *sx, ay - *sy);
    return 1;
}

void dunmap_compose(void)
{
    int i;

    canvas_use(&g.composer);
    canvas_copy_all(g.back);
    /* Index 0 is the cutout's clear: none in the chrome but the hole. */
    for (i = 0; i < CANVAS_BYTES; i++) {
        if (!g.composer.screen[i]) {
            g.composer.screen[i] = CHROME_DARK;
        }
    }
    canvas_box(&g_area, 0);
    if (g.view.mode == AUTOMAP_3D) {
        canvas_copy_part(g.grid3d, GRID3D_W, 0, 0, GRID3D_W, GRID3D_H, g_grid_rect.x, g_grid_rect.y);
    }
    g.changed = 0;
}

const unsigned char *dunmap_pixels(void)
{
    return g.composer.screen;
}

int dunmap_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

/*
 * Ground units a screen pixel is at the view's centre: from above the scale's
 * own; from behind, by how far the camera stands from where it looks, across its
 * field of view, and the ground seen slanting stretched up the screen.
 */
static void ground_per_pixel(float *across, float *up)
{
    float d;

    if (g.view.mode == AUTOMAP_TOP) {
        *across = 1.0f / g.view.scale;
        *up = *across;
        return;
    }
    d = df_sqrt(BACK_DIST * BACK_DIST + BACK_RISE * BACK_RISE) * (SCALE_START / g.view.scale);
    *across = 2.0f * d * TAN_HALF_FOV / (g.view.x1 - g.view.x0);
    *up = *across / SLANT;
}

/* The view moved by screen pixels, up the map and across it, as the view is turned. */
static void pan(float across, float up)
{
    float fx;
    float fz;
    float per_across;
    float per_up;

    ground_per_pixel(&per_across, &per_up);
    across *= per_across;
    up *= per_up;
    fx = df_sin(g.view.yaw);
    fz = df_cos(g.view.yaw);
    /* Up is along the yaw; across is a quarter turn clockwise from it. */
    g.view.cx += fx * up + fz * across;
    g.view.cz += fz * up - fx * across;
    set_camera();
}

static void turn(float by)
{
    g.view.yaw = df_wrap_tau(g.view.yaw + by);
    set_camera();
}

static void zoom(int in)
{
    g.view.scale *= in ? 1.25f : 0.8f;
    g.view.scale = g.view.scale < SCALE_LEAST ? SCALE_LEAST : (g.view.scale > SCALE_MOST ? SCALE_MOST : g.view.scale);
    set_camera();
}

static void storey_by(int by)
{
    g.storey += by;
    set_slice();
    set_camera();
}

/* Back to the player, the way they face up, their own storey. */
static void recentre(void)
{
    g.view.cx = g.feet[0];
    g.view.cz = g.feet[2];
    g.view.yaw = g.player_yaw;
    g.storey = 0;
    set_slice();
    set_camera();
}

void dunmap_drag(int dx, int dy)
{
    /* The map follows the pointer: the view's centre goes the other way, in screen pixels. */
    pan(-(float)dx * SCREEN_X_PER, (float)dy * SCREEN_Y_PER);
    g.changed = 1;
}

void dunmap_turn_by(int dx)
{
    turn((float)dx * DRAG_TURN);
    g.changed = 1;
}

int dunmap_repeats(int x, int y)
{
    return rect_has(&g_up_rect, x, y) || rect_has(&g_down_rect, x, y) || rect_has(&g_left_rect, x, y)
           || rect_has(&g_right_rect, x, y) || rect_has(&g_turn_left_rect, x, y) || rect_has(&g_turn_right_rect, x, y);
}

int dunmap_on_map(int x, int y)
{
    return rect_has(&g_area, x, y);
}

int dunmap_click(int x, int y)
{
    g.changed = 1;
    if (rect_has(&g_exit_rect, x, y)) {
        return DUNMAP_LEAVE;
    }
    if (rect_has(&g_grid_rect, x, y)) {
        g.view.mode = g.view.mode == AUTOMAP_TOP ? AUTOMAP_3D : AUTOMAP_TOP;
        set_camera();
    } else if (rect_has(&g_up_rect, x, y)) {
        pan(0.0f, PAN_PIXELS);
    } else if (rect_has(&g_down_rect, x, y)) {
        pan(0.0f, -PAN_PIXELS);
    } else if (rect_has(&g_left_rect, x, y)) {
        pan(-PAN_PIXELS, 0.0f);
    } else if (rect_has(&g_right_rect, x, y)) {
        pan(PAN_PIXELS, 0.0f);
    } else if (rect_has(&g_turn_left_rect, x, y)) {
        turn(-TURN_STEP);
    } else if (rect_has(&g_turn_right_rect, x, y)) {
        turn(TURN_STEP);
    } else if (rect_has(&g_upstairs_rect, x, y)) {
        storey_by(1);
    } else if (rect_has(&g_downstairs_rect, x, y)) {
        storey_by(-1);
    } else if (rect_has(&g_compass_rect, x, y)) {
        recentre();
    }
    return DUNMAP_STAY;
}

void dunmap_wheel(int notches)
{
    while (notches > 0) {
        zoom(1);
        notches--;
    }
    while (notches < 0) {
        zoom(0);
        notches++;
    }
    g.changed = 1;
}

int dunmap_key(int vk)
{
    g.changed = 1;
    switch (vk) {
    case VK_ESCAPE_KEY:
    case VK_M_KEY:
        return DUNMAP_LEAVE;
    case VK_UP_KEY:
        pan(0.0f, PAN_PIXELS);
        break;
    case VK_DOWN_KEY:
        pan(0.0f, -PAN_PIXELS);
        break;
    case VK_LEFT_KEY:
        pan(-PAN_PIXELS, 0.0f);
        break;
    case VK_RIGHT_KEY:
        pan(PAN_PIXELS, 0.0f);
        break;
    case VK_PRIOR_KEY:
        storey_by(1);
        break;
    case VK_NEXT_KEY:
        storey_by(-1);
        break;
    case VK_HOME_KEY:
        recentre();
        break;
    default:
        break;
    }
    return DUNMAP_STAY;
}
