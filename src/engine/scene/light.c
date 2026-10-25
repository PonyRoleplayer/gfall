#include "engine/scene/light.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/*
 * See light.h and docs/lighting.md. Everything here runs at load or
 * when a switch changes, never per frame, so C float-to-int casts are
 * allowed; draw.c does the per-frame part.
 */

/* Torch colour the falloff pictures carry: warm, red strongest. */
#define LIGHT_R 1.00f
#define LIGHT_G 0.80f
#define LIGHT_B 0.55f
/*
 * The falloff times this, clipped at 1: a brighter core with the same
 * reach. At 1 the pool was too faint against the ambient in a preview.
 */
#define LIGHT_GAIN 1.5f
/* Corners a decal may have: a face of 32 cut by 4 sides, with room. */
#define DECAL_MAX 40
/* Corners a face may have here; a larger face is cut triangle by triangle. */
#define FACE_CORNERS 32

static int g_on = 1;
static int g_ambient = 112;
static int g_range = 3;
static int g_view;

static const Scene *g_scene;
static int g_nlights;
static int *g_first;
static int *g_count;
static float *g_box;
static Decal *g_decal;
static int g_ndecal;
static int g_decal_cap;
static float *g_corner;
static int g_ncorner;
static int g_corner_cap;
/* Per sprite: the lights' sum on it, per channel, 0..255 meaning 0..1. */
static df_u8 *g_sprite_light;
static int g_nsprites;
static int g_sprite_cap;
static int g_light_cap;

static float root(float x)
{
    if (x <= 0.0f) {
        return 0.0f;
    }
    __asm__ __volatile__("fsqrt" : "=t"(x) : "0"(x));
    return x;
}

void light_set_on(int on)
{
    g_on = on ? 1 : 0;
}

int light_on(void)
{
    return g_on;
}

void light_set_ambient(int level)
{
    if (level < 0) {
        level = 0;
    }
    if (level > 255) {
        level = 255;
    }
    g_ambient = level;
}

int light_ambient(void)
{
    return g_ambient;
}

void light_set_range(int scale)
{
    if (scale < 1) {
        scale = 1;
    }
    if (scale > 8) {
        scale = 8;
    }
    g_range = scale;
}

int light_range(void)
{
    return g_range;
}

void light_set_view(int units)
{
    if (units < 0) {
        units = 0;
    }
    if (units > LIGHT_VIEW_MAX) {
        units = LIGHT_VIEW_MAX;
    }
    g_view = units;
}

int light_view(void)
{
    return g_view;
}

int light_count(void)
{
    return g_nlights;
}

int light_active(void)
{
    return g_on && g_nlights > 0;
}

const Light *light_at(int i)
{
    if (!g_scene || i < 0 || i >= g_nlights) {
        return 0;
    }
    return &g_scene->lights[i];
}

float light_reach(int i)
{
    const Light *li;

    li = light_at(i);
    return li ? li->radius * (float)g_range : 0.0f;
}

int light_box(int i, float *cx, float *cy, float *cz, float *ex, float *ey, float *ez)
{
    const float *b;

    if (i < 0 || i >= g_nlights || !g_count || g_count[i] < 1) {
        return 0;
    }
    b = &g_box[i * 6];
    *cx = (b[0] + b[3]) * 0.5f;
    *cy = (b[1] + b[4]) * 0.5f;
    *cz = (b[2] + b[5]) * 0.5f;
    *ex = (b[3] - b[0]) * 0.5f;
    *ey = (b[4] - b[1]) * 0.5f;
    *ez = (b[5] - b[2]) * 0.5f;
    return 1;
}

int light_decals(int i, const Decal **out)
{
    if (i < 0 || i >= g_nlights || !g_count) {
        *out = 0;
        return 0;
    }
    *out = &g_decal[g_first[i]];
    return g_count[i];
}

const float *light_corners(void)
{
    return g_corner;
}

int light_decal_total(void)
{
    return g_ndecal;
}

