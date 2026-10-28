#include "engine/scene/light.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/core/prof.h"
#include "engine/scene/stream.h"

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
/* Outdoors, how lit the lamps are: 0..255, from the hour. */
static int g_lamps = 255;

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
/*
 * Per model, 1 + the mover that moves it, 0 for none: a moving model
 * gets no decals, which are cut where it stood, but one colour, from
 * the lights round its box (g_mover_light, three bytes a mover).
 */
static df_u16 *g_moves;
static int g_moves_cap;
static df_u8 *g_mover_light;
static int g_mover_cap;
static int g_nmodels;
static int g_nmovers;
/*
 * The models within reach of the light being cut, which are all that
 * can stand between it and a face it reaches (seen).
 */
static int *g_near;
static int g_nnear;
static int g_near_cap;
static int g_shadow = 1;
/*
 * The heap epoch the buffers above were allocated in. They are reused
 * by a rebuild of the same place (lightrange) only: a new place in
 * LEVELFALL or RENDERFALL is a new temp heap, and the old one, buffers
 * and all, is gone (mem_epoch).
 */
static df_u32 g_epoch;
/* Copied, so a question between places never reads the last one's Scene. */
static int g_outdoor;

/*
 * A streamed scene (light_stream_begin). Each light's decals are its
 * own, in a heap per slot of the stream, freed with the chunk; g_decal
 * and g_corner are then the scratch a light is cut into. Lights to cut
 * and slots whose sprites to light wait in a queue for the service.
 */
#define SCRATCH_DECALS 512
#define SCRATCH_CORNERS 4096
static int g_stream;
static Decal **g_ldecal;
static float **g_lcorner;
static df_u8 *g_pending;
static int *g_queue;
static int g_qhead;
static int g_qcount;
static int g_per_slot;
/* The stream's slots (stream_slots): 121 for the world, 25 for a dungeon at radius 1. */
static int g_nslots;
static MemHeap *g_slot_heap[STREAM_SLOTS];
/* A slot's chunk and its box (models and sprites), kept for when it goes. */
static int g_slot_live[STREAM_SLOTS];
static float g_slot_box[STREAM_SLOTS][6];
static df_u8 g_slot_sprites[STREAM_SLOTS];
static int g_sprite_slots;
/* The longest light or slot of sprites the service did since light_stream_worst asked. */
static df_u32 g_unit_worst;
/*
 * The light being cut now, -1 for none: indoors its shadow rays make one
 * light tens of ms on the 233, so its cut goes a face at a time across
 * the service's calls, the model and face it is at kept here.
 */
static int g_cut = -1;
static int g_cut_model;
static int g_cut_face;

static float root(float x)
{
    if (x <= 0.0f) {
        return 0.0f;
    }
    __asm__ __volatile__("fsqrt" : "=t"(x) : "0"(x));
    return x;
}

void light_set_shadow(int on)
{
    g_shadow = on ? 1 : 0;
}

