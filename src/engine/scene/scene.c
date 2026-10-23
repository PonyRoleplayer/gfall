#include "engine/scene/scene.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"

/* 0 lists every stored entity, the same path as a null camera. */
static int g_cull = 1;

void scene_set_cull(int on)
{
    g_cull = on ? 1 : 0;
}

int scene_cull(void)
{
    return g_cull;
}

/*
 * Turns a local point into a world point. The three columns of m[] are
 * the rotated axes. Built once when the entity is placed.
 */

static void rot_x(float a, float *y, float *z)
{
    float c, s, y1, z1;

    c = df_cos(a);
    s = df_sin(a);
    y1 = *y * c - *z * s;
    z1 = *y * s + *z * c;
    *y = y1;
    *z = z1;
}

static void rot_y(float a, float *x, float *z)
{
    float c, s, x1, z1;

    c = df_cos(a);
    s = df_sin(a);
    x1 = *x * c + *z * s;
    z1 = -*x * s + *z * c;
    *x = x1;
    *z = z1;
}

static void rot_z(float a, float *x, float *y)
{
    float c, s, x1, y1;

    c = df_cos(a);
    s = df_sin(a);
    x1 = *x * c - *y * s;
    y1 = *x * s + *y * c;
    *x = x1;
    *y = y1;
}

/* X, then Y, then Z. */
static void rot_xyz(float rx, float ry, float rz, float *x, float *y, float *z)
{
    rot_x(rx, y, z);
    rot_y(ry, x, z);
    rot_z(rz, x, y);
}

static void set_axes(Instance *in,
                     void (*spin)(float, float, float, float *, float *, float *),
                     float rx, float ry, float rz)
{
    float x, y, z;

    x = 1.0f;
    y = 0.0f;
    z = 0.0f;
    spin(rx, ry, rz, &x, &y, &z);
    in->m[0] = x;
    in->m[3] = y;
    in->m[6] = z;

    x = 0.0f;
    y = 1.0f;
    z = 0.0f;
    spin(rx, ry, rz, &x, &y, &z);
    in->m[1] = x;
    in->m[4] = y;
    in->m[7] = z;

    x = 0.0f;
    y = 0.0f;
    z = 1.0f;
    spin(rx, ry, rz, &x, &y, &z);
    in->m[2] = x;
    in->m[5] = y;
    in->m[8] = z;
}

void instance_set_rotation(Instance *in, float rx, float ry, float rz)
{
    set_axes(in, rot_xyz, df_wrap_tau(rx), df_wrap_tau(ry), df_wrap_tau(rz));
}

void instance_point(const Instance *in, float lx, float ly, float lz,
                    float *x, float *y, float *z)
{
    *x = in->m[0] * lx + in->m[1] * ly + in->m[2] * lz + in->x;
    *y = in->m[3] * lx + in->m[4] * ly + in->m[5] * lz + in->y;
    *z = in->m[6] * lx + in->m[7] * ly + in->m[8] * lz + in->z;
}

void mesh_measure(Mesh *mesh)
{
    float minx, miny, minz, maxx, maxy, maxz;
    int t;
    int k;

    if (!mesh || mesh->ntris < 1 || !mesh->tris) {
        return;
    }
    if (mesh->bound_n == mesh->ntris) {
        return;
    }
    minx = maxx = mesh->tris[0].x[0];
    miny = maxy = mesh->tris[0].y[0];
    minz = maxz = mesh->tris[0].z[0];
    for (t = 0; t < mesh->ntris; t++) {
        for (k = 0; k < 3; k++) {
            float x, y, z;

            x = mesh->tris[t].x[k];
            y = mesh->tris[t].y[k];
            z = mesh->tris[t].z[k];
            if (x < minx) {
                minx = x;
            }
            if (y < miny) {
                miny = y;
            }
            if (z < minz) {
                minz = z;
            }
            if (x > maxx) {
                maxx = x;
            }
            if (y > maxy) {
                maxy = y;
            }
            if (z > maxz) {
                maxz = z;
            }
        }
    }
    mesh->cx = (minx + maxx) * 0.5f;
    mesh->cy = (miny + maxy) * 0.5f;
    mesh->cz = (minz + maxz) * 0.5f;
    mesh->ex = (maxx - minx) * 0.5f;
    mesh->ey = (maxy - miny) * 0.5f;
    mesh->ez = (maxz - minz) * 0.5f;
    mesh->bound_n = mesh->ntris;
}

int instance_world_box(const Instance *in, Bound *out)
{
    float lx, ly, lz;
    float lex, ley, lez;

    if (!out || !in || !in->mesh) {
        return 0;
    }
    mesh_measure(in->mesh);
    if (in->mesh->bound_n != in->mesh->ntris || in->mesh->ntris < 1) {
        return 0;
    }
    lx = in->mesh->cx;
    ly = in->mesh->cy;
    lz = in->mesh->cz;
    lex = in->mesh->ex;
    ley = in->mesh->ey;
    lez = in->mesh->ez;
    instance_point(in, lx, ly, lz, &out->cx, &out->cy, &out->cz);
    out->ex = df_absf(in->m[0]) * lex + df_absf(in->m[1]) * ley + df_absf(in->m[2]) * lez;
    out->ey = df_absf(in->m[3]) * lex + df_absf(in->m[4]) * ley + df_absf(in->m[5]) * lez;
    out->ez = df_absf(in->m[6]) * lex + df_absf(in->m[7]) * ley + df_absf(in->m[8]) * lez;
    return 1;
}

int instance_faces(const Instance *in, const Camera *cam, const Basis *basis,
                   float nx, float ny, float nz,
                   float lx, float ly, float lz)
{
    float wx, wy, wz;
    float vx, vy, vz;
    float x1, z1;
    float vnx, vny, vnz;

    if (!in || !cam || !basis) {
        return 1;
    }
    if (nx * nx + ny * ny + nz * nz < 1.0e-8f) {
        return 1;
    }
    wx = in->m[0] * nx + in->m[1] * ny + in->m[2] * nz;
    wy = in->m[3] * nx + in->m[4] * ny + in->m[5] * nz;
    wz = in->m[6] * nx + in->m[7] * ny + in->m[8] * nz;
    x1 = wx * basis->rx + wz * basis->rz;
    z1 = wx * basis->fx + wz * basis->fz;
    vnx = x1;
    vny = wy * basis->cp - z1 * basis->sp;
    vnz = wy * basis->sp + z1 * basis->cp;
    instance_point(in, lx, ly, lz, &wx, &wy, &wz);
    camera_view(cam, basis, wx, wy, wz, &vx, &vy, &vz);
    return vnx * vx + vny * vy + vnz * vz < 0.0f;
}

