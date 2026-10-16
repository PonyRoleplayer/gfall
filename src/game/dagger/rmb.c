#include "game/dagger/rmb.h"
#include "game/dagger/arch3d.h"
#include "game/dagger/bsa.h"
#include "game/dagger/texture.h"
#include "engine/assets/loader.h"
#include "engine/core/mem.h"

/*
 * RMB fixed header is 6776 bytes, then one subrecord per building.
 * A subrecord is an exterior list and an interior list. BlockDataSizes
 * jumps past both, including the padding byte. Misc props follow the
 * subrecords. Model ids are objectId1 * 100 + objectId2. File yaw is
 * the same 2048-unit circle as an RDB, and place_model's negation is
 * instance_set_rotation with the angle flipped. Y in the file is down.
 *
 * A building sits at (X, 0, 4096 - Z) and its model at (X, -Y, Z) in
 * that frame. Props use (X, -Y - 4, Z + 4096). Ground is a 16 by 16
 * grid of 256-unit quads. Tile row 0 is the high-Z side, matching the
 * 4096 - Z flip, so a building and the dirt under it share one axis.
 */

#define FLD_BYTES 6776
#define FLD_SIZES 1603
#define FLD_GROUND 1739
#define RMB_SIDE 4096.0f
#define GROUND_TILE 256.0f
#define GROUND_TEX 64.0f
#define PROP_DROP 4.0f
#define ROT_STEP (DF_TAU / 2048.0f)
#define MAX_INST 2048
#define MAX_FLAT 2048
#define MAX_CACHE 512
#define EDITOR_ARCHIVE 199
#define MODEL_BYTES 66
#define FLAT_BYTES 17

typedef struct CacheEnt {
    df_u32 id;
    Mesh *mesh;
} CacheEnt;

static Mesh *cached_mesh(CacheEnt *cache, int *ncache, Bsa *arch, df_u32 id)
{
    int i;
    const BsaRec *rec;
    df_u8 *bytes;
    Mesh *mesh;

    for (i = 0; i < *ncache; i++) {
        if (cache[i].id == id) {
            return cache[i].mesh;
        }
    }
    if (*ncache >= MAX_CACHE) {
        return 0;
    }
    rec = bsa_find_id(arch, id);
    if (!rec) {
        return 0;
    }
    bytes = bsa_read(arch, rec);
    if (!bytes) {
        return 0;
    }
    {
        ArchLoad load;

        load.id = id;
        load.mesh = 0;
        if (!loader_call("arch3d", 0, bytes, rec->size, &load)) {
            return 0;
        }
        mesh = load.mesh;
    }
    if (!mesh || mesh->ntris < 1) {
        return 0;
    }
    cache[*ncache].id = id;
    cache[*ncache].mesh = mesh;
    (*ncache)++;
    return mesh;
}

static void place_child(Instance *in, float sub_yaw, float model_yaw,
                        float lx, float ly, float lz,
                        float ox, float oy, float oz)
{
    Instance parent;

    parent.x = ox;
    parent.y = oy;
    parent.z = oz;
    instance_set_rotation(&parent, 0.0f, df_wrap_tau(-sub_yaw), 0.0f);
    instance_point(&parent, lx, ly, lz, &in->x, &in->y, &in->z);
    instance_set_rotation(in, 0.0f, df_wrap_tau(-(sub_yaw + model_yaw)), 0.0f);
}

static void add_model(Block *block, CacheEnt *cache, int *ncache, Bsa *arch,
                      int id, float sub_yaw, float model_yaw,
                      float lx, float ly, float lz,
                      float ox, float oy, float oz)
{
    Mesh *mesh;
    Instance *in;

    if (id <= 0 || block->ninst >= MAX_INST) {
        return;
    }
    mesh = cached_mesh(cache, ncache, arch, (df_u32)id);
    if (!mesh) {
        return;
    }
    in = &block->inst[block->ninst];
    in->mesh = mesh;
    place_child(in, sub_yaw, model_yaw, lx, ly, lz, ox, oy, oz);
    block->ninst++;
    block->ntris += mesh->ntris;
}

static int model_id(const df_u8 *p)
{
    df_i16 id1;
    int id2;

    id1 = df_ri16(p);
    id2 = (int)p[2];
    if (id1 < 0) {
        return 0;
    }
    return (int)id1 * 100 + id2;
}

