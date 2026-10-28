#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/gfx/draw.h"
#include "engine/assets/level.h"
#include "engine/core/log.h"
#include "engine/core/prof.h"
#include "engine/gfx/glide.h"
#include "engine/ui/console.h"
#include "engine/ui/shot.h"
#include "engine/ui/build.h"
#include "engine/ui/hud.h"
#include "engine/ui/font.h"
#include "engine/gfx/texcache.h"
#include "engine/gfx/texres.h"
#include "engine/gfx/mip.h"
#include "engine/scene/light.h"
#include "engine/scene/entity.h"
#include "engine/scene/clock.h"
#include "engine/scene/daylight.h"
#include "engine/scene/weather.h"
#include "engine/scene/walker.h"
#include "engine/scene/interact.h"
#include "engine/sound/ambience.h"
#include "engine/gfx/sky.h"

/*
 * View signs live in camera_view. This file only clips and submits.
 * Texel v grows down the source image. Row 0 is uploaded first, and
 * Glide t = 0 is that first row, so v is not flipped.
 * TEX_V_SIGN is the one place to mirror it if a wall is upside down.
 *
 * Glide 2 tiles a texture across s,t in [0, 256], for any pixel size.
 * A coordinate in [0, 1] covers a single texel, so the face is one colour.
 * A non-square picture is uploaded as a square of repeated copies.
 * image_inv shrinks s,t so one copy still fills the face.
 */

#define TEX_V_SIGN 1.0f
#define GLIDE_TEXEL 256.0f
/* Matches the block loader's placement cap. draw.h exports it for viewers. */
#define VIS_MAX DRAW_VIS_MAX
/* A streamed world's sharper copies a frame, at most: about a millisecond on the PCI bus. */
#define STREAM_UPLOAD_BYTES (64u * 1024u)

/*
 * SST-1 screen coordinates live in about +/-2048. Past that the edge
 * walker wraps and a close wall turns into stripes, or the chip walks
 * a triangle several screens wide. camera_project is
 *   sx = 320 + x * focal / z
 * The clip is the picture plus a few pixels, not a 1600-pixel guard.
 * A wall against the lens used to be submitted with a corner at
 * x = -3242. The visible part is the same. The walker no longer
 * starts thousands of pixels off the glass. The margin follows
 * camera_focal, so a wider field stays inside the SST-1 range.
 */
#define CLIP_MARGIN 16.0f
#define CLIP_MAX GLIDE_POLY_MAX
/* A face of more corners than this is drawn as triangles: 6 planes may add 6. 26 holds ARCH3D's 24. */
#define FACE_MAX (CLIP_MAX - 6)

static void (*g_extra)(void);
static void (*g_under)(void);
static float g_far = DF_FAR;
/*
 * The far clip plane polygons are cut at: g_far, or with fog on the
 * fog table's first solid entry when that is nearer (df_fog_solid),
 * the same distance the scene culls at. Past it the fog paints a pixel
 * the clear colour exactly, so drawing it would only cost fill.
 * Recomputed only when the far clip or fog changes.
 */
static float g_limit = DF_FAR;
static float g_limit_far = -1.0f;
static int g_limit_fog = -1;
static int g_faces = 1;
static int g_vis_models;
static int g_vis_sprites;
static int g_faces_kept;
static int g_faces_drop;

/*
 * Picture slopes, plus CLIP_MARGIN. Divided once per frame. A triangle
 * already inside the picture never divides again.
 */
static float g_clip_x = (320.0f + CLIP_MARGIN) / 417.0f;
static float g_clip_y = (240.0f + CLIP_MARGIN) / 417.0f;
static float g_focal = 417.0f;

/*
 * Where the card's fog table goes solid now: the far clip's own
 * (df_fog_far), or nearer when the weather closes it in (fog_apply).
 * The cull (scene_set_fog_far) and the clip below follow it, so a fog
 * or a storm also draws less.
 */
static float g_fog_now = -1.0f;
static float g_fog_near_now = -1.0f;
static float g_limit_fog_now = -1.0f;
/*
 * The far clip this frame: the far setting, or under the weather's
 * simulated view (weather_sim) its own distance with the far setting as
 * the ceiling. See docs/weather.md.
 */
static float g_far_now = DF_FAR;

static void clip_slopes(void)
{
    int fog;

    fog = glide_fog_on();
    if (g_far_now != g_limit_far || fog != g_limit_fog || g_fog_now != g_limit_fog_now) {
        float solid;

        g_limit_far = g_far_now;
        g_limit_fog = fog;
        g_limit_fog_now = g_fog_now;
        g_limit = g_far_now;
        if (fog) {
            solid = df_fog_solid(g_fog_now > 0.0f ? g_fog_now : df_fog_far(g_far_now));
            if (solid < g_limit) {
                g_limit = solid;
            }
        }
    }
    g_focal = camera_focal();
    if (g_focal < 1.0f) {
        g_focal = 1.0f;
    }
    g_clip_x = ((float)DF_VIEW_W * 0.5f + CLIP_MARGIN) / g_focal;
    g_clip_y = ((float)DF_VIEW_H * 0.5f + CLIP_MARGIN) / g_focal;
}

/* Not "far": windows.h defines far and near as empty macros. */
void draw_set_far(float dist)
{
    if (dist < DF_NEAR * 2.0f) {
        dist = DF_NEAR * 2.0f;
    }
    if (dist > DF_FAR_MAX) {
        dist = DF_FAR_MAX;
    }
    g_far = dist;
    g_far_now = dist;
}

void draw_view(float *far_now, float *fog_near, float *fog_far)
{
    *far_now = g_far_now;
    *fog_near = g_fog_near_now;
    *fog_far = g_fog_now;
}

float draw_far(void)
{
    return g_far;
}

void draw_set_faces(int on)
{
    g_faces = on ? 1 : 0;
}

int draw_faces(void)
{
    return g_faces;
}

void draw_stats(int *models, int *sprites, int *kept, int *dropped)
{
    if (models) {
        *models = g_vis_models;
    }
    if (sprites) {
        *sprites = g_vis_sprites;
    }
    if (kept) {
        *kept = g_faces_kept;
    }
    if (dropped) {
        *dropped = g_faces_drop;
    }
}

/*
 * World frame time, from one draw_frame to the next, read with the
 * performance counter (about 1.19 MHz on Windows 98; GetTickCount steps
 * in 55 ms there and cannot time a frame). An interval that spans a
 * console frame is dropped, so the frozen panel does not count. Each
 * second of frames is summed into one report: mean and worst.
 */
static df_u32 g_ft_freq;
static LARGE_INTEGER g_ft_last;
static int g_ft_have;
static df_u32 g_ft_sum;
static df_u32 g_ft_worst;
static int g_ft_frames;
static float g_ft_avg_ms;
static float g_ft_worst_ms;
static int g_ft_done;
static DWORD g_ft_hitch;

static void frame_time_read(LARGE_INTEGER *t)
{
    if (g_ft_freq == 0) {
        LARGE_INTEGER f;

        if (QueryPerformanceFrequency(&f) && f.HighPart == 0 && f.LowPart >= 1000u) {
            g_ft_freq = f.LowPart;
        } else {
            g_ft_freq = 1000u;
        }
    }
    if (g_ft_freq == 1000u || !QueryPerformanceCounter(t)) {
        t->HighPart = 0;
        t->LowPart = GetTickCount();
    }
}

/*
 * fresh drops the interval since the last world frame. Returns that
 * interval in seconds, 0 when dropped: what the world clock runs on.
 */
static float frame_time_tick(int fresh)
{
    LARGE_INTEGER now;
    df_u32 dt;

    frame_time_read(&now);
    if (fresh || !g_ft_have) {
        g_ft_last = now;
        g_ft_have = 1;
        g_ft_sum = 0;
        g_ft_worst = 0;
        g_ft_frames = 0;
        return 0.0f;
    }
    dt = (df_u32)(now.QuadPart - g_ft_last.QuadPart);
    g_ft_last = now;
    g_ft_sum += dt;
    g_ft_frames++;
    if (dt > g_ft_worst) {
        g_ft_worst = dt;
    }
    /* A tenth of a second is a stall worth a debug line, once a second. */
    if (dt > g_ft_freq / 10u && log_wants(LOG_DEBUG)
        && GetTickCount() - g_ft_hitch > 1000u) {
        g_ft_hitch = GetTickCount();
        log_print(LOG_DEBUG, "frame: one frame took %u ms",
                  (unsigned int)(dt / (g_ft_freq / 1000u)));
    }
    if (g_ft_sum >= g_ft_freq) {
        g_ft_avg_ms = (float)g_ft_sum * 1000.0f / ((float)g_ft_freq * (float)g_ft_frames);
        g_ft_worst_ms = (float)g_ft_worst * 1000.0f / (float)g_ft_freq;
        g_ft_done = g_ft_frames;
        g_ft_sum = 0;
        g_ft_worst = 0;
        g_ft_frames = 0;
    }
    return (float)dt / (float)g_ft_freq;
}

int draw_frame_time(float *avg_ms, float *worst_ms)
{
    if (avg_ms) {
        *avg_ms = g_ft_avg_ms;
    }
    if (worst_ms) {
        *worst_ms = g_ft_worst_ms;
    }
    return g_ft_done;
}

void draw_set_extra(void (*fn)(void))
{
    g_extra = fn;
}

void draw_set_under(void (*fn)(void))
{
    g_under = fn;
}

typedef struct Vtx {
    float x, y, z;
    float u, v;
} Vtx;

/* Positive means inside. Near, then far, then the four sides. */
static float plane_side(int plane, const Vtx *v)
{
    switch (plane) {
    case 0:
        return v->z - DF_NEAR;
    case 1:
        return g_limit - v->z;
    case 2:
        return v->z * g_clip_x + v->x;
    case 3:
        return v->z * g_clip_x - v->x;
    case 4:
        return v->z * g_clip_y + v->y;
    case 5:
        return v->z * g_clip_y - v->y;
    default:
        return 1.0f;
    }
}

static void lerp_vtx(Vtx *hit, const Vtx *a, const Vtx *b, float t)
{
    hit->x = a->x + (b->x - a->x) * t;
    hit->y = a->y + (b->y - a->y) * t;
    hit->z = a->z + (b->z - a->z) * t;
    hit->u = a->u + (b->u - a->u) * t;
    hit->v = a->v + (b->v - a->v) * t;
}

static int clip_plane(const Vtx *in, int n, Vtx *out, int plane)
{
    int i;
    int nout;
    float dist[CLIP_MAX];

    nout = 0;
    for (i = 0; i < n; i++) {
        dist[i] = plane_side(plane, &in[i]);
    }
    for (i = 0; i < n; i++) {
        int j;
        int ain;
        int bin;
        float denom;
        float t;
        Vtx hit;

        j = (i + 1) % n;
        ain = dist[i] >= 0.0f;
        bin = dist[j] >= 0.0f;
        if (ain && bin) {
            if (nout < CLIP_MAX) {
                out[nout++] = in[j];
            }
            continue;
        }
        denom = dist[i] - dist[j];
        if (df_absf(denom) < 1.0e-8f) {
            continue;
        }
        /*
         * Always walk from the inside vertex to the outside one. Two
         * triangles that share this edge see it in opposite orders, and
         * a lerp from either end rounds differently. The two cut points
         * then snap to different 1/16 pixels on the card and leave a
         * crack along a wall that crosses the near plane or the glass.
         * From the inside end, both triangles do the same arithmetic on
         * the same numbers and meet exactly.
         */
        if (ain) {
            t = dist[i] / denom;
            lerp_vtx(&hit, &in[i], &in[j], t);
        } else {
            t = dist[j] / -denom;
            lerp_vtx(&hit, &in[j], &in[i], t);
        }
        if (ain && !bin) {
            if (nout < CLIP_MAX) {
                out[nout++] = hit;
            }
        } else if (!ain && bin) {
            if (nout < CLIP_MAX) {
                out[nout++] = hit;
            }
            if (nout < CLIP_MAX) {
                out[nout++] = in[j];
            }
        }
    }
    return nout;
}

/* n corners of a convex polygon, n at most FACE_MAX. */
static int clip_view(const Vtx *in, int n, Vtx *out)
{
    Vtx buf[2][CLIP_MAX];
    int src;
    int nsrc;
    int plane;
    int i;

    for (i = 0; i < n; i++) {
        buf[0][i] = in[i];
    }
    src = 0;
    nsrc = n;
    for (plane = 0; plane < 6; plane++) {
        int dst;
        int ndst;
        int inside;
        int k;

        inside = 1;
        for (k = 0; k < nsrc; k++) {
            if (plane_side(plane, &buf[src][k]) < 0.0f) {
                inside = 0;
                break;
            }
        }
        if (inside) {
            continue;
        }
        dst = src ^ 1;
        ndst = clip_plane(buf[src], nsrc, buf[dst], plane);
        if (ndst < 3) {
            return 0;
        }
        src = dst;
        nsrc = ndst;
    }
    for (i = 0; i < nsrc; i++) {
        out[i] = buf[src][i];
    }
    return nsrc;
}

static int project_vert(DrawVert *dv, const Vtx *v, float inv_w, float inv_h,
                        float cr, float cg, float cb)
{
    float sx, sy, oow, ooz;

    /*
     * camera_project's arithmetic, in place: a call of seven arguments a
     * vertex was some twenty cycles of pushes on the 233. g_focal is
     * camera_focal(), set each frame by clip_slopes before anything is drawn.
     */
    if (v->z < DF_NEAR) {
        return 0;
    }
    oow = 1.0f / v->z;
    ooz = (65535.0f * DF_NEAR) * oow;
    sx = (float)DF_VIEW_W * 0.5f + v->x * g_focal * oow;
    sy = (float)DF_VIEW_H * 0.5f + v->y * g_focal * oow;
    if (sx < -2048.0f || sx > 2048.0f || sy < -2048.0f || sy > 2048.0f) {
        return 0;
    }
    dv->x = sx;
    dv->y = sy;
    dv->oow = oow;
    dv->ooz = ooz;
    dv->sow = v->u * inv_w * GLIDE_TEXEL * oow;
    dv->tow = TEX_V_SIGN * v->v * inv_h * GLIDE_TEXEL * oow;
    dv->r = cr;
    dv->g = cg;
    dv->b = cb;
    return 1;
}

