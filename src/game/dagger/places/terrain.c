#include "game/dagger/places/terrain.h"
#include "game/dagger/formats/worldmap.h"
#include "engine/core/mem.h"

/* See terrain.h and docs/streaming.md (The land). */

#define CHUNK_SIDE (TERRAIN_CELLS * TERRAIN_CELL)
/* DFU's weights: the height map, its finer heights, the small noise. */
#define BASE_SCALE 8.0f
#define DETAIL_SCALE 4.0f
#define NOISE_SCALE 10.0f
/* The small noise's two waves, in tiles: our grid is 4 tiles, so no finer than 8. */
#define NOISE_LOW_TILES 16.0
#define NOISE_HIGH_TILES 8.0
/* Ground kinds: DFU's 3-octave weight at 0.05 a tile; dirt under, stone over. */
#define KIND_FREQ 0.05
#define KIND_DIRT 0.5f
#define KIND_STONE 0.95f
#define TILE_TEXELS 64.0f

static int ifloor(double v)
{
    int i;

    i = (int)v;
    return (double)i > v ? i - 1 : i;
}

static df_u32 hash2(int x, int z, df_u32 seed)
{
    df_u32 h;

    h = (df_u32)x * 0x8da6b343u ^ (df_u32)z * 0xd8163841u ^ seed * 0xcb1ab31fu;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

/* Gradient noise, 0..1 about 0.5, as Unity's Mathf.PerlinNoise is, of our own. */
static float noise01(double x, double z, df_u32 seed)
{
    static const float gx[8] = { 1.0f, -1.0f, 0.0f, 0.0f, 0.7071f, -0.7071f, 0.7071f, -0.7071f };
    static const float gz[8] = { 0.0f, 0.0f, 1.0f, -1.0f, 0.7071f, 0.7071f, -0.7071f, -0.7071f };
    int ix;
    int iz;
    float fx;
    float fz;
    float d[4];
    float u;
    float w;
    float n;
    int k;

    ix = ifloor(x);
    iz = ifloor(z);
    fx = (float)(x - (double)ix);
    fz = (float)(z - (double)iz);
    for (k = 0; k < 4; k++) {
        df_u32 h;
        float ox;
        float oz;

        h = hash2(ix + (k & 1), iz + (k >> 1), seed) & 7u;
        ox = fx - (float)(k & 1);
        oz = fz - (float)(k >> 1);
        d[k] = gx[h] * ox + gz[h] * oz;
    }
    u = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
    w = fz * fz * fz * (fz * (fz * 6.0f - 15.0f) + 10.0f);
    n = d[0] + (d[1] - d[0]) * u;
    n = n + (d[2] + (d[3] - d[2]) * u - n) * w;
    n = 0.5f + n;
    return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
}

static float catmull(float p0, float p1, float p2, float p3, float t)
{
    return 0.5f * (2.0f * p1 + (p2 - p0) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t
                   + (3.0f * (p1 - p2) + p3 - p0) * t * t * t);
}

/* The height map's byte at grid corner (gx, gz): the pixel whose south-east corner it is. */
static float base_at(int gx, int gz)
{
    return (float)worldmap_height(gx - 1, WORLDMAP_H - 1 - gz);
}

/* A finer height at third (fx, fz) of the world, 3 a pixel east and north. */
static float detail_at(int fx, int fz)
{
    df_u8 d[9];
    int px;
    int pz;

    px = fx >= 0 ? fx / 3 : -1;
    pz = fz >= 0 ? fz / 3 : -1;
    if (px < 0 || pz < 0 || px >= WORLDMAP_W || pz >= WORLDMAP_H) {
        return base_at(px + 1, pz);
    }
    worldmap_detail(px, WORLDMAP_H - 1 - pz, d);
    return (float)d[(fz - pz * 3) * 3 + (fx - px * 3)];
}

/* Catmull-Rom over a grid whose samples sit at (i + off) * step, read by get. */
static float bicubic(double wx, double wz, double step, double off, float (*get)(int, int))
{
    double fx;
    double fz;
    int x0;
    int z0;
    float col[4];
    int j;

    fx = wx / step - off;
    fz = wz / step - off;
    x0 = ifloor(fx);
    z0 = ifloor(fz);
    for (j = 0; j < 4; j++) {
        col[j] = catmull(get(x0 - 1, z0 - 1 + j), get(x0, z0 - 1 + j), get(x0 + 1, z0 - 1 + j),
                         get(x0 + 2, z0 - 1 + j), (float)(fx - (double)x0));
    }
    return catmull(col[0], col[1], col[2], col[3], (float)(fz - (double)z0));
}

float terrain_natural(double wx, double wz)
{
    double tx;
    double tz;
    float s;

    s = BASE_SCALE * bicubic(wx, wz, (double)WORLDMAP_PIXEL, 0.0, base_at);
    s += DETAIL_SCALE * bicubic(wx, wz, (double)WORLDMAP_PIXEL / 3.0, 0.5, detail_at);
    /* DFU's small bumps, low times high, at waves our grid can hold. */
    tx = wx / (double)TERRAIN_TILE;
    tz = wz / (double)TERRAIN_TILE;
    s += NOISE_SCALE * 0.5f * noise01(tx / NOISE_LOW_TILES, tz / NOISE_LOW_TILES, 11u)
        * 0.5f * noise01(tx / NOISE_HIGH_TILES, tz / NOISE_HIGH_TILES, 23u);
    return s < TERRAIN_OCEAN ? TERRAIN_OCEAN : s;
}

float terrain_scaled(double wx, double wz, const TerrainFlat *flat, int nflat)
{
    float s;
    int i;

    s = terrain_natural(wx, wz);
    for (i = 0; i < nflat; i++) {
        const TerrainFlat *f;
        double dx;
        double dz;
        float k;

        f = &flat[i];
        dx = wx < f->x0 ? f->x0 - wx : (wx > f->x1 ? wx - f->x1 : 0.0);
        dz = wz < f->z0 ? f->z0 - wz : (wz > f->z1 ? wz - f->z1 : 0.0);
        if (dx >= TERRAIN_BLEND || dz >= TERRAIN_BLEND) {
            continue;
        }
        /* 1 on the town, easing to 0 TERRAIN_BLEND out, smooth at both ends. */
        k = (float)((1.0 - dx / TERRAIN_BLEND) * (1.0 - dz / TERRAIN_BLEND));
        k = k * k * (3.0f - 2.0f * k);
        s += (f->s - s) * k;
    }
    return s;
}

float terrain_y(float scaled)
{
    return (scaled - TERRAIN_OCEAN) * TERRAIN_UNIT;
}

float terrain_level(double x0, double z0, double x1, double z1)
{
    float sum;
    int i;
    int j;

    sum = 0.0f;
    for (j = 0; j < 9; j++) {
        for (i = 0; i < 9; i++) {
            sum += terrain_natural(x0 + (x1 - x0) * (double)i / 8.0, z0 + (z1 - z0) * (double)j / 8.0);
        }
    }
    return sum / 81.0f;
}

/* ---- Tiles ---------------------------------------------------------- */

/*
 * DFU's marching table: a tile's four corner kinds (bit 0 of each: 1
 * for dirt and stone, 0 for water and grass) make a shape, their mean a
 * ring (water to dirt, dirt to grass, grass to stone, stone), and the
 * two pick the archive's transition record, turned (0x40) and flipped
 * (0x80) to the shape.
 */
static df_u8 g_lut[64];
static int g_lut_made;

static df_u8 lut_entry(int record, int turn, int flip)
{
    return (df_u8)(record | (turn ? 0x40 : 0) | (flip ? 0x80 : 0));
}

static void lut_range(int start, int end, int shape, int saddle, int reverse, int at)
{
    df_u8 *t;

    t = &g_lut[at];
    t[0] = lut_entry(start, 0, 0);
    t[15] = lut_entry(end, 0, 0);
    if (reverse) {
        t[1] = lut_entry(shape + 2, 1, 1);
        t[2] = lut_entry(shape + 2, 0, 0);
        t[3] = lut_entry(shape + 1, 1, 1);
        t[4] = lut_entry(shape + 2, 0, 1);
        t[5] = lut_entry(shape + 1, 0, 1);
        t[6] = lut_entry(saddle, 1, 0);
        t[7] = lut_entry(shape, 1, 1);
        t[8] = lut_entry(shape + 2, 1, 0);
        t[9] = lut_entry(saddle, 0, 0);
        t[10] = lut_entry(shape + 1, 0, 0);
        t[11] = lut_entry(shape, 0, 0);
        t[12] = lut_entry(shape + 1, 1, 0);
        t[13] = lut_entry(shape, 0, 1);
        t[14] = lut_entry(shape, 1, 0);
    } else {
        t[1] = lut_entry(shape, 1, 0);
        t[2] = lut_entry(shape, 0, 1);
        t[3] = lut_entry(shape + 1, 1, 0);
        t[4] = lut_entry(shape, 0, 0);
        t[5] = lut_entry(shape + 1, 0, 0);
        t[6] = lut_entry(saddle, 0, 0);
        t[7] = lut_entry(shape + 2, 1, 0);
        t[8] = lut_entry(shape, 1, 1);
        t[9] = lut_entry(saddle, 1, 0);
        t[10] = lut_entry(shape + 1, 0, 1);
        t[11] = lut_entry(shape + 2, 0, 1);
        t[12] = lut_entry(shape + 1, 1, 1);
        t[13] = lut_entry(shape + 2, 0, 0);
        t[14] = lut_entry(shape + 2, 1, 1);
    }
}

static void make_lut(void)
{
    if (g_lut_made) {
        return;
    }
    lut_range(0, 1, 5, 48, 0, 0);
    lut_range(2, 1, 10, 51, 1, 16);
    lut_range(2, 3, 15, 53, 0, 32);
    lut_range(3, 3, 15, 53, 1, 48);
    g_lut_made = 1;
}

/* The kind at world tile corner (tx, tz), of scaled height s. */
static int corner_kind(int tx, int tz, float s)
{
    float jitter;
    float w;
    double f;

    if (s <= TERRAIN_OCEAN + 0.01f) {
        return TERRAIN_WATER;
    }
    /* The beach, its line a little ragged. */
    jitter = (float)(hash2(tx, tz, 5u) & 1023u) * (3.0f / 1023.0f) - 1.5f;
    if (s <= TERRAIN_BEACH + jitter) {
        return TERRAIN_DIRT;
    }
    f = KIND_FREQ;
    w = 0.9f * noise01((double)tx * f, (double)tz * f, 417028u)
        + 0.36f * noise01((double)tx * f * 2.0, (double)tz * f * 2.0, 417029u)
        + 0.144f * noise01((double)tx * f * 4.0, (double)tz * f * 4.0, 417030u);
    if (w < KIND_DIRT) {
        return TERRAIN_DIRT;
    }
    return w > KIND_STONE ? TERRAIN_STONE : TERRAIN_GRASS;
}

static float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

/* Bilinear in a 5 x 5 grid [z][x], at grid units gx, gz (0..4). */
static float grid_at(const float g[TERRAIN_CELLS + 1][TERRAIN_CELLS + 1], float gx, float gz)
{
    int ix;
    int iz;
    float fx;
    float fz;

    ix = (int)gx;
    iz = (int)gz;
    ix = ix < 0 ? 0 : (ix > TERRAIN_CELLS - 1 ? TERRAIN_CELLS - 1 : ix);
    iz = iz < 0 ? 0 : (iz > TERRAIN_CELLS - 1 ? TERRAIN_CELLS - 1 : iz);
    fx = gx - (float)ix;
    fz = gz - (float)iz;
    return lerpf(lerpf(g[iz][ix], g[iz][ix + 1], fx), lerpf(g[iz + 1][ix], g[iz + 1][ix + 1], fx), fz);
}

void terrain_chunk(TerrainChunk *t, int cx, int cz, const TerrainFlat *flat, int nflat)
{
    float s[TERRAIN_CELLS + 1][TERRAIN_CELLS + 1];
    df_u8 kind[TERRAIN_TILES + 1][TERRAIN_TILES + 1];
    double x0;
    double z0;
    int i;
    int j;

    make_lut();
    t->cx = cx;
    t->cz = cz;
    x0 = (double)cx * CHUNK_SIDE;
    z0 = (double)cz * CHUNK_SIDE;
    for (j = 0; j <= TERRAIN_CELLS; j++) {
        for (i = 0; i <= TERRAIN_CELLS; i++) {
            s[j][i] = terrain_scaled(x0 + (double)(i * TERRAIN_CELL), z0 + (double)(j * TERRAIN_CELL),
                                     flat, nflat);
            t->y[j][i] = terrain_y(s[j][i]);
        }
    }
    t->covered = 0;
    for (j = 0; j < TERRAIN_CELLS; j++) {
        for (i = 0; i < TERRAIN_CELLS; i++) {
            double mx;
            double mz;
            int k;

            mx = x0 + ((double)i + 0.5) * TERRAIN_CELL;
            mz = z0 + ((double)j + 0.5) * TERRAIN_CELL;
            for (k = 0; k < nflat; k++) {
                if (mx > flat[k].x0 && mx < flat[k].x1 && mz > flat[k].z0 && mz < flat[k].z1) {
                    t->covered |= 1u << (j * TERRAIN_CELLS + i);
                }
            }
        }
    }
    /* Each tile corner's kind, its height bilinear in the grid (the beach and the sea follow it). */
    for (j = 0; j <= TERRAIN_TILES; j++) {
        for (i = 0; i <= TERRAIN_TILES; i++) {
            float sc;

            sc = grid_at((const float (*)[TERRAIN_CELLS + 1])s, (float)i / 4.0f, (float)j / 4.0f);
            kind[j][i] = (df_u8)corner_kind(cx * TERRAIN_TILES + i, cz * TERRAIN_TILES + j, sc);
        }
    }
    /* Each tile from its corners: j counts north, the tiles' rows from the north. */
    for (j = 0; j < TERRAIN_TILES; j++) {
        for (i = 0; i < TERRAIN_TILES; i++) {
            int b0;
            int b1;
            int b2;
            int b3;
            int shape;
            int ring;

            b0 = kind[j][i];
            b1 = kind[j][i + 1];
            b2 = kind[j + 1][i];
            b3 = kind[j + 1][i + 1];
            shape = (b0 & 1) | (b1 & 1) << 1 | (b2 & 1) << 2 | (b3 & 1) << 3;
            ring = (b0 + b1 + b2 + b3) >> 2;
            t->tiles[(TERRAIN_TILES - 1 - j) * TERRAIN_TILES + i] = g_lut[shape | ring << 4];
        }
    }
}

float terrain_height_in(const TerrainChunk *t, float lx, float lz)
{
    return grid_at(t->y, lx / (float)TERRAIN_CELL, lz / (float)TERRAIN_CELL);
}

float terrain_slope_in(const TerrainChunk *t, float lx, float lz)
{
    float dx;
    float dz;
    int ix;
    int iz;

    ix = (int)(lx / (float)TERRAIN_CELL);
    iz = (int)(lz / (float)TERRAIN_CELL);
    ix = ix < 0 ? 0 : (ix > TERRAIN_CELLS - 1 ? TERRAIN_CELLS - 1 : ix);
    iz = iz < 0 ? 0 : (iz > TERRAIN_CELLS - 1 ? TERRAIN_CELLS - 1 : iz);
    dx = (t->y[iz][ix + 1] - t->y[iz][ix] + t->y[iz + 1][ix + 1] - t->y[iz + 1][ix]) * 0.5f;
    dz = (t->y[iz + 1][ix] - t->y[iz][ix] + t->y[iz + 1][ix + 1] - t->y[iz][ix + 1]) * 0.5f;
    dx = dx < 0.0f ? -dx : dx;
    dz = dz < 0.0f ? -dz : dz;
    return (dx > dz ? dx : dz) / (float)TERRAIN_CELL;
}

/* ---- Meshes --------------------------------------------------------- */

typedef struct Leaf {
    /* South-west tile corner (tile units from the chunk's south-west), and its sides in tiles. */
    int tx;
    int tz;
    int nx;
    int nz;
    int tile;
    int slot;
} Leaf;

static float root(float v)
{
    __asm__ __volatile__("fsqrt" : "=t"(v) : "0"(v));
    return v;
}

/*
 * One triangle as its own face, its normal from its corners. up 1: the
 * normal must point up (ground), else it must point along (ox, oz) (a
 * skirt, outward); the corners are swapped when it does not.
 */
static void put_tri(Mesh *m, const float *p, const float *uv, int slot, int up, float ox, float oz)
{
    Tri *tri;
    Face *f;
    float ax, ay, az, bx, by, bz, nx, ny, nz, l;
    int order[3];
    int k;

    ax = p[3] - p[0];
    ay = p[4] - p[1];
    az = p[5] - p[2];
    bx = p[6] - p[0];
    by = p[7] - p[1];
    bz = p[8] - p[2];
    /* cross(c - a, b - a): up for rmb.c's ground winding. */
    nx = by * az - bz * ay;
    ny = bz * ax - bx * az;
    nz = bx * ay - by * ax;
    order[0] = 0;
    order[1] = 1;
    order[2] = 2;
    if ((up && ny < 0.0f) || (!up && nx * ox + nz * oz < 0.0f)) {
        order[1] = 2;
        order[2] = 1;
        nx = -nx;
        ny = -ny;
        nz = -nz;
    }
    l = root(nx * nx + ny * ny + nz * nz);
    if (l <= 0.0f) {
        return;
    }
    tri = &m->tris[m->ntris];
    for (k = 0; k < 3; k++) {
        tri->x[k] = p[order[k] * 3];
        tri->y[k] = p[order[k] * 3 + 1];
        tri->z[k] = p[order[k] * 3 + 2];
        tri->u[k] = uv[order[k] * 2];
        tri->v[k] = uv[order[k] * 2 + 1];
    }
    tri->tex_file = 0;
    tri->tex_image = 0;
    tri->slot = (df_u16)slot;
    f = &m->faces[m->nfaces];
    f->nx = nx / l;
    f->ny = ny / l;
    f->nz = nz / l;
    f->first = m->ntris;
    f->ntris = 1;
    f->fan = 0;
    m->ntris++;
    m->nfaces++;
}

static Mesh *new_mesh(int tris)
{
    Mesh *m;

    m = (Mesh *)mem_alloc(sizeof(Mesh));
    if (!m) {
        return 0;
    }
    mem_zero(m, sizeof(Mesh));
    m->tris = (Tri *)mem_alloc((df_u32)tris * (df_u32)sizeof(Tri));
    m->faces = (Face *)mem_alloc((df_u32)tris * (df_u32)sizeof(Face));
    if (!m->tris || !m->faces) {
        return 0;
    }
    m->face_cap = tris;
    return m;
}

/* The mesh's slots, once each, as its slot keys (texcache's nearest-use notes). */
static void key_slots(Mesh *m)
{
    df_u16 keys[64];
    int n;
    int t;

    n = 0;
    for (t = 0; t < m->ntris; t++) {
        int k;

        for (k = 0; k < n && keys[k] != m->tris[t].slot; k++) {
        }
        if (k == n && n < 64) {
            keys[n++] = m->tris[t].slot;
        }
    }
    m->slot_keys = n ? (df_u16 *)mem_alloc((df_u32)n * (df_u32)sizeof(df_u16)) : 0;
    for (t = 0; m->slot_keys && t < n; t++) {
        m->slot_keys[t] = keys[t];
    }
    m->nslot_keys = m->slot_keys ? n : 0;
    m->have_keys = 1;
}

static int tile_at(const TerrainChunk *t, int tx, int tz)
{
    return t->tiles[(TERRAIN_TILES - 1 - tz) * TERRAIN_TILES + tx];
}

/*
 * A tile corner's texels over a leaf, as rmb.c's corner_uv turns a
 * tile's: lx east and lz north, in tiles from the leaf's corner. The
 * tile repeats every 64 texels, so a whole number of tiles added to v
 * changes nothing; ext (the leaf's longer side) keeps it positive.
 */
static void leaf_uv(int tile, int ext, float lx, float lz, float *u, float *v)
{
    float su;
    float sv;
    float fe;
    int turn;
    int flip;

    fe = (float)ext;
    turn = (tile & 0x40) != 0;
    flip = (tile & 0x80) != 0;
    if (turn && flip) {
        su = fe - lz;
        sv = lx;
    } else if (turn) {
        su = lz;
        sv = fe - lx;
    } else if (flip) {
        su = fe - lx;
        sv = fe - lz;
    } else {
        su = lx;
        sv = lz;
    }
    *u = su * TILE_TEXELS;
    *v = (fe - sv) * TILE_TEXELS;
}

/*
 * A cell's tiles as rectangles of one tile each, greedily: the first
 * tile not taken, as wide as its row runs, as tall as whole rows of
 * that width run. Inside a cell the height is bilinear, so a rectangle's
 * edge is straight and a neighbour's corner on it lies on it: no crack.
 */
static void cell_leaves(const TerrainChunk *t, int ci, int cj, Leaf *leaf, int *nleaf)
{
    df_u8 taken[4][4];
    int x;
    int z;

    mem_zero(taken, sizeof(taken));
    for (z = 0; z < 4; z++) {
        for (x = 0; x < 4; x++) {
            int tile;
            int w;
            int h;
            int i;
            int j;
            Leaf *l;

            if (taken[z][x]) {
                continue;
            }
            tile = tile_at(t, ci * 4 + x, cj * 4 + z);
            for (w = 1; x + w < 4 && !taken[z][x + w] && tile_at(t, ci * 4 + x + w, cj * 4 + z) == tile; w++) {
            }
            for (h = 1; z + h < 4; h++) {
                for (i = 0; i < w && !taken[z + h][x + i]
                     && tile_at(t, ci * 4 + x + i, cj * 4 + z + h) == tile; i++) {
                }
                if (i < w) {
                    break;
                }
            }
            for (j = 0; j < h; j++) {
                for (i = 0; i < w; i++) {
                    taken[z + j][x + i] = 1;
                }
            }
            l = &leaf[(*nleaf)++];
            l->tx = ci * 4 + x;
            l->tz = cj * 4 + z;
            l->nx = w;
            l->nz = h;
            l->tile = tile;
            l->slot = 0;
        }
    }
}

Mesh *terrain_near_mesh(const TerrainChunk *t, int (*slot_of)(int record, void *user), void *user)
{
    Leaf leaf[TERRAIN_TILES * TERRAIN_TILES];
    int nleaf;
    Mesh *m;
    int ci;
    int cj;
    int i;

    nleaf = 0;
    for (cj = 0; cj < TERRAIN_CELLS; cj++) {
        for (ci = 0; ci < TERRAIN_CELLS; ci++) {
            if (!(t->covered & (1u << (cj * TERRAIN_CELLS + ci)))) {
                cell_leaves(t, ci, cj, leaf, &nleaf);
            }
        }
    }
    if (nleaf < 1) {
        return 0;
    }
    /* Each leaf's picture, then the leaves in order of it: one bind a run. */
    for (i = 0; i < nleaf; i++) {
        leaf[i].slot = slot_of(leaf[i].tile & 0x3f, user);
    }
    for (i = 1; i < nleaf; i++) {
        Leaf l;
        int j;

        l = leaf[i];
        for (j = i; j > 0 && leaf[j - 1].slot > l.slot; j--) {
            leaf[j] = leaf[j - 1];
        }
        leaf[j] = l;
    }
    m = new_mesh(nleaf * 2);
    if (!m) {
        return 0;
    }
    for (i = 0; i < nleaf; i++) {
        const Leaf *l;
        float p[4][3];
        float uv[4][2];
        float tri[9];
        float tuv[6];
        int k;

        l = &leaf[i];
        if (l->slot < 0) {
            continue;
        }
        /* Corners 0 south-west, 1 south-east, 2 north-east, 3 north-west. */
        for (k = 0; k < 4; k++) {
            float lx;
            float lz;
            float gx;
            float gz;

            lx = (float)((k == 1 || k == 2) ? l->nx : 0);
            lz = (float)(k >= 2 ? l->nz : 0);
            p[k][0] = ((float)l->tx + lx) * (float)TERRAIN_TILE;
            p[k][2] = ((float)l->tz + lz) * (float)TERRAIN_TILE;
            /* Bilinear in the leaf's own cell, so an edge between cells is straight. */
            gx = p[k][0] / (float)TERRAIN_CELL;
            gz = p[k][2] / (float)TERRAIN_CELL;
            {
                int ix;
                int iz;

                ix = l->tx / 4;
                iz = l->tz / 4;
                p[k][1] = lerpf(lerpf(t->y[iz][ix], t->y[iz][ix + 1], gx - (float)ix),
                                lerpf(t->y[iz + 1][ix], t->y[iz + 1][ix + 1], gx - (float)ix),
                                gz - (float)iz);
            }
            leaf_uv(l->tile, l->nx > l->nz ? l->nx : l->nz, lx, lz, &uv[k][0], &uv[k][1]);
        }
        /* As rmb.c's ground: 0 1 2, then 0 2 3. */
        for (k = 0; k < 2; k++) {
            int a;
            int b;
            int c;
            int q;
            int idx[3];

            a = 0;
            b = k == 0 ? 1 : 2;
            c = k == 0 ? 2 : 3;
            idx[0] = a;
            idx[1] = b;
            idx[2] = c;
            for (q = 0; q < 3; q++) {
                tri[q * 3] = p[idx[q]][0];
                tri[q * 3 + 1] = p[idx[q]][1];
                tri[q * 3 + 2] = p[idx[q]][2];
                tuv[q * 2] = uv[idx[q]][0];
                tuv[q * 2 + 1] = uv[idx[q]][1];
            }
            put_tri(m, tri, tuv, l->slot, 1, 0.0f, 0.0f);
        }
    }
    key_slots(m);
    mesh_measure(m);
    return m->ntris > 0 ? m : 0;
}

Mesh *terrain_far_mesh(const TerrainChunk *t, int slot, int side)
{
    Mesh *m;
    float scale;
    int ci;
    int cj;
    int e;

    m = new_mesh(2 * 4 + 2 * 8);
    if (!m) {
        return 0;
    }
    scale = (float)side / (float)CHUNK_SIDE;
    for (cj = 0; cj < 2; cj++) {
        for (ci = 0; ci < 2; ci++) {
            float p[4][3];
            float uv[4][2];
            float tri[9];
            float tuv[6];
            int k;

            /* A 2048 cell is a town's when its middle is (towns lie on 2048s). */
            if (t->covered & (1u << ((cj * 2 + 1) * TERRAIN_CELLS + ci * 2 + 1))
                && t->covered & (1u << (cj * 2 * TERRAIN_CELLS + ci * 2))) {
                continue;
            }
            for (k = 0; k < 4; k++) {
                int gx;
                int gz;

                gx = ci * 2 + ((k == 1 || k == 2) ? 2 : 0);
                gz = cj * 2 + (k >= 2 ? 2 : 0);
                p[k][0] = (float)(gx * TERRAIN_CELL);
                p[k][1] = t->y[gz][gx];
                p[k][2] = (float)(gz * TERRAIN_CELL);
                uv[k][0] = p[k][0] * scale;
                uv[k][1] = ((float)CHUNK_SIDE - p[k][2]) * scale;
            }
            for (k = 0; k < 2; k++) {
                int idx[3];
                int q;

                idx[0] = 0;
                idx[1] = k == 0 ? 1 : 2;
                idx[2] = k == 0 ? 2 : 3;
                for (q = 0; q < 3; q++) {
                    tri[q * 3] = p[idx[q]][0];
                    tri[q * 3 + 1] = p[idx[q]][1];
                    tri[q * 3 + 2] = p[idx[q]][2];
                    tuv[q * 2] = uv[idx[q]][0];
                    tuv[q * 2 + 1] = uv[idx[q]][1];
                }
                put_tri(m, tri, tuv, slot, 1, 0.0f, 0.0f);
            }
        }
    }
    /*
     * Skirts: each edge's two 2048 lengths hang down by twice how far the
     * fine grid's middle point stands off the coarse line, and a little:
     * a near neighbour's edge, bent there, then never shows sky under it.
     */
    for (e = 0; e < 4; e++) {
        int s;

        for (s = 0; s < 2; s++) {
            int ax, az, bx, bz, mx, mz;
            float depth;
            float off;
            float p[4][3];
            float uv[4][2];
            float tri[9];
            float tuv[6];
            float ox;
            float oz;
            int cell;
            int k;

            /* Grid corners along edge e: 0 south, 1 east, 2 north, 3 west. */
            if (e == 0 || e == 2) {
                az = bz = mz = e == 0 ? 0 : TERRAIN_CELLS;
                ax = s * 2;
                bx = ax + 2;
                mx = ax + 1;
                cell = (e == 0 ? 0 : TERRAIN_CELLS - 1) * TERRAIN_CELLS + mx;
            } else {
                ax = bx = mx = e == 1 ? TERRAIN_CELLS : 0;
                az = s * 2;
                bz = az + 2;
                mz = az + 1;
                cell = mz * TERRAIN_CELLS + (e == 1 ? TERRAIN_CELLS - 1 : 0);
            }
            if (t->covered & (1u << cell)) {
                continue;
            }
            off = t->y[mz][mx] - (t->y[az][ax] + t->y[bz][bx]) * 0.5f;
            off = off < 0.0f ? -off : off;
            depth = 64.0f + 2.0f * off;
            ox = e == 1 ? 1.0f : (e == 3 ? -1.0f : 0.0f);
            oz = e == 2 ? 1.0f : (e == 0 ? -1.0f : 0.0f);
            for (k = 0; k < 4; k++) {
                int gx;
                int gz;

                gx = k == 0 || k == 3 ? ax : bx;
                gz = k == 0 || k == 3 ? az : bz;
                p[k][0] = (float)(gx * TERRAIN_CELL);
                p[k][1] = t->y[gz][gx] - (k >= 2 ? depth : 0.0f);
                p[k][2] = (float)(gz * TERRAIN_CELL);
                uv[k][0] = p[k][0] * scale;
                uv[k][1] = ((float)CHUNK_SIDE - p[k][2]) * scale;
            }
            for (k = 0; k < 2; k++) {
                int idx[3];
                int q;

                idx[0] = 0;
                idx[1] = k == 0 ? 1 : 2;
                idx[2] = k == 0 ? 2 : 3;
                for (q = 0; q < 3; q++) {
                    tri[q * 3] = p[idx[q]][0];
                    tri[q * 3 + 1] = p[idx[q]][1];
                    tri[q * 3 + 2] = p[idx[q]][2];
                    tuv[q * 2] = uv[idx[q]][0];
                    tuv[q * 2 + 1] = uv[idx[q]][1];
                }
                put_tri(m, tri, tuv, slot, 0, ox, oz);
            }
        }
    }
    if (m->ntris < 1) {
        return 0;
    }
    key_slots(m);
    mesh_measure(m);
    return m;
}