int light_shadow(void)
{
    return g_shadow;
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
    /* A range too small to hold anything is "on": `lightview 1`. */
    if (units > 0 && units < LIGHT_VIEW_SMALL) {
        units = LIGHT_VIEW_ON;
    }
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

/* An integer hash to 0..255: Thomas Wang's, cheap and well mixed. */
static int flicker_noise(df_u32 n)
{
    n = (n ^ 61u) ^ (n >> 16);
    n += n << 3;
    n ^= n >> 4;
    n *= 0x27d4eb2du;
    n ^= n >> 15;
    return (int)(n & 255u);
}

/*
 * Value noise: a random level at each tick of LIGHT_FLICKER_RATE, and a
 * smooth step between ticks, so a flame dims and brightens rather than
 * jumping. Each light hashes its own index into the ticks. Once a frame
 * for each lit flame: two hashes and a blend, no float-to-int cast.
 */
int light_flicker(int i, float seconds)
{
    const Light *li;
    float t;
    float f;
    int tick;
    int a;
    int b;
    int level;

    li = light_at(i);
    if (!li || !li->flicker) {
        return 255;
    }
    t = seconds * LIGHT_FLICKER_RATE;
    tick = df_ifloor(t);
    f = t - (float)tick;
    f = f * f * (3.0f - 2.0f * f);
    a = flicker_noise((df_u32)tick * 2654435761u + (df_u32)i * 40503u);
    b = flicker_noise((df_u32)(tick + 1) * 2654435761u + (df_u32)i * 40503u);
    level = df_iround((float)a + (float)(b - a) * f);
    return LIGHT_FLICKER_LOW + level * (255 - LIGHT_FLICKER_LOW) / 255;
}

int light_active(void)
{
    return g_on && g_nlights > 0 && (!g_outdoor || g_lamps > 0);
}

int light_outdoor(void)
{
    return g_outdoor;
}

void light_set_lamps(int level)
{
    if (level < 0) {
        level = 0;
    }
    if (level > 255) {
        level = 255;
    }
    g_lamps = level;
}

int light_lamps(void)
{
    return g_lamps;
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
    *out = g_stream ? g_ldecal[i] : &g_decal[g_first[i]];
    return g_count[i];
}

const float *light_corners(void)
{
    return g_stream ? 0 : g_corner;
}

const float *light_corners_of(int i)
{
    if (!g_stream) {
        return g_corner;
    }
    return i >= 0 && i < g_nlights ? g_lcorner[i] : 0;
}

static int g_stream_decals;

int light_decal_total(void)
{
    return g_stream ? g_stream_decals : g_ndecal;
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
/*
 * Where a light meets a face's plane: the plane's unit normal toward
 * the light, the light's height h over it, the radius rr of the circle
 * it reaches there, the circle's centre, and two axes across the plane
 * (U, V) the square around the circle is cut along.
 */
typedef struct Frame {
    float nx, ny, nz;
    float h, rr;
    float cx, cy, cz;
    float ux, uy, uz, vx, vy, vz;
} Frame;

/* 0 when the light is behind the plane (and it is one-sided) or out of reach. */
static int light_frame(const float *vis, const float *p0, int two_sided,
                       const Light *li, float reach, Frame *f)
{
    float len;

    f->nx = vis[0];
    f->ny = vis[1];
    f->nz = vis[2];
    len = root(f->nx * f->nx + f->ny * f->ny + f->nz * f->nz);
    if (len < 1.0e-6f) {
        return 0;
    }
    f->nx /= len;
    f->ny /= len;
    f->nz /= len;
    f->h = f->nx * (li->x - p0[0]) + f->ny * (li->y - p0[1]) + f->nz * (li->z - p0[2]);
    if (f->h < 0.0f) {
        if (!two_sided) {
            return 0;
        }
        f->nx = -f->nx;
        f->ny = -f->ny;
        f->nz = -f->nz;
        f->h = -f->h;
    }
    if (f->h >= reach) {
        return 0;
    }
    f->rr = root(reach * reach - f->h * f->h);
    if (f->rr < 1.0f) {
        return 0;
    }
    f->cx = li->x - f->h * f->nx;
    f->cy = li->y - f->h * f->ny;
    f->cz = li->z - f->h * f->nz;
    /* Any two axes across the plane. A floor gets world x and z. */
    if (df_absf(f->nx) < 0.6f) {
        f->ux = 0.0f;
        f->uy = f->nz;
        f->uz = -f->ny;
    } else {
        f->ux = -f->nz;
        f->uy = 0.0f;
        f->uz = f->nx;
    }
    len = root(f->ux * f->ux + f->uy * f->uy + f->uz * f->uz);
    f->ux /= len;
    f->uy /= len;
    f->uz /= len;
    f->vx = f->ny * f->uz - f->nz * f->uy;
    f->vy = f->nz * f->ux - f->nx * f->uz;
    f->vz = f->nx * f->uy - f->ny * f->ux;
    return 1;
}

/* World corners into the frame's U, V. */
static void to_frame(const Frame *f, const float (*p)[3], int n, Cut *a)
{
    int k;

    for (k = 0; k < n; k++) {
        float dx, dy, dz;

        dx = p[k][0] - f->cx;
        dy = p[k][1] - f->cy;
        dz = p[k][2] - f->cz;
        a[k].x = p[k][0];
        a[k].y = p[k][1];
        a[k].z = p[k][2];
        a[k].u = dx * f->ux + dy * f->uy + dz * f->uz;
        a[k].v = dx * f->vx + dy * f->vy + dz * f->vz;
    }
}

/* The part of a[0..n) inside the square round the circle, back in a. */
static int to_square(Cut *a, int n, float rr)
{
    Cut b[DECAL_MAX];
    int m;

    m = cut_side(a, n, b, 0, rr, 1.0f);
    m = m >= 3 ? cut_side(b, m, a, 0, -rr, -1.0f) : 0;
    m = m >= 3 ? cut_side(a, m, b, 1, rr, 1.0f) : 0;
    m = m >= 3 ? cut_side(b, m, a, 1, -rr, -1.0f) : 0;
    return m >= 3 ? m : 0;
}

/* Where the segment o + t d, t in 0..len, enters a world box; 0 when it misses. */
static int seg_box(const float *o, const float *d, float len, const Bound *b)
{
    float c[3], e[3];
    float t0, t1;
    int k;

    c[0] = b->cx;
    c[1] = b->cy;
    c[2] = b->cz;
    e[0] = b->ex;
    e[1] = b->ey;
    e[2] = b->ez;
    t0 = 0.0f;
    t1 = len;
    for (k = 0; k < 3; k++) {
        float a, z;

        if (d[k] > -1.0e-6f && d[k] < 1.0e-6f) {
            if (o[k] < c[k] - e[k] || o[k] > c[k] + e[k]) {
                return 0;
            }
            continue;
        }
        a = (c[k] - e[k] - o[k]) / d[k];
        z = (c[k] + e[k] - o[k]) / d[k];
        if (a > z) {
            float w;

            w = a;
            a = z;
            z = w;
        }
        t0 = a > t0 ? a : t0;
        t1 = z < t1 ? z : t1;
        if (t0 > t1) {
            return 0;
        }
    }
    return 1;
}

/* 1 when nothing of the models near the light lies on the way from it to x, y, z. */
static int light_reaches(const Light *li, float x, float y, float z)
{
    float o[3];
    float d[3];
    float len;
    float t;
    int i;

    o[0] = li->x;
    o[1] = li->y;
    o[2] = li->z;
    d[0] = x - o[0];
    d[1] = y - o[1];
    d[2] = z - o[2];
    len = root(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    /* Stop short of the face itself. */
    if (len < 2.0f) {
        return 1;
    }
    d[0] /= len;
    d[1] /= len;
    d[2] /= len;
    len -= 1.0f;
    for (i = 0; i < g_nnear; i++) {
        const Instance *in;
        Bound b;
        int m;

        m = g_near[i];
        in = &g_scene->models[m];
        if (g_scene->bounds) {
            b = g_scene->bounds[m];
        } else if (!instance_world_box(in, &b)) {
            continue;
        }
        if (seg_box(o, d, len, &b) && instance_ray(in, o, d, len, &t)) {
            return 0;
        }
    }
    return 1;
}

/*
 * A decal the light cannot see is not cut: a lamp on the floor above
 * does not light the room below through its floor, nor a torch the
 * next room through a wall. It is seen when the way to its middle, or
 * to any of four of its corners drawn a fifth of the way in, is clear.
 * Moving models do not shade (they are not in g_near).
 */
static int seen(const Light *li, const Cut *a, int m)
{
    float cx, cy, cz;
    int j;
    int k;

    /*
     * A streamed world's lamps are street lamps, and its houses have no
     * insides drawn: the rays were 97% of a lamp's cut (2 ms on the host,
     * a dropped frame or two on the 233) and changed next to nothing. A
     * face turned away from the lamp takes no pool anyway (light_frame).
     * A streamed dungeon (not outdoor) keeps them: a torch must not light
     * the next corridor through its wall. Its cut goes a face a call.
     */
    if (!g_shadow || (g_stream && g_outdoor) || g_nnear < 1) {
        return 1;
    }
    cx = cy = cz = 0.0f;
    for (k = 0; k < m; k++) {
        cx += a[k].x;
        cy += a[k].y;
        cz += a[k].z;
    }
    cx /= (float)m;
    cy /= (float)m;
    cz /= (float)m;
    if (light_reaches(li, cx, cy, cz)) {
        return 1;
    }
    for (j = 0; j < 4; j++) {
        k = j * m / 4;
        if (light_reaches(li, a[k].x + (cx - a[k].x) * 0.2f, a[k].y + (cy - a[k].y) * 0.2f,
                          a[k].z + (cz - a[k].z) * 0.2f)) {
            return 1;
        }
    }
    return 0;
}

static void cut_face(const float (*p)[3], int n, const float *vis, int two_sided,
                     const Light *li, float reach, int fill, int light)
{
    float nx, ny, nz, h, rr;
    Frame f;
    Cut a[DECAL_MAX];
    int m;
    int k;

    if (n < 3 || n > DECAL_MAX - 8 || !light_frame(vis, p[0], two_sided, li, reach, &f)) {
        return;
    }
    nx = f.nx;
    ny = f.ny;
    nz = f.nz;
    h = f.h;
    rr = f.rr;
    to_frame(&f, p, n, a);
    /* The square the circle fits in. */
    m = to_square(a, n, rr);
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
    /*
     * The shadow rays only when filling: the count (light_build's first
     * pass) is then an upper bound, a decal a ray would drop counted too,
     * and the rays, three quarters of the cut, are cast once, not twice.
     */
    if (fill && !seen(li, a, m)) {
        return;
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
 * A flat face of many triangles, as the ground is (a patch of 256-unit
 * tiles, two triangles each), would take a decal per triangle: about 32
 * on open ground under one street lamp, each transformed, clipped and
 * set up on the 233 every pass. When the triangles cover the whole of
 * the square that lies inside the face's outline, that rectangle is one
 * decal instead. A face that is not flat, or has a hole there (a tile
 * left out), returns 0 and is cut triangle by triangle.
 */
#define MERGE_FLAT 0.5f

static int merge_face(const Instance *in, const Mesh *mesh, const Face *face,
                      const float *vis, const Light *li, float reach, int fill, int light)
{
    const Tri *t0;
    float p0[3];
    float rect[4][3];
    float lo_u, hi_u, lo_v, hi_v;
    float area;
    Frame f;
    int t;
    int k;

    t0 = &mesh->tris[face->first];
    instance_point(in, t0->x[0], t0->y[0], t0->z[0], &p0[0], &p0[1], &p0[2]);
    if (!light_frame(vis, p0, 0, li, reach, &f)) {
        /* Before trusting that for every triangle, the face must be flat. */
        f.nx = vis[0];
        f.ny = vis[1];
        f.nz = vis[2];
        area = root(f.nx * f.nx + f.ny * f.ny + f.nz * f.nz);
        if (area < 1.0e-6f) {
            return 0;
        }
        f.nx /= area;
        f.ny /= area;
        f.nz /= area;
        for (t = face->first; t < face->first + face->ntris; t++) {
            for (k = 0; k < 3; k++) {
                float q[3];

                instance_point(in, mesh->tris[t].x[k], mesh->tris[t].y[k], mesh->tris[t].z[k],
                               &q[0], &q[1], &q[2]);
                if (df_absf(f.nx * (q[0] - p0[0]) + f.ny * (q[1] - p0[1])
                            + f.nz * (q[2] - p0[2])) > MERGE_FLAT) {
                    return 0;
                }
            }
        }
        return 1;
    }
    lo_u = lo_v = 1.0e30f;
    hi_u = hi_v = -1.0e30f;
    area = 0.0f;
    for (t = face->first; t < face->first + face->ntris; t++) {
        float q[3][3];
        Cut c[DECAL_MAX];
        int m;

        for (k = 0; k < 3; k++) {
            instance_point(in, mesh->tris[t].x[k], mesh->tris[t].y[k], mesh->tris[t].z[k],
                           &q[k][0], &q[k][1], &q[k][2]);
            if (df_absf(f.nx * (q[k][0] - f.cx) + f.ny * (q[k][1] - f.cy)
                        + f.nz * (q[k][2] - f.cz)) > MERGE_FLAT) {
                return 0;
            }
        }
        to_frame(&f, (const float (*)[3])q, 3, c);
        for (k = 0; k < 3; k++) {
            lo_u = c[k].u < lo_u ? c[k].u : lo_u;
            hi_u = c[k].u > hi_u ? c[k].u : hi_u;
            lo_v = c[k].v < lo_v ? c[k].v : lo_v;
            hi_v = c[k].v > hi_v ? c[k].v : hi_v;
        }
        m = to_square(c, 3, f.rr);
        for (k = 0; k < m; k++) {
            const Cut *e0;
            const Cut *e1;

            e0 = &c[k];
            e1 = &c[(k + 1) % m];
            area += e0->u * e1->v - e1->u * e0->v;
        }
    }
    area = df_absf(area) * 0.5f;
    lo_u = lo_u > -f.rr ? lo_u : -f.rr;
    hi_u = hi_u < f.rr ? hi_u : f.rr;
    lo_v = lo_v > -f.rr ? lo_v : -f.rr;
    hi_v = hi_v < f.rr ? hi_v : f.rr;
    if (hi_u <= lo_u || hi_v <= lo_v) {
        return 1;
    }
    /*
     * Every triangle wound the same way, so the signed sum is the area
     * covered; the rectangle is whole when it matches.
     */
    if (area < (hi_u - lo_u) * (hi_v - lo_v) * 0.999f) {
        return 0;
    }
    for (k = 0; k < 4; k++) {
        float u;
        float v;

        u = (k == 1 || k == 2) ? hi_u : lo_u;
        v = (k >= 2) ? hi_v : lo_v;
        rect[k][0] = f.cx + u * f.ux + v * f.vx;
        rect[k][1] = f.cy + u * f.uy + v * f.vy;
        rect[k][2] = f.cz + u * f.uz + v * f.vz;
    }
    cut_face((const float (*)[3])rect, 4, vis, 0, li, reach, fill, light);
    return 1;
}

/*
 * The face as one polygon when its triangles are an exact fan, as the
 * ARCH3D loader makes them; otherwise each triangle on its own.
 */
/* Faces f0..f1-1 of a model: a stream's cut goes a face at a time. */
static void light_faces(const Instance *in, const Light *li, float reach, int fill, int light, int f0, int f1)
{
    const Mesh *mesh;
    int f;

    mesh = in->mesh;
    for (f = f0; f < f1 && f < mesh->nfaces; f++) {
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
        if (face->ntris > 1 && merge_face(in, mesh, face, vis, li, reach, fill, light)) {
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

static void light_mesh(const Instance *in, const Light *li, float reach, int fill, int light)
{
    light_faces(in, li, reach, fill, light, 0, in->mesh->nfaces);
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
        /* The models it reaches, the still ones: they take its decals and shade it. */
        g_nnear = 0;
        for (m = 0; m < scene->nmodels && g_nnear < g_near_cap; m++) {
            const Instance *in;
            const Bound *bd;

            in = &scene->models[m];
            if (!in->mesh || !in->mesh->faces || (m < g_nmodels && g_moves[m])) {
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
            g_near[g_nnear++] = m;
        }
        for (m = 0; m < g_nnear; m++) {
            light_mesh(&scene->models[g_near[m]], li, reach, fill, i);
        }
    }
}

/* The lights' sum at a point, per channel, 0..255 meaning 0..1. */
static void light_point(const Scene *scene, float x, float y, float z, df_u8 *out)
{
    float r;
    float g;
    float b;
    int i;

    r = 0.0f;
    g = 0.0f;
    b = 0.0f;
    for (i = 0; i < scene->nlights; i++) {
        const Light *li;
        float reach, dx, dy, dz, d2, l;

        li = &scene->lights[i];
        reach = li->radius * (float)g_range;
        dx = x - li->x;
        dy = y - li->y;
        dz = z - li->z;
        d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= reach * reach) {
            continue;
        }
        l = falloff(root(d2) / reach, 1.0f);
        r += l * LIGHT_R;
        g += l * LIGHT_G;
        b += l * LIGHT_B;
    }
    out[0] = byte01(r);
    out[1] = byte01(g);
    out[2] = byte01(b);
}

/* Every sprite's light, from every light that reaches it. */
static void light_sprites(const Scene *scene)
{
    int s;

    for (s = 0; s < scene->nsprites; s++) {
        const Flat *fl;

        fl = &scene->sprites[s];
        light_point(scene, fl->x, fl->y, fl->z, &g_sprite_light[s * 3]);
    }
}

/*
 * Which models move, and each mover's light at its model's box as the
 * place built it. Before the decals, which skip the moving ones.
 */
static int light_movers(const Scene *scene)
{
    int i;

    g_nmodels = 0;
    g_nmovers = 0;
    if (!scene->movers || scene->nmovers < 1) {
        return 1;
    }
    if (scene->nmodels > g_moves_cap) {
        g_moves = (df_u16 *)mem_alloc((df_u32)scene->nmodels * (df_u32)sizeof(df_u16));
        g_moves_cap = g_moves ? scene->nmodels : 0;
    }
    if (scene->nmovers > g_mover_cap) {
        g_mover_light = (df_u8 *)mem_alloc((df_u32)scene->nmovers * 3u);
        g_mover_cap = g_mover_light ? scene->nmovers : 0;
    }
    if (!g_moves || !g_mover_light || g_moves_cap < scene->nmodels
        || g_mover_cap < scene->nmovers) {
        return 0;
    }
    for (i = 0; i < scene->nmodels; i++) {
        g_moves[i] = 0;
    }
    for (i = 0; i < scene->nmovers && i < 65535; i++) {
        const Mover *mv;
        Bound b;
        int m;

        mv = &scene->movers[i];
        m = mv->model;
        g_mover_light[i * 3] = 0;
        g_mover_light[i * 3 + 1] = 0;
        g_mover_light[i * 3 + 2] = 0;
        if (m < 0 || m >= scene->nmodels) {
            continue;
        }
        g_moves[m] = (df_u16)(i + 1);
        if (instance_world_box(&scene->models[m], &b)) {
            light_point(scene, b.cx, b.cy, b.cz, &g_mover_light[i * 3]);
        }
    }
    g_nmodels = scene->nmodels;
    g_nmovers = scene->nmovers;
    return 1;
}

static void stream_rebuild(void);
static void cut_abandon(void);

int light_build(const Scene *scene)
{
    int nd;
    int nc;

    if (g_stream) {
        /* A switch changed (lightrange, lightshadow): every loaded light again, a few a frame. */
        if (scene == g_scene) {
            stream_rebuild();
            return g_stream_decals;
        }
        light_stream_end();
    }
    g_scene = scene;
    g_outdoor = scene ? scene->outdoor : 0;
    g_nlights = 0;
    g_ndecal = 0;
    g_ncorner = 0;
    g_nsprites = 0;
    if (g_epoch != mem_epoch()) {
        /* Allocated on a heap that may be gone: never reuse them. */
        g_decal_cap = 0;
        g_corner_cap = 0;
        g_light_cap = 0;
        g_sprite_cap = 0;
        g_moves_cap = 0;
        g_mover_cap = 0;
        g_moves = 0;
        g_mover_light = 0;
        g_near = 0;
        g_near_cap = 0;
        g_epoch = mem_epoch();
    }
    g_nmodels = 0;
    g_nmovers = 0;
    if (!scene || scene->nlights < 1 || !scene->lights) {
        return 0;
    }
    if (!light_movers(scene)) {
        log_text(LOG_ERROR, "lights: out of memory, the place is unlit");
        return -1;
    }
    if (scene->nmodels > g_near_cap) {
        g_near = (int *)mem_alloc((df_u32)scene->nmodels * (df_u32)sizeof(int));
        g_near_cap = g_near ? scene->nmodels : 0;
        if (!g_near) {
            log_text(LOG_ERROR, "lights: out of memory, the place is unlit");
            return -1;
        }
    }
    /*
     * Count, then allocate what this place needs; a rebuild that fits
     * reuses it. The count skips the shadow rays (cut_face): an upper
     * bound, the castle's 7710 for its 5238.
     */
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
    if (!light_active() || g_outdoor) {
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

df_u32 light_model_rgb(int m, df_u32 base)
{
    const df_u8 *l;
    int i;

    if (!light_active() || g_outdoor || m < 0 || m >= g_nmodels || !g_moves[m]) {
        return base;
    }
    i = (int)g_moves[m] - 1;
    if (i >= g_nmovers) {
        return base;
    }
    l = &g_mover_light[i * 3];
    return rgb(g_ambient * (255 + l[0]) / 255,
               g_ambient * (255 + l[1]) / 255,
               g_ambient * (255 + l[2]) / 255);
}

df_u32 light_sprite_over(int s, df_u32 base)
{
    const df_u8 *l;
    int r;
    int g;
    int b;

    if (s < 0 || s >= g_nsprites || !g_scene) {
        return base;
    }
    if (g_scene->sprites[s].glow) {
        return 0xffffffu;
    }
    if (!light_active()) {
        return base;
    }
    /*
     * base times (1 + passes * lamp colour * light): what LAMP_PASSES
     * decals give a wall, near enough. Each product stays under 2^31.
     */
    l = &g_sprite_light[s * 3];
    r = (int)((base >> 16) & 255u);
    g = (int)((base >> 8) & 255u);
    b = (int)(base & 255u);
    return rgb(r + r * (l[0] * g_lamps / 255) * LAMP_PASSES * LAMP_R / (255 * 255),
               g + g * (l[1] * g_lamps / 255) * LAMP_PASSES * LAMP_G / (255 * 255),
               b + b * (l[2] * g_lamps / 255) * LAMP_PASSES * LAMP_B / (255 * 255));
}

/*
 * A streamed scene (docs/streaming.md, Lights). The chunks come and go
 * a slot at a time; a slot's lights are cut when it arrives, against the
 * models of the loaded slots they reach, and so are the lights of its
 * neighbours that reach into it, before and after it goes. The cutting
 * is the light's own work, queued and done a few lights a frame under
 * light_stream_service's budget: a chunk arrives at the edge of the
 * stream, where its lamps are too far to draw, so a pool a few frames
 * late is never seen.
 */

static float slot_reach(void)
{
    /* RMB lamps all burn at one radius; the largest a light of the slot has would do as well. */
    return 160.0f * (float)g_range;
}

int light_stream_begin(const Scene *scene)
{
    int i;
    int s;

    light_build(0);
    g_scene = scene;
    g_outdoor = scene->outdoor;
    g_epoch = mem_epoch();
    g_nlights = scene->nlights;
    g_nsprites = scene->nsprites;
    g_nmodels = scene->nmodels;
    g_nmovers = 0;
    g_count = (int *)mem_alloc((df_u32)g_nlights * (df_u32)sizeof(int));
    g_first = (int *)mem_alloc((df_u32)g_nlights * (df_u32)sizeof(int));
    g_box = (float *)mem_alloc((df_u32)g_nlights * 6u * (df_u32)sizeof(float));
    g_ldecal = (Decal **)mem_alloc((df_u32)g_nlights * (df_u32)sizeof(Decal *));
    g_lcorner = (float **)mem_alloc((df_u32)g_nlights * (df_u32)sizeof(float *));
    g_pending = (df_u8 *)mem_alloc((df_u32)g_nlights);
    g_nslots = stream_slots();
    g_queue = (int *)mem_alloc((df_u32)(g_nlights + g_nslots) * (df_u32)sizeof(int));
    g_sprite_light = (df_u8 *)mem_alloc((df_u32)(g_nsprites > 0 ? g_nsprites : 1) * 3u);
    g_moves = (df_u16 *)mem_alloc((df_u32)(g_nmodels > 0 ? g_nmodels : 1) * (df_u32)sizeof(df_u16));
    g_near = (int *)mem_alloc((df_u32)(g_nmodels > 0 ? g_nmodels : 1) * (df_u32)sizeof(int));
    g_decal = (Decal *)mem_alloc(SCRATCH_DECALS * (df_u32)sizeof(Decal));
    g_corner = (float *)mem_alloc(SCRATCH_CORNERS * LIGHT_CORNER * (df_u32)sizeof(float));
    if (!g_count || !g_first || !g_box || !g_ldecal || !g_lcorner || !g_pending || !g_queue
        || !g_sprite_light || !g_moves || !g_near || !g_decal || !g_corner) {
        log_text(LOG_ERROR, "lights: out of memory, the stream is unlit");
        g_nlights = 0;
        g_nsprites = 0;
        g_nmodels = 0;
        g_epoch = 0;
        return 0;
    }
    g_near_cap = g_nmodels;
    g_decal_cap = 0;
    g_corner_cap = 0;
    g_light_cap = 0;
    g_sprite_cap = 0;
    g_moves_cap = 0;
    g_mover_cap = 0;
    for (i = 0; i < g_nlights; i++) {
        g_count[i] = 0;
        g_first[i] = 0;
        g_ldecal[i] = 0;
        g_lcorner[i] = 0;
        g_pending[i] = 0;
    }
    for (i = 0; i < g_nsprites * 3; i++) {
        g_sprite_light[i] = 0;
    }
    for (i = 0; i < g_nmodels; i++) {
        g_moves[i] = 0;
    }
    for (s = 0; s < g_nslots; s++) {
        g_slot_heap[s] = 0;
        g_slot_live[s] = 0;
        g_slot_sprites[s] = 0;
    }
    /* The stream's arrays are its slots' ranges end to end (stream_open). */
    g_per_slot = g_nlights / g_nslots;
    g_sprite_slots = g_nsprites / g_nslots;
    g_cut = -1;
    g_qhead = 0;
    g_qcount = 0;
    g_stream_decals = 0;
    g_stream = 1;
    return 1;
}

void light_stream_end(void)
{
    int s;

    if (!g_stream) {
        return;
    }
    for (s = 0; s < g_nslots; s++) {
        mem_heap_delete(g_slot_heap[s]);
        g_slot_heap[s] = 0;
    }
    g_cut = -1;
    g_stream = 0;
    g_nlights = 0;
    g_nsprites = 0;
    g_nmodels = 0;
    g_ndecal = 0;
    g_ncorner = 0;
    g_scene = 0;
    /* What was allocated went with the heap it was on; never reused. */
    g_epoch = 0;
}

int light_streaming(void)
{
    return g_stream;
}

/* Queue entries: a light, or g_nslots ahead of the lights, a slot whose sprites to light. */
static void queue_push(int v)
{
    if (g_qcount < g_nlights + g_nslots) {
        g_queue[(g_qhead + g_qcount) % (g_nlights + g_nslots)] = v;
        g_qcount++;
    }
}

static void queue_light(int i)
{
    if (!g_pending[i]) {
        g_pending[i] = 1;
        queue_push(i);
    }
}

static void queue_sprites(int s)
{
    if (!g_slot_sprites[s]) {
        g_slot_sprites[s] = 1;
        queue_push(g_nlights + s);
    }
}

static int boxes_meet(const float *a, const float *b, float grow)
{
    return a[0] - grow <= b[3] && b[0] <= a[3] + grow
        && a[2] - grow <= b[5] && b[2] <= a[5] + grow;
}

/* Light i's decals go, and the heap they were in has them back. */
static void clear_light(int i)
{
    MemHeap *h;

    h = g_slot_heap[i / g_per_slot];
    if (h) {
        mem_heap_free(h, g_ldecal[i]);
        mem_heap_free(h, g_lcorner[i]);
    }
    g_stream_decals -= g_count[i];
    g_ldecal[i] = 0;
    g_lcorner[i] = 0;
    g_count[i] = 0;
}

/*
 * Cut light i against the still models of the loaded slots it reaches,
 * into its slot's heap: cut_start finds the models and empties the
 * scratch, cut_run cuts them a face at a time until the deadline,
 * cut_finish keeps what was cut. 0 from cut_start: nothing to cut.
 */
static int cut_start(int i)
{
    const Light *li;
    float reach;
    float lb[6];
    int s;
    int n;

    clear_light(i);
    li = &g_scene->lights[i];
    reach = li->radius * (float)g_range;
    s = i / g_per_slot;
    if (reach < 1.0f || !g_slot_live[s]) {
        return 0;
    }
    lb[0] = li->x - reach;
    lb[1] = li->y - reach;
    lb[2] = li->z - reach;
    lb[3] = li->x + reach;
    lb[4] = li->y + reach;
    lb[5] = li->z + reach;
    g_nnear = 0;
    for (n = 0; n < g_nslots; n++) {
        int first;
        int near;
        int far;
        int rest;
        int pass;

        if (!g_slot_live[n] || !boxes_meet(g_slot_box[n], lb, 0.0f)) {
            continue;
        }
        /* The near ground and the rest: the far ground draws only where a lamp is too far to. */
        stream_slot_models(n, &first, &near, &far, &rest);
        for (pass = 0; pass < 2; pass++) {
            int m;
            int end;

            m = pass ? first + near + far : first;
            end = pass ? m + rest : first + near;
            for (; m < end && g_nnear < g_near_cap; m++) {
                const Instance *in;
                const Bound *bd;

                in = &g_scene->models[m];
                if (!in->mesh || !in->mesh->faces || g_moves[m]) {
                    continue;
                }
                bd = &g_scene->bounds[m];
                if (df_absf(bd->cx - li->x) > bd->ex + reach
                    || df_absf(bd->cy - li->y) > bd->ey + reach
                    || df_absf(bd->cz - li->z) > bd->ez + reach) {
                    continue;
                }
                g_near[g_nnear++] = m;
            }
        }
    }
    g_decal_cap = SCRATCH_DECALS;
    g_corner_cap = SCRATCH_CORNERS;
    g_ndecal = 0;
    g_ncorner = 0;
    g_cut = i;
    g_cut_model = 0;
    g_cut_face = 0;
    return 1;
}

/* The light being cut, a face at a time, until done (1) or start + limit has passed (0; limit 0: one face). */
static int cut_run(df_u32 start, df_u32 limit)
{
    const Light *li;
    float reach;

    li = &g_scene->lights[g_cut];
    reach = li->radius * (float)g_range;
    while (g_cut_model < g_nnear) {
        const Instance *in;

        in = &g_scene->models[g_near[g_cut_model]];
        if (!in->mesh || g_cut_face >= in->mesh->nfaces) {
            g_cut_model++;
            g_cut_face = 0;
            continue;
        }
        light_faces(in, li, reach, 1, g_cut, g_cut_face, g_cut_face + 1);
        g_cut_face++;
        if (limit == 0 || prof_now() - start >= limit) {
            return g_cut_model >= g_nnear;
        }
    }
    return 1;
}

static void cut_finish(void)
{
    MemHeap *was;
    Decal *d;
    float *c;
    int i;
    int s;

    i = g_cut;
    g_cut = -1;
    s = i / g_per_slot;
    if (g_ndecal < 1) {
        g_count[i] = 0;
        return;
    }
    if (!g_slot_heap[s]) {
        g_slot_heap[s] = mem_heap_new();
    }
    was = mem_heap_use(g_slot_heap[s]);
    d = g_slot_heap[s] ? (Decal *)mem_alloc((df_u32)g_ndecal * (df_u32)sizeof(Decal)) : 0;
    c = g_slot_heap[s] ? (float *)mem_alloc((df_u32)g_ncorner * LIGHT_CORNER * (df_u32)sizeof(float)) : 0;
    mem_heap_use(was);
    if (!d || !c) {
        if (g_slot_heap[s]) {
            mem_heap_free(g_slot_heap[s], d);
            mem_heap_free(g_slot_heap[s], c);
        }
        g_count[i] = 0;
        return;
    }
    mem_copy(d, g_decal, (df_u32)g_ndecal * (df_u32)sizeof(Decal));
    mem_copy(c, g_corner, (df_u32)g_ncorner * LIGHT_CORNER * (df_u32)sizeof(float));
    g_ldecal[i] = d;
    g_lcorner[i] = c;
    g_stream_decals += g_count[i];
}

/* Slot s's sprites, from the lights of the slots near enough to reach them. */
static void stream_sprites(int s)
{
    float reach;
    int i;

    g_slot_sprites[s] = 0;
    if (!g_slot_live[s]) {
        return;
    }
    reach = slot_reach();
    for (i = s * g_sprite_slots; i < (s + 1) * g_sprite_slots; i++) {
        const Flat *fl;
        float r;
        float g;
        float b;
        int n;

        fl = &g_scene->sprites[i];
        g_sprite_light[i * 3] = 0;
        g_sprite_light[i * 3 + 1] = 0;
        g_sprite_light[i * 3 + 2] = 0;
        if (fl->w <= 0.0f || fl->glow) {
            continue;
        }
        r = g = b = 0.0f;
        for (n = 0; n < g_nslots; n++) {
            int k;

            if (!g_slot_live[n] || !boxes_meet(g_slot_box[n], g_slot_box[s], reach)) {
                continue;
            }
            for (k = n * g_per_slot; k < (n + 1) * g_per_slot; k++) {
                const Light *li;
                float lr, dx, dy, dz, d2, l;

                li = &g_scene->lights[k];
                lr = li->radius * (float)g_range;
                if (lr < 1.0f) {
                    continue;
                }
                dx = fl->x - li->x;
                dy = fl->y - li->y;
                dz = fl->z - li->z;
                d2 = dx * dx + dy * dy + dz * dz;
                if (d2 >= lr * lr) {
                    continue;
                }
                l = falloff(root(d2) / lr, 1.0f);
                r += l * LIGHT_R;
                g += l * LIGHT_G;
                b += l * LIGHT_B;
            }
        }
        g_sprite_light[i * 3] = byte01(r);
        g_sprite_light[i * 3 + 1] = byte01(g);
        g_sprite_light[i * 3 + 2] = byte01(b);
    }
}

/* The lights of other loaded slots that reach slot s's box, and their slots' sprites. */
static void queue_neighbours(int s)
{
    float reach;
    int n;

    reach = slot_reach();
    for (n = 0; n < g_nslots; n++) {
        int k;
        int any;

        if (n == s || !g_slot_live[n] || !boxes_meet(g_slot_box[n], g_slot_box[s], reach)) {
            continue;
        }
        any = 0;
        for (k = n * g_per_slot; k < (n + 1) * g_per_slot; k++) {
            const Light *li;
            float lr;
            float lb[6];

            li = &g_scene->lights[k];
            lr = li->radius * (float)g_range;
            if (lr < 1.0f) {
                continue;
            }
            lb[0] = li->x - lr;
            lb[1] = li->y - lr;
            lb[2] = li->z - lr;
            lb[3] = li->x + lr;
            lb[4] = li->y + lr;
            lb[5] = li->z + lr;
            if (boxes_meet(g_slot_box[s], lb, 0.0f)) {
                queue_light(k);
                any = 1;
            }
        }
        if (any) {
            queue_sprites(n);
        }
    }
}

void light_stream_changed(int s, int loaded)
{
    int first;
    int near;
    int far;
    int rest;
    int i;
    int any;

    if (!g_stream || s < 0 || s >= g_nslots) {
        return;
    }
    stream_slot_models(s, &first, &near, &far, &rest);
    if (!loaded) {
        cut_abandon();
        for (i = s * g_per_slot; i < (s + 1) * g_per_slot; i++) {
            g_stream_decals -= g_count[i];
            g_count[i] = 0;
            g_ldecal[i] = 0;
            g_lcorner[i] = 0;
        }
        mem_heap_delete(g_slot_heap[s]);
        g_slot_heap[s] = 0;
        for (i = s * g_sprite_slots; i < (s + 1) * g_sprite_slots; i++) {
            g_sprite_light[i * 3] = g_sprite_light[i * 3 + 1] = g_sprite_light[i * 3 + 2] = 0;
        }
        for (i = s * (g_nmodels / g_nslots); i < (s + 1) * (g_nmodels / g_nslots); i++) {
            g_moves[i] = 0;
        }
        g_slot_live[s] = 0;
        /* Its neighbours' pools that lay over it, cut again without it. */
        queue_neighbours(s);
        return;
    }
    /* The slot's box, models and sprites, for the lights that reach it after it goes too. */
    any = 0;
    for (i = first; i < first + near + far + rest; i++) {
        const Bound *b;
        float *bx;

        b = &g_scene->bounds[i];
        if (!g_scene->models[i].mesh || b->ex < 0.0f) {
            continue;
        }
        bx = g_slot_box[s];
        if (!any) {
            bx[0] = b->cx - b->ex;
            bx[1] = b->cy - b->ey;
            bx[2] = b->cz - b->ez;
            bx[3] = b->cx + b->ex;
            bx[4] = b->cy + b->ey;
            bx[5] = b->cz + b->ez;
            any = 1;
            continue;
        }
        bx[0] = b->cx - b->ex < bx[0] ? b->cx - b->ex : bx[0];
        bx[1] = b->cy - b->ey < bx[1] ? b->cy - b->ey : bx[1];
        bx[2] = b->cz - b->ez < bx[2] ? b->cz - b->ez : bx[2];
        bx[3] = b->cx + b->ex > bx[3] ? b->cx + b->ex : bx[3];
        bx[4] = b->cy + b->ey > bx[4] ? b->cy + b->ey : bx[4];
        bx[5] = b->cz + b->ez > bx[5] ? b->cz + b->ez : bx[5];
    }
    for (i = s * g_sprite_slots; i < (s + 1) * g_sprite_slots; i++) {
        const Flat *fl;
        float *bx;

        fl = &g_scene->sprites[i];
        if (fl->w <= 0.0f) {
            continue;
        }
        bx = g_slot_box[s];
        if (!any) {
            bx[0] = bx[3] = fl->x;
            bx[1] = bx[4] = fl->y;
            bx[2] = bx[5] = fl->z;
            any = 1;
        }
        bx[0] = fl->x < bx[0] ? fl->x : bx[0];
        bx[1] = fl->y < bx[1] ? fl->y : bx[1];
        bx[2] = fl->z < bx[2] ? fl->z : bx[2];
        bx[3] = fl->x > bx[3] ? fl->x : bx[3];
        bx[4] = fl->y > bx[4] ? fl->y : bx[4];
        bx[5] = fl->z > bx[5] ? fl->z : bx[5];
    }
    if (!any) {
        return;
    }
    /* Its moving models (doors) take no decals: they would stay where the door stood. */
    for (i = 0; i < g_scene->nmovers; i++) {
        const Mover *mv;

        mv = &g_scene->movers[i];
        if (mv->inst && mv->model >= first && mv->model < first + near + far + rest) {
            g_moves[mv->model] = 1;
        }
    }
    g_slot_live[s] = 1;
    for (i = s * g_per_slot; i < (s + 1) * g_per_slot; i++) {
        if (g_scene->lights[i].radius > 0.0f) {
            queue_light(i);
        }
    }
    queue_sprites(s);
    queue_neighbours(s);
}

static void stream_rebuild(void)
{
    int i;
    int s;

    for (s = 0; s < g_nslots; s++) {
        if (!g_slot_live[s]) {
            continue;
        }
        for (i = s * g_per_slot; i < (s + 1) * g_per_slot; i++) {
            if (g_scene->lights[i].radius > 0.0f) {
                queue_light(i);
            }
        }
        queue_sprites(s);
    }
}

void light_stream_service(float budget_ms)
{
    df_u32 start;
    df_u32 limit;

    if (!g_stream) {
        return;
    }
    start = prof_now();
    limit = (df_u32)df_iround(budget_ms * (float)prof_per_ms());
    for (;;) {
        df_u32 t0;

        t0 = prof_now();
        if (g_cut < 0) {
            int v;

            if (g_qcount < 1) {
                break;
            }
            v = g_queue[g_qhead];
            g_qhead = (g_qhead + 1) % (g_nlights + g_nslots);
            g_qcount--;
            if (v >= g_nlights) {
                stream_sprites(v - g_nlights);
            } else {
                g_pending[v] = 0;
                if (cut_start(v) && cut_run(start, limit)) {
                    cut_finish();
                }
            }
        } else if (cut_run(start, limit)) {
            cut_finish();
        }
        t0 = prof_now() - t0;
        g_unit_worst = t0 > g_unit_worst ? t0 : g_unit_worst;
        /* One unit at least; uncalibrated, one a call. */
        if (limit == 0 || prof_now() - start >= limit) {
            break;
        }
    }
}

/*
 * The light being cut is given up and queued again from the start: its
 * models went with a slot, or moved with the origin. A slot coming in
 * needs nothing: its neighbours' lights are queued again anyway.
 */
static void cut_abandon(void)
{
    int i;

    if (g_cut < 0) {
        return;
    }
    i = g_cut;
    g_cut = -1;
    g_count[i] = 0;
    queue_light(i);
}

float light_stream_worst(void)
{
    df_u32 per;
    float ms;

    per = prof_per_ms();
    ms = per ? (float)g_unit_worst / (float)per : 0.0f;
    g_unit_worst = 0;
    return ms;
}

int light_stream_pending(void)
{
    return g_stream ? g_qcount + (g_cut >= 0) : 0;
}

void light_stream_shift(float dx, float dz)
{
    int i;
    int s;

    if (!g_stream) {
        return;
    }
    cut_abandon();
    for (s = 0; s < g_nslots; s++) {
        g_slot_box[s][0] -= dx;
        g_slot_box[s][3] -= dx;
        g_slot_box[s][2] -= dz;
        g_slot_box[s][5] -= dz;
    }
    for (i = 0; i < g_nlights; i++) {
        int k;
        int q;

        if (g_count[i] < 1) {
            continue;
        }
        g_box[i * 6] -= dx;
        g_box[i * 6 + 3] -= dx;
        g_box[i * 6 + 2] -= dz;
        g_box[i * 6 + 5] -= dz;
        for (k = 0; k < g_count[i]; k++) {
            Decal *d;

            d = &g_ldecal[i][k];
            /* Its plane through the moved points: n . (p - shift) + d' = 0. */
            d->d += d->nx * dx + d->nz * dz;
            d->cx -= dx;
            d->cz -= dz;
            for (q = d->first; q < d->first + d->n; q++) {
                g_lcorner[i][q * LIGHT_CORNER] -= dx;
                g_lcorner[i][q * LIGHT_CORNER + 2] -= dz;
            }
        }
    }
}
