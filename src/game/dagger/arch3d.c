#include "game/dagger/arch3d.h"
#include "engine/core/mem.h"
#include "engine/scene/scene.h"

#define MAX_FACE 24
/* Corners a face may have once weld has inserted its neighbours' corners. */
#define WELD_MAX 32
/*
 * How far off an edge, in file units (1/256 of a world unit), a point
 * may sit and still be welded into it, and how far from either end it
 * has to be. 16 is 1/16 of a world unit, well under any real feature.
 */
#define WELD_NEAR 16.0
/*
 * Face corners of one model weld looks at; past this the model is not
 * welded. The most in ARCH3D.BSA is 2720, model 451. The tables below
 * are static, about 84 bytes a corner: some 260 KB.
 */
#define WELD_POINTS 3072
/*
 * A file point no farther than this from the straight line between its
 * neighbours, in file units, is on that edge, not a corner: 4 is 1/64
 * of a world unit. The thinnest real feature, a 2-unit door jamb, puts
 * its corners about 100 units off.
 */
#define STRAIGHT_NEAR 4.0
#define POINT_DIV 256.0f
#define UV_DIV 16.0f

static int g_uv_fail;
static int g_bad_refs;
/* decode_plane runs three times a model; it counts bad references once. */
static int g_counting;

int arch3d_uv_failures(void)
{
    return g_uv_fail;
}

int arch3d_bad_refs(void)
{
    return g_bad_refs;
}

typedef struct Pt {
    int x, y, z;
    int u, v;
} Pt;

static void uv_unpack(int *u)
{
    int next_mult;
    int prev_mult;
    int mult;

    /* Ids under 905 store a few UVs packed against a multiple of 8192. */
    if (*u > -14336 && *u < 14336 && *u != -7168) {
        return;
    }
    next_mult = *u - 1;
    next_mult = next_mult >> 13;
    next_mult++;
    next_mult = next_mult << 13;
    prev_mult = next_mult - 8192;
    if (*u - prev_mult < next_mult - *u) {
        mult = prev_mult;
    } else {
        mult = next_mult;
    }
    *u -= mult;
}

