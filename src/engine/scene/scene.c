#include "engine/scene/scene.h"
#include "engine/core/df.h"

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
 * A wall against the lens. The frustum is a pyramid, so a wall in
 * front of the eye still leaves every box behind it "inside" out to
 * the fog. Drawing those is the slow frame: the floor is two models,
 * the same street aimed at a wall is a couple of hundred.
 *
 * One rectangle per front face, clipped to the picture. A floor or
 * ceiling is not a cover. Tall faces at the same depth then share one
 * rectangle, so the panels of a facade count as the wall. A shallow
 * band does not join them. The depth is the nearest vertex. A face
 * that reaches the edges of the picture hides every box more than
 * COVER_THICK past that depth. A smaller face hides only a box whose
 * corners project inside it. mesh id 0 is a ground patch. This is
 * not a raster of the wall.
 */
#define COVER_NEAR 512.0f
#define COVER_SPAN 320.0f
#define COVER_H 32.0f
#define COVER_THICK 32.0f
#define COVER_MAX 4
/* A roof slope's rectangle is mostly sky. The triangles have to fill it.
   A piece below COVER_SOLID is split once into four before that test. */
#define COVER_FILL 0.55f
#define COVER_SOLID 0.80f

typedef struct Cover {
    float sx0, sy0, sx1, sy1;
    float z1;
    /* Screen area of the triangles, not of the rectangle. */
    float fill;
} Cover;

static Cover g_cover[COVER_MAX];
static int g_ncover;
static Cover g_facade[COVER_MAX];
static int g_nfacade;
static int g_cover_draw = 1;

static void box_depth(const Camera *cam, const Basis *basis,
                      float cx, float cy, float cz,
                      float ex, float ey, float ez,
                      float *z0, float *z1)
{
    float fx;
    float fy;
    float fz;
    float z;
    float rad;

    fx = basis->fx * basis->cp;
    fy = basis->sp;
    fz = basis->fz * basis->cp;
    z = (cx - cam->x) * fx + (cy - cam->y) * fy + (cz - cam->z) * fz;
    rad = ex * df_absf(fx) + ey * df_absf(fy) + ez * df_absf(fz);
    *z0 = z - rad;
    *z1 = z + rad;
}

static void view_local(const Instance *in, const Camera *cam, const Basis *basis,
                       float lx, float ly, float lz,
                       float *vx, float *vy, float *vz)
{
    float wx, wy, wz;

    instance_point(in, lx, ly, lz, &wx, &wy, &wz);
    camera_view(cam, basis, wx, wy, wz, vx, vy, vz);
}

/*
 * A floor or a ceiling. Its normal is more vertical than horizontal,
 * so the street in front of the eye is not behind it. A corner that
 * misses the glass must not be clamped onto the opposite edge: that
 * turned the eave into a rectangle the size of the picture.
 */
static int face_overhead(const Instance *in, float nx, float ny, float nz)
{
    float wx, wy, wz;

    wx = in->m[0] * nx + in->m[1] * ny + in->m[2] * nz;
    wy = in->m[3] * nx + in->m[4] * ny + in->m[5] * nz;
    wz = in->m[6] * nx + in->m[7] * ny + in->m[8] * nz;
    return wy * wy > wx * wx + wz * wz;
}

#define CV_MAX 12

typedef struct Cv {
    float x, y, z;
} Cv;

static float cv_dist(int plane, float hx, float hy, const Cv *v)
{
    switch (plane) {
    case 0:
        return v->z - DF_NEAR;
    case 1:
        return COVER_SPAN - v->z;
    case 2:
        return v->z * hx + v->x;
    case 3:
        return v->z * hx - v->x;
    case 4:
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
        denom = dist[i] - dist[j];
        if (denom > -1.0e-8f && denom < 1.0e-8f) {
            continue;
        }
        t = dist[i] / denom;
        hit.x = in[i].x + (in[j].x - in[i].x) * t;
        hit.y = in[i].y + (in[j].y - in[i].y) * t;
        hit.z = in[i].z + (in[j].z - in[i].z) * t;
        if (ain && !bin) {
            if (nout < CV_MAX) {
                out[nout++] = hit;
            }
        } else if (!ain && bin) {
            if (nout < CV_MAX) {
                out[nout++] = hit;
            }
            if (nout < CV_MAX) {
                out[nout++] = in[j];
            }
        }
    }
    return nout;
}

static float cover_area(const Cover *c)
{
    return (c->sx1 - c->sx0) * (c->sy1 - c->sy0);
}

