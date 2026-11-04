/*
 * PONY97: an embedded, palette-shaded model on the normal Glide 2 path.
 *
 * Only this app knows pony_model.h. The engine receives one ordinary
 * Level, Mesh and Image, with no Daggerfall loader or data directory.
 * All conversion and bounds work happens once before gfx_prepare; the
 * frame loop changes the orbit camera and triangle visibility only.
 * See docs/pony97.md.
 */
#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "engine/platform/win32.h"
#include "engine/platform/input.h"
#include "engine/gfx/gfx.h"
#include "engine/gfx/glide.h"
#include "engine/scene/light.h"
#include "engine/ui/console.h"
#include "engine/ui/font.h"
#include "engine/ui/hud.h"
#include "pony_model.h"

/*
 * The card's near plane is four engine units. Scaling once makes a
 * model unit 64 engine units, leaving useful depth precision and an
 * inspection distance comfortably outside that plane.
 */
#define MODEL_SCALE 64.0f
#define SWATCH_SIDE 16
#define SWATCH_COUNT (SWATCH_SIDE * SWATCH_SIDE)
#define ORBIT_SPEED 1.6f
#define SPIN_SPEED 0.32f
#define ELEVATION_LIMIT 1.2f

/* gnu90 compile-time guards: the generated atlas must fit one P_8 image. */
typedef char pony_colors_fit[(PONY_COLOR_COUNT > 0 &&
                             PONY_COLOR_COUNT <= SWATCH_COUNT) ? 1 : -1];
typedef char pony_vertices_match[(sizeof(pony_vertices) / sizeof(pony_vertices[0])
                                 == PONY_VERTEX_COUNT) ? 1 : -1];
typedef char pony_triangles_match[(sizeof(pony_triangles) / sizeof(pony_triangles[0])
                                  == PONY_TRIANGLE_COUNT) ? 1 : -1];

/*
 * Permanent app storage, never a frame allocation. The generated indexed
 * vertices stay read-only; expanding to the engine's triangle format is
 * a startup cost, about 73 KB for the present 1068-triangle model.
 */
static Tri g_tris[PONY_TRIANGLE_COUNT];
/* Plane equations are computed once; their sign alone rejects backfaces. */
typedef struct PonyPlane {
    float nx, ny, nz, d;
} PonyPlane;

static PonyPlane g_planes[PONY_TRIANGLE_COUNT];
static Mesh g_mesh;
static Instance g_model;
static Bound g_bound;
static Level g_level;
static Palette g_palette;
static df_u8 g_pixels[SWATCH_COUNT];
static Image g_image;
static Image *g_images[1] = { &g_image };
static const df_u8 g_cutout[1] = { 0 };
static df_u16 g_slots[1] = { 0 };
static float g_radius;
static float g_azimuth;
static float g_elevation;
static float g_distance;
static int g_spin;