static int face_uv(const Pt *in, int n, int *ou, int *ov)
{
    float p0x, p0y, p0z, p1x, p1y, p1z, p2x, p2y, p2z;
    float v0x, v0y, v0z, v1x, v1y, v1z;
    float d00, d10, scale;
    float x0, y0, x1, y1, x2, y2;
    float u0, u1, u2, v0, v1, v2;
    float det;
    float xi0, xi1, xi2, yi0, yi1, yi2, zi0, zi1, zi2;
    float ua, ub, ud, va, vb, vd;
    int i;

    if (n < 3) {
        return 0;
    }
    p0x = (float)in[0].x;
    p0y = (float)in[0].y;
    p0z = (float)in[0].z;
    p1x = (float)in[1].x;
    p1y = (float)in[1].y;
    p1z = (float)in[1].z;
    p2x = (float)in[2].x;
    p2y = (float)in[2].y;
    p2z = (float)in[2].z;
    v0x = p1x - p0x;
    v0y = p1y - p0y;
    v0z = p1z - p0z;
    v1x = p2x - p0x;
    v1y = p2y - p0y;
    v1z = p2z - p0z;
    d00 = v0x * v0x + v0y * v0y + v0z * v0z;
    if (d00 < 1.0f) {
        return 0;
    }
    d10 = v1x * v0x + v1y * v0y + v1z * v0z;
    scale = d10 / d00;
    v1x -= v0x * scale;
    v1y -= v0y * scale;
    v1z -= v0z * scale;
    {
        float s;

        s = v0x * v0x + v0y * v0y + v0z * v0z;
        __asm__ __volatile__("fsqrt" : "=t"(s) : "0"(s));
        if (s < 1.0e-8f) {
            return 0;
        }
        v0x /= s;
        v0y /= s;
        v0z /= s;
        s = v1x * v1x + v1y * v1y + v1z * v1z;
        __asm__ __volatile__("fsqrt" : "=t"(s) : "0"(s));
        if (s < 1.0e-8f) {
            return 0;
        }
        v1x /= s;
        v1y /= s;
        v1z /= s;
    }
    x0 = (float)(int)(p0x * v0x + p0y * v0y + p0z * v0z);
    y0 = (float)(int)(p0x * v1x + p0y * v1y + p0z * v1z);
    x1 = (float)(int)(p1x * v0x + p1y * v0y + p1z * v0z);
    y1 = (float)(int)(p1x * v1x + p1y * v1y + p1z * v1z);
    x2 = (float)(int)(p2x * v0x + p2y * v0y + p2z * v0z);
    y2 = (float)(int)(p2x * v1x + p2y * v1y + p2z * v1z);
    u0 = (float)in[0].u;
    v0 = (float)in[0].v;
    u1 = (float)in[0].u + (float)in[1].u;
    v1 = (float)in[0].v + (float)in[1].v;
    u2 = u1 + (float)in[2].u;
    v2 = v1 + (float)in[2].v;
    det = x0 * y1 + y0 * x2 + x1 * y2 - y1 * x2 - y0 * x1 - x0 * y2;
    if (det == 0.0f) {
        return 0;
    }
    xi0 = (y1 - y2) / det;
    xi1 = (-x1 + x2) / det;
    xi2 = (x1 * y2 - x2 * y1) / det;
    yi0 = (-y0 + y2) / det;
    yi1 = (x0 - x2) / det;
    yi2 = (-x0 * y2 + x2 * y0) / det;
    zi0 = (y0 - y1) / det;
    zi1 = (-x0 + x1) / det;
    zi2 = (x0 * y1 - x1 * y0) / det;
    ua = u0 * xi0 + u1 * yi0 + u2 * zi0;
    ub = u0 * xi1 + u1 * yi1 + u2 * zi1;
    ud = u0 * xi2 + u1 * yi2 + u2 * zi2;
    va = v0 * xi0 + v1 * yi0 + v2 * zi0;
    vb = v0 * xi1 + v1 * yi1 + v2 * zi1;
    vd = v0 * xi2 + v1 * yi2 + v2 * zi2;
    for (i = 0; i < n; i++) {
        if (i == 0) {
            ou[i] = in[0].u;
            ov[i] = in[0].v;
        } else if (i == 1) {
            ou[i] = in[0].u + in[1].u;
            ov[i] = in[0].v + in[1].v;
        } else if (i == 2) {
            ou[i] = in[0].u + in[1].u + in[2].u;
            ov[i] = in[0].v + in[1].v + in[2].v;
        } else {
            float px, py, pz, s, t;

            px = (float)in[i].x;
            py = (float)in[i].y;
            pz = (float)in[i].z;
            s = (float)(int)(px * v0x + py * v0y + pz * v0z);
            t = (float)(int)(px * v1x + py * v1y + pz * v1z);
            ou[i] = (int)(s * ua + t * ub + ud);
            ov[i] = (int)(s * va + t * vb + vd);
        }
    }
    return 1;
}

/*
 * The corner to fan from: the one whose triangle with its next two
 * corners is largest, so push_face takes a real normal from the first
 * triangle. A corner weld inserted along an edge next to the apex makes
 * a zero-area triangle; the card skips it, and it keeps the fan exact,
 * which draw.c and the occlusion pass need. In ARCH3D.BSA 2 faces are
 * not convex, and their fans from this corner still cover them exactly.
 */
static int fan_apex(const Pt *pts, int n)
{
    double best;
    int apex;
    int k;

    best = -1.0;
    apex = 0;
    for (k = 0; k < n; k++) {
        const Pt *a;
        const Pt *b;
        const Pt *c;
        double ax, ay, az, bx, by, bz, cx, cy, cz, d;

        a = &pts[k];
        b = &pts[(k + 1) % n];
        c = &pts[(k + 2) % n];
        ax = (double)(b->x - a->x);
        ay = (double)(b->y - a->y);
        az = (double)(b->z - a->z);
        bx = (double)(c->x - a->x);
        by = (double)(c->y - a->y);
        bz = (double)(c->z - a->z);
        cx = ay * bz - az * by;
        cy = az * bx - ax * bz;
        cz = ax * by - ay * bx;
        d = cx * cx + cy * cy + cz * cz;
        if (d > best) {
            best = d;
            apex = k;
        }
    }
    return apex;
}