/* Keep the largest faces. One mesh must not union a roof and a wall. */
static float poly_area(const float *sx, const float *sy, int n)
{
    float a;
    int i;

    a = 0.0f;
    for (i = 0; i < n; i++) {
        int j;

        j = (i + 1) % n;
        a += sx[i] * sy[j] - sx[j] * sy[i];
    }
    if (a < 0.0f) {
        a = -a;
    }
    return a * 0.5f;
}

static void consider_cover(float sx0, float sy0, float sx1, float sy1,
                           float z1, float fill)
{
    float area;
    int i;
    int small;
    float small_a;

    if (z1 > 1.0e8f || sx1 - sx0 < 8.0f || sy1 - sy0 < 8.0f) {
        return;
    }
    area = (sx1 - sx0) * (sy1 - sy0);
    if (g_ncover < COVER_MAX) {
        g_cover[g_ncover].sx0 = sx0;
        g_cover[g_ncover].sy0 = sy0;
        g_cover[g_ncover].sx1 = sx1;
        g_cover[g_ncover].sy1 = sy1;
        g_cover[g_ncover].z1 = z1;
        g_cover[g_ncover].fill = fill;
        g_ncover++;
        return;
    }
    small = 0;
    small_a = cover_area(&g_cover[0]);
    for (i = 1; i < g_ncover; i++) {
        float a;

        a = cover_area(&g_cover[i]);
        if (a < small_a) {
            small = i;
            small_a = a;
        }
    }
    if (area <= small_a) {
        return;
    }
    g_cover[small].sx0 = sx0;
    g_cover[small].sy0 = sy0;
    g_cover[small].sx1 = sx1;
    g_cover[small].sy1 = sy1;
    g_cover[small].z1 = z1;
    g_cover[small].fill = fill;
}

static void cv_mid(Cv *o, const Cv *a, const Cv *b)
{
    o->x = (a->x + b->x) * 0.5f;
    o->y = (a->y + b->y) * 0.5f;
    o->z = (a->z + b->z) * 0.5f;
}

static int point_in_poly(float x, float y, const float *sx, const float *sy, int n)
{
    int i;
    int sign;

    sign = 0;
    for (i = 0; i < n; i++) {
        int j;
        float cross;
        int s;

        j = (i + 1) % n;
        cross = (sx[j] - sx[i]) * (y - sy[i]) - (sy[j] - sy[i]) * (x - sx[i]);
        if (cross > 0.5f) {
            s = 1;
        } else if (cross < -0.5f) {
            s = -1;
        } else {
            continue;
        }
        if (sign == 0) {
            sign = s;
        } else if (s != sign) {
            return 0;
        }
    }
    return 1;
}

/* Largest square grown from the centroid that stays inside the polygon. */
static void inner_rect(const float *sx, const float *sy, int n,
                       float ox0, float oy0, float ox1, float oy1,
                       float *ix0, float *iy0, float *ix1, float *iy1)
{
    float cx;
    float cy;
    float lo;
    float hi;
    int i;
    int k;

    cx = 0.0f;
    cy = 0.0f;
    for (i = 0; i < n; i++) {
        cx += sx[i];
        cy += sy[i];
    }
    cx /= (float)n;
    cy /= (float)n;
    lo = 0.0f;
    hi = 1.0f;
    for (k = 0; k < 8; k++) {
        float t;
        float x0, y0, x1, y1;

        t = (lo + hi) * 0.5f;
        x0 = cx + (ox0 - cx) * t;
        y0 = cy + (oy0 - cy) * t;
        x1 = cx + (ox1 - cx) * t;
        y1 = cy + (oy1 - cy) * t;
        if (point_in_poly(x0, y0, sx, sy, n) &&
            point_in_poly(x1, y0, sx, sy, n) &&
            point_in_poly(x0, y1, sx, sy, n) &&
            point_in_poly(x1, y1, sx, sy, n)) {
            lo = t;
        } else {
            hi = t;
        }
    }
    *ix0 = cx + (ox0 - cx) * lo;
    *iy0 = cy + (oy0 - cy) * lo;
    *ix1 = cx + (ox1 - cx) * lo;
    *iy1 = cy + (oy1 - cy) * lo;
}

/*
 * Clip one triangle to the picture. 1 when it still covers pixels.
 * sx0..sy1 is the outer square. ix0..iy1 is the inner square, whose
 * edges sit inside the triangle. z1 and filled are written.
 */