/*
 * How much light a point gets, 0..1. dr is distance over reach; cosine
 * is the angle to the face's normal, 1 for a sprite. A quadratic
 * falloff to zero at the reach, and a Lambert term softened so a torch
 * on its own wall still lights the wall around it, times LIGHT_GAIN.
 */
static float falloff(float dr, float cosine)
{
    float a;

    if (dr >= 1.0f) {
        return 0.0f;
    }
    a = 1.0f - dr;
    a = LIGHT_GAIN * a * a * (0.35f + 0.65f * cosine);
    return a > 1.0f ? 1.0f : a;
}

static df_u8 byte01(float v)
{
    if (v <= 0.0f) {
        return 0;
    }
    if (v >= 1.0f) {
        return 255;
    }
    return (df_u8)(v * 255.0f + 0.5f);
}

/*
 * Variant v is a light at height (v + 0.5) / LIGHT_VARIANTS of its reach
 * above the face. The picture spans the circle the light reaches on the
 * face, radius R' = R sqrt(1 - h^2); a texel at rho of that radius is at
 * distance R sqrt((1 - h^2) rho^2 + h^2). Its colour is the light added
 * to the wall, so the edge, and everything past the circle, is 0: the
 * blend dst + dst * src leaves the wall exactly as it was there.
 */
void light_texture(int variant, df_u16 *out)
{
    float hb;
    int x;
    int y;

    if (variant < 0) {
        variant = 0;
    }
    if (variant >= LIGHT_VARIANTS) {
        variant = LIGHT_VARIANTS - 1;
    }
    hb = ((float)variant + 0.5f) / (float)LIGHT_VARIANTS;
    for (y = 0; y < LIGHT_TEX; y++) {
        for (x = 0; x < LIGHT_TEX; x++) {
            float px;
            float py;
            float rho2;
            float dr;
            float l;
            int r5;
            int g6;
            int b5;

            px = ((float)x + 0.5f) / (float)LIGHT_TEX * 2.0f - 1.0f;
            py = ((float)y + 0.5f) / (float)LIGHT_TEX * 2.0f - 1.0f;
            rho2 = px * px + py * py;
            l = 0.0f;
            if (rho2 < 1.0f) {
                dr = root((1.0f - hb * hb) * rho2 + hb * hb);
                l = falloff(dr, dr > 0.0f ? hb / dr : 1.0f);
            }
            r5 = (int)(l * LIGHT_R * 31.0f + 0.5f);
            g6 = (int)(l * LIGHT_G * 63.0f + 0.5f);
            b5 = (int)(l * LIGHT_B * 31.0f + 0.5f);
            out[y * LIGHT_TEX + x] = (df_u16)((r5 << 11) | (g6 << 5) | b5);
        }
    }
}

typedef struct Cut {
    float x, y, z;
    float u, v;
} Cut;

/* The part of in[] with sign * (axis - limit) <= 0, lerped from the inside. */
static int cut_side(const Cut *in, int n, Cut *out, int axis, float limit, float sign)
{
    int i;
    int m;

    m = 0;
    for (i = 0; i < n && m < DECAL_MAX - 1; i++) {
        const Cut *a;
        const Cut *b;
        float da;
        float db;

        a = &in[i];
        b = &in[(i + 1) % n];
        da = sign * ((axis ? a->v : a->u) - limit);
        db = sign * ((axis ? b->v : b->u) - limit);
        if (da <= 0.0f) {
            out[m++] = *a;
        }
        if ((da <= 0.0f) != (db <= 0.0f)) {
            const Cut *p;
            const Cut *q;
            float dp;
            float dq;
            float t;

            /* From the inside end, as draw.c cuts. */
            if (da <= 0.0f) {
                p = a;
                q = b;
                dp = da;
                dq = db;
            } else {
                p = b;
                q = a;
                dp = db;
                dq = da;
            }
            t = dp / (dp - dq);
            out[m].x = p->x + (q->x - p->x) * t;
            out[m].y = p->y + (q->y - p->y) * t;
            out[m].z = p->z + (q->z - p->z) * t;
            out[m].u = p->u + (q->u - p->u) * t;
            out[m].v = p->v + (q->v - p->v) * t;
            m++;
        }
    }
    return m;
}