static void emit_tri(Mesh *mesh, int cap, const Pt *pts, const int *uv_u, const int *uv_v,
                      int i0, int i1, int i2, df_u16 file, df_u16 image)
{
    Tri *t;
    int k;
    int idx[3];

    if (mesh->ntris >= cap || i0 == i1 || i1 == i2 || i0 == i2) {
        return;
    }
    idx[0] = i0;
    idx[1] = i1;
    idx[2] = i2;
    t = &mesh->tris[mesh->ntris];
    for (k = 0; k < 3; k++) {
        int p;

        p = idx[k];
        t->x[k] = (float)pts[p].x / POINT_DIV;
        /* File Y grows downward. The camera treats larger Y as up. */
        t->y[k] = -(float)pts[p].y / POINT_DIV;
        t->z[k] = (float)pts[p].z / POINT_DIV;
        t->u[k] = (float)uv_u[p] / UV_DIV;
        t->v[k] = (float)uv_v[p] / UV_DIV;
    }
    t->tex_file = file;
    t->tex_image = image;
    t->slot = 0xffff;
    mesh->ntris++;
}

/* Triangles emit_fan makes for this face: a fan over every point. */
static int tri_count(const Pt *pts, int npts)
{
    (void)pts;
    return npts >= 3 ? npts - 2 : 0;
}

/*
 * One plane. 1 means pts and tex are filled. 0 means this plane was
 * skipped and *p advanced. -1 means the record ends here.
 */
static int decode_plane(const df_u8 *rec, df_u32 size, df_u32 *p_io,
                        int v25, df_i32 point_off, df_i32 point_end, df_u32 id,
                        Pt *pts, int *npts_out, df_u16 *tex_out)
{
    df_u32 p;
    int npts;
    int k;

    p = *p_io;
    if (p + 8 > size) {
        return -1;
    }
    npts = rec[p];
    *tex_out = df_ru16(rec + p + 2);
    p += 8;
    if (npts < 3 || npts > MAX_FACE) {
        if (npts > MAX_FACE || p + (df_u32)npts * 8 > size) {
            return -1;
        }
        *p_io = p + (df_u32)npts * 8;
        return 0;
    }
    if (p + (df_u32)npts * 8 > size) {
        return -1;
    }
    for (k = 0; k < npts; k++) {
        df_i32 off;
        df_i32 u;
        df_i32 v;
        df_i32 pp;

        off = df_ri32(rec + p);
        u = df_ri16(rec + p + 4);
        v = df_ri16(rec + p + 6);
        p += 8;
        if (k < 3 && id < 905) {
            uv_unpack(&u);
            uv_unpack(&v);
        }
        /*
         * A point reference is a byte offset into the point table. In
         * v2.6 and v2.7 the table's points are 12 bytes, so it is the
         * offset itself. In v2.5 it counts 4 bytes a point: index * 4.
         * It used to be read as an index, multiplied by 12, which sent
         * the 8 v2.5 records in ARCH3D.BSA (41500-41505, 41509, 41510)
         * three times past their tables into the rest of the file:
         * points in the millions and faces collapsed to one point.
         * Only points inside the table are taken now.
         */
        if (v25) {
            pp = point_off + (off / 4) * 12;
        } else {
            pp = point_off + off;
        }
        if (off < 0 || pp < point_off || pp + 12 > point_end || (df_u32)pp + 12 > size) {
            /* This face only: the next plane starts after its references. */
            g_bad_refs += g_counting;
            *p_io = *p_io + 8u + (df_u32)npts * 8u;
            return 0;
        }
        pts[k].x = df_ri32(rec + pp);
        pts[k].y = df_ri32(rec + pp + 4);
        pts[k].z = df_ri32(rec + pp + 8);
        pts[k].u = u;
        pts[k].v = v;
    }
    *p_io = p;
    *npts_out = npts;
    return 1;
}

/*
 * The cross product of the emitted winding points away from the side
 * you are meant to see. A ground quad does the same: its winding points
 * down, and the patch normal is stored pointing up. Flip this one too,
 * so instance_faces keeps the near wall instead of the far one.
 */
