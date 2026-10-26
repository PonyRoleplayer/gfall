#include "engine/scene/collide.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/*
 * See collide.h and docs/collision.md. Per frame: one gather, a few
 * sweeps over the gathered set. No allocation after collide_use, and no
 * float-to-int cast outside it (df_ifloor), so the x87 never changes its
 * rounding mode in the frame.
 */

/* The model grid's cells, world units on a side: a quarter of a city block. */
#define CELL 512.0f
#define GRID_MAX 256
/*
 * The gap a sweep leaves before what it hits, in ellipsoid space (a unit
 * is the body's radius): 1/200 of it, a tenth of a world unit. Without
 * it the body ends exactly on the plane and the next sweep starts
 * embedded in it.
 */
#define VERY_CLOSE 0.005f
/* Slides per sweep: hit, slide along, hit again. Five settles any corner. */
#define SLIDE_STEPS 5

/* A triangle in ellipsoid space: corners, unit normal, plane n . p + d = 0. */
typedef struct CTri {
    float a[3];
    float b[3];
    float c[3];
    float n[3];
    float d;
} CTri;

static CTri g_set[COLLIDE_SET];
static int g_nset;
static int g_dropped;

static float g_rx = 16.0f;
static float g_ry = 36.0f;
static float g_irx = 1.0f / 16.0f;
static float g_iry = 1.0f / 36.0f;

/* The model grid: for each cell, the models whose box reaches into it. */
static const Scene *g_scene;
static int g_nmodels;
/* Scene.version the grid was built for. */
static int g_version;
static df_u32 g_epoch;
static int g_built;
static float g_x0;
static float g_z0;
static float g_cell;
static int g_gw;
static int g_gh;
static int *g_first;
static int *g_refs;
/* Gather stamps, so a model in several cells is gathered once. */
static df_u32 *g_mark;
static df_u32 g_stamp;
static float g_miny;
static float g_maxy;

static float root(float x)
{
    if (x <= 0.0f) {
        return 0.0f;
    }
    __asm__ __volatile__("fsqrt" : "=t"(x) : "0"(x));
    return x;
}