static int fill_order(int count, int *out, int cap)
{
    int i;
    int n;

    if (!out || cap < 1 || count < 1) {
        return 0;
    }
    n = count;
    if (n > cap) {
        n = cap;
    }
    for (i = 0; i < n; i++) {
        out[i] = i;
    }
    return n;
}

/*
 * clip_far is the card clip. Fog, when on, is solid closer than that
 * and the clear is the same colour, so a box past the solid distance
 * cannot be seen. The fraction matches gfx_set_far. Glide will not
 * put solid fog closer than the fog start, and neither does this.
 */
static float g_clip = DF_FAR;
static int g_fog = 1;

void scene_set_limits(float clip_far, int fog)
{
    if (clip_far < DF_NEAR * 2.0f) {
        clip_far = DF_NEAR * 2.0f;
    }
    if (clip_far > DF_FAR_MAX) {
        clip_far = DF_FAR_MAX;
    }
    g_clip = clip_far;
    g_fog = fog ? 1 : 0;
}

static float visible_far(void)
{
    float far;
    float fog_far;

    far = g_clip;
    if (!g_fog) {
        return far;
    }
    fog_far = g_clip * (DF_FOG_FAR / DF_FAR);
    if (fog_far < DF_FOG_NEAR + 1.0f) {
        fog_far = DF_FOG_NEAR + 1.0f;
    }
    if (fog_far < far) {
        far = fog_far;
    }
    return far;
}

/* Inward normal. dot(n, point) + d >= 0 is inside. n is unit length. */
typedef struct Plane {
    float nx, ny, nz, d;
} Plane;

static void plane_through(Plane *p, float nx, float ny, float nz,
                          float px, float py, float pz)
{
    float len;
    float inv;

    len = nx * nx + ny * ny + nz * nz;
    __asm__ __volatile__("fsqrt" : "=t"(len) : "0"(len));
    if (len < 1.0e-8f) {
        p->nx = 0.0f;
        p->ny = 1.0f;
        p->nz = 0.0f;
        p->d = -py;
        return;
    }
    inv = 1.0f / len;
    p->nx = nx * inv;
    p->ny = ny * inv;
    p->nz = nz * inv;
    p->d = -(p->nx * px + p->ny * py + p->nz * pz);
}

/*
 * Screen edges, not the Glide guard. The guard is wide so a triangle
 * that crosses the picture stays inside +/-2048. A box that misses the
 * picture has no pixels. Slopes match camera_project: half the view
 * divided by the focal length.
 */
static void frustum_planes(const Camera *cam, float far, Plane *p)
{
    Basis basis;
    float fx, fy, fz;
    float rx, ry, rz;
    float ux, uy, uz;
    float sx, sy;

    camera_basis(cam, &basis);
    fx = basis.fx * basis.cp;
    fy = basis.sp;
    fz = basis.fz * basis.cp;
    rx = basis.rx;
    ry = 0.0f;
    rz = basis.rz;
    ux = -basis.sp * basis.fx;
    uy = basis.cp;
    uz = -basis.sp * basis.fz;
    sx = (float)DF_VIEW_W * 0.5f / camera_focal();
    sy = (float)DF_VIEW_H * 0.5f / camera_focal();

    /* Far first: a city is mostly past this plane. */
    plane_through(&p[0], -fx, -fy, -fz,
                  cam->x + fx * far, cam->y + fy * far, cam->z + fz * far);
    plane_through(&p[1], fx, fy, fz,
                  cam->x + fx * DF_NEAR, cam->y + fy * DF_NEAR, cam->z + fz * DF_NEAR);
    plane_through(&p[2], rx + fx * sx, ry + fy * sx, rz + fz * sx,
                  cam->x, cam->y, cam->z);
    plane_through(&p[3], -rx + fx * sx, -ry + fy * sx, -rz + fz * sx,
                  cam->x, cam->y, cam->z);
    plane_through(&p[4], ux + fx * sy, uy + fy * sy, uz + fz * sy,
                  cam->x, cam->y, cam->z);
    plane_through(&p[5], -ux + fx * sy, -uy + fy * sy, -uz + fz * sy,
                  cam->x, cam->y, cam->z);
}

/* True when every corner is outside this plane. */
static int box_out(const Plane *p, float cx, float cy, float cz,
                   float ex, float ey, float ez)
{
    float dist;
    float rad;

    dist = p->nx * cx + p->ny * cy + p->nz * cz + p->d;
    rad = ex * df_absf(p->nx) + ey * df_absf(p->ny) + ez * df_absf(p->nz);
    return dist + rad < 0.0f;
}

static int box_visible(const Plane *planes, float cx, float cy, float cz,
                       float ex, float ey, float ez)
{
    int i;

    for (i = 0; i < 6; i++) {
        if (box_out(&planes[i], cx, cy, cz, ex, ey, ez)) {
            return 0;
        }
    }
    return 1;
}

static int mesh_box(Mesh *mesh, float *cx, float *cy, float *cz,
                    float *ex, float *ey, float *ez)
{
    mesh_measure(mesh);
    if (!mesh || mesh->bound_n != mesh->ntris || mesh->ntris < 1) {
        return 0;
    }
    *cx = mesh->cx;
    *cy = mesh->cy;
    *cz = mesh->cz;
    *ex = mesh->ex;
    *ey = mesh->ey;
    *ez = mesh->ez;
    return 1;
}

/*
 * World box of the mesh. The half-size is the local half-size times
 * the absolute columns of m[], so a rotated model stays inside the box.
 */
static int instance_box(const Instance *in, float *cx, float *cy, float *cz,
                        float *ex, float *ey, float *ez)
{
    float lx, ly, lz;
    float lex, ley, lez;

    if (!in || !mesh_box(in->mesh, &lx, &ly, &lz, &lex, &ley, &lez)) {
        return 0;
    }
    instance_point(in, lx, ly, lz, cx, cy, cz);
    *ex = df_absf(in->m[0]) * lex + df_absf(in->m[1]) * ley + df_absf(in->m[2]) * lez;
    *ey = df_absf(in->m[3]) * lex + df_absf(in->m[4]) * ley + df_absf(in->m[5]) * lez;
    *ez = df_absf(in->m[6]) * lex + df_absf(in->m[7]) * ley + df_absf(in->m[8]) * lez;
    return 1;
}