static void push_face(Mesh *mesh, int first)
{
    Tri *t;
    float ax, ay, az;
    float bx, by, bz;
    Face *f;

    if (!mesh->faces || mesh->nfaces >= mesh->face_cap || mesh->ntris <= first) {
        return;
    }
    t = &mesh->tris[first];
    ax = t->x[1] - t->x[0];
    ay = t->y[1] - t->y[0];
    az = t->z[1] - t->z[0];
    bx = t->x[2] - t->x[0];
    by = t->y[2] - t->y[0];
    bz = t->z[2] - t->z[0];
    f = &mesh->faces[mesh->nfaces];
    f->nx = az * by - ay * bz;
    f->ny = ax * bz - az * bx;
    f->nz = ay * bx - ax * by;
    f->first = first;
    f->ntris = mesh->ntris - first;
    mesh->nfaces++;
}

/* Texel coordinates for each file point, uv_u and uv_v in 1/UV_DIV texel. */
static void plane_uv(const Pt *pts, int npts, int *uv_u, int *uv_v)
{
    int k;

    if (npts == 3) {
        uv_u[0] = pts[0].u;
        uv_v[0] = pts[0].v;
        uv_u[1] = pts[0].u + pts[1].u;
        uv_v[1] = pts[0].v + pts[1].v;
        uv_u[2] = uv_u[1] + pts[2].u;
        uv_v[2] = uv_v[1] + pts[2].v;
    } else if (!face_uv(pts, npts, uv_u, uv_v)) {
        g_uv_fail++;
        uv_u[0] = pts[0].u;
        uv_v[0] = pts[0].v;
        uv_u[1] = pts[0].u + pts[1].u;
        uv_v[1] = pts[0].v + pts[1].v;
        uv_u[2] = uv_u[1] + pts[2].u;
        uv_v[2] = uv_v[1] + pts[2].v;
        for (k = 3; k < npts; k++) {
            uv_u[k] = uv_u[2];
            uv_v[k] = uv_v[2];
        }
    }
}

/* Squared distance of p from the line through a and b, in file units. */
static double off_line2(const Pt *a, const Pt *b, const Pt *p)
{
    double ex, ey, ez, px, py, pz, cx, cy, cz, len2;

    ex = (double)b->x - a->x;
    ey = (double)b->y - a->y;
    ez = (double)b->z - a->z;
    px = (double)p->x - a->x;
    py = (double)p->y - a->y;
    pz = (double)p->z - a->z;
    len2 = ex * ex + ey * ey + ez * ez;
    if (len2 <= 0.0) {
        return px * px + py * py + pz * pz;
    }
    cx = py * ez - pz * ey;
    cy = pz * ex - px * ez;
    cz = px * ey - py * ex;
    return (cx * cx + cy * cy + cz * cz) / len2;
}

/*
 * Drops the file points that lie on the straight line between their
 * neighbours, and repeats, so only the outline's real corners stay.
 * A point the file puts along an edge is often where the next face
 * ends; weld puts it back exactly where another face of this model
 * has a corner. Kept everywhere, it put a corner on a model's outer
 * edge that the next model's face, along the same line, does not have:
 * a new T-junction between two models, in the stair hall's floor.
 *
 * This is a distance test. The old loader used the turn seen from two
 * points back, under 0.6 degrees, and on a 2-unit door jamb or frame a
 * real corner turns less than that: 477 faces lost a triangle-sized
 * piece, up to 16% of their area. uv_u may be 0.
 */
static int straighten(Pt *pts, int n, int *uv_u, int *uv_v)
{
    int changed;

    do {
        int i;

        changed = 0;
        for (i = 0; i < n && n > 3; i++) {
            const Pt *a;
            const Pt *b;
            int k;

            a = &pts[(i + n - 1) % n];
            b = &pts[(i + 1) % n];
            if (off_line2(a, b, &pts[i]) > STRAIGHT_NEAR * STRAIGHT_NEAR) {
                continue;
            }
            /* Only a point between its neighbours, not a spike back along the line. */
            if (((double)pts[i].x - a->x) * ((double)b->x - pts[i].x)
                + ((double)pts[i].y - a->y) * ((double)b->y - pts[i].y)
                + ((double)pts[i].z - a->z) * ((double)b->z - pts[i].z) < 0.0) {
                continue;
            }
            for (k = i; k < n - 1; k++) {
                pts[k] = pts[k + 1];
                if (uv_u) {
                    uv_u[k] = uv_u[k + 1];
                    uv_v[k] = uv_v[k + 1];
                }
            }
            n--;
            changed = 1;
            i--;
        }
    } while (changed);
    return n;
}

