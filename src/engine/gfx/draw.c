#include "engine/gfx/draw.h"
#include "engine/assets/level.h"
#include "engine/gfx/glide.h"
#include "engine/ui/console.h"
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
/* Matches the block loader's placement cap, so the visible list is not cut. */
#define VIS_MAX 2048

/*
 * SST-1 screen coordinates live in about +/-2048. Past that the edge
 * walker wraps and the triangle turns into stripes. camera_project is
 *   sx = 320 + x * focal / z
 * so a guard of 1600 pixels from the centre stays inside that range
 * when the camera pitches or a wall gets close. The guard follows
 * camera_focal, so a wider field does not push the clip into the SST-1 wrap.
 */
#define GLIDE_GUARD 1600.0f
#define CLIP_MAX 16

static void (*g_extra)(void);
static void (*g_under)(void);
static float g_far = DF_FAR;
static int g_faces = 1;
static int g_vis_models;
static int g_vis_sprites;
static int g_faces_kept;
static int g_faces_drop;

static float clip_guard(void)
{
    return GLIDE_GUARD / camera_focal();
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
        return v->z * clip_guard() + v->x;
    case 3:
        return v->z * clip_guard() - v->x;
    case 4:
        return v->z * clip_guard() + v->y;
    case 5:
        return v->z * clip_guard() - v->y;
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
        t = dist[i] / denom;
        lerp_vtx(&hit, &in[i], &in[j], t);
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

/* Local point straight into view space. One less round trip through memory. */
static void view_point(const Instance *in, const Camera *cam, const Basis *basis,
                       float lx, float ly, float lz, float *vx, float *vy, float *vz)
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
                      const Instance *in, int first, int count, int sharp)
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
        Vtx clipped[CLIP_MAX];
        int k;
        int nout;
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
        nout = clip_view(view, clipped);
        if (nout < 3) {
            continue;
        }
        /*
         * A null image is a texture the loader could not read.
         * Draw it flat magenta so the face is not silently dropped.
         */
        if (!img || img->width < 1 || img->height < 1) {
            glide_flat(1);
            submit(clipped, nout, 1.0f, 1.0f, 255.0f, 0.0f, 255.0f);
            glide_flat(0);
            continue;
        }
        texcache_bind((int)tri->slot, sharp);
        image_inv(img, &inv_w, &inv_h);
        submit(clipped, nout, inv_w, inv_h, 255.0f, 255.0f, 255.0f);
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
                      const Flat *flat, int sharp)
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
    Vtx clipped[CLIP_MAX];
    int nout;
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
    image_inv(img, &inv_w, &inv_h);
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
    texcache_bind((int)flat->slot, sharp);
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
        nout = clip_view(tri, clipped);
        if (nout < 3) {
            continue;
        }
        submit(clipped, nout, inv_w, inv_h, 255.0f, 255.0f, 255.0f);
    }
}

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
    texcache_detail(cam, level);
    camera_basis(cam, &basis);
    glide_sprite_alpha(0);
    norder = scene_visible_models(&level->scene, cam, order, VIS_MAX);
    g_vis_models = norder;
    for (oi = 0; oi < norder; oi++) {
        const Instance *in;
        int sharp_model;

        in = &level->scene.models[order[oi]];
        if (!in->mesh) {
            continue;
        }
        sharp_model = texcache_near_model(order[oi]);
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
                draw_tris(cam, &basis, level, in, face->first, face->ntris, sharp_model);
            }
        } else {
            draw_tris(cam, &basis, level, in, 0, in->mesh->ntris, sharp_model);
        }
    }
    glide_sprite_alpha(1);
    norder = scene_visible_sprites(&level->scene, cam, order, VIS_MAX);
    g_vis_sprites = norder;
    for (oi = 0; oi < norder; oi++) {
        draw_flat(cam, &basis, level, &level->scene.sprites[order[oi]],
                  texcache_near_flat(order[oi]));
    }
    glide_sprite_alpha(0);
    if (g_under) {
        g_under();
    }
    hud_draw(cam);
    if (g_extra) {
        g_extra();
    }
    glide_swap();
}