static float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static int make_level(void)
{
    int i;

    /*
     * Each colour owns exactly one texel. All three corners use its
     * centre, so the texture is a flat-colour swatch, not a painted UV
     * map. No mip chain: averaging this atlas would mix unrelated hues.
     * Index zero is opaque too; the separate cutout flag says so.
     */
    for (i = 0; i < SWATCH_COUNT; i++) {
        g_pixels[i] = (df_u8)i;
        g_palette.r[i] = i < PONY_COLOR_COUNT ? pony_colors[i][0] : 255;
        g_palette.g[i] = i < PONY_COLOR_COUNT ? pony_colors[i][1] : 255;
        g_palette.b[i] = i < PONY_COLOR_COUNT ? pony_colors[i][2] : 255;
    }
    for (i = 0; i < PONY_TRIANGLE_COUNT; i++) {
        Tri *t;
        int colour;
        int k;
        float u;
        float v;

        t = &g_tris[i];
        colour = pony_triangles[i][3];
        if (colour >= PONY_COLOR_COUNT) {
            return 0;
        }
        u = (float)(colour % SWATCH_SIDE) + 0.5f;
        v = (float)(colour / SWATCH_SIDE) + 0.5f;
        for (k = 0; k < 3; k++) {
            int vertex;

            vertex = pony_triangles[i][k];
            if (vertex >= PONY_VERTEX_COUNT) {
                return 0;
            }
            t->x[k] = pony_vertices[vertex][0] * MODEL_SCALE;
            t->y[k] = pony_vertices[vertex][1] * MODEL_SCALE;
            t->z[k] = pony_vertices[vertex][2] * MODEL_SCALE;
            t->u[k] = u;
            t->v[k] = v;
        }
        t->slot = 0;
        {
            PonyPlane *p;
            float ax, ay, az;
            float bx, by, bz;

            /* CCW cross product points outwards in the Y-up source. */
            p = &g_planes[i];
            ax = t->x[1] - t->x[0];
            ay = t->y[1] - t->y[0];
            az = t->z[1] - t->z[0];
            bx = t->x[2] - t->x[0];
            by = t->y[2] - t->y[0];
            bz = t->z[2] - t->z[0];
            p->nx = ay * bz - az * by;
            p->ny = az * bx - ax * bz;
            p->nz = ax * by - ay * bx;
            p->d = p->nx * t->x[0] + p->ny * t->y[0] + p->nz * t->z[0];
        }
    }
    /*
     * Baked palette shades provide the facets, without daylight. The
     * app rejects backfaces using the cached planes below: the engine
     * deliberately treats isolated triangle faces as two-sided.
     */
    g_mesh.tris = g_tris;
    g_mesh.ntris = PONY_TRIANGLE_COUNT;
    g_mesh.slot_keys = g_slots;
    g_mesh.nslot_keys = 1;
    g_mesh.have_keys = 1;
    g_model.mesh = &g_mesh;
    instance_set_rotation(&g_model, 0.0f, 0.0f, 0.0f);
    if (!instance_world_box(&g_model, &g_bound)) {
        return 0;
    }
    g_radius = df_sqrt(g_bound.ex * g_bound.ex + g_bound.ey * g_bound.ey
                       + g_bound.ez * g_bound.ez);
    if (g_radius < DF_NEAR) {
        return 0;
    }
    g_image.width = g_image.height = SWATCH_SIDE;
    g_image.pot_w = g_image.pot_h = SWATCH_SIDE;
    g_image.span_w = g_image.span_h = SWATCH_SIDE;
    g_image.index = g_image.pot = g_pixels;
    g_level.images = g_images;
    g_level.cutout = g_cutout;
    g_level.nimages = 1;
    g_level.palette = &g_palette;
    g_level.scene.models = &g_model;
    g_level.scene.bounds = &g_bound;
    g_level.scene.nmodels = 1;
    return 1;
}

static void reset_orbit(void)
{
    /* A front three-quarter view; the source model's front is +Z. */
    g_azimuth = 0.8f;
    g_elevation = 0.16f;
    g_distance = g_radius * 3.0f;
    g_spin = 1;
}

static void place_camera(Camera *cam)
{
    float horizontal;

    horizontal = g_distance * df_cos(g_elevation);
    camera_set(cam,
               g_bound.cx + horizontal * df_sin(g_azimuth),
               g_bound.cy + g_distance * df_sin(g_elevation),
               g_bound.cz + horizontal * df_cos(g_azimuth),
               g_azimuth + DF_TAU * 0.5f, -g_elevation);
}

/*
 * The eye layers are outward-facing discs: their backs must not show
 * through the opposite side of the head. Use Tri.slot's documented skip
 * sentinel, tested by draw_tris before transform/clip/bind. Positions,
 * UVs, triangle count, bounds and texture keys never change after load.
 *
 * n dot eye > d is the front half-space. No normalisation or per-frame
 * square root is needed, only three multiplies and a comparison. This
 * is valid because the app's instance remains at the identity transform;
 * adding instance motion would require taking the eye into its frame.
 * A zero-area triangle has a zero plane and is always skipped.
 */
static void show_front_faces(const Camera *cam)
{
    static float eye_x, eye_y, eye_z;
    static int have_eye;
    int i;

    if (have_eye && cam->x == eye_x && cam->y == eye_y && cam->z == eye_z) {
        return;
    }
    eye_x = cam->x;
    eye_y = cam->y;
    eye_z = cam->z;
    have_eye = 1;
    for (i = 0; i < PONY_TRIANGLE_COUNT; i++) {
        const PonyPlane *p;

        p = &g_planes[i];
        g_tris[i].slot = p->nx * eye_x + p->ny * eye_y + p->nz * eye_z > p->d
                       ? 0 : 0xffff;
    }
}