static float dot3(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void collide_set_shape(float rx, float ry)
{
    g_rx = rx > 1.0f ? rx : 1.0f;
    g_ry = ry > 1.0f ? ry : 1.0f;
    g_irx = 1.0f / g_rx;
    g_iry = 1.0f / g_ry;
}

void collide_extent(float *miny, float *maxy)
{
    *miny = g_miny;
    *maxy = g_maxy;
}

void collide_stats(int *gathered, int *dropped)
{
    *gathered = g_nset;
    *dropped = g_dropped;
}

/* ---- The model grid ------------------------------------------------- */

static void model_box(const Scene *scene, int i, Bound *out)
{
    if (scene->bounds) {
        *out = scene->bounds[i];
        return;
    }
    if (!instance_world_box(&scene->models[i], out)) {
        out->cx = out->cy = out->cz = 0.0f;
        out->ex = out->ey = out->ez = -1.0f;
    }
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* The cells a box covers, clamped to the grid. */
static void cell_span(float minx, float minz, float maxx, float maxz,
                      int *x0, int *z0, int *x1, int *z1)
{
    *x0 = clampi(df_ifloor((minx - g_x0) / g_cell), 0, g_gw - 1);
    *z0 = clampi(df_ifloor((minz - g_z0) / g_cell), 0, g_gh - 1);
    *x1 = clampi(df_ifloor((maxx - g_x0) / g_cell), 0, g_gw - 1);
    *z1 = clampi(df_ifloor((maxz - g_z0) / g_cell), 0, g_gh - 1);
}

int collide_use(const Scene *scene)
{
    float minx, minz, maxx, maxz;
    int i;
    int nref;
    int pass;

    if (!scene || scene->nmodels < 1) {
        g_built = 0;
        return 0;
    }
    if (g_built && scene == g_scene && scene->nmodels == g_nmodels && g_epoch == mem_epoch()
        && scene->version == g_version) {
        return 1;
    }
    g_built = 0;
    minx = minz = g_miny = 1.0e30f;
    maxx = maxz = g_maxy = -1.0e30f;
    for (i = 0; i < scene->nmodels; i++) {
        Bound b;

        model_box(scene, i, &b);
        if (b.ex < 0.0f) {
            continue;
        }
        minx = b.cx - b.ex < minx ? b.cx - b.ex : minx;
        maxx = b.cx + b.ex > maxx ? b.cx + b.ex : maxx;
        minz = b.cz - b.ez < minz ? b.cz - b.ez : minz;
        maxz = b.cz + b.ez > maxz ? b.cz + b.ez : maxz;
        g_miny = b.cy - b.ey < g_miny ? b.cy - b.ey : g_miny;
        g_maxy = b.cy + b.ey > g_maxy ? b.cy + b.ey : g_maxy;
    }
    if (maxx < minx) {
        return 0;
    }
    /* A wider cell for a huge place, so the grid stays at most GRID_MAX a side. */
    g_cell = CELL;
    while ((maxx - minx) / g_cell >= (float)GRID_MAX || (maxz - minz) / g_cell >= (float)GRID_MAX) {
        g_cell *= 2.0f;
    }
    g_x0 = minx;
    g_z0 = minz;
    g_gw = df_ifloor((maxx - minx) / g_cell) + 1;
    g_gh = df_ifloor((maxz - minz) / g_cell) + 1;
    g_first = (int *)mem_alloc((df_u32)(g_gw * g_gh + 1) * (df_u32)sizeof(int));
    g_mark = (df_u32 *)mem_alloc((df_u32)scene->nmodels * (df_u32)sizeof(df_u32));
    if (!g_first || !g_mark) {
        log_text(LOG_ERROR, "collide: out of memory for the model grid");
        return 0;
    }
    /* Count each cell's models, then lay them out, then fill. */
    nref = 0;
    for (pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            int run;

            g_refs = (int *)mem_alloc((df_u32)(nref > 0 ? nref : 1) * (df_u32)sizeof(int));
            if (!g_refs) {
                log_text(LOG_ERROR, "collide: out of memory for the model grid");
                return 0;
            }
            run = 0;
            for (i = 0; i <= g_gw * g_gh; i++) {
                int n;

                n = g_first[i];
                g_first[i] = run;
                run += n;
            }
        }
        for (i = 0; i < scene->nmodels; i++) {
            Bound b;
            int x0, z0, x1, z1, x, z;

            model_box(scene, i, &b);
            if (b.ex < 0.0f || !scene->models[i].mesh) {
                continue;
            }
            cell_span(b.cx - b.ex, b.cz - b.ez, b.cx + b.ex, b.cz + b.ez, &x0, &z0, &x1, &z1);
            for (z = z0; z <= z1; z++) {
                for (x = x0; x <= x1; x++) {
                    if (pass == 0) {
                        g_first[z * g_gw + x]++;
                        nref++;
                    } else {
                        g_refs[g_first[z * g_gw + x]++] = i;
                    }
                }
            }
        }
    }
    /* The fill moved each start to the next cell's: shift back. */
    for (i = g_gw * g_gh; i > 0; i--) {
        g_first[i] = g_first[i - 1];
    }
    g_first[0] = 0;
    for (i = 0; i < scene->nmodels; i++) {
        g_mark[i] = 0;
    }
    g_stamp = 0;
    g_scene = scene;
    g_nmodels = scene->nmodels;
    g_version = scene->version;
    g_epoch = mem_epoch();
    g_built = 1;
    log_print(LOG_INFO, "collide: %d models in a %d x %d grid of %d, %d entries",
              scene->nmodels, g_gw, g_gh, df_iround(g_cell), nref);
    return 1;
}

/* ---- The gather ----------------------------------------------------- */

/*
 * One model's triangles that meet the box. The box goes into the model's
 * own frame (the matrix is a rotation, so its transpose undoes it), and
 * each triangle is tested there against its own corners' range: no
 * transform for the many that miss. A survivor is turned into the world,
 * scaled into ellipsoid space, and given its plane.
 */
static void gather_model(const Instance *in, const float *wc, const float *we)
{
    const float *m;
    const Mesh *mesh;
    float lc[3];
    float le[3];
    float dx, dy, dz;
    int t;

    m = in->m;
    mesh = in->mesh;
    dx = wc[0] - in->x;
    dy = wc[1] - in->y;
    dz = wc[2] - in->z;
    lc[0] = m[0] * dx + m[3] * dy + m[6] * dz;
    lc[1] = m[1] * dx + m[4] * dy + m[7] * dz;
    lc[2] = m[2] * dx + m[5] * dy + m[8] * dz;
    le[0] = df_absf(m[0]) * we[0] + df_absf(m[3]) * we[1] + df_absf(m[6]) * we[2];
    le[1] = df_absf(m[1]) * we[0] + df_absf(m[4]) * we[1] + df_absf(m[7]) * we[2];
    le[2] = df_absf(m[2]) * we[0] + df_absf(m[5]) * we[1] + df_absf(m[8]) * we[2];
    for (t = 0; t < mesh->ntris; t++) {
        const Tri *tri;
        CTri *ct;
        float e1[3];
        float e2[3];
        float len;
        int k;

        tri = &mesh->tris[t];
        /* A face the loader could not texture is not part of the place. */
        if (tri->slot == 0xffff) {
            continue;
        }
        if ((tri->x[0] < lc[0] - le[0] && tri->x[1] < lc[0] - le[0] && tri->x[2] < lc[0] - le[0])
            || (tri->x[0] > lc[0] + le[0] && tri->x[1] > lc[0] + le[0] && tri->x[2] > lc[0] + le[0])
            || (tri->y[0] < lc[1] - le[1] && tri->y[1] < lc[1] - le[1] && tri->y[2] < lc[1] - le[1])
            || (tri->y[0] > lc[1] + le[1] && tri->y[1] > lc[1] + le[1] && tri->y[2] > lc[1] + le[1])
            || (tri->z[0] < lc[2] - le[2] && tri->z[1] < lc[2] - le[2] && tri->z[2] < lc[2] - le[2])
            || (tri->z[0] > lc[2] + le[2] && tri->z[1] > lc[2] + le[2] && tri->z[2] > lc[2] + le[2])) {
            continue;
        }
        if (g_nset >= COLLIDE_SET) {
            g_dropped++;
            continue;
        }
        ct = &g_set[g_nset];
        for (k = 0; k < 3; k++) {
            float *p;

            p = k == 0 ? ct->a : (k == 1 ? ct->b : ct->c);
            instance_point(in, tri->x[k], tri->y[k], tri->z[k], &p[0], &p[1], &p[2]);
            p[0] *= g_irx;
            p[1] *= g_iry;
            p[2] *= g_irx;
        }
        for (k = 0; k < 3; k++) {
            e1[k] = ct->b[k] - ct->a[k];
            e2[k] = ct->c[k] - ct->a[k];
        }
        ct->n[0] = e1[1] * e2[2] - e1[2] * e2[1];
        ct->n[1] = e1[2] * e2[0] - e1[0] * e2[2];
        ct->n[2] = e1[0] * e2[1] - e1[1] * e2[0];
        len = root(dot3(ct->n, ct->n));
        /* A sliver has no plane to slide on; its neighbours carry the shape. */
        if (len < 1.0e-6f) {
            continue;
        }
        ct->n[0] /= len;
        ct->n[1] /= len;
        ct->n[2] /= len;
        ct->d = -dot3(ct->n, ct->a);
        g_nset++;
    }
}

int collide_gather(float minx, float miny, float minz,
                   float maxx, float maxy, float maxz)
{
    float wc[3];
    float we[3];
    int x0, z0, x1, z1, x, z;

    g_nset = 0;
    g_dropped = 0;
    if (!g_built) {
        return 0;
    }
    wc[0] = (minx + maxx) * 0.5f;
    wc[1] = (miny + maxy) * 0.5f;
    wc[2] = (minz + maxz) * 0.5f;
    we[0] = (maxx - minx) * 0.5f;
    we[1] = (maxy - miny) * 0.5f;
    we[2] = (maxz - minz) * 0.5f;
    g_stamp++;
    cell_span(minx, minz, maxx, maxz, &x0, &z0, &x1, &z1);
    for (z = z0; z <= z1; z++) {
        for (x = x0; x <= x1; x++) {
            int r;

            for (r = g_first[z * g_gw + x]; r < g_first[z * g_gw + x + 1]; r++) {
                int i;
                Bound b;

                i = g_refs[r];
                if (g_mark[i] == g_stamp) {
                    continue;
                }
                g_mark[i] = g_stamp;
                model_box(g_scene, i, &b);
                if (df_absf(b.cx - wc[0]) > b.ex + we[0] || df_absf(b.cy - wc[1]) > b.ey + we[1]
                    || df_absf(b.cz - wc[2]) > b.ez + we[2]) {
                    continue;
                }
                gather_model(&g_scene->models[i], wc, we);
            }
        }
    }
    if (g_dropped > 0) {
        log_print(LOG_WARN, "collide: %d triangles past the %d the set holds were left out",
                  g_dropped, COLLIDE_SET);
    }
    return g_nset;
}

/* ---- The sweep ------------------------------------------------------ */

typedef struct Contact {
    int found;
    float t;
    float point[3];
} Contact;

/* The smaller root of a t^2 + b t + c in (0, limit), if any. */
static int lowest_root(float a, float b, float c, float limit, float *out)
{
    float det;
    float s;
    float r1;
    float r2;

    if (df_absf(a) < 1.0e-12f) {
        return 0;
    }
    det = b * b - 4.0f * a * c;
    if (det < 0.0f) {
        return 0;
    }
    s = root(det);
    r1 = (-b - s) / (2.0f * a);
    r2 = (-b + s) / (2.0f * a);
    if (r1 > r2) {
        float tmp;

        tmp = r1;
        r1 = r2;
        r2 = tmp;
    }
    if (r1 > 0.0f && r1 < limit) {
        *out = r1;
        return 1;
    }
    if (r2 > 0.0f && r2 < limit) {
        *out = r2;
        return 1;
    }
    return 0;
}

/* p on the triangle's plane inside it: barycentric, as Ericson's 3.4. */
static int inside(const CTri *tr, const float *p)
{
    float v0[3], v1[3], v2[3];
    float d00, d01, d02, d11, d12, den, u, v;
    int k;

    for (k = 0; k < 3; k++) {
        v0[k] = tr->c[k] - tr->a[k];
        v1[k] = tr->b[k] - tr->a[k];
        v2[k] = p[k] - tr->a[k];
    }
    d00 = dot3(v0, v0);
    d01 = dot3(v0, v1);
    d02 = dot3(v0, v2);
    d11 = dot3(v1, v1);
    d12 = dot3(v1, v2);
    den = d00 * d11 - d01 * d01;
    if (den <= 1.0e-12f) {
        return 0;
    }
    u = (d11 * d02 - d01 * d12) / den;
    v = (d00 * d12 - d01 * d02) / den;
    return u >= 0.0f && v >= 0.0f && u + v <= 1.0f;
}

/*
 * A unit sphere at base moving by vel against one triangle, both sides.
 * Updates c when this contact comes sooner. vv is |vel|^2.
 */
static void sweep_tri(const CTri *tr, const float *base, const float *vel, float vv, Contact *c)
{
    float n[3];
    float dist;
    float ndv;
    float t0;
    float t1;
    float t;
    float point[3];
    int found;
    int embedded;
    int k;

    n[0] = tr->n[0];
    n[1] = tr->n[1];
    n[2] = tr->n[2];
    dist = dot3(n, base) + tr->d;
    ndv = dot3(n, vel);
    /* Both sides count: face the plane toward the sphere. */
    if (dist < 0.0f) {
        n[0] = -n[0];
        n[1] = -n[1];
        n[2] = -n[2];
        dist = -dist;
        ndv = -ndv;
    }
    /* Moving away: nothing to meet, and an embedded body can leave. */
    if (ndv > 0.0f) {
        return;
    }
    embedded = 0;
    if (ndv > -1.0e-7f) {
        if (dist >= 1.0f) {
            return;
        }
        embedded = 1;
        t0 = 0.0f;
        t1 = 1.0f;
    } else {
        t0 = (1.0f - dist) / ndv;
        t1 = (-1.0f - dist) / ndv;
        if (t0 > t1) {
            t = t0;
            t0 = t1;
            t1 = t;
        }
        if (t0 > 1.0f || t1 < 0.0f) {
            return;
        }
        t0 = t0 < 0.0f ? 0.0f : t0;
        t1 = t1 > 1.0f ? 1.0f : t1;
    }
    found = 0;
    t = c->found ? c->t : 1.0f;
    /* The sphere meets the plane inside the triangle: that is the first contact. */
    if (!embedded && t0 < t) {
        float p[3];

        for (k = 0; k < 3; k++) {
            p[k] = base[k] - n[k] + vel[k] * t0;
        }
        if (inside(tr, p)) {
            c->found = 1;
            c->t = t0;
            c->point[0] = p[0];
            c->point[1] = p[1];
            c->point[2] = p[2];
            return;
        }
    }
    /* Otherwise it may meet a corner or an edge first. */
    {
        const float *v[3];
        float nt;

        v[0] = tr->a;
        v[1] = tr->b;
        v[2] = tr->c;
        for (k = 0; k < 3; k++) {
            float d[3];
            float b;
            float cc;

            d[0] = base[0] - v[k][0];
            d[1] = base[1] - v[k][1];
            d[2] = base[2] - v[k][2];
            b = 2.0f * dot3(vel, d);
            cc = dot3(d, d) - 1.0f;
            if (lowest_root(vv, b, cc, t, &nt)) {
                t = nt;
                found = 1;
                point[0] = v[k][0];
                point[1] = v[k][1];
                point[2] = v[k][2];
            }
        }
        for (k = 0; k < 3; k++) {
            const float *p1;
            const float *p2;
            float edge[3];
            float btv[3];
            float es, edv, edb, a, b, cc, f;
            int j;

            p1 = v[k];
            p2 = v[(k + 1) % 3];
            for (j = 0; j < 3; j++) {
                edge[j] = p2[j] - p1[j];
                btv[j] = p1[j] - base[j];
            }
            es = dot3(edge, edge);
            edv = dot3(edge, vel);
            edb = dot3(edge, btv);
            a = es * -vv + edv * edv;
            b = es * (2.0f * dot3(vel, btv)) - 2.0f * edv * edb;
            cc = es * (1.0f - dot3(btv, btv)) + edb * edb;
            if (lowest_root(a, b, cc, t, &nt)) {
                f = (edv * nt - edb) / es;
                if (f >= 0.0f && f <= 1.0f) {
                    t = nt;
                    found = 1;
                    for (j = 0; j < 3; j++) {
                        point[j] = p1[j] + f * edge[j];
                    }
                }
            }
        }
    }
    if (found) {
        c->found = 1;
        c->t = t;
        c->point[0] = point[0];
        c->point[1] = point[1];
        c->point[2] = point[2];
    }
}

float collide_slide(float *pos, float vx, float vy, float vz, CollideHit *hit)
{
    float base[3];
    float vel[3];
    float start[3];
    float want;
    int step;
    int i;

    if (hit) {
        hit->hit = 0;
        hit->floor_y = -2.0f;
        hit->ceiling_y = 2.0f;
    }
    want = root(vx * vx + vy * vy + vz * vz);
    if (want < 1.0e-6f) {
        return 1.0f;
    }
    base[0] = pos[0] * g_irx;
    base[1] = pos[1] * g_iry;
    base[2] = pos[2] * g_irx;
    vel[0] = vx * g_irx;
    vel[1] = vy * g_iry;
    vel[2] = vz * g_irx;
    start[0] = pos[0];
    start[1] = pos[1];
    start[2] = pos[2];
    for (step = 0; step < SLIDE_STEPS; step++) {
        Contact c;
        float vv;
        float len;
        float dest[3];
        float sn[3];
        float sl;
        float back;
        float wx, wy, wz, wl;

        vv = dot3(vel, vel);
        if (vv < 1.0e-12f) {
            break;
        }
        len = root(vv);
        c.found = 0;
        c.t = 1.0f;
        for (i = 0; i < g_nset; i++) {
            sweep_tri(&g_set[i], base, vel, vv, &c);
        }
        if (!c.found) {
            base[0] += vel[0];
            base[1] += vel[1];
            base[2] += vel[2];
            break;
        }
        for (i = 0; i < 3; i++) {
            dest[i] = base[i] + vel[i];
        }
        /* Stop just short of the contact, and keep the contact as far off. */
        back = c.t * len;
        if (back >= VERY_CLOSE) {
            float k;

            k = (back - VERY_CLOSE) / len;
            for (i = 0; i < 3; i++) {
                base[i] += vel[i] * k;
                c.point[i] -= vel[i] / len * VERY_CLOSE;
            }
        }
        /* Slide along the plane through the contact, facing the sphere's centre. */
        for (i = 0; i < 3; i++) {
            sn[i] = base[i] - c.point[i];
        }
        sl = root(dot3(sn, sn));
        if (sl < 1.0e-6f) {
            break;
        }
        for (i = 0; i < 3; i++) {
            sn[i] /= sl;
        }
        if (hit) {
            /* The normal back in the world: scaled by 1 / radius, then unit. */
            wx = sn[0] * g_irx;
            wy = sn[1] * g_iry;
            wz = sn[2] * g_irx;
            wl = root(wx * wx + wy * wy + wz * wz);
            wy = wl > 0.0f ? wy / wl : 0.0f;
            hit->hit = 1;
            hit->floor_y = wy > hit->floor_y ? wy : hit->floor_y;
            hit->ceiling_y = wy < hit->ceiling_y ? wy : hit->ceiling_y;
        }
        {
            float into;

            into = 0.0f;
            for (i = 0; i < 3; i++) {
                into += sn[i] * (dest[i] - c.point[i]);
            }
            for (i = 0; i < 3; i++) {
                vel[i] = dest[i] - sn[i] * into - c.point[i];
            }
        }
    }
    pos[0] = base[0] * g_rx;
    pos[1] = base[1] * g_ry;
    pos[2] = base[2] * g_rx;
    {
        float dx, dy, dz;

        dx = pos[0] - start[0];
        dy = pos[1] - start[1];
        dz = pos[2] - start[2];
        return root(dx * dx + dy * dy + dz * dz) / want;
    }
}

/* ---- Static queries ------------------------------------------------- */

/* Closest point of a triangle to p: Ericson, Real-Time Collision Detection 5.1.5. */
static void closest(const CTri *tr, const float *p, float *out)
{
    float ab[3], ac[3], ap[3], bp[3], cp[3];
    float d1, d2, d3, d4, d5, d6, va, vb, vc, v, w, den;
    int k;

    for (k = 0; k < 3; k++) {
        ab[k] = tr->b[k] - tr->a[k];
        ac[k] = tr->c[k] - tr->a[k];
        ap[k] = p[k] - tr->a[k];
        bp[k] = p[k] - tr->b[k];
        cp[k] = p[k] - tr->c[k];
    }
    d1 = dot3(ab, ap);
    d2 = dot3(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        for (k = 0; k < 3; k++) {
            out[k] = tr->a[k];
        }
        return;
    }
    d3 = dot3(ab, bp);
    d4 = dot3(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        for (k = 0; k < 3; k++) {
            out[k] = tr->b[k];
        }
        return;
    }
    vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        v = d1 / (d1 - d3);
        for (k = 0; k < 3; k++) {
            out[k] = tr->a[k] + v * ab[k];
        }
        return;
    }
    d5 = dot3(ab, cp);
    d6 = dot3(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        for (k = 0; k < 3; k++) {
            out[k] = tr->c[k];
        }
        return;
    }
    vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        w = d2 / (d2 - d6);
        for (k = 0; k < 3; k++) {
            out[k] = tr->a[k] + w * ac[k];
        }
        return;
    }
    va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f) {
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        for (k = 0; k < 3; k++) {
            out[k] = tr->b[k] + w * (tr->c[k] - tr->b[k]);
        }
        return;
    }
    den = 1.0f / (va + vb + vc);
    v = vb * den;
    w = vc * den;
    for (k = 0; k < 3; k++) {
        out[k] = tr->a[k] + ab[k] * v + ac[k] * w;
    }
}