/*
 * A flat faces yaw and stays upright, so it fits in a box of half the
 * width on X and Z and half the height on Y.
 */
static int flat_box(const Flat *fl, float *cx, float *cy, float *cz,
                    float *ex, float *ey, float *ez)
{
    if (!fl || fl->w < 1.0f || fl->h < 1.0f) {
        return 0;
    }
    *cx = fl->x;
    *cy = fl->y;
    *cz = fl->z;
    *ex = fl->w * 0.5f;
    *ey = fl->h * 0.5f;
    *ez = *ex;
    return 1;
}

static int model_box(const Scene *scene, int index,
                     float *cx, float *cy, float *cz,
                     float *ex, float *ey, float *ez)
{
    const Bound *b;

    if (scene->bounds) {
        b = &scene->bounds[index];
        *cx = b->cx;
        *cy = b->cy;
        *cz = b->cz;
        *ex = b->ex;
        *ey = b->ey;
        *ez = b->ez;
        return 1;
    }
    return instance_box(&scene->models[index], cx, cy, cz, ex, ey, ez);
}

static int take_visible(const Scene *scene, const Plane *planes,
                        int first, int count, int *out, int cap, int n)
{
    int i;
    int end;

    end = first + count;
    if (first < 0) {
        first = 0;
    }
    if (end > scene->nmodels) {
        end = scene->nmodels;
    }
    for (i = first; i < end; i++) {
        float cx, cy, cz, ex, ey, ez;

        if (!model_box(scene, i, &cx, &cy, &cz, &ex, &ey, &ez)) {
            continue;
        }
        if (!box_visible(planes, cx, cy, cz, ex, ey, ez)) {
            continue;
        }
        if (n >= cap) {
            return n;
        }
        out[n++] = i;
    }
    return n;
}

/*
 * Occlusion.
 *
 * The frustum is a pyramid, so a wall in front of the eye still leaves
 * every box behind it "inside" out to the fog. Drawing those is the
 * slow frame: the floor is two models, the same street aimed at a wall
 * is a couple of hundred.
 *
 * The answer is a coarse depth picture built on the Pentium before
 * anything goes to the card:
 *
 *   1. The picture is cut into OCC_W x OCC_H cells of OCC_CELL pixels.
 *   2. Big front faces of the nearest models are projected and
 *      rasterized into those cells. A cell is written only when one
 *      face covers all of it. It keeps 1/z of the farthest point that
 *      face can have inside the cell. Of several faces, the nearest
 *      bound wins.
 *   3. A box is hidden when its screen rectangle lands only on written
 *      cells, and every one of them is nearer than the nearest point of
 *      the box.
 *
 * Every step errs toward drawing. A cell is written only when the face
 * is known to be over every pixel of it, and only with a depth no
 * nearer than the face really is there. A box is dropped only when
 * every pixel it could reach is behind such a cell. So the cull can
 * keep a model that turns out to be hidden, and the Z buffer sorts that
 * out, but it cannot drop one that has a pixel on the glass.
 *
 * Joining walls is not a separate step. Two panels of one facade fill
 * their own cells, and the box behind the pair is tested against all of
 * them. A gap between the panels is a run of empty cells, so whatever
 * shows through the gap stays. The price of one face per cell is the
 * seam: a cell that two faces share between them is left empty.
 *
 * The cost is held under the vertex work it saves. A cell is 16 pixels,
 * so the grid is 40 x 30 floats and sits in the Pentium's 16 KB data
 * cache. 8-pixel cells hid 29% of what the frustum kept in Privateer's
 * Hold against 27.5% for 16, at about twice the fill. A row of cells is
 * a span solved per edge, not a test per cell. A model whose box is
 * already hidden adds no faces. See docs/culling.md for the numbers.
 */

#define OCC_CELL 16
/*
 * Pixel to cell is a multiply and df_ifloor, never a C cast: on the
 * x87 a cast switches the rounding mode, about 30 cycles a time.
 */
#define OCC_INV (1.0f / (float)OCC_CELL)
#define OCC_W (DF_VIEW_W / OCC_CELL)
#define OCC_H (DF_VIEW_H / OCC_CELL)
/* A model is looked at only when part of its box is nearer than this. */
#define OCC_NEAR 640.0f
/* A screen box smaller than 3x3 cells cannot fill enough cells to pay. */
#define OCC_MODEL_AREA (9.0f * OCC_CELL * OCC_CELL)
/* Nearest models looked at per frame, and faces drawn into the cells. */
#define OCC_MODELS 32
#define OCC_FACES 96
/*
 * Faces are clipped this far past the picture, not to its edge, so a
 * wall that fills the glass also fills the border cells.
 */
#define OCC_MARGIN 16.0f
/*
 * A cell is tested grown by this much on every side. It covers the
 * card's 1/16-pixel vertex snap and float error at the outline.
 */
#define OCC_EDGE 0.25f
/*
 * A box has to be this much farther than the cell, as a factor on 1/z.
 * 0.1% keeps a sign or a door that is flush with its wall.
 */
#define OCC_SLACK 1.001f
/*
 * A plane is rebuilt from its fan as one polygon of at most OCC_POLY
 * corners. Each of the five clip planes adds at most one corner.
 */
#define OCC_POLY 16
#define CV_MAX (OCC_POLY + 6)
/* Corners closer than this, in pixels, after clipping are one corner. */
#define OCC_DUP 0.01f
/*
 * A corner may turn the wrong way by this much, as sin^2 of the angle,
 * before the outline counts as notched: float error on a clipped
 * corner, about 1e-5, not a notch a cell could sit in.
 */
#define OCC_BEND 1.0e-8f

/* 1/z per cell. 0 is no cover. */
static float g_occ[OCC_W * OCC_H];
/* Written cells, inclusive. Empty when x0 > x1. Clearing reads only this. */
static int g_occ_x0 = OCC_W;
static int g_occ_y0 = OCC_H;
static int g_occ_x1 = -1;
static int g_occ_y1 = -1;
static int g_occ_on = 1;
static int g_occ_faces;
static int g_occ_hidden;
static int g_cover_draw;

void scene_set_occlude(int on)
{
    g_occ_on = on ? 1 : 0;
}

int scene_occlude(void)
{
    return g_occ_on;
}

void scene_occ_stats(int *faces, int *hidden)
{
    if (faces) {
        *faces = g_occ_faces;
    }
    if (hidden) {
        *hidden = g_occ_hidden;
    }
}

