#include "engine/scene/scene.h"
#include "engine/core/df.h"

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
    sx = (float)DF_VIEW_W * 0.5f / DF_FOCAL;
    sy = (float)DF_VIEW_H * 0.5f / DF_FOCAL;

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

int scene_visible_models(const Scene *scene, const Camera *cam, int *out, int cap)
{
    Plane planes[6];
    int n;
    int s;

    if (!scene || !out || cap < 1 || scene->nmodels < 1) {
        return 0;
    }
    if (!cam) {
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
        return n;
    }
    return take_visible(scene, planes, 0, scene->nmodels, out, cap, 0);
}

int scene_visible_sprites(const Scene *scene, const Camera *cam, int *out, int cap)
{
    Plane planes[6];
    int i;
    int n;

    if (!scene || !out || cap < 1 || scene->nsprites < 1) {
        return 0;
    }
    if (!cam) {
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
    return n;
}
