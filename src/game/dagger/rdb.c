#include "game/dagger/rdb.h"
#include "game/dagger/arch3d.h"
#include "game/dagger/bsa.h"
#include "engine/assets/loader.h"
#include "engine/core/mem.h"

#define MAX_INST 2048
#define MAX_FLAT 2048
#define MAX_MARK 256
#define MAX_LIGHT 1024
#define EDITOR_ARCHIVE 199
/* TEXTURE.210 holds the torches, candles, and braziers: flats that give light. */
#define LIGHTS_ARCHIVE 210
#define ROT_STEP (DF_TAU / 2048.0f)

/* Used when a block has none of that kind, so the pointer is never null. */
static Instance g_none_inst;
static ModelAct g_none_act;
static Flat g_none_flat;
static Marker g_none_mark;

static int parse_id(const df_u8 *s, df_u32 *id)
{
    int i;
    df_u32 v;

    if (s[0] == 0xff) {
        return 0;
    }
    v = 0;
    for (i = 0; i < 5; i++) {
        if (s[i] == ' ' || s[i] == 0) {
            break;
        }
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
        v = v * 10 + (df_u32)(s[i] - '0');
    }
    if (i == 0) {
        return 0;
    }
    *id = v;
    return 1;
}

/* Where a mesh read by arch_cached is allocated: 0 is the current heap. */
static MemHeap *g_arch_heap;

void arch_cache_heap(MemHeap *h)
{
    g_arch_heap = h;
}