/* Clears last frame's cells. Only the rectangle that was written. */
static void occ_reset(void)
{
    int y;

    if (g_occ_x0 <= g_occ_x1) {
        for (y = g_occ_y0; y <= g_occ_y1; y++) {
            mem_zero(&g_occ[y * OCC_W + g_occ_x0],
                     (df_u32)(g_occ_x1 - g_occ_x0 + 1) * (df_u32)sizeof(float));
        }
    }
    g_occ_x0 = OCC_W;
    g_occ_y0 = OCC_H;
    g_occ_x1 = -1;
    g_occ_y1 = -1;
    g_occ_faces = 0;
}

/*
 * The camera as three rows, the same numbers camera_view uses: view x
 * is r, view y is u, view z is f. Built once per call, so a box or a
 * vertex is three dot products and no function call.
 */
typedef struct View {
    float ex, ey, ez;
    float rx, ry, rz;
    float ux, uy, uz;
    float fx, fy, fz;
    float focal;
} View;

static void view_build(View *v, const Camera *cam)
{
    Basis b;

    camera_basis(cam, &b);
    v->ex = cam->x;
    v->ey = cam->y;
    v->ez = cam->z;
    v->rx = b.rx;
    v->ry = 0.0f;
    v->rz = b.rz;
    v->ux = -b.sp * b.fx;
    v->uy = b.cp;
    v->uz = -b.sp * b.fz;
    v->fx = b.cp * b.fx;
    v->fy = b.sp;
    v->fz = b.cp * b.fz;
    v->focal = camera_focal();
}

/*
 * Screen rectangle of a world box, and the view depth of its nearest
 * point. The box is turned into view space as a center plus half-sizes,
 * the same trick as instance_world_box, so the view box holds every
 * corner. x / z over a depth range peaks at one end of it, so two
 * divisions bound the whole box. 0 when part of it is nearer than the
 * near plane: the box then reaches the eye and is never hidden.
 */
static int box_screen(const View *v, float cx, float cy, float cz,
                      float ex, float ey, float ez,
                      float *sx0, float *sy0, float *sx1, float *sy1,
                      float *znear)
{
    float dx, dy, dz;
    float vx, vy, vz;
    float hx, hy, hz;
    float i0, i1;
    float a, b;

    dx = cx - v->ex;
    dy = cy - v->ey;
    dz = cz - v->ez;
    vz = dx * v->fx + dy * v->fy + dz * v->fz;
    hz = ex * df_absf(v->fx) + ey * df_absf(v->fy) + ez * df_absf(v->fz);
    if (vz - hz < DF_NEAR) {
        return 0;
    }
    vx = dx * v->rx + dy * v->ry + dz * v->rz;
    vy = dx * v->ux + dy * v->uy + dz * v->uz;
    hx = ex * df_absf(v->rx) + ey * df_absf(v->ry) + ez * df_absf(v->rz);
    hy = ex * df_absf(v->ux) + ey * df_absf(v->uy) + ez * df_absf(v->uz);
    i0 = v->focal / (vz - hz);
    i1 = v->focal / (vz + hz);
    a = (vx - hx) * i0;
    b = (vx - hx) * i1;
    *sx0 = (float)DF_VIEW_W * 0.5f + (a < b ? a : b);
    a = (vx + hx) * i0;
    b = (vx + hx) * i1;
    *sx1 = (float)DF_VIEW_W * 0.5f + (a > b ? a : b);
    a = (vy - hy) * i0;
    b = (vy - hy) * i1;
    *sy0 = (float)DF_VIEW_H * 0.5f + (a < b ? a : b);
    a = (vy + hy) * i0;
    b = (vy + hy) * i1;
    *sy1 = (float)DF_VIEW_H * 0.5f + (a > b ? a : b);
    *znear = vz - hz;
    return 1;
}