/* span/pot is the fraction of the square that holds one copy. */
static void image_inv(const Image *img, float *inv_w, float *inv_h)
{
    *inv_w = (float)img->span_w / ((float)img->pot_w * (float)img->width);
    *inv_h = (float)img->span_h / ((float)img->pot_h * (float)img->height);
}

/*
 * 1 when every corner is inside the near plane, the far clip, and the
 * picture. A polygon that crosses the glass is cut to it. One that is
 * already on the glass is projected directly.
 */
static int poly_inside(const Vtx *v, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        float x;
        float y;
        float z;
        float gx;
        float gy;

        x = v[i].x;
        y = v[i].y;
        z = v[i].z;
        if (z < DF_NEAR || z > g_limit) {
            return 0;
        }
        gx = z * g_clip_x;
        gy = z * g_clip_y;
        if (gx + x < 0.0f || gx - x < 0.0f || gy + y < 0.0f || gy - y < 0.0f) {
            return 0;
        }
    }
    return 1;
}

static void submit(const Vtx *poly, int n, float inv_w, float inv_h,
                   float cr, float cg, float cb);

/*
 * How many times submit sends each polygon: 1, except in the lamp pass,
 * which lays each decal LAMP_PASSES deep. The corners are transformed,
 * clipped and projected once for all of them.
 */
static int g_repeat = 1;

static void emit_poly(const Vtx *view, int n, float inv_w, float inv_h,
                      float cr, float cg, float cb)
{
    Vtx clipped[CLIP_MAX];
    int nout;

    if (poly_inside(view, n)) {
        submit(view, n, inv_w, inv_h, cr, cg, cb);
        return;
    }
    nout = clip_view(view, n, clipped);
    if (nout < 3) {
        return;
    }
    submit(clipped, nout, inv_w, inv_h, cr, cg, cb);
}

static void emit_tri(const Vtx *view, float inv_w, float inv_h,
                     float cr, float cg, float cb)
{
    emit_poly(view, 3, inv_w, inv_h, cr, cg, cb);
}

static void submit(const Vtx *poly, int n, float inv_w, float inv_h,
                   float cr, float cg, float cb)
{
    DrawVert dv[CLIP_MAX];
    int i;
    int ok;

    if (n < 3 || n > CLIP_MAX) {
        return;
    }
    ok = 1;
    for (i = 0; i < n; i++) {
        if (!project_vert(&dv[i], &poly[i], inv_w, inv_h, cr, cg, cb)) {
            ok = 0;
            break;
        }
    }
    if (!ok) {
        return;
    }
    for (i = 0; i < g_repeat; i++) {
        glide_polygon(dv, n);
    }
}

static const Image *level_image(const Level *level, int slot);

/*
 * image_inv is two divides. A face repeats the same picture, so the
 * pair is stored by slot and reused. The pointer compare alone does not
 * notice a new place: LEVELFALL's next place is a new temp heap that
 * can hand out the same addresses, so a picture could take the last
 * place's scale. draw_prepare empties the table for each level.
 */
#define INV_SLOTS 4096
static const Image *g_inv_img[INV_SLOTS];
static float g_inv_w[INV_SLOTS];
static float g_inv_h[INV_SLOTS];

void draw_prepare(void)
{
    int i;

    for (i = 0; i < INV_SLOTS; i++) {
        g_inv_img[i] = 0;
    }
}

static void image_scale(const Image *img, int slot, float *inv_w, float *inv_h)
{
    if ((unsigned)slot < INV_SLOTS && g_inv_img[slot] == img) {
        *inv_w = g_inv_w[slot];
        *inv_h = g_inv_h[slot];
        return;
    }
    image_inv(img, inv_w, inv_h);
    if ((unsigned)slot < INV_SLOTS) {
        g_inv_img[slot] = img;
        g_inv_w[slot] = *inv_w;
        g_inv_h[slot] = *inv_h;
    }
}

/* Local point straight into view space. One less round trip through memory. */
/* Inlined. A call per vertex is three calls per triangle on a 233. */
static __inline__ void view_point(const Instance *in, const Camera *cam, const Basis *basis,
                                  float lx, float ly, float lz,
                                  float *vx, float *vy, float *vz)
{
    float dx, dy, dz;
    float x1, z1;

    dx = in->m[0] * lx + in->m[1] * ly + in->m[2] * lz + in->x - cam->x;
    dy = in->m[3] * lx + in->m[4] * ly + in->m[5] * lz + in->y - cam->y;
    dz = in->m[6] * lx + in->m[7] * ly + in->m[8] * lz + in->z - cam->z;
    x1 = dx * basis->rx + dz * basis->rz;
    z1 = dx * basis->fx + dz * basis->fz;
    *vx = x1;
    *vy = dy * basis->cp - z1 * basis->sp;
    *vz = dy * basis->sp + z1 * basis->cp;
}

static void draw_tris(const Camera *cam, const Basis *basis, const Level *level,
                      const Instance *in, int first, int count)
{
    int t;
    int end;

    end = first + count;
    if (first < 0) {
        first = 0;
    }
    if (!in->mesh || end > in->mesh->ntris) {
        end = in->mesh ? in->mesh->ntris : 0;
    }
    for (t = first; t < end; t++) {
        const Tri *tri;
        const Image *img;
        Vtx view[3];
        int k;
        float inv_w;
        float inv_h;

        tri = &in->mesh->tris[t];
        if (tri->slot == 0xffff) {
            continue;
        }
        img = level_image(level, (int)tri->slot);
        for (k = 0; k < 3; k++) {
            view_point(in, cam, basis, tri->x[k], tri->y[k], tri->z[k],
                       &view[k].x, &view[k].y, &view[k].z);
            view[k].u = tri->u[k];
            view[k].v = tri->v[k];
        }
        /*
         * A null image is a texture the loader could not read.
         * Draw it flat magenta so the face is not silently dropped.
         */
        if (!img || img->width < 1 || img->height < 1) {
            glide_flat(1);
            emit_tri(view, 1.0f, 1.0f, 255.0f, 0.0f, 255.0f);
            glide_flat(0);
            continue;
        }
        texcache_bind((int)tri->slot);
        image_scale(img, (int)tri->slot, &inv_w, &inv_h);
        emit_tri(view, inv_w, inv_h, 255.0f, 255.0f, 255.0f);
    }
}

/*
 * 1 when a face's triangles are one polygon fanned from its first
 * corner, as the ARCH3D loader emits them: every triangle starts on
 * corner 0 and on the corner the last one ended on, with the same
 * position, the same texel, and the same picture. Checked once per
 * face and kept in face->fan.
 */
static int face_fan(const Mesh *mesh, Face *face)
{
    if (face->fan == 0) {
        const Tri *t0;
        int t;
        int ok;

        t0 = &mesh->tris[face->first];
        ok = face->ntris >= 2 && face->ntris + 2 <= FACE_MAX;
        for (t = face->first + 1; ok && t < face->first + face->ntris; t++) {
            const Tri *a;
            const Tri *b;

            a = &mesh->tris[t - 1];
            b = &mesh->tris[t];
            ok = b->slot == t0->slot
                && b->x[0] == t0->x[0] && b->y[0] == t0->y[0] && b->z[0] == t0->z[0]
                && b->u[0] == t0->u[0] && b->v[0] == t0->v[0]
                && b->x[1] == a->x[2] && b->y[1] == a->y[2] && b->z[1] == a->z[2]
                && b->u[1] == a->u[2] && b->v[1] == a->v[2];
        }
        face->fan = ok ? 1 : 2;
    }
    return face->fan == 1;
}

/*
 * One ARCH3D plane as one polygon: each corner transformed and
 * projected once (a quad is 4, not 6), clipped once, and sent to the
 * card as one planar polygon. A face that is not an exact fan goes
 * triangle by triangle as before.
 */
static void draw_face(const Camera *cam, const Basis *basis, const Level *level,
                      const Instance *in, Face *face)
{
    const Tri *tris;
    const Tri *t0;
    const Image *img;
    Vtx poly[FACE_MAX];
    float inv_w;
    float inv_h;
    int n;
    int t;

    if (face->first + face->ntris > in->mesh->ntris || !face_fan(in->mesh, face)) {
        draw_tris(cam, basis, level, in, face->first, face->ntris);
        return;
    }
    tris = in->mesh->tris;
    t0 = &tris[face->first];
    if (t0->slot == 0xffff) {
        return;
    }
    view_point(in, cam, basis, t0->x[0], t0->y[0], t0->z[0], &poly[0].x, &poly[0].y, &poly[0].z);
    poly[0].u = t0->u[0];
    poly[0].v = t0->v[0];
    view_point(in, cam, basis, t0->x[1], t0->y[1], t0->z[1], &poly[1].x, &poly[1].y, &poly[1].z);
    poly[1].u = t0->u[1];
    poly[1].v = t0->v[1];
    n = 2;
    for (t = face->first; t < face->first + face->ntris; t++) {
        view_point(in, cam, basis, tris[t].x[2], tris[t].y[2], tris[t].z[2],
                   &poly[n].x, &poly[n].y, &poly[n].z);
        poly[n].u = tris[t].u[2];
        poly[n].v = tris[t].v[2];
        n++;
    }
    img = level_image(level, (int)t0->slot);
    if (!img || img->width < 1 || img->height < 1) {
        glide_flat(1);
        emit_poly(poly, n, 1.0f, 1.0f, 255.0f, 0.0f, 255.0f);
        glide_flat(0);
        return;
    }
    texcache_bind((int)t0->slot);
    image_scale(img, (int)t0->slot, &inv_w, &inv_h);
    emit_poly(poly, n, inv_w, inv_h, 255.0f, 255.0f, 255.0f);
}

static void flat_corner(const Camera *cam, const Basis *basis,
                         float wx, float wy, float wz, float u, float v, Vtx *out)
{
    camera_view(cam, basis, wx, wy, wz, &out->x, &out->y, &out->z);
    out->u = u;
    out->v = v;
}

/* 1 (default): the render passes run at the x87's single precision (df_fpu_single). */
static int g_fpu_single = 1;

void draw_set_fpu_single(int on)
{
    g_fpu_single = on ? 1 : 0;
}

int draw_fpu_single(void)
{
    return g_fpu_single;
}

/* How much farther than it stands an entity's views not shown are noted: two levels down. */
#define ENTITY_OTHER_VIEWS 4.0f

/*
 * An entity's pictures noted for the card (docs/entities.md): the view
 * shown, fl, at its distance d; the others as if ENTITY_OTHER_VIEWS
 * farther, two levels down, a sixteenth of the bytes. Each view is its
 * own picture, and one not noted is the first the card frees, so a turn
 * landed on its 8 x 8 copy; all at full size, two people close took most
 * of the card. A turn shows a quarter-size view for a frame or two, then
 * the sharp one.
 */
static void note_entity(const Entity *e, int shown, float d)
{
    int p;
    int k;

    /* Every pose it may turn to or act: a view not noted is the first the card frees. */
    for (p = 0; p < ENTITY_POSES; p++) {
        const EntityPose *pose;

        pose = &e->pose[p];
        if (pose->w[0] == 0 && pose->h[0] == 0) {
            continue;
        }
        for (k = 0; k < ENTITY_VIEWS; k++) {
            if (pose->slot[k] != 0xffff && pose->slot[k] != shown) {
                texcache_note((int)pose->slot[k], d * ENTITY_OTHER_VIEWS);
            }
        }
    }
    texcache_note(shown, d);
}

/*
 * The place's song and sounds from cam, as the first frame would start
 * them: the song read and turned into MIDI buffers here, under the hold,
 * not in the frame (up to 20 ms on the 233).
 */
static void prime_sound(const Level *level, const Camera *cam)
{
    Weather w;
    Daylight dl;
    int lit;

    weather_state(&w);
    lit = level->scene.outdoor && daylight_on();
    if (lit) {
        daylight_at(clock_minute(), &dl);
    }
    ambience_frame(level, cam, &w, lit ? dl.lamps : 0.0f, 0, 0.0f);
}

void draw_prime(const Level *level, const Camera *cam)
{
    const Scene *sc;
    float focal;
    int pass;

    if (!level || !cam) {
        return;
    }
    prime_sound(level, cam);
    if (!texcache_resident()) {
        return;
    }
    sc = &level->scene;
    focal = camera_focal();
    focal = focal < 1.0f ? 1.0f : focal;
    /*
     * As a frame notes them, every loaded picture by its nearest use, then
     * the service with no byte budget; again while it still sends (it sends
     * at most a few a call, the most needed first). Load time: casts are fine.
     */
    for (pass = 0; pass < 256; pass++) {
        int before;
        int after;
        int i;

        for (i = 0; i < sc->nmodels; i++) {
            const Instance *in;
            const Bound *b;
            float dx, dy, dz, d;
            int k;

            in = &sc->models[i];
            if (!in->mesh || !in->mesh->have_keys || sc->bounds[i].ex < 0.0f) {
                continue;
            }
            b = &sc->bounds[i];
            dx = b->cx - cam->x;
            dy = b->cy - cam->y;
            dz = b->cz - cam->z;
            d = df_sqrt(dx * dx + dy * dy + dz * dz) - (b->ex > b->ez ? b->ex : b->ez);
            for (k = 0; k < in->mesh->nslot_keys; k++) {
                texcache_note((int)in->mesh->slot_keys[k], d > 0.0f ? d : 0.0f);
            }
        }
        for (i = 0; i < sc->nsprites; i++) {
            const Flat *fl;
            float dx, dz;

            fl = &sc->sprites[i];
            if (fl->w <= 0.0f || fl->slot == 0xffff) {
                continue;
            }
            dx = fl->x - cam->x;
            dz = fl->z - cam->z;
            texcache_note((int)fl->slot, df_sqrt(dx * dx + dz * dz));
        }
        /* The entities too: townspeople, a dungeon's creatures; as the frame will see them. */
        for (i = 0; sc->entities && i < sc->nentities; i++) {
            const Entity *e;
            Flat fl;
            float dx, dz;

            e = &sc->entities[i];
            if (!e->live || !entity_flat(e, cam->x, cam->z, &fl)) {
                continue;
            }
            dx = fl.x - cam->x;
            dz = fl.z - cam->z;
            note_entity(e, (int)fl.slot, df_sqrt(dx * dx + dz * dz));
        }
        before = 0;
        after = 0;
        texres_stats(0, 0, 0, 0, &before, 0);
        texcache_stream_service(focal, 0xffffffffu);
        texres_stats(0, 0, 0, 0, &after, 0);
        if (after == before) {
            break;
        }
    }
}