/*
 * One face, n world corners, lit by light li reaching R. vis is the
 * face's world normal toward the side it is drawn from; two_sided
 * faces (a single triangle) are lit from either side. Writes the decal
 * when fill is set, and counts it either way.
 */
static void cut_face(const float (*p)[3], int n, const float *vis, int two_sided,
                     const Light *li, float reach, int fill, int light)
{
    float nx, ny, nz, len, h, rr;
    float cx, cy, cz;
    float ux, uy, uz, vx, vy, vz;
    Cut a[DECAL_MAX];
    Cut b[DECAL_MAX];
    int m;
    int k;

    nx = vis[0];
    ny = vis[1];
    nz = vis[2];
    len = root(nx * nx + ny * ny + nz * nz);
    if (len < 1.0e-6f || n < 3 || n > DECAL_MAX - 8) {
        return;
    }
    nx /= len;
    ny /= len;
    nz /= len;
    h = nx * (li->x - p[0][0]) + ny * (li->y - p[0][1]) + nz * (li->z - p[0][2]);
    if (h < 0.0f) {
        if (!two_sided) {
            return;
        }
        nx = -nx;
        ny = -ny;
        nz = -nz;
        h = -h;
    }
    if (h >= reach) {
        return;
    }
    rr = root(reach * reach - h * h);
    if (rr < 1.0f) {
        return;
    }
    cx = li->x - h * nx;
    cy = li->y - h * ny;
    cz = li->z - h * nz;
    /* Any two axes across the plane. */
    if (df_absf(nx) < 0.6f) {
        ux = 0.0f;
        uy = nz;
        uz = -ny;
    } else {
        ux = -nz;
        uy = 0.0f;
        uz = nx;
    }
    len = root(ux * ux + uy * uy + uz * uz);
    ux /= len;
    uy /= len;
    uz /= len;
    vx = ny * uz - nz * uy;
    vy = nz * ux - nx * uz;
    vz = nx * uy - ny * ux;
    for (k = 0; k < n; k++) {
        float dx, dy, dz;

        dx = p[k][0] - cx;
        dy = p[k][1] - cy;
        dz = p[k][2] - cz;
        a[k].x = p[k][0];
        a[k].y = p[k][1];
        a[k].z = p[k][2];
        a[k].u = dx * ux + dy * uy + dz * uz;
        a[k].v = dx * vx + dy * vy + dz * vz;
    }
    /* The square the circle fits in. */
    m = cut_side(a, n, b, 0, rr, 1.0f);
    m = m >= 3 ? cut_side(b, m, a, 0, -rr, -1.0f) : 0;
    m = m >= 3 ? cut_side(a, m, b, 1, rr, 1.0f) : 0;
    m = m >= 3 ? cut_side(b, m, a, 1, -rr, -1.0f) : 0;
    if (m < 3) {
        return;
    }
    /* A patch in the square's corners, wholly outside the circle, adds nothing. */
    {
        float best;
        int inside;

        best = 1.0e30f;
        inside = 1;
        for (k = 0; k < m; k++) {
            const Cut *e0;
            const Cut *e1;
            float ex, ey, t, qx, qy, d2, cross;

            e0 = &a[k];
            e1 = &a[(k + 1) % m];
            ex = e1->u - e0->u;
            ey = e1->v - e0->v;
            /* Nearest point of this edge to the centre. */
            t = ex * ex + ey * ey;
            t = t > 0.0f ? -(e0->u * ex + e0->v * ey) / t : 0.0f;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            qx = e0->u + ex * t;
            qy = e0->v + ey * t;
            d2 = qx * qx + qy * qy;
            best = d2 < best ? d2 : best;
            cross = ex * (-e0->v) - ey * (-e0->u);
            if (cross < 0.0f) {
                inside = 0;
            }
        }
        /* The centre inside a polygon wound either way: all crosses one sign. */
        if (!inside) {
            inside = 1;
            for (k = 0; k < m; k++) {
                const Cut *e0;
                const Cut *e1;

                e0 = &a[k];
                e1 = &a[(k + 1) % m];
                if ((e1->u - e0->u) * (-e0->v) - (e1->v - e0->v) * (-e0->u) > 0.0f) {
                    inside = 0;
                    break;
                }
            }
        }
        if (!inside && best >= rr * rr) {
            return;
        }
    }
    if (fill) {
        Decal *d;
        float *bx;

        if (g_ndecal >= g_decal_cap || g_ncorner + m > g_corner_cap) {
            return;
        }
        d = &g_decal[g_ndecal];
        d->first = g_ncorner;
        d->n = m;
        d->variant = (int)(h / reach * (float)LIGHT_VARIANTS);
        if (d->variant >= LIGHT_VARIANTS) {
            d->variant = LIGHT_VARIANTS - 1;
        }
        d->nx = nx;
        d->ny = ny;
        d->nz = nz;
        d->d = -(nx * p[0][0] + ny * p[0][1] + nz * p[0][2]);
        {
            float lo[3];
            float hi[3];
            int q;

            lo[0] = hi[0] = a[0].x;
            lo[1] = hi[1] = a[0].y;
            lo[2] = hi[2] = a[0].z;
            for (q = 1; q < m; q++) {
                lo[0] = a[q].x < lo[0] ? a[q].x : lo[0];
                lo[1] = a[q].y < lo[1] ? a[q].y : lo[1];
                lo[2] = a[q].z < lo[2] ? a[q].z : lo[2];
                hi[0] = a[q].x > hi[0] ? a[q].x : hi[0];
                hi[1] = a[q].y > hi[1] ? a[q].y : hi[1];
                hi[2] = a[q].z > hi[2] ? a[q].z : hi[2];
            }
            d->cx = (lo[0] + hi[0]) * 0.5f;
            d->cy = (lo[1] + hi[1]) * 0.5f;
            d->cz = (lo[2] + hi[2]) * 0.5f;
            d->ex = (hi[0] - lo[0]) * 0.5f;
            d->ey = (hi[1] - lo[1]) * 0.5f;
            d->ez = (hi[2] - lo[2]) * 0.5f;
        }
        bx = &g_box[light * 6];
        for (k = 0; k < m; k++) {
            float *c;

            c = &g_corner[g_ncorner * LIGHT_CORNER];
            c[0] = a[k].x;
            c[1] = a[k].y;
            c[2] = a[k].z;
            c[3] = a[k].u / (2.0f * rr) + 0.5f;
            c[4] = a[k].v / (2.0f * rr) + 0.5f;
            if (g_count[light] == 0 && k == 0) {
                bx[0] = bx[3] = c[0];
                bx[1] = bx[4] = c[1];
                bx[2] = bx[5] = c[2];
            }
            bx[0] = c[0] < bx[0] ? c[0] : bx[0];
            bx[1] = c[1] < bx[1] ? c[1] : bx[1];
            bx[2] = c[2] < bx[2] ? c[2] : bx[2];
            bx[3] = c[0] > bx[3] ? c[0] : bx[3];
            bx[4] = c[1] > bx[4] ? c[1] : bx[4];
            bx[5] = c[2] > bx[5] ? c[2] : bx[5];
            g_ncorner++;
        }
        g_ndecal++;
        g_count[light]++;
    } else {
        g_ndecal++;
        g_ncorner += m;
    }
}