static void read_model(Block *block, CacheEnt *cache, int *ncache, Bsa *arch,
                       const df_u8 *p, float sub_yaw, float ox, float oy, float oz)
{
    float myaw;

    myaw = (float)df_ri16(p + 52) * ROT_STEP;
    add_model(block, cache, ncache, arch, model_id(p), sub_yaw, myaw,
              (float)df_ri32(p + 36), -(float)df_ri32(p + 40), (float)df_ri32(p + 44),
              ox, oy, oz);
}

static void add_flat(Block *block, const df_u8 *p, float ox, float oz)
{
    df_u16 bits;
    int archive;
    Flat *fl;

    if (block->nflat >= MAX_FLAT) {
        return;
    }
    bits = df_ru16(p + 12);
    archive = (int)(bits >> 7);
    if (archive == EDITOR_ARCHIVE) {
        return;
    }
    fl = &block->flat[block->nflat];
    fl->x = ox + (float)df_ri32(p);
    fl->y = -(float)df_ri32(p + 4);
    fl->z = oz + (float)df_ri32(p + 8);
    fl->w = 0.0f;
    fl->h = 0.0f;
    fl->file = (df_u16)archive;
    fl->image = (df_u16)(bits & 0x7f);
    fl->slot = 0xffff;
    block->nflat++;
}

/* Exterior lists only. The interior list is inside the same size span. */
static void read_exterior(Block *block, CacheEnt *cache, int *ncache, Bsa *arch,
                          const df_u8 *rmb, df_u32 end, df_u32 at,
                          float sx, float sub_yaw, float sz)
{
    int n3d;
    int nflat;
    int nsec;
    int npeople;
    int i;
    float oz;

    if (at + 17 > end) {
        return;
    }
    n3d = (int)rmb[at];
    nflat = (int)rmb[at + 1];
    nsec = (int)rmb[at + 2];
    npeople = (int)rmb[at + 3];
    at += 17;
    oz = RMB_SIDE - sz;
    for (i = 0; i < n3d && at + MODEL_BYTES <= end; i++) {
        read_model(block, cache, ncache, arch, rmb + at, sub_yaw, sx, 0.0f, oz);
        at += MODEL_BYTES;
    }
    for (i = 0; i < nflat && at + FLAT_BYTES <= end; i++) {
        add_flat(block, rmb + at, sx, oz);
        at += FLAT_BYTES;
    }
    if ((df_u32)nsec > 256 || at + (df_u32)nsec * 16u > end) {
        return;
    }
    at += (df_u32)nsec * 16u;
    for (i = 0; i < npeople && at + FLAT_BYTES <= end; i++) {
        add_flat(block, rmb + at, sx, oz);
        at += FLAT_BYTES;
    }
}

static void corner_uv(int corner, int rot, int flip, float *u, float *v)
{
    static const float cu[4] = {0.0f, GROUND_TEX, GROUND_TEX, 0.0f};
    static const float cv[4] = {0.0f, 0.0f, GROUND_TEX, GROUND_TEX};
    int k;

    k = (corner + rot) & 3;
    *u = cu[k];
    *v = cv[k];
    if (flip) {
        *u = GROUND_TEX - *u;
    }
}

static void ground_tri(Tri *tri, float x0, float z0, float x1, float z1, float x2, float z2,
                       int c0, int c1, int c2, int rot, int flip, int file, int image)
{
    tri->x[0] = x0;
    tri->y[0] = 0.0f;
    tri->z[0] = z0;
    tri->x[1] = x1;
    tri->y[1] = 0.0f;
    tri->z[1] = z1;
    tri->x[2] = x2;
    tri->y[2] = 0.0f;
    tri->z[2] = z2;
    corner_uv(c0, rot, flip, &tri->u[0], &tri->v[0]);
    corner_uv(c1, rot, flip, &tri->u[1], &tri->v[1]);
    corner_uv(c2, rot, flip, &tri->u[2], &tri->v[2]);
    tri->tex_file = (df_u16)file;
    tri->tex_image = (df_u16)image;
    tri->slot = 0xffff;
}

