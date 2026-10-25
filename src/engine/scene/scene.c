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

/* Both sides: Moller and Trumbore, "Fast, Minimum Storage Ray/Triangle Intersection" (1997). */
static int ray_tri(const float *o, const float *d, const Tri *tri, float len, float *t)
{
    float e1[3], e2[3], p[3], q[3], s[3];
    float det, inv, u, v, at;

    e1[0] = tri->x[1] - tri->x[0];
    e1[1] = tri->y[1] - tri->y[0];
    e1[2] = tri->z[1] - tri->z[0];
    e2[0] = tri->x[2] - tri->x[0];
    e2[1] = tri->y[2] - tri->y[0];
    e2[2] = tri->z[2] - tri->z[0];
    p[0] = d[1] * e2[2] - d[2] * e2[1];
    p[1] = d[2] * e2[0] - d[0] * e2[2];
    p[2] = d[0] * e2[1] - d[1] * e2[0];
    det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (det > -1.0e-6f && det < 1.0e-6f) {
        return 0;
    }
    inv = 1.0f / det;
    s[0] = o[0] - tri->x[0];
    s[1] = o[1] - tri->y[0];
    s[2] = o[2] - tri->z[0];
    u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
    if (u < 0.0f || u > 1.0f) {
        return 0;
    }
    q[0] = s[1] * e1[2] - s[2] * e1[1];
    q[1] = s[2] * e1[0] - s[0] * e1[2];
    q[2] = s[0] * e1[1] - s[1] * e1[0];
    v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
    if (v < 0.0f || u + v > 1.0f) {
        return 0;
    }
    at = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
    if (at < 0.0f || at > len) {
        return 0;
    }
    *t = at;
    return 1;
}