/*
 * The face as one polygon when its triangles are an exact fan, as the
 * ARCH3D loader makes them; otherwise each triangle on its own.
 */
static void light_mesh(const Instance *in, const Light *li, float reach, int fill, int light)
{
    const Mesh *mesh;
    int f;

    mesh = in->mesh;
    for (f = 0; f < mesh->nfaces; f++) {
        const Face *face;
        const Tri *t0;
        float vis[3];
        float p[FACE_CORNERS][3];
        int fan;
        int t;
        int n;

        face = &mesh->faces[f];
        if (face->ntris < 1 || face->first < 0 || face->first + face->ntris > mesh->ntris) {
            continue;
        }
        t0 = &mesh->tris[face->first];
        if (t0->slot == 0xffff) {
            continue;
        }
        vis[0] = in->m[0] * face->nx + in->m[1] * face->ny + in->m[2] * face->nz;
        vis[1] = in->m[3] * face->nx + in->m[4] * face->ny + in->m[5] * face->nz;
        vis[2] = in->m[6] * face->nx + in->m[7] * face->ny + in->m[8] * face->nz;
        fan = face->ntris + 2 <= FACE_CORNERS;
        for (t = face->first + 1; fan && t < face->first + face->ntris; t++) {
            const Tri *a;
            const Tri *b;

            a = &mesh->tris[t - 1];
            b = &mesh->tris[t];
            fan = b->x[0] == t0->x[0] && b->y[0] == t0->y[0] && b->z[0] == t0->z[0]
                && b->x[1] == a->x[2] && b->y[1] == a->y[2] && b->z[1] == a->z[2];
        }
        if (fan) {
            instance_point(in, t0->x[0], t0->y[0], t0->z[0], &p[0][0], &p[0][1], &p[0][2]);
            instance_point(in, t0->x[1], t0->y[1], t0->z[1], &p[1][0], &p[1][1], &p[1][2]);
            n = 2;
            for (t = face->first; t < face->first + face->ntris; t++) {
                instance_point(in, mesh->tris[t].x[2], mesh->tris[t].y[2], mesh->tris[t].z[2],
                               &p[n][0], &p[n][1], &p[n][2]);
                n++;
            }
            cut_face((const float (*)[3])p, n, vis, face->ntris == 1, li, reach, fill, light);
            continue;
        }
        for (t = face->first; t < face->first + face->ntris; t++) {
            int k;

            for (k = 0; k < 3; k++) {
                instance_point(in, mesh->tris[t].x[k], mesh->tris[t].y[k], mesh->tris[t].z[k],
                               &p[k][0], &p[k][1], &p[k][2]);
            }
            cut_face((const float (*)[3])p, 3, vis, face->ntris == 1, li, reach, fill, light);
        }
    }
}