/*
 * Every face's real corners in one model, and which face each belongs
 * to, filled before the model's triangles are made. g_corners is -1
 * when a model has more than WELD_POINTS; it is then not welded.
 */
static int g_cx[WELD_POINTS];
static int g_cy[WELD_POINTS];
static int g_cz[WELD_POINTS];
static int g_cface[WELD_POINTS];
/* The corner before and after each corner in its own face. */
static Pt g_cprev[WELD_POINTS];
static Pt g_cnext[WELD_POINTS];
static int g_corners;

static void corners_add(const Pt *pts, int n, int face)
{
    int i;

    for (i = 0; i < n && g_corners >= 0; i++) {
        if (g_corners >= WELD_POINTS) {
            g_corners = -1;
            return;
        }
        g_cx[g_corners] = pts[i].x;
        g_cy[g_corners] = pts[i].y;
        g_cz[g_corners] = pts[i].z;
        g_cface[g_corners] = face;
        g_cprev[g_corners] = pts[(i + n - 1) % n];
        g_cnext[g_corners] = pts[(i + 1) % n];
        g_corners++;
    }
}

static double g_near_x[WELD_POINTS];
static double g_near_y[WELD_POINTS];
static double g_near_z[WELD_POINTS];
static int g_near_k[WELD_POINTS];

/*
 * Welds T-junctions inside one model, at load. Where another face of
 * the same model runs along one of this face's edges and ends inside
 * it, a corner of that face lies on the edge, within WELD_NEAR of the
 * line and at least that far from both ends, and the card left a crack
 * along the seam, a pixel here and there that the occlusion pass also
 * has to treat as open. A corner where a face only touches the edge
 * from another direction, such as the end of a crossing wall, closes no
 * seam; inserting it would only put a corner on the model's outer edge
 * that the next model's face does not have, so it is left out. weld inserts a corner
 * there, on the edge itself, in order along it, with the edge's texel
 * coordinates interpolated, exact for the face's flat mapping. Then the
 * two faces share the seam exactly. uv_u may be 0 to count only.
 * Returns the new corner count, never past WELD_MAX. Only one model's
 * corners are seen: a seam between two placed models is as the file
 * has it.
 */