static int key_down(int key)
{
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

static void draw_help(void)
{
    /* Short lines keep the glyph submission cost small on the 233. */
    ui_string(12, DF_VIEW_H - 12, "PONY97 - Starling");
    ui_string(12, 42, "Arrows orbit   +/- zoom   R reset   Esc quit");
    ui_string(12, 26, g_spin ? "Space: stop turntable" : "Space: start turntable");
}

static void run_view(void)
{
    Camera cam;
    DWORD tick;
    int space_was;
    int reset_was;

    reset_orbit();
    place_camera(&cam);
    tick = GetTickCount();
    space_was = key_down(VK_SPACE);
    reset_was = key_down('R');
    for (;;) {
        Input in;
        DWORD now;
        DWORD elapsed;
        float secs;
        int space;
        int reset;
        int turn;
        int tilt;
        int zoom;

        win_pump();
        input_read(&in);
        if (in.quit || win_closed()) {
            break;
        }
        now = GetTickCount();
        elapsed = now - tick; /* Unsigned subtraction also handles tick wrap. */
        tick = now;
        secs = (float)(elapsed > 100u ? 100u : elapsed) * 0.001f;
        space = key_down(VK_SPACE);
        reset = key_down('R');
        /*
         * Track edges even under the console or another window: a held
         * Space/R must not fire when focus returns. Reset the time every
         * iteration too, so returning never catches up a paused orbit.
         */
        if (!gfx_paused() && GetForegroundWindow() == (HWND)win_hwnd()) {
            if (reset && !reset_was) {
                reset_orbit();
            } else {
                if (space && !space_was) {
                    g_spin = !g_spin;
                }
                turn = key_down(VK_RIGHT) - key_down(VK_LEFT);
                tilt = key_down(VK_UP) - key_down(VK_DOWN);
                zoom = (key_down(VK_ADD) || key_down(VK_OEM_PLUS))
                     - (key_down(VK_SUBTRACT) || key_down(VK_OEM_MINUS));
                if (turn || tilt) {
                    g_spin = 0;
                }
                g_azimuth = df_wrap_tau(g_azimuth + secs *
                            ((float)turn * ORBIT_SPEED + (g_spin ? SPIN_SPEED : 0.0f)));
                g_elevation = clampf(g_elevation + (float)tilt * secs * ORBIT_SPEED,
                                     -ELEVATION_LIMIT, ELEVATION_LIMIT);
                /*
                 * Bounds use a sphere: even at the nearest zoom its
                 * nearest point stays beyond the near plane at any
                 * angle. The far plane covers the most distant zoom.
                 */
                g_distance = clampf(g_distance - (float)zoom * secs * g_radius * 2.0f,
                                    g_radius + DF_NEAR * 2.0f, g_radius * 6.0f);
            }
        }
        space_was = space;
        reset_was = reset;
        place_camera(&cam);
        show_front_faces(&cam);
        gfx_frame(&cam, &g_level);
    }
}

static int pony97_main(void *instance)
{
    int result;

    result = 1;
    mem_init();
    if (!make_level()) {
        win_fail("PONY97: embedded model indices or bounds are invalid.");
        mem_shutdown();
        return result;
    }
    /* Empty suffix: the executable directory, with no ARENA2 requirement. */
    if (!win_startup(instance, "", "PONY97", 0)) {
        mem_shutdown();
        return result;
    }
    gfx_set_fog(0);
    gfx_set_mipmap(0);
    gfx_set_far(g_radius * 7.0f + DF_NEAR);
    camera_set_fov(75.0f);
    light_set_on(0);
    scene_set_occlude(0); /* One model cannot be hidden by another model. */
    hud_set(0);
    hud_set_fps(1);
    gfx_set_use_key(0);
    glide_background(26, 30, 44);
    if (!gfx_startup(win_hwnd())) {
        win_fail(gfx_error());
    } else {
        if (!gfx_prepare(&g_level)) {
            win_fail(gfx_error());
        } else {
            log_print(LOG_INFO, "pony97: %d vertices, %d triangles, %d colours",
                      PONY_VERTEX_COUNT, PONY_TRIANGLE_COUNT, PONY_COLOR_COUNT);
            input_begin(win_hwnd());
            input_set_pointer(1);
            gfx_set_extra(draw_help);
            run_view();
            input_end();
            result = 0;
        }
        gfx_shutdown();
    }
    win_shutdown();
    mem_shutdown();
    return result;
}

void WinMainCRTStartup(void)
{
    ExitProcess((UINT)pony97_main(GetModuleHandleA(0)));
}