/* Upright quad. It faces yaw and does not tilt with pitch. */
static const Image *level_image(const Level *level, int slot)
{
    if (!level || slot < 0 || slot >= level->nimages) {
        return 0;
    }
    return level->images[slot];
}

/*
 * Seconds of real time the flats animate by: a flame moves with the
 * clock stopped. Wrapped long before a float loses its fractions.
 */
static float g_anim_time;

/* The frame an animated flat shows now, 0 for a still one. */
static int flat_frame(const Flat *flat, const Image *img)
{
    if (img->frames < 2) {
        return 0;
    }
    /* Held by the game (an entity's swing played once): that frame, whatever the clock. */
    if (flat->held) {
        return (int)flat->frame % img->frames;
    }
    if (flat->rate == 0) {
        return 0;
    }
    return (df_ifloor(g_anim_time * (float)flat->rate) + (int)flat->phase) % img->frames;
}

static void draw_flat(const Camera *cam, const Basis *basis, const Level *level,
                      const Flat *flat)
{
    const Image *img;
    int frame;
    float du;
    float dv;
    float hx;
    float hy;
    float rx, ry, rz;
    float uw;
    float uh;
    float inv_w;
    float inv_h;
    float u0;
    float v0;
    Vtx corner[4];

    if (!flat || flat->slot == 0xffff || flat->w < 1.0f || flat->h < 1.0f) {
        return;
    }
    img = level_image(level, (int)flat->slot);
    if (!img || img->width < 1 || img->height < 1) {
        return;
    }
    hx = flat->w * 0.5f;
    hy = flat->h * 0.5f;
    rx = basis->rx;
    ry = basis->ry;
    rz = basis->rz;
    /*
     * Half a card texel in from every edge. Bilinear filtering at the
     * very edge mixes in the next texel over, which for a sprite is the
     * opposite edge (the texture wraps) or the next tiled copy: a faint
     * line along the top or the side. At a texel centre it reads only
     * the sprite. The picture was rescaled to span_w by span_h, so half
     * a card texel is width / span_w / 2 source texels.
     */
    u0 = img->span_w > 1 ? 0.5f * (float)img->width / (float)img->span_w : 0.0f;
    v0 = img->span_h > 1 ? 0.5f * (float)img->height / (float)img->span_h : 0.0f;
    uw = (float)img->width - u0;
    uh = (float)img->height - v0;
    /* An animated flat's frame: its cell in the sheet, in the picture's texels. */
    frame = flat_frame(flat, img);
    du = 0.0f;
    dv = 0.0f;
    if (frame > 0) {
        du = (float)((frame % img->frame_cols) * img->frame_dx) * (float)img->width / (float)img->span_w;
        dv = (float)((frame / img->frame_cols) * img->frame_dy) * (float)img->height / (float)img->span_h;
        u0 += du;
        v0 += dv;
        uw += du;
        uh += dv;
    }
    image_scale(img, (int)flat->slot, &inv_w, &inv_h);
    /* Mirrored: the left edge takes the right's texels, and the other way. */
    if (flat->flip) {
        float t;

        t = u0;
        u0 = uw;
        uw = t;
    }
    /* 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left. v = 0 is the top row. */
    flat_corner(cam, basis,
                flat->x - rx * hx, flat->y - ry * hx + hy, flat->z - rz * hx,
                u0, v0, &corner[0]);
    flat_corner(cam, basis,
                flat->x + rx * hx, flat->y + ry * hx + hy, flat->z + rz * hx,
                uw, v0, &corner[1]);
    flat_corner(cam, basis,
                flat->x + rx * hx, flat->y + ry * hx - hy, flat->z + rz * hx,
                uw, uh, &corner[2]);
    flat_corner(cam, basis,
                flat->x - rx * hx, flat->y - ry * hx - hy, flat->z - rz * hx,
                u0, uh, &corner[3]);
    texcache_bind((int)flat->slot);
    emit_poly(corner, 4, inv_w, inv_h, 255.0f, 255.0f, 255.0f);
}

static void draw_covers(void);

/*
 * The scene's entities: each one in view and within the visible far,
 * as the sprite of the view the camera has of it. g_vis_entities counts
 * those drawn. noted: their pictures are on the card by distance (a
 * streamed world, or a place loaded whole: texcache_resident), and each
 * drawn notes its own.
 */
static int g_vis_entities;
/*
 * 1 (default): sprites point sampled, crisp texels as Daggerfall drew
 * them; up close a 110-texel townsperson fills 300 pixels, and bilinear
 * filtering smeared each texel over three. 0: bilinear, as walls are.
 */
static int g_sprite_point = 1;

void draw_set_sprite_point(int on)
{
    g_sprite_point = on ? 1 : 0;
}

int draw_sprite_point(void)
{
    return g_sprite_point;
}

static void draw_entities(const Camera *cam, const Basis *basis, const Level *level, int noted)
{
    const Scene *sc;
    int i;

    sc = &level->scene;
    g_vis_entities = 0;
    for (i = 0; i < sc->nentities; i++) {
        const Entity *e;
        Flat fl;
        float dx, dz, d2, r;

        e = &sc->entities[i];
        if (!e->live || !entity_flat(e, cam->x, cam->z, &fl)) {
            continue;
        }
        dx = fl.x - cam->x;
        dz = fl.z - cam->z;
        d2 = dx * dx + dz * dz;
        r = (fl.w > fl.h ? fl.w : fl.h) * 0.5f;
        if (d2 > (g_limit + r) * (g_limit + r)
            || !scene_box_visible(fl.x, fl.y, fl.z, r, fl.h * 0.5f, r)) {
            continue;
        }
        if (noted) {
            note_entity(e, (int)fl.slot, df_sqrt(d2));
        }
        draw_flat(cam, basis, level, &fl);
        g_vis_entities++;
    }
}

/*
 * The falloff pictures the light decals are drawn with, one handle per
 * height band. Uploaded by gfx_prepare before the level's pictures, so
 * the texture budget sees the room they leave; 0 ready when the place
 * has no lights.
 */
static df_u32 g_light_tex[LIGHT_VARIANTS];
static int g_light_ready;
static int g_lights_drawn;
static int g_decals_drawn;
/*
 * A decal is drawn this much nearer the eye than its face, as a factor
 * on its view position: the same pixels, and a depth the test passes
 * on its own face. The W-buffer resolves distance / 4096, so 1/500 is
 * eight steps; the Z-buffer fallback holds it to about 500 units.
 */
#define DECAL_PULL 0.998f

int draw_light_upload(int want)
{
    static df_u16 pixels[LIGHT_TEX * LIGHT_TEX];
    int v;

    g_light_ready = 0;
    if (!want) {
        return 1;
    }
    for (v = 0; v < LIGHT_VARIANTS; v++) {
        light_texture(v, pixels);
        if (!glide_upload_565(pixels, LIGHT_TEX, &g_light_tex[v])) {
            log_text(LOG_WARN, "lights: no room for the falloff pictures, the place is unlit");
            return 0;
        }
    }
    g_light_ready = 1;
    return 1;
}

void draw_light_stats(int *lights, int *decals)
{
    if (lights) {
        *lights = g_lights_drawn;
    }
    if (decals) {
        *decals = g_decals_drawn;
    }
}

/*
 * The light decals of every light the cull lets through: after the
 * models, before the sprites, which take their light as one colour.
 * Each decal is tested on its own: the face side it lights, then its
 * box against the frustum and this frame's cells. In the castle at far
 * 1400 that leaves 10 to 80 a frame of the 100 to 700 a visible light
 * can carry.
 */
/*
 * Outdoors a lamp's decals fade out between these distances from the
 * camera and are not drawn past the second: a pool there is a sliver
 * in the fog, and at far 8192 in Daggerfall 9 of 16 lamps on the glass
 * were past 2500, half the lamp pass's polygons. The fade is the tint,
 * one register a lamp, so a lamp never pops.
 */
#define LAMP_FADE_NEAR 2048.0f
#define LAMP_FADE_FAR 3072.0f

static void draw_lights(const Camera *cam, const Basis *basis)
{
    df_u32 lamp;
    int outdoor;
    int i;

    if (!light_active() || !g_light_ready) {
        return;
    }
    outdoor = light_outdoor();
    /* Outdoors the lamps fade in by the hour, flame-coloured, LAMP_PASSES deep. */
    lamp = (df_u32)light_lamps();
    if (outdoor) {
        g_repeat = LAMP_PASSES;
    } else {
        glide_tint(0xffffffu);
    }
    glide_decal(1);
    for (i = 0; i < light_count(); i++) {
        const Decal *d;
        const float *corners;
        float cx, cy, cz, ex, ey, ez;
        int n;
        int k;
        df_u32 flame;

        /* A light with no decals: a streamed scene's empty slots, most of them. */
        if (!light_box(i, &cx, &cy, &cz, &ex, &ey, &ez)) {
            continue;
        }
        /* A flame wavers: its pool dims and brightens on its own (docs/lighting.md). */
        flame = (df_u32)light_flicker(i, g_anim_time);
        if (!outdoor) {
            glide_tint((flame << 16) | (flame << 8) | flame);
        }
        if (outdoor) {
            const Light *li;
            float dx, dy, dz, d2;
            df_u32 l;

            li = light_at(i);
            dx = li->x - cam->x;
            dy = li->y - cam->y;
            dz = li->z - cam->z;
            d2 = dx * dx + dy * dy + dz * dz;
            if (d2 >= LAMP_FADE_FAR * LAMP_FADE_FAR) {
                continue;
            }
            l = lamp;
            if (d2 > LAMP_FADE_NEAR * LAMP_FADE_NEAR) {
                float dist;

                dist = d2;
                __asm__ __volatile__("fsqrt" : "=t"(dist) : "0"(dist));
                l = (df_u32)df_iround((float)lamp * (LAMP_FADE_FAR - dist)
                                      * (1.0f / (LAMP_FADE_FAR - LAMP_FADE_NEAR)));
            }
            l = l * flame / 255u;
            if (l == 0) {
                continue;
            }
            glide_tint(((l * LAMP_R / 255u) << 16) | ((l * LAMP_G / 255u) << 8)
                       | (l * LAMP_B / 255u));
        }
        if (!scene_box_visible(cx, cy, cz, ex, ey, ez)) {
            continue;
        }
        g_lights_drawn++;
        n = light_decals(i, &d);
        corners = light_corners_of(i);
        for (k = 0; k < n; k++) {
            const Decal *dc;
            Vtx v[FACE_MAX];
            int q;

            dc = &d[k];
            if (dc->nx * cam->x + dc->ny * cam->y + dc->nz * cam->z + dc->d <= 0.0f
                || dc->n > FACE_MAX
                || !scene_box_visible(dc->cx, dc->cy, dc->cz, dc->ex, dc->ey, dc->ez)) {
                continue;
            }
            for (q = 0; q < dc->n; q++) {
                const float *c;
                float x, y, z;

                c = &corners[(dc->first + q) * LIGHT_CORNER];
                camera_view(cam, basis, c[0], c[1], c[2], &x, &y, &z);
                v[q].x = x * DECAL_PULL;
                v[q].y = y * DECAL_PULL;
                v[q].z = z * DECAL_PULL;
                v[q].u = c[3];
                v[q].v = c[4];
            }
            glide_bind(g_light_tex[dc->variant]);
            emit_poly(v, dc->n, 1.0f, 1.0f, 255.0f, 255.0f, 255.0f);
            g_decals_drawn++;
        }
    }
    g_repeat = 1;
    glide_decal(0);
}

/*
 * Fog for the frame: clear to near_z, solid at fog_far. The table, the
 * cull and the clip move only when either moves by more than a fraction
 * of a percent: during a change that is a 64-entry table a frame.
 */
static void fog_apply(float near_z, float fog_far)
{
    if (near_z > fog_far - 1.0f) {
        near_z = fog_far - 1.0f;
    }
    if (g_fog_now > 0.0f && df_absf(fog_far - g_fog_now) < fog_far * 0.002f
        && df_absf(near_z - g_fog_near_now) < fog_far * 0.002f) {
        return;
    }
    g_fog_now = fog_far;
    g_fog_near_now = near_z;
    glide_fog_range(near_z, fog_far);
    scene_set_fog_far(fog_far);
}

/* At night the simulated view shortens by this much: dark is near. */
#define WEATHERSIM_NIGHT 0.4f
/* And never under this, whatever the weather. */
#define WEATHERSIM_MIN 256.0f