static int measure_tri(const Cv *a, const Cv *b, const Cv *c,
                       float hx, float hy,
                       float *sx0, float *sy0, float *sx1, float *sy1,
                       float *ix0, float *iy0, float *ix1, float *iy1,
                       float *z1, float *filled)
{
    Cv poly[2][CV_MAX];
    float psx[CV_MAX];
    float psy[CV_MAX];
    int src;
    int npoly;
    int plane;
    int np;
    int k;

    poly[0][0] = *a;
    poly[0][1] = *b;
    poly[0][2] = *c;
    npoly = 3;
    src = 0;
    for (plane = 0; plane < 6; plane++) {
        int dst;
        int ndst;

        dst = src ^ 1;
        ndst = cv_clip(poly[src], npoly, poly[dst], plane, hx, hy);
        if (ndst < 3) {
            return 0;
        }
        src = dst;
        npoly = ndst;
    }
    *sx0 = 1.0e9f;
    *sy0 = 1.0e9f;
    *sx1 = -1.0e9f;
    *sy1 = -1.0e9f;
    *z1 = 1.0e9f;
    np = 0;
    for (k = 0; k < npoly; k++) {
        float sx, sy, oow, ooz;
        float vz;

        vz = poly[src][k].z;
        if (!camera_project(poly[src][k].x, poly[src][k].y, vz,
                            &sx, &sy, &oow, &ooz)) {
            continue;
        }
        if (np < CV_MAX) {
            psx[np] = sx;
            psy[np] = sy;
            np++;
        }
        if (sx < *sx0) {
            *sx0 = sx;
        }
        if (sy < *sy0) {
            *sy0 = sy;
        }
        if (sx > *sx1) {
            *sx1 = sx;
        }
        if (sy > *sy1) {
            *sy1 = sy;
        }
        if (vz > DF_NEAR + 1.0f && vz < *z1) {
            *z1 = vz;
        }
    }
    if (np < 3) {
        return 0;
    }
    *filled = poly_area(psx, psy, np);
    inner_rect(psx, psy, np, *sx0, *sy0, *sx1, *sy1, ix0, iy0, ix1, iy1);
    return 1;
}

/* Four pieces. A triangle fills at most half of its square, so the bar is 0.40. */
static void split_tri(const Cv *a, const Cv *b, const Cv *c, float hx, float hy)
{
    Cv ab, bc, ca;
    Cv p[3];
    int i;

    cv_mid(&ab, a, b);
    cv_mid(&bc, b, c);
    cv_mid(&ca, c, a);
    p[0] = *a;
    p[1] = ab;
    p[2] = ca;
    for (i = 0; i < 4; i++) {
        float sx0, sy0, sx1, sy1, z1, filled;
        float ix0, iy0, ix1, iy1;
        const Cv *u;
        const Cv *v;
        const Cv *w;

        if (i == 1) {
            u = b;
            v = &bc;
            w = &ab;
        } else if (i == 2) {
            u = c;
            v = &ca;
            w = &bc;
        } else if (i == 3) {
            u = &ab;
            v = &bc;
            w = &ca;
        } else {
            u = &p[0];
            v = &p[1];
            w = &p[2];
        }
        if (measure_tri(u, v, w, hx, hy, &sx0, &sy0, &sx1, &sy1,
                        &ix0, &iy0, &ix1, &iy1, &z1, &filled)) {
            float outer;

            outer = (sx1 - sx0) * (sy1 - sy0);
            if (outer < 64.0f || filled < outer * 0.40f) {
                continue;
            }
            consider_cover(ix0, iy0, ix1, iy1, z1, filled);
        }
    }
}

