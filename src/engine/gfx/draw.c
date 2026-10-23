#include "engine/gfx/draw.h"
#include "engine/assets/level.h"
#include "engine/gfx/glide.h"
#include "engine/ui/console.h"
#include "engine/ui/shot.h"
#include "engine/ui/build.h"
#include "engine/ui/hud.h"
#include "engine/gfx/texcache.h"

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
#define CLIP_MAX 16

static void (*g_extra)(void);
static void (*g_under)(void);
static float g_far = DF_FAR;
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

static void clip_slopes(void)
{
    g_focal = camera_focal();
    if (g_focal < 1.0f) {
        g_focal = 1.0f;
    }
    g_clip_x = ((float)DF_VIEW_W * 0.5f + CLIP_MARGIN) / g_focal;
    g_clip_y = ((float)DF_VIEW_H * 0.5f + CLIP_MARGIN) / g_focal;
}

void draw_set_far(float far)
{
    if (far < DF_NEAR * 2.0f) {
        far = DF_NEAR * 2.0f;
    }
    if (far > DF_FAR_MAX) {
        far = DF_FAR_MAX;
    }
    g_far = far;
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
        return g_far - v->z;
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

static int clip_view(const Vtx *in, Vtx *out)
{
    Vtx buf[2][CLIP_MAX];
    int src;
    int nsrc;
    int plane;
    int i;

    buf[0][0] = in[0];
    buf[0][1] = in[1];
    buf[0][2] = in[2];
    src = 0;
    nsrc = 3;
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

    if (!camera_project(v->x, v->y, v->z, &sx, &sy, &oow, &ooz)) {
        return 0;
    }
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
 * 1 when every vertex is inside the near plane, the far clip, and the
 * picture. A triangle that crosses the glass is cut to it. One that
 * is already on the glass is projected directly.
 */
static int tri_inside(const Vtx *v)
{
    int i;

    for (i = 0; i < 3; i++) {
        float x;
        float y;
        float z;
        float gx;
        float gy;

        x = v[i].x;
        y = v[i].y;
        z = v[i].z;
        if (z < DF_NEAR || z > g_far) {
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

static void emit_tri(const Vtx *view, float inv_w, float inv_h,
                     float cr, float cg, float cb)
{
    Vtx clipped[CLIP_MAX];
    int nout;

    if (tri_inside(view)) {
        submit(view, 3, inv_w, inv_h, cr, cg, cb);
        return;
    }
    nout = clip_view(view, clipped);
    if (nout < 3) {
        return;
    }
    submit(clipped, nout, inv_w, inv_h, cr, cg, cb);
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
    for (i = 1; i < n - 1; i++) {
        glide_triangle(&dv[0], &dv[i], &dv[i + 1]);
    }
}

static const Image *level_image(const Level *level, int slot);

/*
 * image_inv is two divides. A face repeats the same picture, so the
 * pair is stored by slot and reused. The pointer compare notices a
 * new place without clearing the table.
 */
#define INV_SLOTS 512
static const Image *g_inv_img[INV_SLOTS];
static float g_inv_w[INV_SLOTS];
static float g_inv_h[INV_SLOTS];

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

static void flat_corner(const Camera *cam, const Basis *basis,
                         float wx, float wy, float wz, float u, float v, Vtx *out)
{
    camera_view(cam, basis, wx, wy, wz, &out->x, &out->y, &out->z);
    out->u = u;
    out->v = v;
}

/* Upright quad. It faces yaw and does not tilt with pitch. */
static const Image *level_image(const Level *level, int slot)
{
    if (!level || slot < 0 || slot >= level->nimages) {
        return 0;
    }
    return level->images[slot];
}

static void draw_flat(const Camera *cam, const Basis *basis, const Level *level,
                      const Flat *flat)
{
    const Image *img;
    float hx;
    float hy;
    float rx, ry, rz;
    float uw;
    float uh;
    float inv_w;
    float inv_h;
        Vtx corner[4];
        Vtx tri[3];
        int pass;

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
    uw = (float)img->width;
    uh = (float)img->height;
    image_scale(img, (int)flat->slot, &inv_w, &inv_h);
    /* 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left. v = 0 is the top row. */
    flat_corner(cam, basis,
                flat->x - rx * hx, flat->y - ry * hx + hy, flat->z - rz * hx,
                0.0f, 0.0f, &corner[0]);
    flat_corner(cam, basis,
                flat->x + rx * hx, flat->y + ry * hx + hy, flat->z + rz * hx,
                uw, 0.0f, &corner[1]);
    flat_corner(cam, basis,
                flat->x + rx * hx, flat->y + ry * hx - hy, flat->z + rz * hx,
                uw, uh, &corner[2]);
    flat_corner(cam, basis,
                flat->x - rx * hx, flat->y - ry * hx - hy, flat->z - rz * hx,
                0.0f, uh, &corner[3]);
    texcache_bind((int)flat->slot);
    for (pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            tri[0] = corner[0];
            tri[1] = corner[1];
            tri[2] = corner[2];
        } else {
            tri[0] = corner[0];
            tri[1] = corner[2];
            tri[2] = corner[3];
        }
        emit_tri(tri, inv_w, inv_h, 255.0f, 255.0f, 255.0f);
    }
}

static void draw_covers(void);

void draw_frame(const Camera *cam, const Level *level)
{
    Basis basis;
    int order[VIS_MAX];
    int norder;
    int oi;

    if (!level) {
        return;
    }
    console_bind(cam);
    console_pump();
    if (console_open()) {
        console_present(cam);
        return;
    }
    console_release();
    g_vis_models = 0;
    g_vis_sprites = 0;
    g_faces_kept = 0;
    g_faces_drop = 0;
    glide_clear();
    /*
     * Text binds its own sheet through glide_bind. That leaves g_bound
     * naming a model slot whose texels are no longer current, so the
     * next frame would paint the font across those faces.
     */
    texcache_forget();
    clip_slopes();
    camera_basis(cam, &basis);
    glide_sprite_alpha(0);
    norder = scene_visible_models(&level->scene, cam, order, VIS_MAX);
    g_vis_models = norder;
    for (oi = 0; oi < norder; oi++) {
        const Instance *in;

        in = &level->scene.models[order[oi]];
        if (!in->mesh) {
            continue;
        }
        if (in->mesh->nfaces > 0 && in->mesh->faces) {
            int f;

            for (f = 0; f < in->mesh->nfaces; f++) {
                const Face *face;
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
                draw_tris(cam, &basis, level, in, face->first, face->ntris);
            }
        } else {
            draw_tris(cam, &basis, level, in, 0, in->mesh->ntris);
        }
    }
    glide_sprite_alpha(1);
    norder = scene_visible_sprites(&level->scene, cam, order, VIS_MAX);
    g_vis_sprites = norder;
    for (oi = 0; oi < norder; oi++) {
        draw_flat(cam, &basis, level, &level->scene.sprites[order[oi]]);
    }
    glide_sprite_alpha(0);
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
    glide_swap();
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
    glide_triangle(&v[0], &v[1], &v[2]);
    glide_triangle(&v[0], &v[2], &v[3]);
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
