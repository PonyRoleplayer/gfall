#include "game/dagger/arch3d.h"
#include "engine/core/mem.h"

#define MAX_FACE 24
#define POINT_DIV 256.0f
#define UV_DIV 16.0f

static int g_uv_fail;

int arch3d_uv_failures(void)
{
    return g_uv_fail;
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

static int corners_of(const Pt *pts, int n, int *out)
{
    int count;
    int point;

    count = 0;
    for (point = 0; point < n; point++) {
        int i0, i1, i2, corner;
        float l0x, l0y, l0z, l1x, l1y, l1z;
        float cx, cy, cz, l0, l1, cross2;

        if (point < n - 2) {
            i0 = point;
            i1 = point + 1;
            i2 = point + 2;
            corner = point + 1;
        } else if (point < n - 1) {
            i0 = point;
            i1 = point + 1;
            i2 = 0;
            corner = point + 1;
        } else {
            i0 = point;
            i1 = 0;
            i2 = 1;
            corner = 0;
        }
        l0x = (float)(pts[i1].x - pts[i0].x);
        l0y = (float)(pts[i1].y - pts[i0].y);
        l0z = (float)(pts[i1].z - pts[i0].z);
        l1x = (float)(pts[i2].x - pts[i0].x);
        l1y = (float)(pts[i2].y - pts[i0].y);
        l1z = (float)(pts[i2].z - pts[i0].z);
        cx = l0y * l1z - l0z * l1y;
        cy = l0z * l1x - l0x * l1z;
        cz = l0x * l1y - l0y * l1x;
        l0 = l0x * l0x + l0y * l0y + l0z * l0z;
        l1 = l1x * l1x + l1y * l1y + l1z * l1z;
        cross2 = cx * cx + cy * cy + cz * cz;
        /* About 0.6 degrees. Drops points that sit on the previous edge. */
        if (l0 > 1.0f && l1 > 1.0f && cross2 > l0 * l1 * 1.0e-4f) {
            out[count++] = corner;
        }
    }
    return count;
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

Mesh *arch3d_load(const df_u8 *rec, df_u32 size, df_u32 id)
{
    int v25;
    df_i32 plane_count;
    df_i32 point_off;
    df_i32 plane_off;
    int cap;
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
    plane_count = df_ri32(rec + 8);
    point_off = df_ri32(rec + 48);
    plane_off = df_ri32(rec + 60);
    if (plane_count < 0 || plane_count > 4096) {
        return 0;
    }
    if (point_off < 64 || plane_off < 64 || (df_u32)plane_off >= size) {
        return 0;
    }
    cap = (int)(size / 8) + 4;
    if (cap < 4) {
        cap = 4;
    }
    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    if (!mesh) {
        return 0;
    }
    mesh->id = id;
    mesh->tris = (Tri *)mem_alloc((df_u32)cap * (df_u32)sizeof(Tri));
    if (!mesh->tris) {
        return 0;
    }
    p = (df_u32)plane_off;
    for (plane = 0; plane < plane_count; plane++) {
        int npts;
        df_u16 tex;
        Pt pts[MAX_FACE];
        int uv_u[MAX_FACE];
        int uv_v[MAX_FACE];
        int corners[MAX_FACE];
        int nc;
        int k;
        int bad;

        if (p + 8 > size) {
            break;
        }
        npts = rec[p];
        tex = df_ru16(rec + p + 2);
        p += 8;
        if (npts < 3 || npts > MAX_FACE) {
            if (npts > MAX_FACE || p + (df_u32)npts * 8 > size) {
                break;
            }
            p += (df_u32)npts * 8;
            continue;
        }
        if (p + (df_u32)npts * 8 > size) {
            break;
        }
        bad = 0;
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
            if (v25) {
                pp = point_off + off * 12;
            } else {
                pp = point_off + off;
            }
            if (pp < 0 || (df_u32)pp + 12 > size) {
                bad = 1;
                break;
            }
            pts[k].x = df_ri32(rec + pp);
            pts[k].y = df_ri32(rec + pp + 4);
            pts[k].z = df_ri32(rec + pp + 8);
            pts[k].u = u;
            pts[k].v = v;
        }
        if (bad) {
            break;
        }
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
        nc = corners_of(pts, npts, corners);
        if (nc < 3) {
            for (k = 0; k < npts; k++) {
                corners[k] = k;
            }
            nc = npts;
        }
        for (k = 1; k < nc - 1; k++) {
            emit_tri(mesh, cap, pts, uv_u, uv_v, corners[0], corners[k], corners[k + 1],
                     (df_u16)(tex >> 7), (df_u16)(tex & 0x7f));
        }
    }
    return mesh;
}