/*
 * Rain and snow: WX_N drops in a box round the camera, each at a fixed
 * place in a lattice of boxes that fills the world, so walking through
 * them is parallax, not a pattern stuck to the glass. They fall in real
 * seconds, so a stopped clock still rains. The weather's rain or snow
 * (0..1) says how many are drawn.
 *
 * Rain is a streak, one line on the card; snow a flake, a little square
 * a few pixels across. Both are flat colour, half transparent, with the
 * depth test, so they pass behind walls, and with fog. A screen of
 * scrolling rain pictures is the usual trick, but on a Voodoo 1 every
 * layer of it is a full screen of blended fill, about 7 ms; 300 lines
 * are a few thousand pixels and one setup each.
 */
#define WX_N 320
#define RAIN_W 768.0f
#define RAIN_H 512.0f
#define RAIN_FALL 1100.0f
#define RAIN_WIND_X 90.0f
#define RAIN_WIND_Z 40.0f
/* A streak is where the drop was this long ago. */
#define RAIN_STREAK 0.025f
#define SNOW_W 640.0f
#define SNOW_H 400.0f
#define SNOW_FALL 70.0f
#define SNOW_SWAY 18.0f
#define SNOW_SIZE 1.6f

static float g_wx[WX_N];
static float g_wy[WX_N];
static float g_wz[WX_N];
static float g_wphase[WX_N];
static int g_wx_ready;
static float g_wtime;
static df_u32 g_wseed = 0x9e3779b9u;
/* Lightning: time to the next stroke, and what is left of this one. */
static float g_flash_wait = 6.0f;
static float g_flash_left;
/* 1 on the frame a stroke begins: its thunder follows (docs/sound.md). */
static int g_stroke;

static float wx_rand(void)
{
    g_wseed = g_wseed * 1664525u + 1013904223u;
    return (float)(g_wseed >> 16) * (1.0f / 65536.0f);
}

/* v moved by whole spans to within half a span of c. */
static float wrap_near(float v, float c, float span)
{
    return v - span * (float)df_ifloor((v - c) / span + 0.5f);
}

/* A segment cut to the six planes, from the inside end. 0 when none is left. */
static int clip_segment(Vtx *a, Vtx *b)
{
    int plane;

    for (plane = 0; plane < 6; plane++) {
        float da;
        float db;
        Vtx hit;

        da = plane_side(plane, a);
        db = plane_side(plane, b);
        if (da < 0.0f && db < 0.0f) {
            return 0;
        }
        if (da < 0.0f) {
            lerp_vtx(&hit, b, a, db / (db - da));
            *a = hit;
        } else if (db < 0.0f) {
            lerp_vtx(&hit, a, b, da / (da - db));
            *b = hit;
        }
    }
    return 1;
}

static int wx_vert(DrawVert *dv, const Vtx *v, float r, float g, float b)
{
    if (!camera_project(v->x, v->y, v->z, &dv->x, &dv->y, &dv->oow, &dv->ooz)) {
        return 0;
    }
    dv->sow = 0.0f;
    dv->tow = 0.0f;
    dv->r = r;
    dv->g = g;
    dv->b = b;
    return 1;
}

/* The lightning's light this frame, 0..1, and the next stroke scheduled. */
static float flash_tick(float storm, float secs)
{
    float f;

    f = 0.0f;
    g_stroke = 0;
    if (g_flash_left > 0.0f) {
        /* A stroke flickers: bright, dim, bright, gone. */
        f = g_flash_left > 0.17f ? 1.0f : (g_flash_left > 0.10f ? 0.25f : 0.7f);
        g_flash_left -= secs;
    }
    if (storm < 0.3f) {
        return f;
    }
    g_flash_wait -= secs;
    if (g_flash_wait <= 0.0f) {
        g_flash_left = 0.25f;
        g_stroke = 1;
        g_flash_wait = (4.0f + 14.0f * wx_rand()) / storm;
    }
    return f;
}

static void draw_precip(const Camera *cam, const Basis *basis, const Weather *w,
                        float light, float secs)
{
    int n;
    int i;

    if (w->rain < 0.01f && w->snow < 0.01f) {
        return;
    }
    if (!g_wx_ready) {
        for (i = 0; i < WX_N; i++) {
            g_wx[i] = wx_rand();
            g_wy[i] = wx_rand();
            g_wz[i] = wx_rand();
            g_wphase[i] = wx_rand() * DF_TAU;
        }
        g_wx_ready = 1;
    }
    g_wtime += secs;
    /* Wrapped well before a float loses the fractions of a second. */
    if (g_wtime > 3600.0f) {
        g_wtime -= 3600.0f;
    }
    if (light > 1.0f) {
        light = 1.0f;
    }
    glide_flat(1);
    if (w->rain >= 0.01f) {
        float r, g, b;

        r = 150.0f * light + 30.0f;
        g = 160.0f * light + 30.0f;
        b = 180.0f * light + 35.0f;
        glide_opacity(130, 0);
        n = df_iround(w->rain * (float)WX_N);
        for (i = 0; i < n; i++) {
            Vtx a;
            Vtx e;
            DrawVert da;
            DrawVert de;
            float x, y, z;

            x = wrap_near(g_wx[i] * RAIN_W + RAIN_WIND_X * g_wtime, cam->x, RAIN_W);
            y = wrap_near(g_wy[i] * RAIN_H - RAIN_FALL * g_wtime, cam->y, RAIN_H);
            z = wrap_near(g_wz[i] * RAIN_W + RAIN_WIND_Z * g_wtime, cam->z, RAIN_W);
            camera_view(cam, basis, x, y, z, &a.x, &a.y, &a.z);
            camera_view(cam, basis, x - RAIN_WIND_X * RAIN_STREAK, y + RAIN_FALL * RAIN_STREAK,
                        z - RAIN_WIND_Z * RAIN_STREAK, &e.x, &e.y, &e.z);
            a.u = a.v = e.u = e.v = 0.0f;
            if (!clip_segment(&a, &e) || !wx_vert(&da, &a, r, g, b) || !wx_vert(&de, &e, r, g, b)) {
                continue;
            }
            glide_line(&da, &de);
        }
    }
    if (w->snow >= 0.01f) {
        float c;

        c = 200.0f * light + 50.0f;
        glide_opacity(210, 0);
        n = df_iround(w->snow * (float)WX_N);
        for (i = 0; i < n; i++) {
            DrawVert q[4];
            float x, y, z, vx, vy, vz, sx, sy, oow, ooz, half;
            int k;

            x = g_wx[i] * SNOW_W + SNOW_SWAY * df_sin(g_wtime * 1.3f + g_wphase[i]);
            y = g_wy[i] * SNOW_H - SNOW_FALL * g_wtime;
            z = g_wz[i] * SNOW_W + SNOW_SWAY * df_cos(g_wtime * 0.9f + g_wphase[i]);
            x = wrap_near(x, cam->x, SNOW_W);
            y = wrap_near(y, cam->y, SNOW_H);
            z = wrap_near(z, cam->z, SNOW_W);
            camera_view(cam, basis, x, y, z, &vx, &vy, &vz);
            if (vz < DF_NEAR * 2.0f || vz > g_limit
                || !camera_project(vx, vy, vz, &sx, &sy, &oow, &ooz)) {
                continue;
            }
            /* Only a flake whose centre is on the glass: every corner stays near it. */
            if (sx < 0.0f || sx > (float)DF_VIEW_W || sy < 0.0f || sy > (float)DF_VIEW_H) {
                continue;
            }
            half = SNOW_SIZE * g_focal / vz;
            half = half < 0.75f ? 0.75f : (half > 3.0f ? 3.0f : half);
            for (k = 0; k < 4; k++) {
                q[k].x = sx + ((k == 1 || k == 2) ? half : -half);
                q[k].y = sy + (k >= 2 ? -half : half);
                q[k].oow = oow;
                q[k].ooz = ooz;
                q[k].sow = 0.0f;
                q[k].tow = 0.0f;
                q[k].r = c;
                q[k].g = c;
                q[k].b = c + 8.0f;
            }
            glide_polygon(q, 4);
        }
    }
    glide_opacity(-1, 0);
    glide_flat(0);
}

/*
 * Clouds dim the key light and flatten the ambient toward its own grey;
 * lightning lifts the ambient for a stroke. See docs/weather.md.
 */
static void weather_light(Daylight *dl, const Weather *w, float flash)
{
    float c;
    float l;
    float dim;

    c = w->cloud;
    dl->kr *= 1.0f - 0.6f * c;
    dl->kg *= 1.0f - 0.6f * c;
    dl->kb *= 1.0f - 0.6f * c;
    l = (dl->ar + dl->ag + dl->ab) * (1.0f / 3.0f);
    dim = 1.0f - 0.12f * c;
    dl->ar = (dl->ar + (l - dl->ar) * 0.6f * c) * dim + 0.7f * flash;
    dl->ag = (dl->ag + (l - dl->ag) * 0.6f * c) * dim + 0.7f * flash;
    dl->ab = (dl->ab + (l - dl->ab) * 0.6f * c) * dim + 0.75f * flash;
}

/*
 * The sky, first in the frame: SKY_SEGS flat panels round the camera,
 * each a column of the picture from just below the horizon to its top
 * row, and above them a fan to the zenith with the dome picture
 * (sky_cap) on it. Depth is neither read
 * nor written, so the panels sit only a few units out (SKY_RADIUS, or
 * less with a short far clip) and the world draws over them.
 *
 * A column's v runs straight with height on its panel, the cylinder
 * the original sky pictures are drawn for: a sky row is as tall as it
 * is wide, so 220 rows reach 220 / (1024 / 2 pi) = 1.35 radii up, about
 * 53 degrees. The panel runs past the bottom of the picture and the
 * clamp holds the horizon row below it. 64 polygons, most cut away by
 * the clip at once. Looking level, the
 * sky is about half a screen of fill a frame.
 */
#define SKY_SEGS 32
#define SKY_RADIUS 16.0f
/* The picture's top row: 220 rows of 1024 / 2 pi a radius. */
#define SKY_TOP ((float)SKY_H / SKY_ROWS_PER_RADIUS)
/* The dome's apex, straight above, this many radii up. */
#define SKY_APEX 4.0f
/*
 * Only a little below the horizon. The Voodoo pays a clock for every
 * pixel it draws, hidden or not, and below the horizon the ground
 * covers the sky; past the ground's end the clear is the horizon's
 * colour already. 45 degrees down cost half a screen of fill a frame.
 */
#define SKY_BOTTOM -0.1f
#define SKY_ROWS_PER_RADIUS ((float)SKY_W / DF_TAU)

static float g_sky_sin[SKY_SEGS + 1];
/* The level's sky_serial the card's sky was read for (draw_prepare). */
static int g_sky_serial;
static float g_sky_cos[SKY_SEGS + 1];
static int g_sky_trig;

static void sky_corner(const Camera *cam, const Basis *basis, float r, int seg, float up,
                       float u, Vtx *out)
{
    float v;

    camera_view(cam, basis, cam->x + r * g_sky_sin[seg], cam->y + up * r,
                cam->z + r * g_sky_cos[seg], &out->x, &out->y, &out->z);
    v = (float)SKY_H - up * SKY_ROWS_PER_RADIUS;
    out->u = u;
    out->v = v;
}

static void draw_sky(const Camera *cam, const Basis *basis)
{
    float r;
    int seg;
    int per_tile;
    const df_u8 *pal;

    if (!g_sky_trig) {
        for (seg = 0; seg <= SKY_SEGS; seg++) {
            g_sky_sin[seg] = df_sin((float)seg * (DF_TAU / (float)SKY_SEGS));
            g_sky_cos[seg] = df_cos((float)seg * (DF_TAU / (float)SKY_SEGS));
        }
        g_sky_trig = 1;
    }
    /* Inside the far clip with room: the apex is SKY_APEX radii out. */
    r = SKY_RADIUS;
    if (r * (SKY_APEX + 1.0f) > g_limit) {
        r = g_limit / (SKY_APEX + 1.0f);
    }
    per_tile = SKY_SEGS / SKY_TILES;
    pal = sky_tile_palette(0);
    glide_sky(1, pal);
    for (seg = 0; seg < SKY_SEGS; seg++) {
        Vtx v[4];
        float u0;
        float u1;

        u0 = (float)((seg % per_tile) * (SKY_TILE / per_tile));
        u1 = u0 + (float)(SKY_TILE / per_tile);
        /* A tile of a picture coming in has its own palette (sky_update): sent only when it differs. */
        if (sky_tile_palette(seg / per_tile) != pal) {
            pal = sky_tile_palette(seg / per_tile);
            glide_sky(1, pal);
        }
        glide_bind(sky_tile(seg / per_tile));
        sky_corner(cam, basis, r, seg, SKY_TOP, u0, &v[0]);
        sky_corner(cam, basis, r, seg + 1, SKY_TOP, u1, &v[1]);
        sky_corner(cam, basis, r, seg + 1, SKY_BOTTOM, u1, &v[2]);
        sky_corner(cam, basis, r, seg, SKY_BOTTOM, u0, &v[3]);
        emit_poly(v, 4, 1.0f / (float)SKY_TILE, 1.0f / (float)SKY_TILE, 255.0f, 255.0f, 255.0f);
    }
    /* The dome: row 0 of the cap on the panels' tops, its last row at the apex. */
    glide_bind(sky_cap());
    for (seg = 0; seg < SKY_SEGS; seg++) {
        Vtx v[3];
        float u0;
        float u1;

        u0 = (float)(seg * (SKY_CAP / SKY_SEGS));
        u1 = u0 + (float)(SKY_CAP / SKY_SEGS);
        sky_corner(cam, basis, r, seg, SKY_TOP, u0, &v[0]);
        sky_corner(cam, basis, r, seg + 1, SKY_TOP, u1, &v[1]);
        camera_view(cam, basis, cam->x, cam->y + SKY_APEX * r, cam->z, &v[2].x, &v[2].y, &v[2].z);
        v[0].v = 0.5f;
        v[1].v = 0.5f;
        v[2].u = (u0 + u1) * 0.5f;
        v[2].v = (float)SKY_CAP - 0.5f;
        emit_poly(v, 3, 1.0f / (float)SKY_CAP, 1.0f / (float)SKY_CAP, 255.0f, 255.0f, 255.0f);
    }
    glide_sky(0, 0);
}