static void light_all(const Scene *scene, int fill)
{
    int i;
    int m;

    g_ndecal = 0;
    g_ncorner = 0;
    for (i = 0; i < scene->nlights; i++) {
        const Light *li;
        float reach;

        li = &scene->lights[i];
        reach = li->radius * (float)g_range;
        if (fill) {
            g_first[i] = g_ndecal;
            g_count[i] = 0;
        }
        if (reach < 1.0f) {
            continue;
        }
        for (m = 0; m < scene->nmodels; m++) {
            const Instance *in;
            const Bound *bd;

            in = &scene->models[m];
            if (!in->mesh || !in->mesh->faces) {
                continue;
            }
            if (scene->bounds) {
                bd = &scene->bounds[m];
                if (df_absf(bd->cx - li->x) > bd->ex + reach
                    || df_absf(bd->cy - li->y) > bd->ey + reach
                    || df_absf(bd->cz - li->z) > bd->ez + reach) {
                    continue;
                }
            }
            light_mesh(in, li, reach, fill, i);
        }
    }
}

/* Every sprite's light, from every light that reaches it. */
static void light_sprites(const Scene *scene)
{
    int s;

    for (s = 0; s < scene->nsprites; s++) {
        const Flat *fl;
        float r;
        float g;
        float b;
        int i;

        fl = &scene->sprites[s];
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
        for (i = 0; i < scene->nlights; i++) {
            const Light *li;
            float reach, dx, dy, dz, d2, l;

            li = &scene->lights[i];
            reach = li->radius * (float)g_range;
            dx = fl->x - li->x;
            dy = fl->y - li->y;
            dz = fl->z - li->z;
            d2 = dx * dx + dy * dy + dz * dz;
            if (d2 >= reach * reach) {
                continue;
            }
            l = falloff(root(d2) / reach, 1.0f);
            r += l * LIGHT_R;
            g += l * LIGHT_G;
            b += l * LIGHT_B;
        }
        g_sprite_light[s * 3] = byte01(r);
        g_sprite_light[s * 3 + 1] = byte01(g);
        g_sprite_light[s * 3 + 2] = byte01(b);
    }
}