static float clampf(float x, float lo, float hi)
{
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

/*
 * Mesh space straight to view space: the camera rows times m[], built
 * once per model. A vertex is then nine multiplies, the cost draw.c
 * pays for the same point.
 */
typedef struct Xf {
    float m[9];
    float t[3];
} Xf;

typedef struct Cv {
    float x, y, z;
} Cv;

static void xf_build(Xf *x, const Instance *in, const View *v)
{
    float dx, dy, dz;
    int j;

    for (j = 0; j < 3; j++) {
        float ax, ay, az;

        /* Column j of m[] is local axis j in the world. */
        ax = in->m[j];
        ay = in->m[3 + j];
        az = in->m[6 + j];
        x->m[j] = v->rx * ax + v->ry * ay + v->rz * az;
        x->m[3 + j] = v->ux * ax + v->uy * ay + v->uz * az;
        x->m[6 + j] = v->fx * ax + v->fy * ay + v->fz * az;
    }
    dx = in->x - v->ex;
    dy = in->y - v->ey;
    dz = in->z - v->ez;
    x->t[0] = v->rx * dx + v->ry * dy + v->rz * dz;
    x->t[1] = v->ux * dx + v->uy * dy + v->uz * dz;
    x->t[2] = v->fx * dx + v->fy * dy + v->fz * dz;
}

static void xf_point(const Xf *x, float lx, float ly, float lz, Cv *o)
{
    o->x = x->m[0] * lx + x->m[1] * ly + x->m[2] * lz + x->t[0];
    o->y = x->m[3] * lx + x->m[4] * ly + x->m[5] * lz + x->t[1];
    o->z = x->m[6] * lx + x->m[7] * ly + x->m[8] * lz + x->t[2];
}

/*
 * Near, then the four picture edges pushed out by OCC_MARGIN. hx and
 * hy are those slopes. No far plane: a wall that runs past the fog
 * still hides what is behind it, and that is farther still.
 */
static float cv_dist(int plane, float hx, float hy, const Cv *v)
{
    switch (plane) {
    case 0:
        return v->z - DF_NEAR;
    case 1:
        return v->z * hx + v->x;
    case 2:
        return v->z * hx - v->x;
    case 3:
        return v->z * hy + v->y;
    default:
        return v->z * hy - v->y;
    }
}

static int cv_clip(const Cv *in, int n, Cv *out, int plane, float hx, float hy)
{
    float dist[CV_MAX];
    int i;
    int nout;

    if (n > CV_MAX) {
        n = CV_MAX;
    }
    nout = 0;
    for (i = 0; i < n; i++) {
        dist[i] = cv_dist(plane, hx, hy, &in[i]);
    }
    for (i = 0; i < n; i++) {
        int j;
        int ain;
        int bin;
        float denom;
        float t;
        Cv hit;

        j = (i + 1) % n;
        ain = dist[i] >= 0.0f;
        bin = dist[j] >= 0.0f;
        if (ain && bin) {
            if (nout < CV_MAX) {
                out[nout++] = in[j];
            }
            continue;
        }
        if (!ain && !bin) {
            continue;
        }
        denom = dist[i] - dist[j];
        if (denom > -1.0e-8f && denom < 1.0e-8f) {
            continue;
        }
        t = dist[i] / denom;
        hit.x = in[i].x + (in[j].x - in[i].x) * t;
        hit.y = in[i].y + (in[j].y - in[i].y) * t;
        hit.z = in[i].z + (in[j].z - in[i].z) * t;
        if (nout < CV_MAX) {
            out[nout++] = hit;
        }
        if (bin && nout < CV_MAX) {
            out[nout++] = in[j];
        }
    }
    return nout;
}

/* Twice the signed area. Positive turns left. */
static float poly_area2(const float *sx, const float *sy, int n)
{
    float a;
    int i;

    a = 0.0f;
    for (i = 0; i < n; i++) {
        int j;

        j = (i + 1) % n;
        a += sx[i] * sy[j] - sx[j] * sy[i];
    }
    return a;
}

/*
 * 1 when the outline, already turning left, is convex: no corner turns
 * right past OCC_BEND, and x reverses direction at most twice on the
 * way round. Turning one way alone would still pass a star that winds
 * twice. The x count refuses it.
 */
static int poly_convex(const float *px, const float *py, int n)
{
    int first;
    int last;
    int flips;
    int i;

    first = 0;
    last = 0;
    flips = 0;
    for (i = 0; i < n; i++) {
        float ax, ay, bx, by;
        float cross;
        int j;
        int k;
        int sx;

        j = (i + 1) % n;
        k = (i + 2) % n;
        ax = px[j] - px[i];
        ay = py[j] - py[i];
        bx = px[k] - px[j];
        by = py[k] - py[j];
        cross = ax * by - ay * bx;
        if (cross < 0.0f &&
            cross * cross > OCC_BEND * (ax * ax + ay * ay) * (bx * bx + by * by)) {
            return 0;
        }
        sx = ax > OCC_DUP ? 1 : (ax < -OCC_DUP ? -1 : 0);
        if (sx == 0) {
            continue;
        }
        if (first == 0) {
            first = sx;
        } else if (sx != last) {
            flips++;
        }
        last = sx;
    }
    if (first != 0 && first != last) {
        flips++;
    }
    return flips <= 2;
}

/* 1 when the cell at origin x, y is inside every edge. */
static int cell_in(const float *ea, const float *eb, const float *ec, int n,
                   float x, float y)
{
    int i;

    for (i = 0; i < n; i++) {
        if (ea[i] * x + eb[i] * y + ec[i] < 0.0f) {
            return 0;
        }
    }
    return 1;
}

/*
 * Writes the cells the outline fully covers. The outline turns left.
 * w(sx, sy) = wa * sx + wb * sy + wc is 1/z of the plane the face sits
 * on or in front of, and wfar is 1/z of its farthest clipped vertex.
 * Both are bounds on how far the face can be, so a cell keeps the
 * larger, nearer, of the two.
 *
 * Each edge is a line a * x + b * y + c, positive inside. Over a cell
 * that is smallest at one corner, picked by the signs of a and b, so a
 * cell is inside when that one corner is. With that corner baked into
 * c, "inside" is a * x + r >= 0 for the cell origin x, where r is the
 * row's b * y + c. An edge with a > 0 bounds the run from the left, one
 * with a < 0 from the right, so each row's run of covered cells is one
 * multiply per edge, not a test per cell. The two end cells are then
 * checked edge by edge, so rounding in that division cannot let in a
 * cell the outline misses. The same corner rule gives the farthest w
 * in a cell, and w steps by wa * OCC_CELL along the run.
 */
static void occ_fill(const float *px, const float *py, int n,
                     float wa, float wb, float wc, float wfar)
{
    float ea[CV_MAX];
    float eb[CV_MAX];
    float ec[CV_MAX];
    float ia[CV_MAX];
    float minx, miny, maxx, maxy;
    float wox, woy;
    int gx0, gy0, gx1, gy1;
    int gy;
    int i;

    minx = maxx = px[0];
    miny = maxy = py[0];
    for (i = 0; i < n; i++) {
        int j;
        float a, b;

        if (px[i] < minx) {
            minx = px[i];
        }
        if (px[i] > maxx) {
            maxx = px[i];
        }
        if (py[i] < miny) {
            miny = py[i];
        }
        if (py[i] > maxy) {
            maxy = py[i];
        }
        j = (i + 1) % n;
        a = -(py[j] - py[i]);
        b = px[j] - px[i];
        /* Bake in the worst corner of the grown cell, relative to its origin. */
        ec[i] = -(a * px[i] + b * py[i]) +
                a * (a > 0.0f ? -OCC_EDGE : (float)OCC_CELL + OCC_EDGE) +
                b * (b > 0.0f ? -OCC_EDGE : (float)OCC_CELL + OCC_EDGE);
        ea[i] = a;
        eb[i] = b;
        ia[i] = a != 0.0f ? 1.0f / a : 0.0f;
    }
    /* Only cells wholly inside the outline's box can be covered. */
    gx0 = df_ifloor((clampf(minx, 0.0f, (float)DF_VIEW_W) + (float)OCC_CELL - 0.001f) * OCC_INV);
    gy0 = df_ifloor((clampf(miny, 0.0f, (float)DF_VIEW_H) + (float)OCC_CELL - 0.001f) * OCC_INV);
    gx1 = df_ifloor(clampf(maxx, 0.0f, (float)DF_VIEW_W) * OCC_INV) - 1;
    gy1 = df_ifloor(clampf(maxy, 0.0f, (float)DF_VIEW_H) * OCC_INV) - 1;
    if (gx0 > gx1 || gy0 > gy1) {
        return;
    }
    wox = wa > 0.0f ? 0.0f : (float)OCC_CELL;
    woy = wb > 0.0f ? 0.0f : (float)OCC_CELL;
    for (gy = gy0; gy <= gy1; gy++) {
        float y;
        float lo, hi;
        float w, dw;
        float *cell;
        int s, e;
        int gx;

        y = (float)(gy * OCC_CELL);
        lo = (float)(gx0 * OCC_CELL);
        hi = (float)(gx1 * OCC_CELL);
        for (i = 0; i < n && lo <= hi; i++) {
            float r;

            r = eb[i] * y + ec[i];
            if (ea[i] > 0.0f) {
                float x;

                x = -r * ia[i];
                if (x > lo) {
                    lo = x;
                }
            } else if (ea[i] < 0.0f) {
                float x;

                x = -r * ia[i];
                if (x < hi) {
                    hi = x;
                }
            } else if (r < 0.0f) {
                lo = hi + 1.0f;
            }
        }
        if (lo > hi) {
            continue;
        }
        s = df_ifloor(lo * OCC_INV);
        if ((float)(s * OCC_CELL) < lo) {
            s++;
        }
        e = df_ifloor(hi * OCC_INV);
        if (s < gx0) {
            s = gx0;
        }
        if (e > gx1) {
            e = gx1;
        }
        while (s <= e && !cell_in(ea, eb, ec, n, (float)(s * OCC_CELL), y)) {
            s++;
        }
        while (e >= s && !cell_in(ea, eb, ec, n, (float)(e * OCC_CELL), y)) {
            e--;
        }
        if (s > e) {
            continue;
        }
        w = wa * ((float)(s * OCC_CELL) + wox) + wb * (y + woy) + wc;
        dw = wa * (float)OCC_CELL;
        cell = &g_occ[gy * OCC_W + s];
        for (gx = s; gx <= e; gx++) {
            float v;

            v = w < wfar ? wfar : w;
            if (v > *cell) {
                *cell = v;
            }
            cell++;
            w += dw;
        }
        if (s < g_occ_x0) {
            g_occ_x0 = s;
        }
        if (e > g_occ_x1) {
            g_occ_x1 = e;
        }
        if (gy < g_occ_y0) {
            g_occ_y0 = gy;
        }
        if (gy > g_occ_y1) {
            g_occ_y1 = gy;
        }
    }
}

/*
 * One ARCH3D plane into the cells. 1 when it was drawn there.
 *
 * Only what draw_frame puts on the glass can hide anything:
 *   - a plane of two or more triangles has to face the eye, by the
 *     same normal test draw_frame uses. A single triangle is drawn
 *     from both sides, so it counts from both sides.
 *   - every triangle needs a texture slot. 0xffff is never drawn.
 *
 * The loader fans a plane from its first corner: triangle k is corners
 * 0, k + 1 and k + 2, each copied from the same file point. So the
 * polygon comes back exactly when every triangle starts on corner 0
 * and on the corner the last one ended on. A mesh that is not such a
 * fan is not used as a cover. The polygon is transformed once per
 * corner and clipped once, not triangle by triangle.
 *
 * The cells need a convex outline, or a notch would be covered. After
 * clipping and projection the outline has to pass poly_convex.
 *
 * The depth is a plane through the polygon, with Newell's normal, to
 * which every corner contributes. It is pushed back to the farthest
 * corner, so a slightly bent face is never nearer than its plane. That
 * plane gives 1/z as a linear function of the pixel: the ray
 * (x', y', 1), with x' = (sx - 320) / focal, meets n.v + d = 0 at
 * z = -d / (n . ray).
 */
static int occ_face(const Mesh *mesh, const Face *face, const Xf *xf,
                    float hx, float hy, float focal)
{
    const Tri *tris;
    const Tri *t0;
    Cv poly[2][CV_MAX];
    float px[CV_MAX];
    float py[CV_MAX];
    float nx, ny, nz, d, side;
    float zfar, area;
    float wa, wb, wc;
    int t, tend;
    int n, np, k;
    int src, plane;

    tris = mesh->tris;
    tend = face->first + face->ntris;
    if (face->first < 0 || face->ntris < 1 || tend > mesh->ntris ||
        face->ntris + 2 > OCC_POLY) {
        return 0;
    }
    t0 = &tris[face->first];
    xf_point(xf, t0->x[0], t0->y[0], t0->z[0], &poly[0][0]);
    if (face->ntris >= 2) {
        float vx, vy, vz;

        vx = xf->m[0] * face->nx + xf->m[1] * face->ny + xf->m[2] * face->nz;
        vy = xf->m[3] * face->nx + xf->m[4] * face->ny + xf->m[5] * face->nz;
        vz = xf->m[6] * face->nx + xf->m[7] * face->ny + xf->m[8] * face->nz;
        if (vx * poly[0][0].x + vy * poly[0][0].y + vz * poly[0][0].z >= 0.0f) {
            return 0;
        }
    }
    for (t = face->first; t < tend; t++) {
        const Tri *tri;

        tri = &tris[t];
        if (tri->slot == 0xffff) {
            return 0;
        }
        if (t > face->first &&
            (tri->x[0] != t0->x[0] || tri->y[0] != t0->y[0] || tri->z[0] != t0->z[0] ||
             tri->x[1] != tri[-1].x[2] || tri->y[1] != tri[-1].y[2] ||
             tri->z[1] != tri[-1].z[2])) {
            return 0;
        }
    }

    /* Corner 0 is already in view space. */
    n = 1;
    xf_point(xf, t0->x[1], t0->y[1], t0->z[1], &poly[0][n++]);
    for (t = face->first; t < tend; t++) {
        xf_point(xf, tris[t].x[2], tris[t].y[2], tris[t].z[2], &poly[0][n++]);
    }
    nx = ny = nz = 0.0f;
    side = 0.0f;
    for (k = 0; k < n; k++) {
        const Cv *p;
        const Cv *q;

        p = &poly[0][k];
        q = &poly[0][(k + 1) % n];
        nx += (p->y - q->y) * (p->z + q->z);
        ny += (p->z - q->z) * (p->x + q->x);
        nz += (p->x - q->x) * (p->y + q->y);
    }
    if (nx * nx + ny * ny + nz * nz < 1.0e-6f) {
        return 0;
    }
    /* Point the normal at the eye, the view origin, then find the back. */
    for (k = 0; k < n; k++) {
        side += nx * poly[0][k].x + ny * poly[0][k].y + nz * poly[0][k].z;
    }
    if (side > 0.0f) {
        nx = -nx;
        ny = -ny;
        nz = -nz;
    }
    d = -1.0e30f;
    for (k = 0; k < n; k++) {
        float e;

        e = -(nx * poly[0][k].x + ny * poly[0][k].y + nz * poly[0][k].z);
        if (e > d) {
            d = e;
        }
    }
    /* The eye has to be clearly in front of the plane. */
    if (d * d <= 1.0e-6f * (nx * nx + ny * ny + nz * nz)) {
        return 0;
    }

    /* Only a plane the polygon crosses cuts it. */
    src = 0;
    for (plane = 0; plane < 5; plane++) {
        int out;

        out = 0;
        for (k = 0; k < n; k++) {
            if (cv_dist(plane, hx, hy, &poly[src][k]) < 0.0f) {
                out++;
            }
        }
        if (out == 0) {
            continue;
        }
        if (out == n) {
            return 0;
        }
        n = cv_clip(poly[src], n, poly[src ^ 1], plane, hx, hy);
        src ^= 1;
        if (n < 3) {
            return 0;
        }
    }

    zfar = 0.0f;
    np = 0;
    for (k = 0; k < n; k++) {
        float iz, x, y;

        if (poly[src][k].z > zfar) {
            zfar = poly[src][k].z;
        }
        iz = focal / poly[src][k].z;
        x = (float)DF_VIEW_W * 0.5f + poly[src][k].x * iz;
        y = (float)DF_VIEW_H * 0.5f + poly[src][k].y * iz;
        if (np > 0 && df_absf(x - px[np - 1]) < OCC_DUP && df_absf(y - py[np - 1]) < OCC_DUP) {
            continue;
        }
        px[np] = x;
        py[np] = y;
        np++;
    }
    if (np > 1 && df_absf(px[0] - px[np - 1]) < OCC_DUP && df_absf(py[0] - py[np - 1]) < OCC_DUP) {
        np--;
    }
    if (np < 3) {
        return 0;
    }
    /* occ_fill wants the outline turning left. A face seen from its back
       (a single triangle) arrives turning right. */
    area = poly_area2(px, py, np);
    if (area < 0.0f) {
        for (k = 0; k < np / 2; k++) {
            float x, y;

            x = px[k];
            y = py[k];
            px[k] = px[np - 1 - k];
            py[k] = py[np - 1 - k];
            px[np - 1 - k] = x;
            py[np - 1 - k] = y;
        }
        area = -area;
    }
    /* Twice the area. Under two cells, nothing can be fully covered. */
    if (area < 4.0f * (float)(OCC_CELL * OCC_CELL) || !poly_convex(px, py, np)) {
        return 0;
    }
    wa = -nx / (d * focal);
    wb = -ny / (d * focal);
    wc = -(nz - nx * ((float)DF_VIEW_W * 0.5f) / focal -
           ny * ((float)DF_VIEW_H * 0.5f) / focal) / d;
    occ_fill(px, py, np, wa, wb, wc, 1.0f / zfar);
    return 1;
}

static int occ_hides(const View *v, float cx, float cy, float cz,
                     float ex, float ey, float ez);

/*
 * Fills the cells from the visible list. Ground patches are mesh id 0:
 * nothing is under the ground, so they are not worth the time. The
 * nearest OCC_MODELS models go first, so the cap drops the far ones.
 */
static void occ_build(const Scene *scene, const View *v, const int *idx, int n)
{
    int cand[OCC_MODELS];
    float cz[OCC_MODELS];
    float hx, hy;
    int nc;
    int i;

    nc = 0;
    for (i = 0; i < n; i++) {
        const Instance *in;
        float cx, cy, czz, ex, ey, ez;
        float sx0, sy0, sx1, sy1, z;
        int j;

        in = &scene->models[idx[i]];
        if (!in->mesh || in->mesh->id == 0 || in->mesh->nfaces < 1 ||
            !in->mesh->faces || !in->mesh->tris) {
            continue;
        }
        if (!model_box(scene, idx[i], &cx, &cy, &czz, &ex, &ey, &ez)) {
            continue;
        }
        if (box_screen(v, cx, cy, czz, ex, ey, ez, &sx0, &sy0, &sx1, &sy1, &z)) {
            float w;
            float h;

            if (z >= OCC_NEAR) {
                continue;
            }
            w = clampf(sx1, 0.0f, (float)DF_VIEW_W) - clampf(sx0, 0.0f, (float)DF_VIEW_W);
            h = clampf(sy1, 0.0f, (float)DF_VIEW_H) - clampf(sy0, 0.0f, (float)DF_VIEW_H);
            if (w * h < OCC_MODEL_AREA) {
                continue;
            }
        } else {
            /* The eye is in or against the box: the best cover there is. */
            z = 0.0f;
        }
        if (nc == OCC_MODELS && z >= cz[nc - 1]) {
            continue;
        }
        if (nc < OCC_MODELS) {
            nc++;
        }
        j = nc - 1;
        while (j > 0 && cz[j - 1] > z) {
            cand[j] = cand[j - 1];
            cz[j] = cz[j - 1];
            j--;
        }
        cand[j] = idx[i];
        cz[j] = z;
    }
    hx = ((float)DF_VIEW_W * 0.5f + OCC_MARGIN) / v->focal;
    hy = ((float)DF_VIEW_H * 0.5f + OCC_MARGIN) / v->focal;
    for (i = 0; i < nc && g_occ_faces < OCC_FACES; i++) {
        const Instance *in;
        float cx, cy, czz, ex, ey, ez;
        Xf xf;
        int f;

        /*
         * Nearest first, so a model already behind the cells could only
         * add cells farther than the ones it is behind. Skipping it
         * saves its faces. It is still dropped by the filter below.
         */
        if (g_occ_x0 <= g_occ_x1 &&
            model_box(scene, cand[i], &cx, &cy, &czz, &ex, &ey, &ez) &&
            occ_hides(v, cx, cy, czz, ex, ey, ez)) {
            continue;
        }
        in = &scene->models[cand[i]];
        xf_build(&xf, in, v);
        for (f = 0; f < in->mesh->nfaces && g_occ_faces < OCC_FACES; f++) {
            if (occ_face(in->mesh, &in->mesh->faces[f], &xf, hx, hy, v->focal)) {
                g_occ_faces++;
            }
        }
    }
}

/* 1 when every cell under the box is nearer than the box. */
static int occ_hides(const View *v, float cx, float cy, float cz,
                     float ex, float ey, float ez)
{
    float sx0, sy0, sx1, sy1, z;
    float wt;
    int gx0, gy0, gx1, gy1;
    int gx, gy;

    if (!box_screen(v, cx, cy, cz, ex, ey, ez, &sx0, &sy0, &sx1, &sy1, &z)) {
        return 0;
    }
    /* Off the glass is not a view past the wall. */
    gx0 = df_ifloor(clampf(sx0, 0.0f, (float)DF_VIEW_W - 0.5f) * OCC_INV);
    gy0 = df_ifloor(clampf(sy0, 0.0f, (float)DF_VIEW_H - 0.5f) * OCC_INV);
    gx1 = df_ifloor(clampf(sx1, 0.0f, (float)DF_VIEW_W - 0.5f) * OCC_INV);
    gy1 = df_ifloor(clampf(sy1, 0.0f, (float)DF_VIEW_H - 0.5f) * OCC_INV);
    if (gx0 < g_occ_x0 || gy0 < g_occ_y0 || gx1 > g_occ_x1 || gy1 > g_occ_y1) {
        return 0;
    }
    wt = OCC_SLACK / z;
    for (gy = gy0; gy <= gy1; gy++) {
        const float *row;

        row = &g_occ[gy * OCC_W];
        for (gx = gx0; gx <= gx1; gx++) {
            if (row[gx] <= wt) {
                return 0;
            }
        }
    }
    return 1;
}

/* Drops the hidden entries from idx, in place. Storage order is kept. */
static int occ_filter(const Scene *scene, const View *v, int *idx, int n, int sprites)
{
    int i;
    int w;

    w = 0;
    for (i = 0; i < n; i++) {
        float cx, cy, cz, ex, ey, ez;
        int ok;

        if (sprites) {
            ok = flat_box(&scene->sprites[idx[i]], &cx, &cy, &cz, &ex, &ey, &ez);
        } else {
            ok = model_box(scene, idx[i], &cx, &cy, &cz, &ex, &ey, &ez);
        }
        if (ok && occ_hides(v, cx, cy, cz, ex, ey, ez)) {
            g_occ_hidden++;
            continue;
        }
        idx[w++] = idx[i];
    }
    return w;
}

int scene_visible_models(const Scene *scene, const Camera *cam, int *out, int cap)
{
    Plane planes[6];
    View view;
    int n;
    int s;

    /* The sprite pass reads these cells, so a skipped frame leaves none. */
    occ_reset();
    g_occ_hidden = 0;
    if (!scene || !out || cap < 1 || scene->nmodels < 1) {
        return 0;
    }
    if (!cam || !g_cull) {
        return fill_order(scene->nmodels, out, cap);
    }
    frustum_planes(cam, visible_far(), planes);
    n = 0;
    if (scene->nspans > 0 && scene->spans) {
        for (s = 0; s < scene->nspans; s++) {
            const Span *sp;

            sp = &scene->spans[s];
            if (sp->count < 1) {
                continue;
            }
            if (!box_visible(planes, sp->cx, sp->cy, sp->cz, sp->ex, sp->ey, sp->ez)) {
                continue;
            }
            n = take_visible(scene, planes, sp->first, sp->count, out, cap, n);
            if (n >= cap) {
                break;
            }
        }
    } else {
        n = take_visible(scene, planes, 0, scene->nmodels, out, cap, 0);
    }
    if (!g_occ_on || n < 1) {
        return n;
    }
    view_build(&view, cam);
    occ_build(scene, &view, out, n);
    if (g_occ_x0 > g_occ_x1) {
        return n;
    }
    return occ_filter(scene, &view, out, n, 0);
}

/*
 * Sprites never hide anything: a flat has see-through texels. They are
 * hidden by the cells the model pass of the same frame wrote, so call
 * scene_visible_models first, as draw_frame does.
 */
int scene_visible_sprites(const Scene *scene, const Camera *cam, int *out, int cap)
{
    Plane planes[6];
    View view;
    int i;
    int n;

    if (!scene || !out || cap < 1 || scene->nsprites < 1) {
        return 0;
    }
    if (!cam || !g_cull) {
        return fill_order(scene->nsprites, out, cap);
    }
    frustum_planes(cam, visible_far(), planes);
    n = 0;
    for (i = 0; i < scene->nsprites; i++) {
        float cx, cy, cz, ex, ey, ez;

        if (!flat_box(&scene->sprites[i], &cx, &cy, &cz, &ex, &ey, &ez)) {
            continue;
        }
        if (!box_visible(planes, cx, cy, cz, ex, ey, ez)) {
            continue;
        }
        if (n >= cap) {
            break;
        }
        out[n++] = i;
    }
    if (!g_occ_on || g_occ_x0 > g_occ_x1 || n < 1) {
        return n;
    }
    view_build(&view, cam);
    return occ_filter(scene, &view, out, n, 1);
}

/*
 * The debug picture: each written row of cells as runs of neighbours.
 * Built when asked, from the cells the last frame left.
 */
#define OCC_RUNS 512

static short g_run[OCC_RUNS][3];
static int g_nrun;

void scene_set_cover_draw(int on)
{
    g_cover_draw = on ? 1 : 0;
}

int scene_cover_draw(void)
{
    return g_cover_draw;
}

int scene_cover_count(void)
{
    int gy;

    g_nrun = 0;
    if (!g_cover_draw || g_occ_x0 > g_occ_x1) {
        return 0;
    }
    for (gy = g_occ_y0; gy <= g_occ_y1; gy++) {
        int gx;

        gx = g_occ_x0;
        while (gx <= g_occ_x1 && g_nrun < OCC_RUNS) {
            int start;

            if (g_occ[gy * OCC_W + gx] <= 0.0f) {
                gx++;
                continue;
            }
            start = gx;
            while (gx <= g_occ_x1 && g_occ[gy * OCC_W + gx] > 0.0f) {
                gx++;
            }
            g_run[g_nrun][0] = (short)start;
            g_run[g_nrun][1] = (short)(gx - 1);
            g_run[g_nrun][2] = (short)gy;
            g_nrun++;
        }
    }
    return g_nrun;
}

void scene_cover_at(int index, float *x0, float *y0, float *x1, float *y1)
{
    if (index < 0 || index >= g_nrun) {
        return;
    }
    if (x0) {
        *x0 = (float)(g_run[index][0] * OCC_CELL);
    }
    if (y0) {
        *y0 = (float)(g_run[index][2] * OCC_CELL);
    }
    if (x1) {
        *x1 = (float)((g_run[index][1] + 1) * OCC_CELL);
    }
    if (y1) {
        *y1 = (float)((g_run[index][2] + 1) * OCC_CELL);
    }
}