/*
 * `lightview`: for the lights within light_view() units of the camera,
 * the LIGHT_VIEW_COUNT nearest, a square at each and a ring as wide as
 * its reach at its distance: yellow when its decals passed the cull
 * this frame, dim orange when not. Drawn over everything, walls
 * included, so a light behind one still shows.
 *
 * Every coordinate stays well inside the SST-1's +/-2048: a light is
 * drawn only when its centre is on the glass, and its ring only when
 * it is at most LIGHT_VIEW_RING pixels wide. The first version drew
 * every light up to the far clip, and a light beside the camera
 * projected thousands of pixels off the glass: a ring from there sent
 * the card lines it hung on, and the machine with it.
 */
#define LIGHT_VIEW_COUNT 64
#define LIGHT_VIEW_RING 480.0f

static void view_quad(float x, float y, float half, float r, float g, float b)
{
    DrawVert q[4];
    int k;

    for (k = 0; k < 4; k++) {
        q[k].oow = 1.0f;
        q[k].ooz = 1.0f;
        q[k].sow = 0.0f;
        q[k].tow = 0.0f;
        q[k].r = r;
        q[k].g = g;
        q[k].b = b;
    }
    q[0].x = x - half;
    q[0].y = y + half;
    q[1].x = x + half;
    q[1].y = y + half;
    q[2].x = x + half;
    q[2].y = y - half;
    q[3].x = x - half;
    q[3].y = y - half;
    glide_polygon(q, 4);
}

static void draw_light_view(const Camera *cam, const Basis *basis)
{
    int pick[LIGHT_VIEW_COUNT];
    float dist2[LIGHT_VIEW_COUNT];
    float range2;
    int npick;
    int i;
    int j;

    if (light_view() < 1 || light_count() < 1) {
        return;
    }
    /* The nearest few within the range, kept in order by an insertion. */
    range2 = (float)light_view() * (float)light_view();
    npick = 0;
    for (i = 0; i < light_count(); i++) {
        const Light *li;
        float dx, dy, dz, d2;

        li = light_at(i);
        dx = li->x - cam->x;
        dy = li->y - cam->y;
        dz = li->z - cam->z;
        d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > range2 || (npick == LIGHT_VIEW_COUNT && d2 >= dist2[npick - 1])) {
            continue;
        }
        if (npick < LIGHT_VIEW_COUNT) {
            npick++;
        }
        for (j = npick - 1; j > 0 && dist2[j - 1] > d2; j--) {
            dist2[j] = dist2[j - 1];
            pick[j] = pick[j - 1];
        }
        dist2[j] = d2;
        pick[j] = i;
    }
    if (npick < 1) {
        return;
    }
    glide_overlay(1);
    for (j = 0; j < npick; j++) {
        const Light *li;
        float x, y, z, sx, sy, oow, ooz, rad;
        float cx, cy, cz, ex, ey, ez;
        float r, g, b;
        int seg;

        i = pick[j];
        li = light_at(i);
        camera_view(cam, basis, li->x, li->y, li->z, &x, &y, &z);
        if (z < DF_NEAR || !camera_project(x, y, z, &sx, &sy, &oow, &ooz)) {
            continue;
        }
        /* The centre on the glass, or nothing: see above. */
        if (sx < 0.0f || sx > (float)DF_VIEW_W || sy < 0.0f || sy > (float)DF_VIEW_H) {
            continue;
        }
        if (light_box(i, &cx, &cy, &cz, &ex, &ey, &ez)
            && scene_box_visible(cx, cy, cz, ex, ey, ez)) {
            r = 255.0f;
            g = 230.0f;
            b = 60.0f;
        } else {
            r = 150.0f;
            g = 80.0f;
            b = 30.0f;
        }
        view_quad(sx, sy, 3.0f, r, g, b);
        rad = light_reach(i) * g_focal / z;
        if (rad < 4.0f || rad > LIGHT_VIEW_RING) {
            continue;
        }
        for (seg = 0; seg < 32; seg++) {
            DrawVert a;
            DrawVert e;
            float t0, t1;

            t0 = (float)seg * (DF_TAU / 32.0f);
            t1 = (float)(seg + 1) * (DF_TAU / 32.0f);
            a.x = sx + rad * df_cos(t0);
            a.y = sy + rad * df_sin(t0);
            e.x = sx + rad * df_cos(t1);
            e.y = sy + rad * df_sin(t1);
            a.oow = e.oow = 1.0f;
            a.ooz = e.ooz = 1.0f;
            a.sow = e.sow = 0.0f;
            a.tow = e.tow = 0.0f;
            a.r = e.r = r;
            a.g = e.g = g;
            a.b = e.b = b;
            glide_line(&a, &e);
        }
    }
    glide_overlay(0);
}

/*
 * `sndview`: for the emitters within ambience_view() units of the camera,
 * the SOUND_VIEW_COUNT nearest, a square, its word and a ring as wide as
 * its reach at its distance. A loop (a fire) is orange, a now-and-then
 * sound (an animal) green; bright while it plays, dim while silent.
 * Drawn over walls, and inside the SST-1's +/-2048 as lightview is: the
 * centre on the glass, the ring at most LIGHT_VIEW_RING pixels wide.
 * See docs/sound.md.
 */
#define SOUND_VIEW_COUNT 64

static void draw_sound_view(const Camera *cam, const Basis *basis, const Scene *scene)
{
    int pick[SOUND_VIEW_COUNT];
    float dist2[SOUND_VIEW_COUNT];
    const Emitter *em;
    float range2;
    int npick;
    int n;
    int i;
    int j;

    if (ambience_view() < 1 || !scene->emitters || scene->nemitters < 1) {
        return;
    }
    em = scene->emitters;
    n = scene->nemitters;
    range2 = (float)ambience_view() * (float)ambience_view();
    npick = 0;
    for (i = 0; i < n; i++) {
        float dx, dy, dz, d2;

        dx = em[i].x - cam->x;
        dy = em[i].y - cam->y;
        dz = em[i].z - cam->z;
        d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > range2 || (npick == SOUND_VIEW_COUNT && d2 >= dist2[npick - 1])) {
            continue;
        }
        if (npick < SOUND_VIEW_COUNT) {
            npick++;
        }
        for (j = npick - 1; j > 0 && dist2[j - 1] > d2; j--) {
            dist2[j] = dist2[j - 1];
            pick[j] = pick[j - 1];
        }
        dist2[j] = d2;
        pick[j] = i;
    }
    if (npick < 1) {
        return;
    }
    glide_overlay(1);
    for (j = 0; j < npick; j++) {
        const Emitter *e;
        float x, y, z, sx, sy, oow, ooz, rad;
        float r, g, b;
        int on;
        int seg;

        i = pick[j];
        e = &em[i];
        camera_view(cam, basis, e->x, e->y, e->z, &x, &y, &z);
        if (z < DF_NEAR || !camera_project(x, y, z, &sx, &sy, &oow, &ooz)) {
            continue;
        }
        if (sx < 0.0f || sx > (float)DF_VIEW_W || sy < 0.0f || sy > (float)DF_VIEW_H) {
            continue;
        }
        on = ambience_emitter_playing(i);
        if (e->every <= 0.0f) {
            r = on ? 255.0f : 130.0f;
            g = on ? 150.0f : 70.0f;
            b = on ? 40.0f : 25.0f;
        } else {
            r = on ? 140.0f : 40.0f;
            g = on ? 255.0f : 120.0f;
            b = on ? 120.0f : 40.0f;
        }
        view_quad(sx, sy, 3.0f, r, g, b);
        rad = e->reach * g_focal / z;
        if (rad < 4.0f || rad > LIGHT_VIEW_RING) {
            continue;
        }
        for (seg = 0; seg < 32; seg++) {
            DrawVert a;
            DrawVert f;
            float t0, t1;

            t0 = (float)seg * (DF_TAU / 32.0f);
            t1 = (float)(seg + 1) * (DF_TAU / 32.0f);
            a.x = sx + rad * df_cos(t0);
            a.y = sy + rad * df_sin(t0);
            f.x = sx + rad * df_cos(t1);
            f.y = sy + rad * df_sin(t1);
            a.oow = f.oow = 1.0f;
            a.ooz = f.ooz = 1.0f;
            a.sow = f.sow = 0.0f;
            a.tow = f.tow = 0.0f;
            a.r = f.r = r * 0.6f;
            a.g = f.g = g * 0.6f;
            a.b = f.b = b * 0.6f;
            glide_line(&a, &f);
        }
    }
    glide_overlay(0);
    /* The words after the squares: the text sets its own state. */
    for (j = 0; j < npick; j++) {
        const Emitter *e;
        float x, y, z, sx, sy, oow, ooz;

        e = &em[pick[j]];
        if (!e->label) {
            continue;
        }
        camera_view(cam, basis, e->x, e->y, e->z, &x, &y, &z);
        if (z < DF_NEAR || !camera_project(x, y, z, &sx, &sy, &oow, &ooz)) {
            continue;
        }
        if (sx < 0.0f || sx > (float)(DF_VIEW_W - 48) || sy < 0.0f || sy > (float)DF_VIEW_H) {
            continue;
        }
        ui_string(df_iround(sx) + 6, df_iround(sy) + ui_line_height() / 2, e->label);
    }
}

/*
 * The use key: the left button or E, on its edge (docs/interact.md). The
 * console has its own keys and is not drawn through here while open.
 */
static int g_use_was = 1;

/*
 * 1 (default): the frame polls the use key itself, the left button or
 * E, as the viewers want. 0: the game owns its keys (its left button
 * attacks) and asks for a use with interact_request_use.
 */
static int g_use_key = 1;

/* The game's pause (gfx_set_pause): the frame is drawn, the world stands still. */
static int g_pause;

void draw_set_pause(int on)
{
    g_pause = on ? 1 : 0;
}

int draw_pause(void)
{
    return g_pause;
}

void draw_set_use_key(int on)
{
    g_use_key = on ? 1 : 0;
    g_use_was = 0;
}

static int use_edge(void)
{
    int down;
    int edge;

    edge = interact_take_use();
    if (!g_use_key) {
        return edge;
    }
    down = ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (GetAsyncKeyState('E') & 0x8000)) ? 1 : 0;
    edge = edge || (down && !g_use_was);
    g_use_was = down;
    return edge;
}

/* One edge of a box in the world, cut at the near plane, then on the glass. */
static void view_edge(const Camera *cam, const Basis *basis, const float *a, const float *b,
                      float r, float g, float bl)
{
    float va[3];
    float vb[3];
    DrawVert p;
    DrawVert q;
    float oow, ooz;

    camera_view(cam, basis, a[0], a[1], a[2], &va[0], &va[1], &va[2]);
    camera_view(cam, basis, b[0], b[1], b[2], &vb[0], &vb[1], &vb[2]);
    if (va[2] < DF_NEAR && vb[2] < DF_NEAR) {
        return;
    }
    if (va[2] < DF_NEAR || vb[2] < DF_NEAR) {
        float *n;
        const float *f;
        float t;
        int k;

        n = va[2] < DF_NEAR ? va : vb;
        f = va[2] < DF_NEAR ? vb : va;
        t = (DF_NEAR - n[2]) / (f[2] - n[2]);
        for (k = 0; k < 3; k++) {
            n[k] += (f[k] - n[k]) * t;
        }
        n[2] = DF_NEAR;
    }
    if (!camera_project(va[0], va[1], va[2], &p.x, &p.y, &oow, &ooz)
        || !camera_project(vb[0], vb[1], vb[2], &q.x, &q.y, &oow, &ooz)) {
        return;
    }
    /*
     * Cut to the glass (Liang and Barsky): a box corner beside the eye
     * projects thousands of pixels out, and the SST-1 hangs on a line
     * past +/-2048 (docs/lighting.md, lightview).
     */
    {
        float t0, t1, dx, dy;
        float pv[4], qv[4];
        int k;

        t0 = 0.0f;
        t1 = 1.0f;
        dx = q.x - p.x;
        dy = q.y - p.y;
        pv[0] = -dx;
        qv[0] = p.x;
        pv[1] = dx;
        qv[1] = (float)DF_VIEW_W - 1.0f - p.x;
        pv[2] = -dy;
        qv[2] = p.y;
        pv[3] = dy;
        qv[3] = (float)DF_VIEW_H - 1.0f - p.y;
        for (k = 0; k < 4; k++) {
            float r;

            if (pv[k] == 0.0f) {
                if (qv[k] < 0.0f) {
                    return;
                }
                continue;
            }
            r = qv[k] / pv[k];
            if (pv[k] < 0.0f) {
                if (r > t1) {
                    return;
                }
                if (r > t0) {
                    t0 = r;
                }
            } else {
                if (r < t0) {
                    return;
                }
                if (r < t1) {
                    t1 = r;
                }
            }
        }
        q.x = p.x + dx * t1;
        q.y = p.y + dy * t1;
        p.x = p.x + dx * t0;
        p.y = p.y + dy * t0;
    }
    p.oow = q.oow = 1.0f;
    p.ooz = q.ooz = 1.0f;
    p.sow = q.sow = 0.0f;
    p.tow = q.tow = 0.0f;
    p.r = q.r = r;
    p.g = q.g = g;
    p.b = q.b = bl;
    glide_line(&p, &q);
}

/*
 * The box a usable is taken by, as eight world corners: a model's box as
 * it stands, a sprite's, or the passage's own about its face.
 */