/* One rectangle per front face, clipped to the picture. */
static void push_cover(const Instance *in, const Camera *cam, const Basis *basis)
{
    float hx;
    float hy;
    int f;

    if (!in->mesh || in->mesh->id == 0) {
        return;
    }
    if (in->mesh->nfaces < 1 || !in->mesh->faces || !in->mesh->tris) {
        return;
    }
    hx = ((float)DF_VIEW_W * 0.5f) / camera_focal();
    hy = ((float)DF_VIEW_H * 0.5f) / camera_focal();
    for (f = 0; f < in->mesh->nfaces; f++) {
        const Face *face;
        float minz;
        int t;
        int tend;
        int sample;

        face = &in->mesh->faces[f];
        if (face->ntris < 1 || face->first < 0) {
            continue;
        }
        if (face_overhead(in, face->nx, face->ny, face->nz)) {
            continue;
        }
        tend = face->first + face->ntris;
        if (tend > in->mesh->ntris) {
            tend = in->mesh->ntris;
        }
        minz = 1.0e9f;
        sample = 0;
        for (t = face->first; t < tend; t++) {
            const Tri *tri;
            int k;

            tri = &in->mesh->tris[t];
            for (k = 0; k < 3; k++) {
                float vx, vy, vz;

                view_local(in, cam, basis, tri->x[k], tri->y[k], tri->z[k],
                           &vx, &vy, &vz);
                if (vz < minz) {
                    minz = vz;
                }
                if (vz >= DF_NEAR && vz <= COVER_SPAN) {
                    sample = 1;
                }
            }
        }
        if (!sample || minz >= COVER_NEAR) {
            continue;
        }
        if (face->ntris >= 2) {
            const Tri *tri;

            tri = &in->mesh->tris[face->first];
            if (!instance_faces(in, cam, basis, face->nx, face->ny, face->nz,
                                tri->x[0], tri->y[0], tri->z[0])) {
                continue;
            }
        }
        {
            float sx0, sy0, sx1, sy1, z1, filled;
            int any;

            sx0 = 1.0e9f;
            sy0 = 1.0e9f;
            sx1 = -1.0e9f;
            sy1 = -1.0e9f;
            z1 = 1.0e9f;
            filled = 0.0f;
            any = 0;
            for (t = face->first; t < tend; t++) {
                const Tri *tri;
                Cv v0, v1, v2;
                float x0, y0, x1, y1, zn, fa;
                float ix0, iy0, ix1, iy1;

                tri = &in->mesh->tris[t];
                view_local(in, cam, basis, tri->x[0], tri->y[0], tri->z[0],
                           &v0.x, &v0.y, &v0.z);
                view_local(in, cam, basis, tri->x[1], tri->y[1], tri->z[1],
                           &v1.x, &v1.y, &v1.z);
                view_local(in, cam, basis, tri->x[2], tri->y[2], tri->z[2],
                           &v2.x, &v2.y, &v2.z);
                if (!measure_tri(&v0, &v1, &v2, hx, hy, &x0, &y0, &x1, &y1,
                                 &ix0, &iy0, &ix1, &iy1, &zn, &fa)) {
                    continue;
                }
                if (x0 < sx0) {
                    sx0 = x0;
                }
                if (y0 < sy0) {
                    sy0 = y0;
                }
                if (x1 > sx1) {
                    sx1 = x1;
                }
                if (y1 > sy1) {
                    sy1 = y1;
                }
                if (zn < z1) {
                    z1 = zn;
                }
                filled += fa;
                any = 1;
            }
            if (any) {
                float box;

                box = (sx1 - sx0) * (sy1 - sy0);
                if (box >= 64.0f && filled >= box * COVER_SOLID) {
                    consider_cover(sx0, sy0, sx1, sy1, z1, filled);
                } else {
                    for (t = face->first; t < tend; t++) {
                        const Tri *tri;
                        Cv v0, v1, v2;

                        tri = &in->mesh->tris[t];
                        view_local(in, cam, basis, tri->x[0], tri->y[0], tri->z[0],
                                   &v0.x, &v0.y, &v0.z);
                        view_local(in, cam, basis, tri->x[1], tri->y[1], tri->z[1],
                                   &v1.x, &v1.y, &v1.z);
                        view_local(in, cam, basis, tri->x[2], tri->y[2], tri->z[2],
                                   &v2.x, &v2.y, &v2.z);
                        split_tri(&v0, &v1, &v2, hx, hy);
                    }
                }
            }
        }
    }
}

static void find_covers(const Scene *scene, const Camera *cam, const Basis *basis,
                        const int *idx, int n)
{
    int i;

    g_ncover = 0;
    for (i = 0; i < n; i++) {
        float cx, cy, cz, ex, ey, ez;
        float z0, z1;

        if (!model_box(scene, idx[i], &cx, &cy, &cz, &ex, &ey, &ez)) {
            continue;
        }
        if (ey < COVER_H) {
            continue;
        }
        box_depth(cam, basis, cx, cy, cz, ex, ey, ez, &z0, &z1);
        (void)z1;
        if (z0 >= COVER_NEAR) {
            continue;
        }
        push_cover(&scene->models[idx[i]], cam, basis);
    }
}

static int cover_tall(const Cover *c)
{
    return c->sy1 - c->sy0 >= 200.0f;
}