Mesh *arch_cached(ArchEnt *cache, int *ncache, Bsa *arch, df_u32 id)
{
    int i;
    const BsaRec *rec;
    df_u8 *bytes;
    Mesh *mesh;

    if (!cache || !ncache || !arch) {
        return 0;
    }
    for (i = 0; i < *ncache; i++) {
        if (cache[i].id == id) {
            return cache[i].mesh;
        }
    }
    if (*ncache >= ARCH_CACHE) {
        return 0;
    }
    rec = bsa_find_id(arch, id);
    if (!rec) {
        return 0;
    }
    bytes = bsa_read_scratch(arch, rec, BSA_SCRATCH_MODEL);
    if (!bytes) {
        return 0;
    }
    {
        ArchLoad load;
        MemHeap *was;
        int ok;

        load.id = id;
        load.mesh = 0;
        was = g_arch_heap ? mem_heap_use(g_arch_heap) : 0;
        ok = loader_call("arch3d", 0, bytes, rec->size, &load);
        if (g_arch_heap) {
            mem_heap_use(was);
        }
        if (!ok) {
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

int block_reserve(Block *block, int ninst, int nflat, int nmark)
{
    if (!block) {
        return 0;
    }
    if (ninst < 0) {
        ninst = 0;
    }
    if (nflat < 0) {
        nflat = 0;
    }
    if (nmark < 0) {
        nmark = 0;
    }
    if (ninst > MAX_INST) {
        ninst = MAX_INST;
    }
    if (nflat > MAX_FLAT) {
        nflat = MAX_FLAT;
    }
    if (nmark > MAX_MARK) {
        nmark = MAX_MARK;
    }
    block->inst_cap = ninst;
    block->flat_cap = nflat;
    block->mark_cap = nmark;
    block->inst = ninst
        ? (Instance *)mem_alloc((df_u32)ninst * (df_u32)sizeof(Instance))
        : &g_none_inst;
    block->flat = nflat
        ? (Flat *)mem_alloc((df_u32)nflat * (df_u32)sizeof(Flat))
        : &g_none_flat;
    block->marker = nmark
        ? (Marker *)mem_alloc((df_u32)nmark * (df_u32)sizeof(Marker))
        : &g_none_mark;
    block->act = ninst
        ? (ModelAct *)mem_alloc((df_u32)ninst * (df_u32)sizeof(ModelAct))
        : &g_none_act;
    if (!block->inst || !block->flat || !block->marker || !block->act) {
        return 0;
    }
    if (ninst) {
        int i;

        mem_zero(block->act, (df_u32)ninst * (df_u32)sizeof(ModelAct));
        for (i = 0; i < ninst; i++) {
            block->act[i].record = -1;
            block->act[i].next = -1;
        }
    }
    return 1;
}

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

/* File angles are the opposite turn, about Y then X then Z. */
static void place_model(Instance *in, float rx, float ry, float rz)
{
    float x, y, z;

    rx = df_wrap_tau(-rx);
    ry = df_wrap_tau(-ry);
    rz = df_wrap_tau(-rz);

    x = 1.0f;
    y = 0.0f;
    z = 0.0f;
    rot_y(ry, &x, &z);
    rot_x(rx, &y, &z);
    rot_z(rz, &x, &y);
    in->m[0] = x;
    in->m[3] = y;
    in->m[6] = z;

    x = 0.0f;
    y = 1.0f;
    z = 0.0f;
    rot_y(ry, &x, &z);
    rot_x(rx, &y, &z);
    rot_z(rz, &x, &y);
    in->m[1] = x;
    in->m[4] = y;
    in->m[7] = z;

    x = 0.0f;
    y = 0.0f;
    z = 1.0f;
    rot_y(ry, &x, &z);
    rot_x(rx, &y, &z);
    rot_z(rz, &x, &y);
    in->m[2] = x;
    in->m[5] = y;
    in->m[8] = z;
}

/*
 * The model resource past its turn and model index (Daggerfall Unity's
 * RdbModelResource): the trigger and starting lock (u32 at 14), a sound
 * id (18), and the offset of an action record in the block file (i32 at
 * 19), which is the axis, time (u16, twentieths of a second), size (u16),
 * the next object's offset (i32) and the action's flags. The list of
 * models gives each a five-digit id and three letters after it.
 */
static void read_act(ModelAct *act, const df_u8 *rdb, df_u32 size, df_u32 at, df_u32 res,
                     df_u16 mindex, df_u32 id)
{
    const df_u8 *ref;
    df_i32 ao;

    act->pos = at;
    act->id = id;
    ref = rdb + 20 + (df_u32)mindex * 8;
    act->tag[0] = (char)ref[5];
    act->tag[1] = (char)ref[6];
    act->tag[2] = (char)ref[7];
    act->tag[3] = 0;
    act->next = -1;
    if (res + 23 > size) {
        return;
    }
    act->trigger = df_ru32(rdb + res + 14);
    act->sound = rdb[res + 18];
    ao = df_ri32(rdb + res + 19);
    if (ao > 0 && (df_u32)ao + 10 <= size) {
        act->axis = rdb[ao];
        act->duration = df_ru16(rdb + ao + 1);
        act->magnitude = df_ru16(rdb + ao + 3);
        act->next = df_ri32(rdb + ao + 5);
        act->flags = rdb[ao + 9];
    }
}

/* Upper bound on what the walk below will place. A missing mesh still counts. */
static void rdb_tally(const df_u8 *rdb, df_u32 size, df_u32 width, df_u32 height,
                      df_u32 root, int *ninst, int *nflat, int *nmark, int *nlight)
{
    df_u32 cell;

    *ninst = 0;
    *nflat = 0;
    *nmark = 0;
    *nlight = 0;
    for (cell = 0; cell < width * height; cell++) {
        df_i32 off;
        int guard_cells;

        off = df_ri32(rdb + root + cell * 4);
        guard_cells = 0;
        while (off >= 0 && guard_cells < 4096) {
            df_u32 at;
            df_u8 type;
            df_u32 res;
            df_u16 mindex;
            df_u16 bitfield;
            df_u32 id;

            guard_cells++;
            at = (df_u32)off;
            if (at + 25 > size) {
                break;
            }
            type = rdb[at + 20];
            res = df_ru32(rdb + at + 21);
            if (type == 0x01 && res + 14 <= size && *ninst < MAX_INST) {
                mindex = df_ru16(rdb + res + 12);
                if (mindex < 750 && parse_id(rdb + 20 + (df_u32)mindex * 8, &id)) {
                    (*ninst)++;
                }
            } else if (type == 0x02 && res + 10 <= size && *nlight < MAX_LIGHT) {
                (*nlight)++;
            } else if (type == 0x03 && res + 11 <= size) {
                bitfield = df_ru16(rdb + res);
                if ((int)(bitfield >> 7) == EDITOR_ARCHIVE) {
                    if (*nmark < MAX_MARK) {
                        (*nmark)++;
                    }
                } else if (*nflat < MAX_FLAT) {
                    (*nflat)++;
                }
            }
            off = df_ri32(rdb + at);
        }
    }
}

Block *rdb_load(const df_u8 *rdb, df_u32 size, Bsa *arch, ArchEnt *cache, int *ncache)
{
    df_u32 width;
    df_u32 height;
    df_u32 root;
    Block *block;
    int want_inst;
    int want_flat;
    int want_mark;
    int want_light;
    df_u32 cell;
    int guard_cells;

    if (!rdb || !arch || !cache || !ncache || size < 20 + 750 * 8) {
        return 0;
    }
    width = df_ru32(rdb + 4);
    height = df_ru32(rdb + 8);
    root = df_ru32(rdb + 12);
    if (width == 0 || height == 0 || width > 64 || height > 64) {
        return 0;
    }
    if (root > size || width * height * 4 > size - root) {
        return 0;
    }
    rdb_tally(rdb, size, width, height, root, &want_inst, &want_flat, &want_mark, &want_light);
    block = (Block *)mem_alloc(sizeof(Block));
    if (!block) {
        return 0;
    }
    if (!block_reserve(block, want_inst, want_flat, want_mark)) {
        return 0;
    }
    if (want_light > 0) {
        block->light = (Light *)mem_alloc((df_u32)want_light * (df_u32)sizeof(Light));
        if (!block->light) {
            return 0;
        }
    }
    for (cell = 0; cell < width * height; cell++) {
        df_i32 off;

        off = df_ri32(rdb + root + cell * 4);
        guard_cells = 0;
        while (off >= 0 && guard_cells < 4096) {
            df_u32 at;
            df_u8 type;
            df_u32 res;
            df_i32 xr, yr, zr;
            df_u16 mindex;
            df_u16 bitfield;
            df_u32 id;
            int archive;
            int record;
            Mesh *mesh;
            Instance *in;
            Flat *fl;

            guard_cells++;
            at = (df_u32)off;
            if (at + 25 > size) {
                break;
            }
            type = rdb[at + 20];
            res = df_ru32(rdb + at + 21);
            if (type == 0x01 && res + 14 <= size && block->ninst < block->inst_cap) {
                xr = df_ri32(rdb + res);
                yr = df_ri32(rdb + res + 4);
                zr = df_ri32(rdb + res + 8);
                mindex = df_ru16(rdb + res + 12);
                if (mindex < 750 && parse_id(rdb + 20 + (df_u32)mindex * 8, &id)) {
                    mesh = arch_cached(cache, ncache, arch, id);
                    if (mesh) {
                        read_act(&block->act[block->ninst], rdb, size, at, res, mindex, id);
                        in = &block->inst[block->ninst];
                        in->mesh = mesh;
                        in->x = (float)df_ri32(rdb + at + 8);
                        in->y = -(float)df_ri32(rdb + at + 12);
                        in->z = (float)df_ri32(rdb + at + 16);
                        place_model(in,
                                    (float)xr * ROT_STEP,
                                    (float)yr * ROT_STEP,
                                    (float)zr * ROT_STEP);
                        block->ninst++;
                        block->ntris += mesh->ntris;
                    }
                }
            } else if (type == 0x02 && res + 10 <= size && block->light
                       && block->nlight < want_light) {
                /*
                 * A light: two 32-bit fields whose meaning is not known,
                 * not kept, then the radius, 16 bits, in world units. 93%
                 * of the 4268 in BLOCKS.BSA sit within 64 units of a
                 * TEXTURE.210 flat, the torch or candle that shows it; the
                 * rest light a room on their own. world.c decides which
                 * flicker. See docs/lighting.md.
                 */
                Light *li;

                li = &block->light[block->nlight];
                li->x = (float)df_ri32(rdb + at + 8);
                li->y = -(float)df_ri32(rdb + at + 12);
                li->z = (float)df_ri32(rdb + at + 16);
                li->radius = (float)df_ru16(rdb + res + 8);
                li->flicker = 0;
                block->nlight++;
            } else if (type == 0x03 && res + 11 <= size) {
                bitfield = df_ru16(rdb + res);
                archive = (int)(bitfield >> 7);
                record = (int)(bitfield & 0x7f);
                if (archive == EDITOR_ARCHIVE) {
                    block->neditor++;
                    if (block->nmarker < block->mark_cap) {
                        Marker *mk;

                        mk = &block->marker[block->nmarker];
                        mk->x = (float)df_ri32(rdb + at + 8);
                        mk->y = -(float)df_ri32(rdb + at + 12);
                        mk->z = (float)df_ri32(rdb + at + 16);
                        mk->record = (df_u16)record;
                        mk->flags = df_ru16(rdb + res + 2);
                        mk->id = df_ru16(rdb + res + 4);
                        mk->action = rdb[res + 10];
                        block->nmarker++;
                    }
                } else if (block->nflat < block->flat_cap) {
                    fl = &block->flat[block->nflat];
                    fl->x = (float)df_ri32(rdb + at + 8);
                    fl->y = -(float)df_ri32(rdb + at + 12);
                    fl->z = (float)df_ri32(rdb + at + 16);
                    fl->file = (df_u16)archive;
                    fl->image = (df_u16)record;
                    fl->slot = 0xffff;
                    fl->glow = (df_u16)(archive == LIGHTS_ARCHIVE);
                    fl->tag = 0;
                    fl->sight = 0;
                    fl->thin = 0;
                    fl->flip = 0;
                    block->nflat++;
                }
            }
            off = df_ri32(rdb + at);
        }
    }
    return block;
}