static int use_corners(const Scene *scene, const Usable *u, float (*c)[3], float *mid)
{
    float ax[3], ay[3], az[3], o[3];
    float ex, ey, ez;
    int k;

    if (u->model >= 0) {
        const Instance *in;

        if (u->model >= scene->nmodels || !scene->models[u->model].mesh) {
            return 0;
        }
        in = &scene->models[u->model];
        mesh_measure(in->mesh);
        if (in->mesh->bound_n < 1) {
            return 0;
        }
        instance_point(in, in->mesh->cx, in->mesh->cy, in->mesh->cz, &o[0], &o[1], &o[2]);
        for (k = 0; k < 3; k++) {
            ax[k] = in->m[k * 3];
            ay[k] = in->m[k * 3 + 1];
            az[k] = in->m[k * 3 + 2];
        }
        ex = in->mesh->ex;
        ey = in->mesh->ey;
        ez = in->mesh->ez;
    } else if (u->sprite >= 0) {
        const Flat *fl;

        if (u->sprite >= scene->nsprites) {
            return 0;
        }
        fl = &scene->sprites[u->sprite];
        o[0] = fl->x;
        o[1] = fl->y;
        o[2] = fl->z;
        ax[0] = 1.0f; ax[1] = 0.0f; ax[2] = 0.0f;
        ay[0] = 0.0f; ay[1] = 1.0f; ay[2] = 0.0f;
        az[0] = 0.0f; az[1] = 0.0f; az[2] = 1.0f;
        ex = fl->w * 0.5f;
        ey = fl->h * 0.5f;
        ez = fl->w * 0.5f;
    } else {
        o[0] = u->cx;
        o[1] = u->cy;
        o[2] = u->cz;
        ax[0] = u->nz; ax[1] = 0.0f; ax[2] = -u->nx;
        ay[0] = 0.0f; ay[1] = 1.0f; ay[2] = 0.0f;
        az[0] = u->nx; az[1] = 0.0f; az[2] = u->nz;
        ex = u->hw;
        ey = u->hh;
        ez = u->hd;
    }
    for (k = 0; k < 8; k++) {
        float sx, sy, sz;
        int j;

        sx = (k & 1) ? ex : -ex;
        sy = (k & 2) ? ey : -ey;
        sz = (k & 4) ? ez : -ez;
        for (j = 0; j < 3; j++) {
            c[k][j] = o[j] + ax[j] * sx + ay[j] * sy + az[j] * sz;
        }
    }
    for (k = 0; k < 3; k++) {
        mid[k] = o[k];
    }
    return 1;
}

/*
 * `useview`: what can be used within interact_view() units, as boxes
 * with their words. Cyan for what moves, gold for a passage to another
 * place, green for a person, white for the rest; the one the use key
 * would take now is drawn bright, the rest dim. The nearest
 * USE_VIEW_COUNT only.
 */
#define USE_VIEW_COUNT 48