/* Adjacent panels of one wall. A sign post across the street does not meet. */
static int cover_meet(const Cover *a, const Cover *b)
{
    float gap;
    float sx0, sy0, sx1, sy1;
    float box;

    gap = 16.0f;
    if (a->z1 > b->z1 + 48.0f || b->z1 > a->z1 + 48.0f) {
        return 0;
    }
    if (a->sx1 + gap < b->sx0 || b->sx1 + gap < a->sx0) {
        return 0;
    }
    if (a->sy1 + gap < b->sy0 || b->sy1 + gap < a->sy0) {
        return 0;
    }
    sx0 = a->sx0 < b->sx0 ? a->sx0 : b->sx0;
    sy0 = a->sy0 < b->sy0 ? a->sy0 : b->sy0;
    sx1 = a->sx1 > b->sx1 ? a->sx1 : b->sx1;
    sy1 = a->sy1 > b->sy1 ? a->sy1 : b->sy1;
    box = (sx1 - sx0) * (sy1 - sy0);
    if (box < 1.0f) {
        return 0;
    }
    /* Joining a slope's pieces must not put the sky back in the box. */
    return a->fill + b->fill >= box * COVER_FILL;
}

static void cover_grow(Cover *u, const Cover *c)
{
    if (c->sx0 < u->sx0) {
        u->sx0 = c->sx0;
    }
    if (c->sy0 < u->sy0) {
        u->sy0 = c->sy0;
    }
    if (c->sx1 > u->sx1) {
        u->sx1 = c->sx1;
    }
    if (c->sy1 > u->sy1) {
        u->sy1 = c->sy1;
    }
    if (c->z1 < u->z1) {
        u->z1 = c->z1;
    }
    u->fill += c->fill;
}

/*
 * Tall faces join only when their rectangles meet. A brick course and
 * the column beside it become one wall. The sign post across the gap
 * stays its own rectangle, so the street between them is not solid.
 */
static void facade_build(void)
{
    int used[COVER_MAX];
    int i;

    g_nfacade = 0;
    for (i = 0; i < COVER_MAX; i++) {
        used[i] = 0;
    }
    for (i = 0; i < g_ncover; i++) {
        int grew;
        Cover u;

        if (used[i] || !cover_tall(&g_cover[i])) {
            continue;
        }
        used[i] = 1;
        u = g_cover[i];
        do {
            int j;

            grew = 0;
            for (j = 0; j < g_ncover; j++) {
                if (used[j] || !cover_tall(&g_cover[j])) {
                    continue;
                }
                if (!cover_meet(&u, &g_cover[j])) {
                    continue;
                }
                used[j] = 1;
                cover_grow(&u, &g_cover[j]);
                grew = 1;
            }
        } while (grew);
        if (g_nfacade < COVER_MAX) {
            g_facade[g_nfacade++] = u;
        }
    }
}

/* 1 when every corner is in front and the projected box sits in the cover. */
static int box_in_cover(const Camera *cam, const Basis *basis, const Cover *c,
                        float cx, float cy, float cz,
                        float ex, float ey, float ez)
{
    static const float s[2] = {-1.0f, 1.0f};
    float sx0, sy0, sx1, sy1;
    int a, b, d;

    sx0 = 1.0e9f;
    sy0 = 1.0e9f;
    sx1 = -1.0e9f;
    sy1 = -1.0e9f;
    for (a = 0; a < 2; a++) {
        for (b = 0; b < 2; b++) {
            for (d = 0; d < 2; d++) {
                float vx, vy, vz;
                float sx, sy, oow, ooz;

                camera_view(cam, basis,
                            cx + s[a] * ex, cy + s[b] * ey, cz + s[d] * ez,
                            &vx, &vy, &vz);
                if (vz < DF_NEAR) {
                    return 0;
                }
                if (!camera_project(vx, vy, vz, &sx, &sy, &oow, &ooz)) {
                    return 0;
                }
                if (sx < sx0) {
                    sx0 = sx;
                }
                if (sy < sy0) {
                    sy0 = sy;
                }
                if (sx > sx1) {
                    sx1 = sx;
                }
                if (sy > sy1) {
                    sy1 = sy;
                }
            }
        }
    }
    return sx0 >= c->sx0 && sy0 >= c->sy0 && sx1 <= c->sx1 && sy1 <= c->sy1;
}