int instance_ray(const Instance *in, const float *o, const float *d, float len, float *t)
{
    float lo[3];
    float ld[3];
    float rx, ry, rz;
    float at;
    int hit;
    int k;

    if (!in || !in->mesh || !in->mesh->tris) {
        return 0;
    }
    /* Into the model's frame: its axes are m's columns, unit and square. */
    rx = o[0] - in->x;
    ry = o[1] - in->y;
    rz = o[2] - in->z;
    lo[0] = in->m[0] * rx + in->m[3] * ry + in->m[6] * rz;
    lo[1] = in->m[1] * rx + in->m[4] * ry + in->m[7] * rz;
    lo[2] = in->m[2] * rx + in->m[5] * ry + in->m[8] * rz;
    ld[0] = in->m[0] * d[0] + in->m[3] * d[1] + in->m[6] * d[2];
    ld[1] = in->m[1] * d[0] + in->m[4] * d[1] + in->m[7] * d[2];
    ld[2] = in->m[2] * d[0] + in->m[5] * d[1] + in->m[8] * d[2];
    hit = 0;
    for (k = 0; k < in->mesh->ntris; k++) {
        if (ray_tri(lo, ld, &in->mesh->tris[k], len, &at)) {
            len = at;
            hit = 1;
        }
    }
    if (hit) {
        *t = len;
    }
    return hit;
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

/* Where draw.c put the fog this frame; 0 is the far clip's own (df_fog_far). */
static float g_fog_far;

void scene_set_fog_far(float fog_far)
{
    g_fog_far = fog_far > 0.0f ? fog_far : 0.0f;
}

static float visible_far(void)
{
    float far;
    float fog_far;

    far = g_clip;
    if (!g_fog) {
        return far;
    }
    /* The table's first solid entry, the same distance draw.c clips at. */
    fog_far = df_fog_solid(g_fog_far > 0.0f ? g_fog_far : df_fog_far(g_clip));
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
 *   2. Front faces of the nearest models are projected and rasterized
 *      into those cells. A face that covers all of a cell writes 1/z of
 *      the farthest point it can have there; of several, the nearest
 *      bound wins. A face that covers part of a cell lists itself on
 *      that cell instead.
 *   3. A cell that no one face covers may be covered by the faces listed
 *      on it together: a wall and its floor, two panels, a door leaf in
 *      its frame. The seam pass cuts them out of the cell and closes it
 *      when nothing a pixel could land in is left. See OCC_PART.
 *   4. A box is hidden when its screen rectangle lands only on covered
 *      cells, and every one of them is nearer than the nearest point of
 *      the box.
 *
 * Every step errs toward drawing. A cell is covered only when the faces
 * are known to be over every sample of it, and only with a depth no
 * nearer than they really are there. A box is dropped only when every
 * pixel it could reach is behind such a cell. So the cull can keep a
 * model that turns out to be hidden, and the Z buffer sorts that out,
 * but it cannot drop one that has a pixel on the glass.
 *
 * Joining walls is not a separate step. Two panels of one facade fill
 * their own cells and share the ones on their seam, and the box behind
 * the pair is tested against all of them. A gap between the panels is a
 * run of open cells, so whatever shows through the gap stays.
 *
 * The cost is held under the work it saves. A cell is 16 pixels, so the
 * grid is 40 x 30 floats and sits in the Pentium's 16 KB data cache. A
 * row of cells is a span solved per edge, not a test per cell. A model
 * whose box is already hidden adds no faces. The seam pass runs only on
 * the cells a box test asks about. See docs/culling.md for the numbers.
 */

#define OCC_CELL 16
/*
 * Pixel to cell is a multiply and df_ifloor, never a C cast: on the
 * x87 a cast switches the rounding mode, about 30 cycles a time.
 */
#define OCC_INV (1.0f / (float)OCC_CELL)
#define OCC_W (DF_VIEW_W / OCC_CELL)
#define OCC_H (DF_VIEW_H / OCC_CELL)
/*
 * A model is looked at only when part of its box is nearer than this.
 * It was 640, which kept a castle hall's walls, a few hundred units
 * past that, from ever covering anything: the rooms behind them drew.
 * What a candidate is worth is its size on the glass, not its distance.
 */
#define OCC_NEAR 4096.0f
/* A screen box smaller than 3x3 cells cannot fill enough cells to pay. */
#define OCC_MODEL_AREA (9.0f * OCC_CELL * OCC_CELL)
/*
 * A face smaller than this on screen is not written. Alone it covers no
 * cell, but a door jamb or a beam end still closes a seam.
 */
#define OCC_FACE_AREA (0.125f * OCC_CELL * OCC_CELL)
/* Models looked at per frame, the largest on screen, and faces drawn into the cells. */
#define OCC_MODELS 48
#define OCC_FACES 128
/*
 * Faces are clipped this far past the picture, not to its edge, so a
 * wall that fills the glass also fills the border cells.
 */
#define OCC_MARGIN 16.0f
/*
 * A cell is tested grown by this much on every side. Outlines are
 * snapped to the 1/16 pixel the card draws at, but a corner the clip
 * made can land one step away from the one draw.c sends, so an edge
 * may lie up to 1/32 pixel off. 1/16 covers that and the fill rule.
 */
#define OCC_EDGE (1.0f / 16.0f)
/*
 * A box has to be this much farther than the cell, as a factor on 1/z.
 * 0.1% keeps a sign or a door that is flush with its wall.
 */
#define OCC_SLACK 1.001f
/*
 * A plane is rebuilt from its fan as one polygon of at most OCC_POLY
 * corners: ARCH3D's 24, since the loader keeps every point. Each of the
 * five clip planes adds at most one corner.
 */
#define OCC_POLY 24
#define CV_MAX (OCC_POLY + 6)
/* Corners closer than this, in pixels, after clipping are one corner. */
#define OCC_DUP 0.01f
/*
 * A corner may turn the wrong way by this much, as sin^2 of the angle,
 * before the outline counts as notched: float error on a clipped
 * corner, about 1e-5, not a notch a cell could sit in.
 */
#define OCC_BEND 1.0e-8f

/*
 * A cell that no single face covers can still be covered by several:
 * a wall and the floor it stands on, two panels, a corner. Each face
 * lists itself on the cells it only partly covers, up to OCC_PART per
 * cell, nearest kept. occ_seam then cuts those faces out of the cell.
 *
 * The outlines are snapped to 1/16 pixel, the coordinates glide.c hands
 * the card. Two faces that share an edge then share it exactly, on the
 * card and here, and the card's fill rule puts every sample on that
 * edge in one of them. So a piece left after the cuts is closed when:
 *   - it is thinner than OCC_NOISE: nothing but rounding in the cuts,
 *     the trace of an edge both faces share. Or
 *   - no sample point lies within OCC_SNAP of it. The card's own edge
 *     can sit up to 1/32 pixel from this one where a clip made the
 *     corner, so anything nearer than 1/16 could fall on either side.
 * Every half pixel is tried, so the test holds whether the SST-1
 * samples pixel centres or pixel corners. A T-junction, where a wall
 * ends partway along the floor's edge, is not shared: its hairline
 * wedge stays open wherever a sample comes near it. So does a door
 * leaf that stops a tenth of a pixel above its threshold.
 */
#define OCC_PART 8
#define OCC_NOISE (1.0f / 4096.0f)
#define OCC_SNAP (1.0f / 16.0f)
/* Pieces a cell may break into while faces are cut out of it. */
#define OCC_PIECES 8
#define OCC_PV (CV_MAX + 4)

/* 1/z per cell. 0 is no cover. */
static float g_occ[OCC_W * OCC_H];
/* The faces written this frame, in screen space, for the seam pass. */
typedef struct OccFace {
    float x[CV_MAX];
    float y[CV_MAX];
    int n;
} OccFace;
static OccFace g_face[OCC_FACES];
/* Partial covers per cell: count, face index, and that face's 1/z bound there. */
static unsigned char g_npart[OCC_W * OCC_H];
static unsigned char g_part_f[OCC_W * OCC_H][OCC_PART];
static float g_part_w[OCC_W * OCC_H][OCC_PART];
/* 1 once the seam pass has tried the cell. See occ_seam. */
static unsigned char g_tried[OCC_W * OCC_H];
/* Cells with a partial cover, inclusive. Empty when x0 > x1. */
static int g_part_x0 = OCC_W;
static int g_part_y0 = OCC_H;
static int g_part_x1 = -1;
static int g_part_y1 = -1;
/* Set once every face is in: only then may a box test resolve a seam. */
static int g_occ_lazy;
static int g_occ_seams;
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
    g_occ_seams = 0;
    g_occ_lazy = 0;
    g_part_x0 = OCC_W;
    g_part_y0 = OCC_H;
    g_part_x1 = -1;
    g_part_y1 = -1;
    mem_zero(g_npart, (df_u32)sizeof(g_npart));
    mem_zero(g_tried, (df_u32)sizeof(g_tried));
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

/* The last scene_visible_models' frustum and view, for scene_box_visible. */
static Plane g_last_planes[6];
static View g_last_view;
static int g_last_ok;

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
        /* Inside end first, as draw.c cuts, so two faces that share
           this edge get the same corner. */
        if (ain) {
            t = dist[i] / denom;
            hit.x = in[i].x + (in[j].x - in[i].x) * t;
            hit.y = in[i].y + (in[j].y - in[i].y) * t;
            hit.z = in[i].z + (in[j].z - in[i].z) * t;
        } else {
            t = dist[j] / -denom;
            hit.x = in[j].x + (in[i].x - in[j].x) * t;
            hit.y = in[j].y + (in[i].y - in[j].y) * t;
            hit.z = in[j].z + (in[i].z - in[j].z) * t;
        }
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
        /*
         * A point the loader kept along an edge snaps up to 1/32 pixel
         * off the line, so it can turn the wrong way by a hair. A dent
         * no deeper than OCC_SNAP from the chord i..k still passes:
         * occ_fill and the seam pass take an outline as the meet of its
         * edges' half-planes, which for such a dent is the dented face,
         * never more. A real notch is far deeper and is still refused.
         */
        if (cross < 0.0f &&
            cross * cross > OCC_BEND * (ax * ax + ay * ay) * (bx * bx + by * by) &&
            cross * cross > OCC_SNAP * OCC_SNAP *
                ((ax + bx) * (ax + bx) + (ay + by) * (ay + by))) {
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

static void occ_mark(int gx, int gy)
{
    if (gx < g_occ_x0) {
        g_occ_x0 = gx;
    }
    if (gx > g_occ_x1) {
        g_occ_x1 = gx;
    }
    if (gy < g_occ_y0) {
        g_occ_y0 = gy;
    }
    if (gy > g_occ_y1) {
        g_occ_y1 = gy;
    }
}

/* Lists face f on cell c, which it only partly covers. A full list keeps the nearest. */
static void occ_part(int c, int f, float w)
{
    int k;
    int far;
    int gx;
    int gy;

    gx = c % OCC_W;
    gy = c / OCC_W;
    if (gx < g_part_x0) {
        g_part_x0 = gx;
    }
    if (gx > g_part_x1) {
        g_part_x1 = gx;
    }
    if (gy < g_part_y0) {
        g_part_y0 = gy;
    }
    if (gy > g_part_y1) {
        g_part_y1 = gy;
    }

    if (g_npart[c] < OCC_PART) {
        g_part_f[c][g_npart[c]] = (unsigned char)f;
        g_part_w[c][g_npart[c]] = w;
        g_npart[c]++;
        return;
    }
    far = 0;
    for (k = 1; k < OCC_PART; k++) {
        if (g_part_w[c][k] < g_part_w[c][far]) {
            far = k;
        }
    }
    if (w > g_part_w[c][far]) {
        g_part_f[c][far] = (unsigned char)f;
        g_part_w[c][far] = w;
    }
}

/*
 * Writes face f into the cells. The outline turns left.
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
 *
 * The opposite corner, the largest value, gives the run of cells the
 * face touches at all. A touched cell outside the full run is listed
 * for the seam pass. That run may include a cell the face only grazes:
 * the seam pass then cuts nothing from it, which costs time, not cover.
 */
static void occ_fill(int f, float wa, float wb, float wc, float wfar)
{
    const float *px;
    const float *py;
    float ea[CV_MAX];
    float eb[CV_MAX];
    float ec[CV_MAX];
    float et[CV_MAX];
    float ia[CV_MAX];
    float minx, miny, maxx, maxy;
    float wox, woy;
    int gx0, gy0, gx1, gy1;
    int tx0, ty0, tx1, ty1;
    int gy;
    int i;
    int n;

    px = g_face[f].x;
    py = g_face[f].y;
    n = g_face[f].n;
    minx = maxx = px[0];
    miny = maxy = py[0];
    for (i = 0; i < n; i++) {
        int j;
        float a, b, c;

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
        c = -(a * px[i] + b * py[i]);
        /* Bake in the worst corner of the grown cell, relative to its origin. */
        ec[i] = c +
                a * (a > 0.0f ? -OCC_EDGE : (float)OCC_CELL + OCC_EDGE) +
                b * (b > 0.0f ? -OCC_EDGE : (float)OCC_CELL + OCC_EDGE);
        /* And the best corner, for the cells it touches. */
        et[i] = c +
                a * (a > 0.0f ? (float)OCC_CELL + OCC_EDGE : -OCC_EDGE) +
                b * (b > 0.0f ? (float)OCC_CELL + OCC_EDGE : -OCC_EDGE);
        ea[i] = a;
        eb[i] = b;
        ia[i] = a != 0.0f ? 1.0f / a : 0.0f;
    }
    if (maxx < 0.0f || maxy < 0.0f || minx > (float)DF_VIEW_W || miny > (float)DF_VIEW_H) {
        return;
    }
    /* Cells wholly inside the outline's box can be covered. */
    gx0 = df_ifloor((clampf(minx, 0.0f, (float)DF_VIEW_W) + (float)OCC_CELL - 0.001f) * OCC_INV);
    gy0 = df_ifloor((clampf(miny, 0.0f, (float)DF_VIEW_H) + (float)OCC_CELL - 0.001f) * OCC_INV);
    gx1 = df_ifloor(clampf(maxx, 0.0f, (float)DF_VIEW_W) * OCC_INV) - 1;
    gy1 = df_ifloor(clampf(maxy, 0.0f, (float)DF_VIEW_H) * OCC_INV) - 1;
    /* Cells that overlap the box can be touched. */
    /* Grown like the cells are, or a face that stops just short of a
       row would never list itself on it. */
    tx0 = df_ifloor(clampf(minx - OCC_EDGE, 0.0f, (float)DF_VIEW_W - 0.5f) * OCC_INV);
    ty0 = df_ifloor(clampf(miny - OCC_EDGE, 0.0f, (float)DF_VIEW_H - 0.5f) * OCC_INV);
    tx1 = df_ifloor(clampf(maxx + OCC_EDGE, 0.0f, (float)DF_VIEW_W - 0.5f) * OCC_INV);
    ty1 = df_ifloor(clampf(maxy + OCC_EDGE, 0.0f, (float)DF_VIEW_H - 0.5f) * OCC_INV);
    wox = wa > 0.0f ? 0.0f : (float)OCC_CELL;
    woy = wb > 0.0f ? 0.0f : (float)OCC_CELL;
    for (gy = ty0; gy <= ty1; gy++) {
        float y;
        float lo, hi;
        float w, dw;
        int s, e;
        int ts, te;
        int gx;

        y = (float)(gy * OCC_CELL);
        /* The touched run. */
        lo = (float)(tx0 * OCC_CELL);
        hi = (float)(tx1 * OCC_CELL);
        for (i = 0; i < n && lo <= hi; i++) {
            float r;

            r = eb[i] * y + et[i];
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
        ts = df_ifloor(lo * OCC_INV);
        if ((float)(ts * OCC_CELL) < lo) {
            ts++;
        }
        te = df_ifloor(hi * OCC_INV);
        if (ts < tx0) {
            ts = tx0;
        }
        if (te > tx1) {
            te = tx1;
        }
        if (ts > te) {
            continue;
        }
        /* The full run, inside the touched one. */
        s = te + 1;
        e = te;
        if (gy >= gy0 && gy <= gy1 && gx0 <= gx1) {
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
            if (lo <= hi) {
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
            }
            if (s > e) {
                s = te + 1;
                e = te;
            }
        }
        w = wa * ((float)(ts * OCC_CELL) + wox) + wb * (y + woy) + wc;
        dw = wa * (float)OCC_CELL;
        for (gx = ts; gx <= te; gx++) {
            float v;
            int c;

            v = w < wfar ? wfar : w;
            w += dw;
            c = gy * OCC_W + gx;
            if (gx >= s && gx <= e) {
                if (v > g_occ[c]) {
                    g_occ[c] = v;
                }
                continue;
            }
            /* Only a face nearer than the cell's own cover can improve it. */
            if (v > g_occ[c]) {
                occ_part(c, f, v);
            }
        }
        if (s <= e) {
            occ_mark(s, gy);
            occ_mark(e, gy);
        }
    }
}

/*
 * A cell, or a piece of one, while faces are cut out of it. The
 * coordinates are relative to the cell's corner, so the numbers stay
 * small and the area of a sliver is exact enough to judge.
 */
typedef struct Pc {
    float x[OCC_PV];
    float y[OCC_PV];
    int n;
} Pc;

/* Keeps the part where sign * (a * x + b * y + c) >= 0. */
static void pc_cut(const Pc *in, float a, float b, float c, float sign, Pc *out)
{
    float d[OCC_PV];
    int i;

    out->n = 0;
    for (i = 0; i < in->n; i++) {
        d[i] = sign * (a * in->x[i] + b * in->y[i] + c);
    }
    for (i = 0; i < in->n; i++) {
        int j;

        j = (i + 1) % in->n;
        if (d[i] >= 0.0f && out->n < OCC_PV) {
            out->x[out->n] = in->x[i];
            out->y[out->n] = in->y[i];
            out->n++;
        }
        if ((d[i] >= 0.0f) != (d[j] >= 0.0f) && out->n < OCC_PV) {
            float t;

            t = d[i] / (d[i] - d[j]);
            out->x[out->n] = in->x[i] + (in->x[j] - in->x[i]) * t;
            out->y[out->n] = in->y[i] + (in->y[j] - in->y[i]) * t;
            out->n++;
        }
    }
}

/*
 * 1 when the piece is only rounding from the cuts, thinner than
 * OCC_NOISE: the trace of an edge two faces share. See OCC_PART. The
 * width of a convex piece across its narrowest direction is at most
 * twice its area over its longest extent, and the longest side of its
 * box is no longer than that extent, so the test can only call a piece
 * thin when it is. Cheap: it runs on every piece a cut makes.
 */
static int pc_thin(const Pc *p)
{
    float minx, miny, maxx, maxy, side;
    int i;

    if (p->n < 3) {
        return 1;
    }
    minx = maxx = p->x[0];
    miny = maxy = p->y[0];
    for (i = 1; i < p->n; i++) {
        if (p->x[i] < minx) {
            minx = p->x[i];
        }
        if (p->x[i] > maxx) {
            maxx = p->x[i];
        }
        if (p->y[i] < miny) {
            miny = p->y[i];
        }
        if (p->y[i] > maxy) {
            maxy = p->y[i];
        }
    }
    side = maxx - minx > maxy - miny ? maxx - minx : maxy - miny;
    return side < OCC_NOISE ||
           df_absf(poly_area2(p->x, p->y, p->n)) <= OCC_NOISE * side;
}

/*
 * 1 when no sample point lies within OCC_SNAP of the piece, so the card
 * cannot draw a pixel in it. See OCC_PART. Sample points are the
 * multiples of a half pixel in screen space; ox, oy is the cell corner.
 * The piece is convex and turns left, as the cell does and every cut
 * keeps. Each edge is pushed out by OCC_SNAP. Then, for each half-pixel
 * row in the piece's box, the edges bound the run of x inside it, the
 * same solve occ_fill uses for a row of cells, and the run is checked
 * for a half-pixel x. A long thin sliver costs one row per half pixel
 * of its height, not a test per point. This is the dear test, so it
 * runs only on what is left once every face has been cut out.
 */
static int pc_nosample(const Pc *p, float ox, float oy)
{
    float ea[OCC_PV];
    float eb[OCC_PV];
    float ec[OCC_PV];
    float minx, miny, maxx, maxy;
    int sy0, sy1, gy;
    int i;

    if (p->n < 3) {
        return 1;
    }
    minx = maxx = p->x[0];
    miny = maxy = p->y[0];
    for (i = 0; i < p->n; i++) {
        float dx, dy, l2, len;
        int j;

        if (p->x[i] < minx) {
            minx = p->x[i];
        }
        if (p->x[i] > maxx) {
            maxx = p->x[i];
        }
        if (p->y[i] < miny) {
            miny = p->y[i];
        }
        if (p->y[i] > maxy) {
            maxy = p->y[i];
        }
        /* Inside is ea * x + eb * y + ec >= 0, in screen space, grown. */
        j = (i + 1) % p->n;
        dx = p->x[j] - p->x[i];
        dy = p->y[j] - p->y[i];
        l2 = dx * dx + dy * dy;
        __asm__ __volatile__("fsqrt" : "=t"(len) : "0"(l2));
        ea[i] = -dy;
        eb[i] = dx;
        ec[i] = dy * (p->x[i] + ox) - dx * (p->y[i] + oy) + OCC_SNAP * len;
    }
    minx += ox - OCC_SNAP;
    maxx += ox + OCC_SNAP;
    sy0 = df_ifloor((miny + oy - OCC_SNAP) * 2.0f) + 1;
    sy1 = df_ifloor((maxy + oy + OCC_SNAP) * 2.0f);
    for (gy = sy0; gy <= sy1; gy++) {
        float y, lo, hi;
        int sx;

        y = (float)gy * 0.5f;
        lo = minx;
        hi = maxx;
        for (i = 0; i < p->n && lo <= hi; i++) {
            float r;

            r = eb[i] * y + ec[i];
            if (ea[i] > 0.0f) {
                float x;

                x = -r / ea[i];
                if (x > lo) {
                    lo = x;
                }
            } else if (ea[i] < 0.0f) {
                float x;

                x = -r / ea[i];
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
        /* The first half pixel at or after lo. */
        sx = df_ifloor(lo * 2.0f);
        if ((float)sx * 0.5f < lo) {
            sx++;
        }
        if ((float)sx * 0.5f <= hi) {
            return 0;
        }
    }
    return 1;
}

/* Drops the pieces that hold no sample. Returns how many are left. */
static int pc_prune(Pc *pieces, int np, float ox, float oy)
{
    int p;
    int w;

    w = 0;
    for (p = 0; p < np; p++) {
        if (!pc_nosample(&pieces[p], ox, oy)) {
            if (w != p) {
                pieces[w] = pieces[p];
            }
            w++;
        }
    }
    return w;
}

/*
 * Cuts face f out of every piece. A piece becomes the parts of it
 * outside each edge in turn: outside edge 0, then inside 0 and outside
 * 1, and so on, which are convex and do not overlap. What is inside
 * every edge is the face, and goes. An edge with the whole piece on one
 * side cuts nothing: wholly inside, it is skipped; wholly outside, the
 * rest of the piece is outside the face and stays as it is. *used is
 * set when the face took something. 0 when the pieces would pass
 * OCC_PIECES even after the ones with no sample are dropped: the cell
 * is then left as it was.
 */
static void pc_copy(Pc *dst, const Pc *src)
{
    int i;

    for (i = 0; i < src->n; i++) {
        dst->x[i] = src->x[i];
        dst->y[i] = src->y[i];
    }
    dst->n = src->n;
}

static int pc_subtract(Pc *pieces, int *np, int f, float ox, float oy, int *used)
{
    static Pc next[OCC_PIECES];
    static Pc work[2];
    const OccFace *fc;
    int nn;
    int p;

    fc = &g_face[f];
    nn = 0;
    for (p = 0; p < *np; p++) {
        const Pc *cur;
        int side;
        int i;
        int took;

        /* cur is the part of the piece inside the edges so far. Cuts go
           straight into next[] and work[], so no piece is copied whole. */
        cur = &pieces[p];
        side = 0;
        took = 1;
        for (i = 0; i < fc->n; i++) {
            float a, b, c;
            int in, out, k, j;

            j = (i + 1) % fc->n;
            a = -(fc->y[j] - fc->y[i]);
            b = fc->x[j] - fc->x[i];
            /* The edge in cell coordinates. */
            c = -(a * (fc->x[i] - ox) + b * (fc->y[i] - oy));
            in = 0;
            out = 0;
            for (k = 0; k < cur->n; k++) {
                if (a * cur->x[k] + b * cur->y[k] + c >= 0.0f) {
                    in++;
                } else {
                    out++;
                }
            }
            if (out == 0) {
                continue;
            }
            if (nn >= OCC_PIECES) {
                nn = pc_prune(next, nn, ox, oy);
                if (nn >= OCC_PIECES) {
                    return 0;
                }
            }
            if (in == 0) {
                /* All of what is left is outside the face. */
                pc_copy(&next[nn], cur);
                if (!pc_thin(&next[nn])) {
                    nn++;
                }
                took = 0;
                break;
            }
            pc_cut(cur, a, b, c, -1.0f, &next[nn]);
            if (!pc_thin(&next[nn])) {
                nn++;
            }
            pc_cut(cur, a, b, c, 1.0f, &work[side]);
            cur = &work[side];
            side ^= 1;
            if (pc_thin(cur)) {
                took = 0;
                break;
            }
        }
        if (took) {
            *used = 1;
        }
    }
    for (p = 0; p < nn; p++) {
        pc_copy(&pieces[p], &next[p]);
    }
    *np = nn;
    return 1;
}

/*
 * The seam pass, for one cell. A cell listed by two or more faces is
 * cut by those faces, nearest first. When the pieces that are left
 * hold no sample, the faces that took a part cover it together, and
 * the farthest of their bounds is the cell's 1/z. It only replaces a
 * farther cover.
 *
 * It runs lazily. A cell is worth cutting only when a box test lands
 * on it, and a visible box stops at its first open cell, so most seam
 * cells are never asked about. occ_hides calls this the first time it
 * meets an uncovered cell with a list, once every face is in, and
 * g_tried keeps the answer for the rest of the frame.
 */
static void occ_seam(int c)
{
    Pc pieces[OCC_PIECES];
    unsigned char fo[OCC_PART];
    float wo[OCC_PART];
    float ox, oy;
    float w;
    int m;
    int k;
    int np;
    int ok;

    g_tried[c] = 1;
    m = g_npart[c];
    if (m < 2) {
        return;
    }
    /* Nearest first: insertion sort of at most OCC_PART. */
    for (k = 0; k < m; k++) {
        int j;

        j = k;
        while (j > 0 && wo[j - 1] < g_part_w[c][k]) {
            wo[j] = wo[j - 1];
            fo[j] = fo[j - 1];
            j--;
        }
        wo[j] = g_part_w[c][k];
        fo[j] = g_part_f[c][k];
    }
    /* None of them can beat the cover the cell already has. */
    if (wo[0] <= g_occ[c]) {
        return;
    }
    pieces[0].n = 4;
    pieces[0].x[0] = -OCC_EDGE;
    pieces[0].y[0] = -OCC_EDGE;
    pieces[0].x[1] = (float)OCC_CELL + OCC_EDGE;
    pieces[0].y[1] = -OCC_EDGE;
    pieces[0].x[2] = (float)OCC_CELL + OCC_EDGE;
    pieces[0].y[2] = (float)OCC_CELL + OCC_EDGE;
    pieces[0].x[3] = -OCC_EDGE;
    pieces[0].y[3] = (float)OCC_CELL + OCC_EDGE;
    np = 1;
    ox = (float)((c % OCC_W) * OCC_CELL);
    oy = (float)((c / OCC_W) * OCC_CELL);
    w = 1.0e30f;
    ok = 1;
    for (k = 0; k < m && np > 0; k++) {
        int used;

        used = 0;
        if (!pc_subtract(pieces, &np, fo[k], ox, oy, &used)) {
            ok = 0;
            break;
        }
        if (used && wo[k] < w) {
            w = wo[k];
        }
    }
    if (!ok || w <= g_occ[c]) {
        return;
    }
    if (np > 0) {
        np = pc_prune(pieces, np, ox, oy);
    }
    if (np == 0) {
        g_occ[c] = w;
        g_occ_seams++;
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
                    float hx, float hy, float focal, int slot)
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
    /* Wholly behind the eye or off one side: no normal, no clip. */
    for (plane = 0; plane < 5; plane++) {
        for (k = 0; k < n; k++) {
            if (cv_dist(plane, hx, hy, &poly[0][k]) >= 0.0f) {
                break;
            }
        }
        if (k == n) {
            return 0;
        }
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
        /* Where the card will put the corner: camera_project's sums,
           then the snap glide.c applies. See OCC_PART. */
        iz = 1.0f / poly[src][k].z;
        x = df_snap16((float)DF_VIEW_W * 0.5f + poly[src][k].x * focal * iz);
        y = df_snap16((float)DF_VIEW_H * 0.5f + poly[src][k].y * focal * iz);
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
    /* Twice the area. A sliver of a face still closes a seam, so the
       bar is a quarter of a cell, not a cell. */
    if (area < 2.0f * OCC_FACE_AREA || !poly_convex(px, py, np)) {
        return 0;
    }
    wa = -nx / (d * focal);
    wb = -ny / (d * focal);
    wc = -(nz - nx * ((float)DF_VIEW_W * 0.5f) / focal -
           ny * ((float)DF_VIEW_H * 0.5f) / focal) / d;
    for (k = 0; k < np; k++) {
        g_face[slot].x[k] = px[k];
        g_face[slot].y[k] = py[k];
    }
    g_face[slot].n = np;
    occ_fill(slot, wa, wb, wc, 1.0f / zfar);
    return 1;
}

static int occ_hides(const View *v, float cx, float cy, float cz,
                     float ex, float ey, float ez);

/*
 * Fills the cells from the visible list. Ground patches are mesh id 0:
 * nothing is under the ground, so they are not worth the time.
 *
 * The OCC_MODELS candidates are the ones whose boxes cover the most of
 * the glass; a box around the eye counts as all of it. They used to be
 * the nearest, which in a hall spent the budget on the chairs and
 * torches by the camera and never reached the walls. The chosen ones
 * are then written nearest first, so a candidate already behind the
 * cells is skipped and the face cap drops the far ones.
 */
static void occ_build(const Scene *scene, const View *v, const int *idx, int n)
{
    int cand[OCC_MODELS];
    float cz[OCC_MODELS];
    float carea[OCC_MODELS];
    float hx, hy;
    int nc;
    int i;
    int j;

    nc = 0;
    for (i = 0; i < n; i++) {
        const Instance *in;
        float cx, cy, czz, ex, ey, ez;
        float sx0, sy0, sx1, sy1, z;
        float area;

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
            area = w * h;
            if (area < OCC_MODEL_AREA) {
                continue;
            }
        } else {
            /* The eye is in or against the box: the best cover there is. */
            z = 0.0f;
            area = (float)(DF_VIEW_W * DF_VIEW_H) * 2.0f;
        }
        /* Kept largest first; a smaller one than the last of a full list waits. */
        if (nc == OCC_MODELS && area <= carea[nc - 1]) {
            continue;
        }
        if (nc < OCC_MODELS) {
            nc++;
        }
        j = nc - 1;
        while (j > 0 && carea[j - 1] < area) {
            cand[j] = cand[j - 1];
            cz[j] = cz[j - 1];
            carea[j] = carea[j - 1];
            j--;
        }
        cand[j] = idx[i];
        cz[j] = z;
        carea[j] = area;
    }
    /* Written nearest first. At most 48: an insertion sort is enough. */
    for (i = 1; i < nc; i++) {
        int c0;
        float z0;

        c0 = cand[i];
        z0 = cz[i];
        j = i;
        while (j > 0 && cz[j - 1] > z0) {
            cand[j] = cand[j - 1];
            cz[j] = cz[j - 1];
            j--;
        }
        cand[j] = c0;
        cz[j] = z0;
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
            if (occ_face(in->mesh, &in->mesh->faces[f], &xf, hx, hy, v->focal,
                         g_occ_faces)) {
                g_occ_faces++;
            }
        }
    }
    /*
     * Every face is in. From here a box test may resolve a seam, and
     * the box of cells to test and to clear reaches the listed cells.
     */
    g_occ_lazy = 1;
    if (g_part_x0 <= g_part_x1) {
        occ_mark(g_part_x0, g_part_y0);
        occ_mark(g_part_x1, g_part_y1);
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
            int c;

            if (row[gx] > wt) {
                continue;
            }
            /* Not covered yet. Faces that share it may still be. */
            c = gy * OCC_W + gx;
            if (!g_occ_lazy || g_tried[c] || g_npart[c] < 2) {
                return 0;
            }
            occ_seam(c);
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
    g_last_ok = 0;
    if (!scene || !out || cap < 1 || scene->nmodels < 1) {
        return 0;
    }
    if (!cam || !g_cull) {
        return fill_order(scene->nmodels, out, cap);
    }
    frustum_planes(cam, visible_far(), planes);
    /* Kept for scene_box_visible: the light pass asks after this call. */
    for (s = 0; s < 6; s++) {
        g_last_planes[s] = planes[s];
    }
    view_build(&g_last_view, cam);
    g_last_ok = 1;
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
 * A box against this frame's frustum and the cells scene_visible_models
 * wrote, the same tests a model box gets. The light pass asks it for
 * each light's decals, after the model pass.
 */
int scene_box_visible(float cx, float cy, float cz, float ex, float ey, float ez)
{
    if (!g_last_ok) {
        return 1;
    }
    if (!box_visible(g_last_planes, cx, cy, cz, ex, ey, ez)) {
        return 0;
    }
    if (!g_occ_on || g_occ_x0 > g_occ_x1) {
        return 1;
    }
    return !occ_hides(&g_last_view, cx, cy, cz, ex, ey, ez);
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
    /* The overlay shows every seam the faces close, asked about or not. */
    if (g_occ_lazy) {
        int c;

        for (c = 0; c < OCC_W * OCC_H; c++) {
            if (!g_tried[c] && g_npart[c] >= 2) {
                occ_seam(c);
            }
        }
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
