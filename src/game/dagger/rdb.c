#include "game/dagger/rdb.h"
#include "game/dagger/arch3d.h"
#include "game/dagger/bsa.h"
#include "engine/assets/loader.h"
#include "engine/core/mem.h"

#define MAX_INST 2048
#define MAX_FLAT 2048
#define MAX_MARK 256
#define MAX_CACHE 512
#define EDITOR_ARCHIVE 199
#define ROT_STEP (DF_TAU / 2048.0f)

typedef struct CacheEnt {
    df_u32 id;
    Mesh *mesh;
} CacheEnt;

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

Block *rdb_load(const df_u8 *rdb, df_u32 size, Bsa *arch)
{
    df_u32 width;
    df_u32 height;
    df_u32 root;
    Block *block;
    CacheEnt cache[MAX_CACHE];
    int ncache;
    df_u32 cell;
    int guard_cells;

    if (!rdb || !arch || size < 20 + 750 * 8) {
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
    block = (Block *)mem_alloc(sizeof(Block));
    if (!block) {
        return 0;
    }
    block->inst = (Instance *)mem_alloc(MAX_INST * (df_u32)sizeof(Instance));
    block->flat = (Flat *)mem_alloc(MAX_FLAT * (df_u32)sizeof(Flat));
    block->marker = (Marker *)mem_alloc(MAX_MARK * (df_u32)sizeof(Marker));
    if (!block->inst || !block->flat || !block->marker) {
        return 0;
    }
    ncache = 0;
    mem_zero(cache, sizeof(cache));
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
            if (type == 0x01 && res + 14 <= size && block->ninst < MAX_INST) {
                xr = df_ri32(rdb + res);
                yr = df_ri32(rdb + res + 4);
                zr = df_ri32(rdb + res + 8);
                mindex = df_ru16(rdb + res + 12);
                if (mindex < 750 && parse_id(rdb + 20 + (df_u32)mindex * 8, &id)) {
                    mesh = cached_mesh(cache, &ncache, arch, id);
                    if (mesh) {
                        in = &block->inst[block->ninst];
                        in->mesh = mesh;
                        in->x = (float)df_ri32(rdb + at + 8);
                        in->y = -(float)df_ri32(rdb + at + 12);
                        in->z = (float)df_ri32(rdb + at + 16);
                        instance_set_dungeon_rotation(in,
                                                      (float)xr * ROT_STEP,
                                                      (float)yr * ROT_STEP,
                                                      (float)zr * ROT_STEP);
                        block->ninst++;
                        block->ntris += mesh->ntris;
                    }
                }
            } else if (type == 0x03 && res + 11 <= size) {
                bitfield = df_ru16(rdb + res);
                archive = (int)(bitfield >> 7);
                record = (int)(bitfield & 0x7f);
                if (archive == EDITOR_ARCHIVE) {
                    block->neditor++;
                    if (block->nmarker < MAX_MARK) {
                        Marker *mk;

                        mk = &block->marker[block->nmarker];
                        mk->x = (float)df_ri32(rdb + at + 8);
                        mk->y = -(float)df_ri32(rdb + at + 12);
                        mk->z = (float)df_ri32(rdb + at + 16);
                        mk->record = (df_u16)record;
                        block->nmarker++;
                    }
                } else if (block->nflat < MAX_FLAT) {
                    fl = &block->flat[block->nflat];
                    fl->x = (float)df_ri32(rdb + at + 8);
                    fl->y = -(float)df_ri32(rdb + at + 12);
                    fl->z = (float)df_ri32(rdb + at + 16);
                    fl->file = (df_u16)archive;
                    fl->image = (df_u16)record;
                    fl->slot = 0xffff;
                    block->nflat++;
                }
            }
            off = df_ri32(rdb + at);
        }
    }
    return block;
}