static int weld(int face, Pt *pts, int n, int *uv_u, int *uv_v)
{
    Pt out[WELD_MAX];
    int ou[WELD_MAX];
    int ov[WELD_MAX];
    double nx, ny, nz, nl, d0;
    int near;
    int m;
    int i;
    int k;

    if (g_corners < 1 || n < 3) {
        return n;
    }
    nx = 0.0;
    ny = 0.0;
    nz = 0.0;
    for (i = 0; i < n; i++) {
        const Pt *a;
        const Pt *b;

        a = &pts[i];
        b = &pts[(i + 1) % n];
        nx += ((double)a->y - b->y) * ((double)a->z + b->z);
        ny += ((double)a->z - b->z) * ((double)a->x + b->x);
        nz += ((double)a->x - b->x) * ((double)a->y + b->y);
    }
    nl = nx * nx + ny * ny + nz * nz;
    if (nl <= 0.0) {
        return n;
    }
    __asm__ __volatile__("fsqrt" : "=t"(nl) : "0"(nl));
    nx /= nl;
    ny /= nl;
    nz /= nl;
    d0 = nx * pts[0].x + ny * pts[0].y + nz * pts[0].z;
    /* Only another face's corners on this face's plane can lie on its edges. */
    near = 0;
    for (k = 0; k < g_corners; k++) {
        double x, y, z, d;

        if (g_cface[k] == face) {
            continue;
        }
        x = (double)g_cx[k];
        y = (double)g_cy[k];
        z = (double)g_cz[k];
        d = nx * x + ny * y + nz * z - d0;
        if (d < -WELD_NEAR || d > WELD_NEAR) {
            continue;
        }
        g_near_x[near] = x;
        g_near_y[near] = y;
        g_near_z[near] = z;
        g_near_k[near] = k;
        near++;
    }
    m = 0;
    for (i = 0; i < n && m < WELD_MAX; i++) {
        const Pt *a;
        const Pt *b;
        double ex, ey, ez, len2, len;
        double hit_t[WELD_MAX];
        int nh;
        int h;

        a = &pts[i];
        b = &pts[(i + 1) % n];
        out[m] = *a;
        ou[m] = uv_u ? uv_u[i] : 0;
        ov[m] = uv_v ? uv_v[i] : 0;
        m++;
        ex = (double)b->x - a->x;
        ey = (double)b->y - a->y;
        ez = (double)b->z - a->z;
        len2 = ex * ex + ey * ey + ez * ez;
        if (len2 < 4.0 * WELD_NEAR * WELD_NEAR) {
            continue;
        }
        len = len2;
        __asm__ __volatile__("fsqrt" : "=t"(len) : "0"(len));
        nh = 0;
        for (k = 0; k < near; k++) {
            double px, py, pz, t, cx, cy, cz;

            px = g_near_x[k] - a->x;
            py = g_near_y[k] - a->y;
            pz = g_near_z[k] - a->z;
            t = (px * ex + py * ey + pz * ez) / len2;
            if (t * len < WELD_NEAR || (1.0 - t) * len < WELD_NEAR) {
                continue;
            }
            cx = px - t * ex;
            cy = py - t * ey;
            cz = pz - t * ez;
            if (cx * cx + cy * cy + cz * cz > WELD_NEAR * WELD_NEAR) {
                continue;
            }
            /* The other face has to run along this edge from that corner. */
            if (off_line2(a, b, &g_cprev[g_near_k[k]]) > WELD_NEAR * WELD_NEAR
                && off_line2(a, b, &g_cnext[g_near_k[k]]) > WELD_NEAR * WELD_NEAR) {
                continue;
            }
            /* Several faces may share that corner: insert it once. */
            for (h = 0; h < nh; h++) {
                if ((t - hit_t[h]) * len < 1.0 && (hit_t[h] - t) * len < 1.0) {
                    break;
                }
            }
            if (h < nh || nh >= WELD_MAX) {
                continue;
            }
            /* In order along the edge. */
            for (h = nh; h > 0 && hit_t[h - 1] > t; h--) {
                hit_t[h] = hit_t[h - 1];
            }
            hit_t[h] = t;
            nh++;
        }
        for (h = 0; h < nh && m < WELD_MAX; h++) {
            double t;

            /*
             * Where the edge passes the corner, not the corner itself,
             * which may sit up to WELD_NEAR off the line: that would
             * bend this edge against whatever lies along the rest of it.
             */
            t = hit_t[h];
            out[m].x = a->x + (int)(ex * t + (ex * t >= 0.0 ? 0.5 : -0.5));
            out[m].y = a->y + (int)(ey * t + (ey * t >= 0.0 ? 0.5 : -0.5));
            out[m].z = a->z + (int)(ez * t + (ez * t >= 0.0 ? 0.5 : -0.5));
            out[m].u = 0;
            out[m].v = 0;
            if (uv_u) {
                ou[m] = uv_u[i] + (int)((double)(uv_u[(i + 1) % n] - uv_u[i]) * t
                                        + (t >= 0.0 ? 0.5 : -0.5));
                ov[m] = uv_v[i] + (int)((double)(uv_v[(i + 1) % n] - uv_v[i]) * t
                                        + (t >= 0.0 ? 0.5 : -0.5));
            }
            m++;
        }
    }
    if (m == n) {
        return n;
    }
    for (i = 0; i < m; i++) {
        pts[i] = out[i];
        if (uv_u) {
            uv_u[i] = ou[i];
            uv_v[i] = ov[i];
        }
    }
    return m;
}

static void emit_fan(Mesh *mesh, int cap, const Pt *pts, int npts,
                     const int *uv_u, const int *uv_v, df_u16 tex)
{
    int apex;
    int k;
    int first;

    apex = fan_apex(pts, npts);
    first = mesh->ntris;
    for (k = 1; k < npts - 1; k++) {
        emit_tri(mesh, cap, pts, uv_u, uv_v, apex, (apex + k) % npts, (apex + k + 1) % npts,
                 (df_u16)(tex >> 7), (df_u16)(tex & 0x7f));
    }
    push_face(mesh, first);
}