static void add_ground(Block *block, const df_u8 *tiles, int file)
{
    Mesh *mesh;
    Tri *tris;
    int count;
    int ty;
    int n;

    count = texture_record_count(file);
    if (count < 1) {
        return;
    }
    tris = (Tri *)mem_alloc(256u * 2u * (df_u32)sizeof(Tri));
    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    if (!tris || !mesh || block->ninst >= MAX_INST) {
        return;
    }
    n = 0;
    for (ty = 0; ty < 16; ty++) {
        int tx;

        for (tx = 0; tx < 16; tx++) {
            int bits;
            int record;
            float x0;
            float z0;
            float x1;
            float z1;

            bits = (int)tiles[ty * 16 + tx];
            record = bits & 0x3f;
            if (record >= count) {
                continue;
            }
            x0 = (float)tx * GROUND_TILE;
            z0 = (float)(15 - ty) * GROUND_TILE;
            x1 = x0 + GROUND_TILE;
            z1 = z0 + GROUND_TILE;
            ground_tri(&tris[n], x0, z0, x1, z0, x1, z1, 0, 1, 2,
                       (bits & 0x40) ? 1 : 0, (bits & 0x80) ? 1 : 0, file, record);
            n++;
            ground_tri(&tris[n], x0, z0, x1, z1, x0, z1, 0, 2, 3,
                       (bits & 0x40) ? 1 : 0, (bits & 0x80) ? 1 : 0, file, record);
            n++;
        }
    }
    if (n < 1) {
        return;
    }
    mesh->id = 0;
    mesh->tris = tris;
    mesh->ntris = n;
    block->inst[block->ninst].mesh = mesh;
    block->inst[block->ninst].x = 0.0f;
    block->inst[block->ninst].y = 0.0f;
    block->inst[block->ninst].z = 0.0f;
    instance_set_rotation(&block->inst[block->ninst], 0.0f, 0.0f, 0.0f);
    block->ninst++;
    block->ntris += n;
}

Block *rmb_load(const df_u8 *rmb, df_u32 size, Bsa *arch, int ground_file)
{
    Block *block;
    CacheEnt cache[MAX_CACHE];
    int ncache;
    int nsub;
    int i;
    df_u32 pos;

    if (!rmb || !arch || size < (df_u32)FLD_BYTES) {
        return 0;
    }
    nsub = (int)rmb[0];
    if (nsub > 32) {
        return 0;
    }
    block = (Block *)mem_alloc(sizeof(Block));
    if (!block) {
        return 0;
    }
    mem_zero(block, sizeof(Block));
    block->inst = (Instance *)mem_alloc(MAX_INST * (df_u32)sizeof(Instance));
    block->flat = (Flat *)mem_alloc(MAX_FLAT * (df_u32)sizeof(Flat));
    if (!block->inst || !block->flat) {
        return 0;
    }
    ncache = 0;
    mem_zero(cache, sizeof(cache));
    if (ground_file > 1) {
        add_ground(block, rmb + FLD_GROUND, ground_file);
    }
    pos = (df_u32)FLD_BYTES;
    for (i = 0; i < nsub; i++) {
        df_i32 span;
        const df_u8 *bp;
        float sx;
        float sz;
        float syaw;

        span = df_ri32(rmb + FLD_SIZES + i * 4);
        if (span < 17 || pos + (df_u32)span > size) {
            return block;
        }
        bp = rmb + 3 + i * 20;
        sx = (float)df_ri32(bp + 8);
        sz = (float)df_ri32(bp + 12);
        syaw = (float)df_ri32(bp + 16) * ROT_STEP;
        read_exterior(block, cache, &ncache, arch, rmb, pos + (df_u32)span, pos, sx, syaw, sz);
        pos += (df_u32)span;
    }
    for (i = 0; i < (int)rmb[1] && pos + MODEL_BYTES <= size; i++) {
        read_model(block, cache, &ncache, arch, rmb + pos, 0.0f, 0.0f, -PROP_DROP, RMB_SIDE);
        pos += MODEL_BYTES;
    }
    for (i = 0; i < (int)rmb[2] && pos + FLAT_BYTES <= size; i++) {
        add_flat(block, rmb + pos, 0.0f, RMB_SIDE);
        pos += FLAT_BYTES;
    }
    return block;
}