/* The center of the box, for a facade wider than any one model. */
static int center_in_cover(const Camera *cam, const Basis *basis, const Cover *c,
                           float cx, float cy, float cz)
{
    float vx, vy, vz;
    float sx, sy, oow, ooz;

    camera_view(cam, basis, cx, cy, cz, &vx, &vy, &vz);
    if (vz < DF_NEAR) {
        return 0;
    }
    if (!camera_project(vx, vy, vz, &sx, &sy, &oow, &ooz)) {
        return 0;
    }
    return sx >= c->sx0 && sy >= c->sy0 && sx <= c->sx1 && sy <= c->sy1;
}

static int hide_covered(const Scene *scene, const Camera *cam, const Basis *basis,
                        int *idx, int n, int sprites)
{
    int i;
    int w;

    if (g_ncover < 1 || n < 1) {
        return n;
    }
    facade_build();
    w = 0;
    for (i = 0; i < n; i++) {
        float cx, cy, cz, ex, ey, ez;
        float z0, z1;
        int ok;
        int c;
        int hide;

        if (sprites) {
            ok = flat_box(&scene->sprites[idx[i]], &cx, &cy, &cz, &ex, &ey, &ez);
        } else {
            ok = model_box(scene, idx[i], &cx, &cy, &cz, &ex, &ey, &ez);
        }
        if (!ok) {
            idx[w++] = idx[i];
            continue;
        }
        box_depth(cam, basis, cx, cy, cz, ex, ey, ez, &z0, &z1);
        (void)z1;
        hide = 0;
        for (c = 0; c < g_ncover; c++) {
            if (z0 <= g_cover[c].z1 + COVER_THICK) {
                continue;
            }
            /*
             * A cover that reaches the edges of the glass hides every
             * box behind it. A large box still pokes off the glass in
             * projection, and that is not a view past the wall. A
             * smaller cover hides only a box that sits inside it.
             */
            if (g_cover[c].sx0 <= 8.0f && g_cover[c].sy0 <= 8.0f &&
                g_cover[c].sx1 >= (float)DF_VIEW_W - 8.0f &&
                g_cover[c].sy1 >= (float)DF_VIEW_H - 8.0f) {
                hide = 1;
                break;
            }
            if (box_in_cover(cam, basis, &g_cover[c], cx, cy, cz, ex, ey, ez)) {
                hide = 1;
                break;
            }
        }
        /*
         * A house behind one wall is wider than the brick. Its box
         * sticks out even when the mesh is hidden, so the center has
         * to sit inside that wall. A gap between two walls is not
         * inside either rectangle.
         */
        if (!hide) {
            int f;

            for (f = 0; f < g_nfacade; f++) {
                if (z0 > g_facade[f].z1 + COVER_THICK &&
                    center_in_cover(cam, basis, &g_facade[f], cx, cy, cz)) {
                    hide = 1;
                    break;
                }
            }
        }
        if (hide) {
            continue;
        }
        idx[w++] = idx[i];
    }
    return w;
}

int scene_visible_models(const Scene *scene, const Camera *cam, int *out, int cap)
{
    Plane planes[6];
    Basis basis;
    int n;
    int s;

    g_ncover = 0;
    g_nfacade = 0;
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
    camera_basis(cam, &basis);
    find_covers(scene, cam, &basis, out, n);
    return hide_covered(scene, cam, &basis, out, n, 0);
}

int scene_visible_sprites(const Scene *scene, const Camera *cam, int *out, int cap)
{
    Plane planes[6];
    Basis basis;
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
    if (g_ncover < 1 || !cam) {
        return n;
    }
    camera_basis(cam, &basis);
    return hide_covered(scene, cam, &basis, out, n, 1);
}

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
    if (!g_cover_draw) {
        return 0;
    }
    return g_ncover;
}

void scene_cover_at(int index, float *x0, float *y0, float *x1, float *y1)
{
    if (index < 0 || index >= g_ncover) {
        return;
    }
    if (x0) {
        *x0 = g_cover[index].sx0;
    }
    if (y0) {
        *y0 = g_cover[index].sy0;
    }
    if (x1) {
        *x1 = g_cover[index].sx1;
    }
    if (y1) {
        *y1 = g_cover[index].sy1;
    }
}

int scene_facade_count(void)
{
    if (!g_cover_draw) {
        return 0;
    }
    return g_nfacade;
}

void scene_facade_at(int index, float *x0, float *y0, float *x1, float *y1)
{
    if (index < 0 || index >= g_nfacade) {
        return;
    }
    if (x0) {
        *x0 = g_facade[index].sx0;
    }
    if (y0) {
        *y0 = g_facade[index].sy0;
    }
    if (x1) {
        *x1 = g_facade[index].sx1;
    }
    if (y1) {
        *y1 = g_facade[index].sy1;
    }
}