int collide_overlap(const float *pos)
{
    float p[3];
    int i;

    p[0] = pos[0] * g_irx;
    p[1] = pos[1] * g_iry;
    p[2] = pos[2] * g_irx;
    for (i = 0; i < g_nset; i++) {
        float q[3];
        float d[3];

        closest(&g_set[i], p, q);
        d[0] = p[0] - q[0];
        d[1] = p[1] - q[1];
        d[2] = p[2] - q[2];
        /* A body resting a sweep's gap away is not inside. */
        if (dot3(d, d) < 0.96f) {
            return 1;
        }
    }
    return 0;
}

int collide_floor(float x, float z, float y_top, float y_bottom, float *y)
{
    float px;
    float pz;
    float best;
    int found;
    int i;

    px = x * g_irx;
    pz = z * g_irx;
    best = y_bottom;
    found = 0;
    for (i = 0; i < g_nset; i++) {
        const CTri *tr;
        float d0, d1, d2, h, wy;

        tr = &g_set[i];
        /*
         * Flat enough to stand on: its normal, scaled back to the world,
         * rises at least COLLIDE_FLOOR. Either way up: the triangles are
         * two-sided, and ARCH3D winds a floor's normal down, away from the
         * side it is seen from.
         */
        wy = tr->n[1] * g_iry;
        if (df_absf(tr->n[1]) < 1.0e-6f || wy * wy < COLLIDE_FLOOR * COLLIDE_FLOOR
                                * (tr->n[0] * tr->n[0] * g_irx * g_irx + wy * wy
                                   + tr->n[2] * tr->n[2] * g_irx * g_irx)) {
            continue;
        }
        /* x, z inside the triangle seen from above: the same side of all three edges. */
        d0 = (tr->b[0] - tr->a[0]) * (pz - tr->a[2]) - (tr->b[2] - tr->a[2]) * (px - tr->a[0]);
        d1 = (tr->c[0] - tr->b[0]) * (pz - tr->b[2]) - (tr->c[2] - tr->b[2]) * (px - tr->b[0]);
        d2 = (tr->a[0] - tr->c[0]) * (pz - tr->c[2]) - (tr->a[2] - tr->c[2]) * (px - tr->c[0]);
        if (!((d0 >= 0.0f && d1 >= 0.0f && d2 >= 0.0f) || (d0 <= 0.0f && d1 <= 0.0f && d2 <= 0.0f))) {
            continue;
        }
        h = -(tr->d + tr->n[0] * px + tr->n[2] * pz) / tr->n[1] * g_ry;
        if (h <= y_top && h >= best) {
            best = h;
            found = 1;
        }
    }
    if (found) {
        *y = best;
    }
    return found;
}