static void draw_use_view(const Camera *cam, const Basis *basis, const Scene *scene)
{
    static const int edges[12][2] = {
        {0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
        {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    int pick[USE_VIEW_COUNT];
    float dist2[USE_VIEW_COUNT];
    float mid[USE_VIEW_COUNT][3];
    float range2;
    int npick;
    int i;
    int j;

    if (interact_view() < 1 || !scene->usables || scene->nusables < 1) {
        return;
    }
    range2 = (float)interact_view() * (float)interact_view();
    npick = 0;
    for (i = 0; i < scene->nusables; i++) {
        float c[8][3];
        float m[3];
        float dx, dy, dz, d2;

        if (!use_corners(scene, &scene->usables[i], c, m)) {
            continue;
        }
        dx = m[0] - cam->x;
        dy = m[1] - cam->y;
        dz = m[2] - cam->z;
        d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > range2 || (npick == USE_VIEW_COUNT && d2 >= dist2[npick - 1])) {
            continue;
        }
        if (npick < USE_VIEW_COUNT) {
            npick++;
        }
        for (j = npick - 1; j > 0 && dist2[j - 1] > d2; j--) {
            dist2[j] = dist2[j - 1];
            pick[j] = pick[j - 1];
        }
        dist2[j] = d2;
        pick[j] = i;
    }
    if (npick < 1) {
        return;
    }
    glide_overlay(1);
    for (j = 0; j < npick; j++) {
        const Usable *u;
        float c[8][3];
        float r, g, b, k;
        int e;

        u = &scene->usables[pick[j]];
        if (!use_corners(scene, u, c, mid[j])) {
            continue;
        }
        switch (u->kind) {
        case USE_MOVER:
            r = 60.0f;
            g = 220.0f;
            b = 255.0f;
            break;
        case USE_PASSAGE:
            r = 255.0f;
            g = 200.0f;
            b = 40.0f;
            break;
        case USE_TALK:
            r = 90.0f;
            g = 255.0f;
            b = 90.0f;
            break;
        default:
            r = 230.0f;
            g = 230.0f;
            b = 230.0f;
            break;
        }
        k = pick[j] == interact_aimed() ? 1.0f : 0.45f;
        for (e = 0; e < 12; e++) {
            view_edge(cam, basis, c[edges[e][0]], c[edges[e][1]], r * k, g * k, b * k);
        }
    }
    glide_overlay(0);
    for (j = 0; j < npick; j++) {
        const Usable *u;
        float x, y, z, sx, sy, oow, ooz;

        u = &scene->usables[pick[j]];
        if (!u->label) {
            continue;
        }
        camera_view(cam, basis, mid[j][0], mid[j][1], mid[j][2], &x, &y, &z);
        if (z < DF_NEAR || !camera_project(x, y, z, &sx, &sy, &oow, &ooz)) {
            continue;
        }
        if (sx < 0.0f || sx > (float)(DF_VIEW_W - 48) || sy < 0.0f || sy > (float)DF_VIEW_H) {
            continue;
        }
        ui_string(df_iround(sx) - ui_string_width(u->label) / 2, df_iround(sy), u->label);
    }
}

/*
 * Water (docs/water.md). 1 (default): its surface drawn, and the eye
 * under it sees through the water's colour, the fog closed in. 0: not
 * drawn, as if dry; the game still knows it is there.
 */
static int g_water = 1;

void draw_set_water(int on)
{
    g_water = on ? 1 : 0;
}

int draw_water(void)
{
    return g_water;
}

/* The surface's opacity, and the fog under it: clear to UNDER_NEAR, solid at UNDER_FAR (16 m). */
#define WATER_ALPHA 150
#define UNDER_NEAR 32.0f
#define UNDER_FAR 640.0f

/*
 * Each water rectangle in view as one flat polygon at its level, from
 * above or below, after everything solid and every sprite: under it they
 * show through its colour, over it they draw in front.
 */
static void draw_water_pass(const Camera *cam, const Basis *basis, const Scene *scene)
{
    int i;

    if (!g_water || !scene->water || scene->nwater < 1) {
        return;
    }
    for (i = 0; i < scene->nwater; i++) {
        const Water *w;
        Vtx v[4];
        float cx, cz, ex, ez, dx, dz;

        w = &scene->water[i];
        cx = (w->x0 + w->x1) * 0.5f;
        cz = (w->z0 + w->z1) * 0.5f;
        ex = (w->x1 - w->x0) * 0.5f;
        ez = (w->z1 - w->z0) * 0.5f;
        /* Its nearest point within the visible far, and its box in the frustum. */
        dx = df_absf(cam->x - cx) - ex;
        dz = df_absf(cam->z - cz) - ez;
        dx = dx > 0.0f ? dx : 0.0f;
        dz = dz > 0.0f ? dz : 0.0f;
        if (dx * dx + dz * dz > g_limit * g_limit || !scene_box_visible(cx, w->y, cz, ex, 1.0f, ez)) {
            continue;
        }
        flat_corner(cam, basis, w->x0, w->y, w->z0, 0.0f, 0.0f, &v[0]);
        flat_corner(cam, basis, w->x1, w->y, w->z0, 0.0f, 0.0f, &v[1]);
        flat_corner(cam, basis, w->x1, w->y, w->z1, 0.0f, 0.0f, &v[2]);
        flat_corner(cam, basis, w->x0, w->y, w->z1, 0.0f, 0.0f, &v[3]);
        glide_water(1, w->color, WATER_ALPHA);
        emit_poly(v, 4, 1.0f, 1.0f, 255.0f, 255.0f, 255.0f);
    }
    glide_water(0, 0, 0);
}

/*
 * `trigview`: the triggers within interact_trigger_view() units (see
 * interact.h). One entry each: a usable that starts a mover or leads
 * somewhere, or a mover only a chain starts. The nearest TRIG_VIEW_COUNT.
 */
#define TRIG_VIEW_COUNT 32
#define TRIG_MOVERS 4096

typedef struct TrigPick {
    int use;
    int mover;
    float d2;
    float mid[3];
} TrigPick;

/* The eight corners of a box about c, half sizes e. */
static void box_corners(const float *c, const float *e, float (*out)[3])
{
    int k;

    for (k = 0; k < 8; k++) {
        out[k][0] = c[0] + ((k & 1) ? e[0] : -e[0]);
        out[k][1] = c[1] + ((k & 2) ? e[1] : -e[1]);
        out[k][2] = c[2] + ((k & 4) ? e[2] : -e[2]);
    }
}

static void trig_box(const Camera *cam, const Basis *basis, float (*c)[3], float r, float g, float b)
{
    static const int edges[12][2] = {
        {0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
        {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    int e;

    for (e = 0; e < 12; e++) {
        view_edge(cam, basis, c[edges[e][0]], c[edges[e][1]], r, g, b);
    }
}

/* What a mover does, in a few words: a teleport and its target, a door and its lock, a turn, a slide, a relay; what it starts next. */
static void mover_words(const Mover *m, char *line, int cap)
{
    int n;

    if (m->teleport) {
        n = log_format(line, cap, "teleport to %d %d %d", df_iround(m->to_x), df_iround(m->to_y),
                       df_iround(m->to_z));
    } else {
        n = log_format(line, cap, "%s%s%s",
                       m->door ? "door" : (m->rx != 0.0f || m->ry != 0.0f || m->rz != 0.0f ? "turn"
                                           : (m->tx != 0.0f || m->ty != 0.0f || m->tz != 0.0f ? "slide"
                                                                                               : "relay")),
                       m->once ? " once" : "",
                       m->state == MOVER_START ? "" : (m->state == MOVER_END ? " at end" : " moving"));
    }
    if (m->lock > 0) {
        n += log_format(line + n, cap - n, " lock %d", m->lock);
    }
    if (m->next >= 0) {
        log_format(line + n, cap - n, " > m%d", m->next);
    }
}

static void draw_trigger_view(const Camera *cam, const Basis *basis, const Scene *scene)
{
    static df_u8 started[TRIG_MOVERS];
    TrigPick pick[TRIG_VIEW_COUNT];
    float range2;
    int npick;
    int touching;
    int i;
    int j;

    if (interact_trigger_view() < 1 || (!scene->usables && !scene->movers)) {
        return;
    }
    range2 = (float)interact_trigger_view() * (float)interact_trigger_view();
    touching = interact_touching();
    for (i = 0; i < scene->nmovers && i < TRIG_MOVERS; i++) {
        started[i] = 0;
    }
    npick = 0;
    /* Usables first, marking the movers they start; then the movers only chains start. */
    for (i = 0; i < scene->nusables + scene->nmovers; i++) {
        TrigPick t;
        float dx, dy, dz;

        t.use = -1;
        t.mover = -1;
        if (i < scene->nusables) {
            const Usable *u;
            float c[8][3];

            u = &scene->usables[i];
            if ((u->kind != USE_MOVER && u->kind != USE_PASSAGE) || !use_corners(scene, u, c, t.mid)) {
                continue;
            }
            t.use = i;
            t.mover = u->kind == USE_MOVER ? u->mover : -1;
            if (t.mover >= 0 && t.mover < TRIG_MOVERS) {
                started[t.mover] = 1;
            }
        } else {
            const Mover *m;
            const Bound *b;

            t.mover = i - scene->nusables;
            m = &scene->movers[t.mover];
            if ((t.mover < TRIG_MOVERS && started[t.mover]) || m->model < 0 || m->model >= scene->nmodels
                || !scene->bounds) {
                continue;
            }
            b = &scene->bounds[m->model];
            t.mid[0] = b->cx;
            t.mid[1] = b->cy;
            t.mid[2] = b->cz;
        }
        dx = t.mid[0] - cam->x;
        dy = t.mid[1] - cam->y;
        dz = t.mid[2] - cam->z;
        t.d2 = dx * dx + dy * dy + dz * dz;
        if (t.d2 > range2 || (npick == TRIG_VIEW_COUNT && t.d2 >= pick[npick - 1].d2)) {
            continue;
        }
        if (npick < TRIG_VIEW_COUNT) {
            npick++;
        }
        for (j = npick - 1; j > 0 && pick[j - 1].d2 > t.d2; j--) {
            pick[j] = pick[j - 1];
        }
        pick[j] = t;
    }
    if (npick < 1) {
        return;
    }
    glide_overlay(1);
    for (j = 0; j < npick; j++) {
        const Usable *u;
        const Mover *m;
        float c[8][3];
        float mid[3];

        u = pick[j].use >= 0 ? &scene->usables[pick[j].use] : 0;
        m = pick[j].mover >= 0 && pick[j].mover < scene->nmovers ? &scene->movers[pick[j].mover] : 0;
        /* By hand: what the use key takes, violet; a passage gold. */
        if (u && (u->reach > 0.0f || u->kind == USE_PASSAGE) && use_corners(scene, u, c, mid)) {
            if (u->kind == USE_PASSAGE) {
                trig_box(cam, basis, c, 255.0f, 200.0f, 40.0f);
            } else {
                trig_box(cam, basis, c, 190.0f, 110.0f, 255.0f);
            }
        }
        /* Walked into: the volume the body sets it off in, orange; red while the body is in it. */
        if (u && u->touch && u->model >= 0 && u->model < scene->nmodels && scene->bounds) {
            const Bound *b;
            float ctr[3];
            float ext[3];

            b = &scene->bounds[u->model];
            ctr[0] = b->cx;
            ctr[1] = b->cy;
            ctr[2] = b->cz;
            ext[0] = b->ex + WALK_RADIUS;
            ext[1] = b->ey;
            ext[2] = b->ez + WALK_RADIUS;
            box_corners(ctr, ext, c);
            if (pick[j].use == touching) {
                trig_box(cam, basis, c, 255.0f, 40.0f, 40.0f);
            } else {
                trig_box(cam, basis, c, 255.0f, 140.0f, 0.0f);
            }
        }
        /* Only a chain starts it: its model's box, dim. */
        if (!u && m && scene->bounds) {
            const Bound *b;
            float ctr[3];
            float ext[3];

            b = &scene->bounds[m->model];
            ctr[0] = b->cx;
            ctr[1] = b->cy;
            ctr[2] = b->cz;
            ext[0] = b->ex;
            ext[1] = b->ey;
            ext[2] = b->ez;
            box_corners(ctr, ext, c);
            trig_box(cam, basis, c, 70.0f, 130.0f, 150.0f);
        }
        /* A teleport: a line to where it sends, and a cross there. */
        if (m && m->teleport) {
            float to[3];
            float a[3];
            float b[3];
            int k;

            to[0] = m->to_x;
            to[1] = m->to_y;
            to[2] = m->to_z;
            view_edge(cam, basis, pick[j].mid, to, 255.0f, 60.0f, 255.0f);
            for (k = 0; k < 3; k++) {
                a[0] = to[0];
                a[1] = to[1];
                a[2] = to[2];
                b[0] = to[0];
                b[1] = to[1];
                b[2] = to[2];
                a[k] -= 16.0f;
                b[k] += 16.0f;
                view_edge(cam, basis, a, b, 255.0f, 60.0f, 255.0f);
            }
        }
    }
    glide_overlay(0);
    for (j = 0; j < npick; j++) {
        const Usable *u;
        const Mover *m;
        char line[3][64];
        float x, y, z, sx, sy, oow, ooz;
        int top;
        int k;

        u = pick[j].use >= 0 ? &scene->usables[pick[j].use] : 0;
        m = pick[j].mover >= 0 && pick[j].mover < scene->nmovers ? &scene->movers[pick[j].mover] : 0;
        camera_view(cam, basis, pick[j].mid[0], pick[j].mid[1], pick[j].mid[2], &x, &y, &z);
        if (z < DF_NEAR || !camera_project(x, y, z, &sx, &sy, &oow, &ooz)) {
            continue;
        }
        /* How it goes off, and how far, in metres (40 units). */
        if (!u) {
            log_format(line[0], 64, "m%d by a chain  %.1fm", pick[j].mover, df_sqrt(pick[j].d2) / 40.0f);
        } else {
            log_format(line[0], 64, "u%d %s  %.1fm", pick[j].use,
                       u->kind == USE_PASSAGE ? "passage"
                                              : (u->touch && u->reach > 0.0f ? "by hand, walked into"
                                                                            : (u->touch ? "walked into" : "by hand")),
                       df_sqrt(pick[j].d2) / 40.0f);
        }
        /* What it does. */
        if (m) {
            mover_words(m, line[1], 64);
        } else {
            log_format(line[1], 64, "%s", u && u->label ? u->label : "");
        }
        /* The game's own numbers for it (Daggerfall: a mover's action and trigger, a passage's way and building). */
        line[2][0] = 0;
        if (u && (u->what || u->tag)) {
            log_format(line[2], 64, "what %d tag %d", u->what, u->tag);
        }
        top = df_iround(sy) - ui_line_height();
        for (k = 0; k < 3; k++) {
            int w;
            int lx;

            if (!line[k][0]) {
                continue;
            }
            w = ui_string_width(line[k]);
            lx = df_iround(sx) - w / 2;
            lx = lx < 0 ? 0 : (lx + w > DF_VIEW_W ? DF_VIEW_W - w : lx);
            if (top + k * ui_line_height() > 0 && top + k * ui_line_height() < DF_VIEW_H) {
                ui_string(lx, top + k * ui_line_height(), line[k]);
            }
        }
    }
}

static void cover_bar(float x, float y, float w, float h, float r, float g, float b);

/* interact_say's line, at the middle of the glass, a third of the way up. */
static void draw_saying(void)
{
    const char *s;
    int alpha;

    int wide;
    int line;
    int x;
    int top;

    s = interact_saying(&alpha);
    if (!s || alpha <= 0) {
        return;
    }
    wide = ui_string_width(s);
    line = ui_line_height();
    x = (DF_VIEW_W - wide) / 2;
    top = DF_VIEW_H / 3;
    /* On a plate, as the screenshot note is: white on a pale wall is lost. */
    glide_dim(1, alpha * 3 / 4);
    cover_bar((float)(x - 6), (float)(top - line - 4), (float)(wide + 12), (float)(line + 8),
              0.0f, 0.0f, 0.0f);
    glide_dim(0, 0);
    ui_string_alpha(x, top, s, alpha);
}

/* A black wash over the whole glass at opacity alpha 0..255. */
static void wash_glass(int alpha)
{
    DrawVert v[4];
    int i;

    if (alpha <= 0) {
        return;
    }
    for (i = 0; i < 4; i++) {
        v[i].oow = 1.0f;
        v[i].ooz = 1.0f;
        v[i].sow = 0.0f;
        v[i].tow = 0.0f;
        v[i].r = 0.0f;
        v[i].g = 0.0f;
        v[i].b = 0.0f;
    }
    v[0].x = 0.0f;
    v[0].y = (float)DF_VIEW_H;
    v[1].x = (float)DF_VIEW_W;
    v[1].y = (float)DF_VIEW_H;
    v[2].x = (float)DF_VIEW_W;
    v[2].y = 0.0f;
    v[3].x = 0.0f;
    v[3].y = 0.0f;
    glide_dim(1, alpha > 255 ? 255 : alpha);
    glide_polygon(v, 4);
    glide_dim(0, 0);
}

/* Holding a passage's last frame dims it this much; the next place comes up from black. */
#define HOLD_DIM 150

static float g_fade_left;
static float g_fade_total;

void draw_fade_in(float secs)
{
    g_fade_total = secs > 0.0f ? secs : 0.0f;
    g_fade_left = g_fade_total;
}

void draw_hold(const char *text)
{
    const df_u8 *pix;
    int stride;

    glide_sprite_alpha(0);
    pix = shot_grab(&stride);
    if (!pix || !glide_lfb_write(pix, stride)) {
        glide_clear();
    }
    wash_glass(HOLD_DIM);
    if (text && text[0]) {
        ui_string((DF_VIEW_W - ui_string_width(text)) / 2, (DF_VIEW_H + ui_line_height()) / 2, text);
    }
    glide_swap();
}

/*
 * Windows after dark: Daggerfall Unity's night window colour, (255, 182,
 * 56) at 0.8, times how lit the lamps are (the hour's), so they come on
 * with the street lamps at dusk.
 */
#define WINDOW_R 204.0f
#define WINDOW_G 146.0f
#define WINDOW_B 45.0f

/* 1 when the mesh has a face whose picture has glass (its slot keys, from paint). */
static int mesh_has_glass(const Mesh *mesh, const df_u8 *glass, int nimages)
{
    int k;

    if (!mesh->have_keys) {
        return 1;
    }
    for (k = 0; k < mesh->nslot_keys; k++) {
        int slot;

        slot = (int)mesh->slot_keys[k];
        if (slot < nimages && glass[slot]) {
            return 1;
        }
    }
    return 0;
}

/*
 * The windows' night pass, outdoors (docs/daylight.md): each visible face
 * whose picture is a window is drawn again, read through a palette that
 * is black but for the glass (glide_glow), added onto the frame. Black
 * adds nothing, so only the glass lights, at full strength however dark
 * the night has made the wall. The fog does not touch an added pass, so
 * each model is faded by hand toward the fog's far end instead.
 */
static void draw_windows(const Camera *cam, const Basis *basis, const Level *level,
                         const int *order, int norder, float lamps,
                         float fog_near, float fog_far)
{
    df_u32 rgb;
    int oi;
    int on;

    if (!level->glass || lamps <= 0.02f) {
        return;
    }
    rgb = ((df_u32)df_iround(WINDOW_R * lamps) << 16) | ((df_u32)df_iround(WINDOW_G * lamps) << 8)
        | (df_u32)df_iround(WINDOW_B * lamps);
    on = 0;
    for (oi = 0; oi < norder; oi++) {
        const Instance *in;
        float fade;
        int f;

        in = &level->scene.models[order[oi]];
        if (!in->mesh || !in->mesh->faces || !mesh_has_glass(in->mesh, level->glass, level->nimages)) {
            continue;
        }
        fade = 1.0f;
        if (level->scene.bounds && fog_far > fog_near) {
            const Bound *b;
            float dx, dz, d;

            b = &level->scene.bounds[order[oi]];
            dx = b->cx - cam->x;
            dz = b->cz - cam->z;
            d = df_sqrt(dx * dx + dz * dz);
            fade = d <= fog_near ? 1.0f : (fog_far - d) / (fog_far - fog_near);
        }
        if (fade <= 0.02f) {
            continue;
        }
        if (fade > 1.0f) {
            fade = 1.0f;
        }
        for (f = 0; f < in->mesh->nfaces; f++) {
            Face *face;
            const Tri *tri;
            int slot;

            face = &in->mesh->faces[f];
            if (face->ntris < 1 || face->first < 0) {
                continue;
            }
            tri = &in->mesh->tris[face->first];
            slot = (int)tri->slot;
            if (slot >= level->nimages || !level->glass[slot]) {
                continue;
            }
            if (g_faces && face->ntris >= 2 &&
                !instance_faces(in, cam, basis, face->nx, face->ny, face->nz,
                                tri->x[0], tri->y[0], tri->z[0])) {
                continue;
            }
            if (!on) {
                glide_glow(1, rgb);
                on = 1;
            }
            {
                int g;

                g = df_iround(fade * 255.0f);
                glide_tint(((df_u32)g << 16) | ((df_u32)g << 8) | (df_u32)g);
            }
            draw_face(cam, basis, level, in, face);
        }
    }
    if (on) {
        glide_glow(0, 0);
    }
}

static int g_cap_models;
static int g_cap_sprites;

/* Once when a visible list reaches the cap, again only after it drops. */
static void cap_warn(int *state, int n, const char *what)
{
    if (n >= VIS_MAX && !*state) {
        log_print(LOG_WARN, "draw: %d visible %s, the cap; the rest are not drawn",
                  n, what);
    }
    *state = n >= VIS_MAX;
}

/* This frame's daylight colours by angle to the key light. */
static df_u32 g_shade[DAYLIGHT_STEPS + 1];

void draw_frame(const Camera *cam, const Level *level)
{
    unsigned short fpu_was;
    Basis basis;
    Daylight dl;
    int order[VIS_MAX];
    int norder;
    int oi;
    int outdoor;
    int lit;
    int sky;
    int sky_alpha;
    Weather w;
    float secs;
    float flash;
    float fog_far;
    float fog_near;
    float kx = 0.0f;
    float ky = 0.0f;
    float kz = 0.0f;
    df_u32 sprite_day;
    df_u32 ambient;
    df_u32 tinted;
    int noted;
    int under;
    df_u32 under_rgb;

    if (!level) {
        return;
    }
    console_bind(cam);
    console_pump();
    if (console_open()) {
        console_present(cam);
        g_ft_have = 0;
        /* The sound runs on under the console. */
        ambience_idle();
        return;
    }
    console_release();
    /* Pictures on the card by distance (a stream's, a place's): the frame notes how near each was drawn. */
    noted = texcache_resident();
    prof_begin(PROF_FRAME);
    prof_begin(PROF_SIM);
    /* Real seconds for the rain and the lightning; game minutes for the clock and the weather. */
    secs = frame_time_tick(0);
    if (secs < 0.0f || secs > 1.0f) {
        secs = 0.0f;
    }
    /* The game's pause (gfx_set_pause): drawn, but no time passes in the world. */
    if (g_pause) {
        secs = 0.0f;
    }
    weather_advance(clock_advance(secs));
    weather_state(&w);
    g_anim_time += secs;
    if (g_anim_time > 3600.0f) {
        g_anim_time -= 3600.0f;
    }
    /*
     * Collision (tcl): the body follows the console's camera, which is
     * the one drawn, and puts it back at its eye before anything reads
     * it. See docs/collision.md.
     */
    if (walker_on() && (const Camera *)console_camera() == cam && !g_pause) {
        walker_step(console_camera(), &level->scene, secs);
    }
    /*
     * Use what the middle of the view rests on, on the use key's edge,
     * then move the doors and levers on (docs/interact.md). Before the
     * cull, which reads the poses they write. Not while paused: nothing
     * is used, walked into or moved.
     */
    if (!g_pause) {
        interact_frame(&level->scene, cam, secs, use_edge());
    }
    /* A teleport used or reached (docs/interact.md): the player's camera goes, the body with it. */
    {
        float tx, ty, tz, tyaw;

        if (interact_teleport(&tx, &ty, &tz, &tyaw) && (const Camera *)console_camera() == cam) {
            Camera *pc;

            pc = console_camera();
            camera_set(pc, tx, ty, tz, tyaw, pc->pitch);
            walker_place();
        }
    }
    prof_end(PROF_SIM);
    g_vis_models = 0;
    g_vis_sprites = 0;
    g_faces_kept = 0;
    g_faces_drop = 0;
    /*
     * Outdoors: the hour's light, and the sky, whose horizon is the clear
     * and the fog. See docs/daylight.md.
     */
    outdoor = level->scene.outdoor;
    lit = outdoor && daylight_on();
    flash = outdoor ? flash_tick(w.storm, secs) : 0.0f;
    if (lit) {
        daylight_at(clock_minute(), &dl);
        weather_light(&dl, &w, flash);
        daylight_table(&dl, g_shade);
        light_set_lamps(df_iround(dl.lamps * 255.0f));
    } else if (outdoor) {
        light_set_lamps(0);
    }
    /*
     * The place's sound: its song, the weather's loops and thunder, its
     * emitters, heard from this camera; then the card is refilled. The
     * night is the lamps' share of the hour. See docs/sound.md.
     */
    prof_begin(PROF_SOUND);
    ambience_frame(level, cam, &w, lit ? dl.lamps : 0.0f, outdoor && g_stroke, secs);
    prof_end(PROF_SOUND);
    /*
     * The view. Outdoors the weather closes the fog in; under the
     * simulation it also sets the far clip, ceiling the far setting,
     * and where the fog starts. See docs/weather.md.
     */
    if (outdoor && weather_on() && weather_sim()) {
        g_far_now = w.view * (1.0f - WEATHERSIM_NIGHT * (lit ? dl.lamps : 0.0f));
        if (g_far_now > g_far) {
            g_far_now = g_far;
        }
        if (g_far_now < WEATHERSIM_MIN) {
            g_far_now = WEATHERSIM_MIN;
        }
        /* Solid on a table entry at or before the clip: nothing past it is half seen. */
        fog_far = df_fog_below(g_far_now);
        if (w.fog < fog_far) {
            fog_far = w.fog;
        }
        fog_near = w.haze * fog_far;
    } else {
        g_far_now = g_far;
        fog_far = df_fog_far(g_far);
        if (outdoor && w.fog < fog_far) {
            fog_far = w.fog;
        }
        fog_near = DF_FOG_NEAR < fog_far * 0.25f ? DF_FOG_NEAR : fog_far * 0.25f;
    }
    /*
     * The eye under water (docs/water.md): the fog closes in to the
     * water's colour, and the cull with it. Its colour is set below,
     * after the sky's.
     */
    under = 0;
    under_rgb = 0;
    if (g_water && scene_water_body(&level->scene, cam->x, cam->y, cam->z) == WATER_HEAD) {
        under = 1;
        scene_water_at(&level->scene, cam->x, cam->z, 0, &under_rgb);
        fog_near = UNDER_NEAR;
        fog_far = UNDER_FAR < fog_far ? UNDER_FAR : fog_far;
    }
    /* Two stores; gfx_set_far and gfx_set_fog set it too, so set it every frame. */
    scene_set_limits(g_far_now, glide_fog_on());
    fog_apply(fog_near, fog_far);
    sky = outdoor && level->sky && sky_on() && sky_ready() && !under;
    sky_alpha = 255;
    if (sky) {
        int hr, hg, hb;

        /* The game changed the sky in place (another climate): the same tiles, another picture. */
        if (level->sky_serial != g_sky_serial) {
            sky_change(level->sky);
            g_sky_serial = level->sky_serial;
        }
        sky_update(clock_minute(), w.sky);
        sky_horizon(&hr, &hg, &hb);
        /*
         * The fog is the horizon's colour exactly, so a fogged house meets
         * the bottom of the sky with no seam; the weather's own sky is
         * already grey. A stroke of lightning whitens it.
         */
        hr = df_iround((float)hr + 160.0f * flash);
        hg = df_iround((float)hg + 160.0f * flash);
        hb = df_iround((float)hb + 170.0f * flash);
        glide_sky_color(1, hr, hg, hb);
        /*
         * The sky fades into the fog as the fog closes in, and in a thick
         * fog is not drawn at all: half a screen of fill saved. A stroke
         * fades it into the whitened fog too, so the whole sky lights up
         * and a far roof is not a pale shape on a dark sky.
         */
        sky_alpha = df_iround(255.0f * (w.fog - 500.0f) * (1.0f / 2100.0f) * (1.0f - flash));
        sky_alpha = sky_alpha < 0 ? 0 : (sky_alpha > 255 ? 255 : sky_alpha);
    } else if (under) {
        /* Under water: the clear and the fog are the water's colour. */
        glide_sky_color(1, (int)((under_rgb >> 16) & 255u), (int)((under_rgb >> 8) & 255u),
                        (int)(under_rgb & 255u));
    } else {
        glide_sky_color(0, 0, 0, 0);
    }
    glide_clear();
    clip_slopes();
    camera_basis(cam, &basis);
    /* Before the sky: the HUD may have left blending or a fade on. */
    glide_sprite_alpha(0);
    /* The render passes at the x87's single precision: float all through (df_fpu_single). */
    fpu_was = g_fpu_single ? df_fpu_single() : 0;
    prof_begin(PROF_SKY);
    if (sky && sky_alpha > 8) {
        if (sky_alpha < 255) {
            glide_opacity(sky_alpha, 0);
        }
        draw_sky(cam, &basis);
        if (sky_alpha < 255) {
            glide_opacity(-1, 0);
        }
    }
    prof_end(PROF_SKY);
    /* The place's ambient, white when it is unlit. See docs/lighting.md. */
    ambient = lit ? g_shade[DAYLIGHT_STEPS / 2] : light_ambient_rgb();
    glide_tint(ambient);
    tinted = ambient;
    g_lights_drawn = 0;
    g_decals_drawn = 0;
    prof_begin(PROF_CULL);
    norder = scene_visible_models(&level->scene, cam, order, VIS_MAX);
    prof_end(PROF_CULL);
    prof_begin(PROF_MODELS);
    g_vis_models = norder;
    cap_warn(&g_cap_models, norder, "models");
    for (oi = 0; oi < norder; oi++) {
        const Instance *in;

        in = &level->scene.models[order[oi]];
        if (!in->mesh) {
            continue;
        }
        /*
         * Pictures on the card by distance (a streamed world, a place past
         * the card): how near each was drawn decides how sharp it is sent.
         */
        if (noted && in->mesh->have_keys) {
            const Bound *b;
            float dx, dy, dz, d;
            int k;

            b = &level->scene.bounds[order[oi]];
            dx = b->cx - cam->x;
            dy = b->cy - cam->y;
            dz = b->cz - cam->z;
            d = df_sqrt(dx * dx + dy * dy + dz * dz) - (b->ex > b->ez ? b->ex : b->ez);
            for (k = 0; k < in->mesh->nslot_keys; k++) {
                texcache_note((int)in->mesh->slot_keys[k], d > 0.0f ? d : 0.0f);
            }
        }
        /* A moving model indoors has no decals: one colour from the lights round it. */
        if (!lit && level->scene.nmovers > 0) {
            df_u32 c;

            c = light_model_rgb(order[oi], ambient);
            if (c != tinted) {
                glide_tint(c);
                tinted = c;
            }
        }
        if (lit) {
            /* The key light in the mesh's frame: one dot product a face. */
            kx = in->m[0] * dl.kx + in->m[3] * dl.ky + in->m[6] * dl.kz;
            ky = in->m[1] * dl.kx + in->m[4] * dl.ky + in->m[7] * dl.kz;
            kz = in->m[2] * dl.kx + in->m[5] * dl.ky + in->m[8] * dl.kz;
        }
        if (in->mesh->nfaces > 0 && in->mesh->faces) {
            int f;

            for (f = 0; f < in->mesh->nfaces; f++) {
                Face *face;
                const Tri *tri;

                face = &in->mesh->faces[f];
                if (face->ntris < 1 || face->first < 0) {
                    continue;
                }
                tri = &in->mesh->tris[face->first];
                if (g_faces && face->ntris >= 2 &&
                    !instance_faces(in, cam, &basis,
                                    face->nx, face->ny, face->nz,
                                    tri->x[0], tri->y[0], tri->z[0])) {
                    g_faces_drop++;
                    continue;
                }
                g_faces_kept++;
                if (lit) {
                    float d;
                    int step;

                    d = face->nx * kx + face->ny * ky + face->nz * kz;
                    /* A lone triangle is drawn from both sides: lit from either. */
                    if (face->ntris == 1 && d < 0.0f) {
                        d = -d;
                    }
                    step = d > 0.0f ? df_iround(d * (float)DAYLIGHT_STEPS) : 0;
                    if (step > DAYLIGHT_STEPS) {
                        step = DAYLIGHT_STEPS;
                    }
                    glide_tint(g_shade[step]);
                }
                draw_face(cam, &basis, level, in, face);
            }
        } else {
            if (lit) {
                glide_tint(g_shade[DAYLIGHT_STEPS / 2]);
            }
            draw_tris(cam, &basis, level, in, 0, in->mesh->ntris);
        }
    }
    prof_end(PROF_MODELS);
    prof_begin(PROF_DECALS);
    draw_lights(cam, &basis);
    prof_end(PROF_DECALS);
    /* Lit windows: the town's glass, with the lamps. */
    prof_begin(PROF_WINDOWS);
    if (lit) {
        draw_windows(cam, &basis, level, order, norder, dl.lamps, fog_near, fog_far);
    }
    prof_end(PROF_WINDOWS);
    prof_begin(PROF_SPRITES);
    glide_sprite_alpha(1);
    if (g_sprite_point) {
        glide_point_sample(1);
    }
    norder = scene_visible_sprites(&level->scene, cam, order, VIS_MAX);
    g_vis_sprites = norder;
    cap_warn(&g_cap_sprites, norder, "sprites");
    sprite_day = lit ? daylight_sprite(&dl, basis.fx, basis.fz) : 0xffffffu;
    for (oi = 0; oi < norder; oi++) {
        /* One colour per sprite from the lights around it; torches full bright. */
        glide_tint(outdoor ? light_sprite_over(order[oi], sprite_day)
                           : light_sprite_rgb(order[oi]));
        if (noted) {
            const Flat *fl;
            float dx, dz;

            fl = &level->scene.sprites[order[oi]];
            dx = fl->x - cam->x;
            dz = fl->z - cam->z;
            texcache_note((int)fl->slot, df_sqrt(dx * dx + dz * dz));
        }
        draw_flat(cam, &basis, level, &level->scene.sprites[order[oi]]);
    }
    /* The entities, each as the view of it the camera has (docs/entities.md), in daylight's colour. */
    if (level->scene.entities) {
        glide_tint(outdoor ? sprite_day : light_ambient_rgb());
        draw_entities(cam, &basis, level, noted);
    }
    glide_sprite_alpha(0);
    if (g_sprite_point) {
        glide_point_sample(0);
    }
    glide_tint(0xffffffu);
    /* The water's surface, over everything under it (docs/water.md). */
    draw_water_pass(cam, &basis, &level->scene);
    prof_end(PROF_SPRITES);
    prof_begin(PROF_PRECIP);
    if (outdoor) {
        float light;

        light = lit ? (dl.ar + dl.ag + dl.ab) * (1.0f / 3.0f) + 0.3f * dl.kg : 1.0f;
        draw_precip(cam, &basis, &w, light + flash, secs);
    }
    prof_end(PROF_PRECIP);
    /* Back before the overlay: a viewer's panel may print a double (a world position). */
    if (g_fpu_single) {
        df_fpu_restore(fpu_was);
    }
    prof_begin(PROF_OVERLAY);
    draw_light_view(cam, &basis);
    draw_sound_view(cam, &basis, &level->scene);
    draw_use_view(cam, &basis, &level->scene);
    draw_trigger_view(cam, &basis, &level->scene);
    draw_saying();
    if (g_under) {
        g_under();
    }
    hud_draw(cam);
    if (g_extra) {
        g_extra();
    }
    shot_note();
    build_legend();
    draw_covers();
    /* Coming through a passage: the new place comes up from black (draw_hold). */
    if (g_fade_left > 0.0f && g_fade_total > 0.0f) {
        wash_glass(df_iround(255.0f * g_fade_left / g_fade_total));
        g_fade_left -= secs;
    }
    prof_end(PROF_OVERLAY);
    /*
     * A streamed world: sharper copies of what was drawn near, after the
     * frame (docs/streaming.md); a place loaded whole, of its entities'
     * pictures (docs/memory.md).
     */
    if (texcache_resident()) {
        prof_begin(PROF_STREAM);
        texcache_stream_service(g_focal, STREAM_UPLOAD_BYTES);
        prof_end(PROF_STREAM);
    }
    prof_begin(PROF_SWAP);
    glide_swap();
    prof_end(PROF_SWAP);
    prof_end(PROF_FRAME);
    {
        LARGE_INTEGER now;

        frame_time_read(&now);
        prof_frame(now.LowPart, g_ft_freq);
    }
}

static void cover_bar(float x, float y, float w, float h, float r, float g, float b)
{
    DrawVert v[4];
    int i;

    if (w < 1.0f || h < 1.0f) {
        return;
    }
    for (i = 0; i < 4; i++) {
        v[i].oow = 1.0f;
        v[i].ooz = 1.0f;
        v[i].sow = 0.0f;
        v[i].tow = 0.0f;
        v[i].r = r;
        v[i].g = g;
        v[i].b = b;
    }
    v[0].x = x;
    v[0].y = y + h;
    v[1].x = x + w;
    v[1].y = y + h;
    v[2].x = x + w;
    v[2].y = y;
    v[3].x = x;
    v[3].y = y;
    glide_polygon(v, 4);
}

/*
 * The occlusion cells of this frame as a green wash. A model or sprite
 * whose screen box lands only on green, and is behind it, was not
 * drawn. One quad per run of cells in a row, so a wall is a few dozen
 * quads, not one per cell.
 */
static void draw_covers(void)
{
    int i;
    int n;
    float x0, y0, x1, y1;

    if (!scene_cover_draw()) {
        return;
    }
    n = scene_cover_count();
    if (n < 1) {
        return;
    }
    glide_dim(1, 72);
    for (i = 0; i < n; i++) {
        scene_cover_at(i, &x0, &y0, &x1, &y1);
        cover_bar(x0, y0, x1 - x0, y1 - y0, 40.0f, 255.0f, 80.0f);
    }
    glide_dim(0, 0);
}