int light_build(const Scene *scene)
{
    int nd;
    int nc;

    g_scene = scene;
    g_nlights = 0;
    g_ndecal = 0;
    g_ncorner = 0;
    g_nsprites = 0;
    if (!scene || scene->nlights < 1 || !scene->lights) {
        return 0;
    }
    /* Count, then allocate what this place needs; a rebuild that fits reuses it. */
    light_all(scene, 0);
    nd = g_ndecal;
    nc = g_ncorner;
    if (nd > g_decal_cap || scene->nlights > g_light_cap || nc > g_corner_cap) {
        g_decal = (Decal *)mem_alloc((df_u32)(nd > 0 ? nd : 1) * (df_u32)sizeof(Decal));
        g_corner = (float *)mem_alloc((df_u32)(nc > 0 ? nc : 1) * LIGHT_CORNER * (df_u32)sizeof(float));
        g_first = (int *)mem_alloc((df_u32)scene->nlights * (df_u32)sizeof(int));
        g_count = (int *)mem_alloc((df_u32)scene->nlights * (df_u32)sizeof(int));
        g_box = (float *)mem_alloc((df_u32)scene->nlights * 6u * (df_u32)sizeof(float));
        if (!g_decal || !g_corner || !g_first || !g_count || !g_box) {
            g_decal_cap = 0;
            g_corner_cap = 0;
            g_light_cap = 0;
            log_text(LOG_ERROR, "lights: out of memory, the place is unlit");
            return -1;
        }
        g_decal_cap = nd;
        g_corner_cap = nc;
        g_light_cap = scene->nlights;
    }
    if (scene->nsprites > g_sprite_cap) {
        g_sprite_light = (df_u8 *)mem_alloc((df_u32)scene->nsprites * 3u);
        if (!g_sprite_light) {
            g_sprite_cap = 0;
            log_text(LOG_ERROR, "lights: out of memory, the place is unlit");
            return -1;
        }
        g_sprite_cap = scene->nsprites;
    }
    g_nlights = scene->nlights;
    g_nsprites = scene->nsprites;
    light_all(scene, 1);
    light_sprites(scene);
    log_print(LOG_INFO, "lights: %d, %d decals of %d corners, %u KB",
              g_nlights, g_ndecal, g_ncorner,
              (unsigned int)(((df_u32)g_ndecal * sizeof(Decal)
                              + (df_u32)g_ncorner * LIGHT_CORNER * sizeof(float)) / 1024u));
    return g_ndecal;
}

static df_u32 rgb(int r, int g, int b)
{
    r = r > 255 ? 255 : r;
    g = g > 255 ? 255 : g;
    b = b > 255 ? 255 : b;
    return ((df_u32)r << 16) | ((df_u32)g << 8) | (df_u32)b;
}

df_u32 light_ambient_rgb(void)
{
    if (!light_active()) {
        return 0xffffffu;
    }
    return rgb(g_ambient, g_ambient, g_ambient);
}

df_u32 light_sprite_rgb(int s)
{
    const df_u8 *l;

    if (!light_active() || s < 0 || s >= g_nsprites) {
        return 0xffffffu;
    }
    if (g_scene->sprites[s].glow) {
        return 0xffffffu;
    }
    /* The same sum the decals give a wall: ambient times (1 + light). */
    l = &g_sprite_light[s * 3];
    return rgb(g_ambient * (255 + l[0]) / 255,
               g_ambient * (255 + l[1]) / 255,
               g_ambient * (255 + l[2]) / 255);
}