Mesh *arch3d_load(const df_u8 *rec, df_u32 size, df_u32 id)
{
    int v25;
    df_i32 plane_count;
    df_i32 point_end;
    df_i32 point_off;
    df_i32 plane_off;
    int cap;
    int nface;
    Mesh *mesh;
    df_u32 p;
    int plane;

    if (!rec || size < 64) {
        return 0;
    }
    if (!(rec[0] == 'v' && rec[1] == '2' && rec[2] == '.' &&
          (rec[3] == '5' || rec[3] == '6' || rec[3] == '7'))) {
        return 0;
    }
    v25 = rec[3] == '5';
    /* The point table: df_ri32(rec + 4) points of 12 bytes from point_off. */
    point_end = df_ri32(rec + 48) + df_ri32(rec + 4) * 12;
    plane_count = df_ri32(rec + 8);
    point_off = df_ri32(rec + 48);
    plane_off = df_ri32(rec + 60);
    if (plane_count < 0 || plane_count > 4096) {
        return 0;
    }
    if (point_off < 64 || plane_off < 64 || (df_u32)plane_off >= size) {
        return 0;
    }
    /* Every face's real corners first, for weld. */
    g_corners = 0;
    p = (df_u32)plane_off;
    for (plane = 0; plane < plane_count; plane++) {
        Pt pts[WELD_MAX];
        int npts;
        df_u16 tex;
        int got;

        got = decode_plane(rec, size, &p, v25, point_off, point_end, id, pts, &npts, &tex);
        if (got < 0) {
            break;
        }
        if (got == 0) {
            continue;
        }
        npts = straighten(pts, npts, 0, 0);
        corners_add(pts, npts, plane);
    }
    /*
     * The old cap was the file length over 8, which reserved a triangle
     * for every few bytes of points. Count the faces, then allocate that.
     */
    cap = 0;
    nface = 0;
    p = (df_u32)plane_off;
    for (plane = 0; plane < plane_count; plane++) {
        Pt pts[WELD_MAX];
        int npts;
        df_u16 tex;
        int got;
        int nt;

        got = decode_plane(rec, size, &p, v25, point_off, point_end, id, pts, &npts, &tex);
        if (got < 0) {
            break;
        }
        if (got == 0) {
            continue;
        }
        npts = straighten(pts, npts, 0, 0);
        npts = weld(plane, pts, npts, 0, 0);
        nt = tri_count(pts, npts);
        cap += nt;
        if (nt > 0) {
            nface++;
        }
    }
    if (cap < 1) {
        return 0;
    }
    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    if (!mesh) {
        return 0;
    }
    mesh->id = id;
    mesh->ntris = 0;
    mesh->tris = (Tri *)mem_alloc((df_u32)cap * (df_u32)sizeof(Tri));
    if (!mesh->tris) {
        return 0;
    }
    mesh->face_cap = nface;
    if (nface > 0) {
        mesh->faces = (Face *)mem_alloc((df_u32)nface * (df_u32)sizeof(Face));
        if (!mesh->faces) {
            mesh->face_cap = 0;
        }
    }
    p = (df_u32)plane_off;
    g_counting = 1;
    for (plane = 0; plane < plane_count; plane++) {
        Pt pts[WELD_MAX];
        int uv_u[WELD_MAX];
        int uv_v[WELD_MAX];
        int npts;
        df_u16 tex;
        int got;

        got = decode_plane(rec, size, &p, v25, point_off, point_end, id, pts, &npts, &tex);
        if (got < 0) {
            break;
        }
        if (got == 0) {
            continue;
        }
        /* The file's texel rule reads its first three points, so before straighten. */
        plane_uv(pts, npts, uv_u, uv_v);
        npts = straighten(pts, npts, uv_u, uv_v);
        npts = weld(plane, pts, npts, uv_u, uv_v);
        emit_fan(mesh, cap, pts, npts, uv_u, uv_v, tex);
    }
    g_counting = 0;
    mesh_measure(mesh);
    return mesh;
}
