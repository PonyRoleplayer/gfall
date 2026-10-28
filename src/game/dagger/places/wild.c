#include "game/dagger/places/wild.h"
#include "game/dagger/formats/bind.h"
#include "game/dagger/formats/bsa.h"
#include "game/dagger/formats/climate.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/tables/forecast.h"
#include "game/dagger/formats/rmb.h"
#include "game/dagger/formats/skyfile.h"
#include "game/dagger/places/people.h"
#include "game/dagger/tables/sounds.h"
#include "game/dagger/tables/swaps.h"
#include "game/dagger/places/terrain.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/formats/worldmap.h"
#include "game/dagger/places/wres.h"
#include "engine/assets/loader.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/core/prof.h"
#include "engine/gfx/mip.h"
#include "engine/scene/clock.h"
#include "engine/scene/stream.h"
#include "engine/scene/light.h"

/*
 * See wild.h and docs/streaming.md. A chunk is an RMB block's square.
 * Its land is terrain.c's: heights and tiles, drawn near as its tiles
 * and far as two by two quads under a picture of them. A location is
 * centred in its map pixel, flat at its level, the land easing to it
 * round it: the block whose corner falls in the chunk is read whole
 * into the chunk, raised to that level, and the land's cells under the
 * location are left to the blocks' own ground (a location of odd width
 * sits on half chunks, which are whole cells).
 */

#define PAL_NAME "ART_PAL.COL"
/* The ground archive's grass (Daggerfall Unity's tile ids: 0 water, 1 dirt, 2 grass, 3 stone). */
#define REC_GRASS 2
/* Where an RMB's 16 x 16 ground tiles start (rmb.c's FLD_GROUND), and its 64 x 64 automap. */
#define RMB_GROUND 1739
#define RMB_AUTOMAP 2251
#define FAMILIES 4
/* The eye above the ground at the start: the walker's. */
#define START_EYE 68.4f
/* TEXTURE.210, the flats that give light; their lamp's radius outdoors (docs/daylight.md). */
#define LIGHT_FLATS 210
#define LAMP_RADIUS 160.0f
/* Frames a second of an animated flat: flames, and everything else (docs/sprites.md). */
#define FLAME_RATE 12
#define FLAT_RATE 5
/* What one chunk can hold: the largest RMB block (167 models, 74 flats, 8 lamps) and its wilderness. */
#define CAP_MODELS 192
#define CAP_SPRITES 144
#define CAP_LIGHTS 16
#define CAP_EMITTERS 80
#define CAP_USABLES 128
#define CAP_MOVERS 8
/* Models a block names at most (167 in the largest, ground included). */
#define CHUNK_MESHES 192
/* Towns whose ground eases into one chunk at most: its pixel's and its neighbours'. */
#define CHUNK_FLATS 9
/*
 * Flora (docs/streaming.md, Flora): Daggerfall Unity's rules, at
 * FLORA_DENSITY of its chances (its density is thousands a map pixel,
 * drawn as one batch on a modern card), FLORA_MAX a chunk, drawn within
 * FLORA_SIGHT of the camera; kept FLORA_CLEAR off a town, off ground
 * steeper than 50 degrees, and off the beach. That many stand; the
 * console's floradensity draws its percent of them, by rank (40, the
 * default, draws 0.12 of DFU's chances).
 */
#define FLORA_MAX 64
#define FLORA_DENSITY 0.3f
#define FLORA_SIGHT 3072
#define FLORA_CLEAR 1024.0
#define FLORA_STEEP 1.19f
/* Pictures one chunk holds at most. */
#define CHUNK_PICS 256
/* Locations whose records are kept, the most recently met. */
#define DUN_CACHE 8
/* RAM caps: TEXTURE files kept whole, pictures nothing holds (wres). */
#define FILE_RAM (4u * 1024u * 1024u)

static const int g_family[FAMILIES] = { TEX_DESERT, TEX_MOUNTAIN, TEX_WOODLAND, TEX_SWAMP };

/* What one chunk owns, in its own heap. */
typedef struct Chunk {
    MemHeap *heap;
    df_u16 pics[CHUNK_PICS];
    int npics;
    /*
     * A settlement's block: its townspeople's clothes, held and read with
     * its pictures (people_slots), so the first of its chunks to come in
     * range reads them under the budget, far off. Its location's type.
     */
    df_u16 folk[PEOPLE_SLOTS];
    int nfolk;
    int town_type;
    /* Its location's block: the location, the block's number in its record, where its corner falls. */
    int has_block;
    const Dungeon *dun;
    int block;
    float bx;
    float bz;
    int family;
    Block *blk;
    /* Where the block's models start in the slot. */
    int rest;
    /* The block's 16 x 16 ground tiles, for its far picture. */
    df_u8 tiles[256];
    /*
     * Where townspeople may walk on the block (docs/entities.md): its
     * 64 x 64 cells of 64 units, row 0 north, each a weight 0..15 (0 not
     * walkable). 0 until the block's bytes are read.
     */
    df_u8 *nav;
    /* The ARCH3D ids its models use, parsed into the cache one a call; where the steps have got to. */
    df_u32 ids[CHUNK_MESHES];
    int nids;
    int at;
    /* The block's ground baked for the far ground (a dynamic picture), and its slot; -1 for none. */
    int far_slot;
    /* The towns whose ground eases into the chunk, and the chunk's land under them. */
    TerrainFlat flats[CHUNK_FLATS];
    int nflats;
    TerrainChunk land;
    /* The land's tiles' family (its pixel's climate's), and its far picture's slot (-1 none). */
    int land_family;
    int land_slot;
    /* The block's level: its ground and everything on it is raised to this world y. */
    float town_y;
    /* The flora to stand: where (the chunk's frame, its base), and which picture of the nature archive. */
    float flora[FLORA_MAX][3];
    df_u8 flora_rec[FLORA_MAX];
    df_u8 flora_thin[FLORA_MAX];
    int nflora;
    int nature;
} Chunk;

typedef struct DunEnt {
    int region;
    int index;
    int stamp;
    Dungeon dun;
    /* Its level (scaled, terrain_level over its rectangle), worked out the first time it is asked. */
    float level;
    int have_level;
} DunEnt;

static int g_open;
static Palette g_pal;
static Sky g_sky;
static Soundscape g_scape;
static int g_winter;
static int g_season;
/* The climate whose sky and weather odds are on: the camera's pixel's, the last that had a family. */
static int g_climate;
static Bsa *g_blocks;
static Bsa *g_arch;
static DunEnt g_dun[DUN_CACHE];
static int g_dun_stamp;
static Level *g_level;

/*
 * A quad at y 0 from (0, 0) to (w, h), as rmb.c's ground: corners low x
 * low z, high x low z, high x high z, low x high z; v falls with z. Its
 * picture is su by sv texels across it (a tile is 64 across 256 units).
 * On the current heap.
 */
static Mesh *make_quad(int slot, int file, int record, float w, float h, float su, float sv)
{
    Mesh *mesh;
    Tri *t;
    Face *face;
    int k;

    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    t = (Tri *)mem_alloc(2u * (df_u32)sizeof(Tri));
    face = (Face *)mem_alloc(sizeof(Face));
    if (!mesh || !t || !face) {
        return 0;
    }
    mem_zero(mesh, sizeof(Mesh));
    mem_zero(t, 2u * (df_u32)sizeof(Tri));
    t[0].x[0] = 0.0f; t[0].z[0] = 0.0f; t[0].u[0] = 0.0f; t[0].v[0] = sv;
    t[0].x[1] = w;    t[0].z[1] = 0.0f; t[0].u[1] = su;   t[0].v[1] = sv;
    t[0].x[2] = w;    t[0].z[2] = h;    t[0].u[2] = su;   t[0].v[2] = 0.0f;
    t[1].x[0] = 0.0f; t[1].z[0] = 0.0f; t[1].u[0] = 0.0f; t[1].v[0] = sv;
    t[1].x[1] = w;    t[1].z[1] = h;    t[1].u[1] = su;   t[1].v[1] = 0.0f;
    t[1].x[2] = 0.0f; t[1].z[2] = h;    t[1].u[2] = 0.0f; t[1].v[2] = 0.0f;
    for (k = 0; k < 2; k++) {
        t[k].tex_file = (df_u16)file;
        t[k].tex_image = (df_u16)record;
        t[k].slot = (df_u16)slot;
    }
    face->nx = 0.0f;
    face->ny = 1.0f;
    face->nz = 0.0f;
    face->first = 0;
    face->ntris = 2;
    mesh->tris = t;
    mesh->ntris = 2;
    mesh->faces = face;
    mesh->nfaces = 1;
    mesh->face_cap = 1;
    mesh->slot_keys = (df_u16 *)mem_alloc(sizeof(df_u16));
    if (mesh->slot_keys) {
        mesh->slot_keys[0] = (df_u16)slot;
        mesh->nslot_keys = 1;
        mesh->have_keys = 1;
    }
    mesh_measure(mesh);
    return mesh;
}

static int family_index(int family)
{
    int f;

    for (f = 0; f < FAMILIES && g_family[f] != family; f++) {
    }
    return f;
}

/* A location's record, read once while it is among the last DUN_CACHE met: its cache entry. */
static DunEnt *location(int region, int index)
{
    int i;
    int old;

    old = 0;
    for (i = 0; i < DUN_CACHE; i++) {
        if (g_dun[i].stamp && g_dun[i].region == region && g_dun[i].index == index) {
            g_dun[i].stamp = ++g_dun_stamp;
            return &g_dun[i];
        }
        if (g_dun[i].stamp < g_dun[old].stamp) {
            old = i;
        }
    }
    {
        MemHeap *scratch;
        MemHeap *was;
        int ok;

        /* The region's files it reads are megabytes: in a heap of their own, gone when it returns. */
        scratch = mem_heap_new();
        was = mem_heap_use(scratch);
        ok = scratch && exterior_load_at(region, index, &g_dun[old].dun);
        mem_heap_use(was);
        mem_heap_delete(scratch);
        if (!ok) {
            g_dun[old].stamp = 0;
            return 0;
        }
    }
    g_dun[old].region = region;
    g_dun[old].index = index;
    g_dun[old].stamp = ++g_dun_stamp;
    g_dun[old].have_level = 0;
    return &g_dun[old];
}

/* A location's size in blocks, from its blocks' places. */
static void location_size(const Dungeon *dun, int *w, int *h)
{
    int i;

    *w = 0;
    *h = 0;
    for (i = 0; i < dun->nblocks; i++) {
        *w = dun->block[i].x + 1 > *w ? dun->block[i].x + 1 : *w;
        *h = dun->block[i].z + 1 > *h ? dun->block[i].z + 1 : *h;
    }
}

/*
 * The town on pixel (px, py), as the land sees it: its rectangle in
 * world units and its level. 0 when the pixel has none (or not a town
 * outside, or one that does not read). dun, when asked, its record.
 */
static int town_flat(int px, int py, TerrainFlat *f, const Dungeon **dun)
{
    const Dungeon *d;
    DunEnt *e;
    int region;
    int index;
    int w;
    int h;

    if (px < 0 || py < 0 || px >= WORLDMAP_W || py >= WORLDMAP_H
        || !worldmap_location(px, py, &region, &index)) {
        return 0;
    }
    e = location(region, index);
    d = e ? &e->dun : 0;
    if (!d || !d->outdoor || d->nblocks < 1) {
        return 0;
    }
    location_size(d, &w, &h);
    /* Centred: (128 - width x 16) / 2 tiles from the pixel's corner (GetLocationTerrainTileOrigin). */
    f->x0 = (double)(px * WORLDMAP_BLOCKS * STREAM_CHUNK + (WORLDMAP_BLOCKS - w) * STREAM_CHUNK / 2);
    f->z0 = (double)((WORLDMAP_H - 1 - py) * WORLDMAP_BLOCKS * STREAM_CHUNK
                     + (WORLDMAP_BLOCKS - h) * STREAM_CHUNK / 2);
    f->x1 = f->x0 + (double)(w * STREAM_CHUNK);
    f->z1 = f->z0 + (double)(h * STREAM_CHUNK);
    if (!e->have_level) {
        e->level = terrain_level(f->x0, f->z0, f->x1, f->z1);
        e->have_level = 1;
    }
    f->s = e->level;
    if (dun) {
        *dun = d;
    }
    return 1;
}

/*
 * The towns whose ground reaches chunk (cx, cz): its pixel's, and on a
 * pixel's edge ring, the neighbours' over that edge (a town eases out
 * TERRAIN_BLEND, one chunk, past its rectangle).
 */
static int gather_flats(int cx, int cz, TerrainFlat *out)
{
    int px;
    int pz;
    int lx;
    int lz;
    int dx;
    int dz;
    int n;

    px = cx / WORLDMAP_BLOCKS;
    pz = cz / WORLDMAP_BLOCKS;
    lx = cx - px * WORLDMAP_BLOCKS;
    lz = cz - pz * WORLDMAP_BLOCKS;
    n = 0;
    for (dz = -1; dz <= 1; dz++) {
        for (dx = -1; dx <= 1; dx++) {
            if ((dx < 0 && lx > 0) || (dx > 0 && lx < WORLDMAP_BLOCKS - 1)
                || (dz < 0 && lz > 0) || (dz > 0 && lz < WORLDMAP_BLOCKS - 1)) {
                continue;
            }
            if (n < CHUNK_FLATS && town_flat(px + dx, WORLDMAP_H - 1 - (pz + dz), &out[n], 0)) {
                n++;
            }
        }
    }
    return n;
}

/* Hold a picture for the chunk, once however many of its models use it. */
static void hold(Chunk *c, int slot)
{
    int i;

    if (slot < 0) {
        return;
    }
    for (i = 0; i < c->npics; i++) {
        if (c->pics[i] == (df_u16)slot) {
            return;
        }
    }
    if (c->npics < CHUNK_PICS) {
        c->pics[c->npics++] = (df_u16)slot;
        /* Counted in now, read later, one a call (step_pictures). */
        wres_hold(slot);
    }
}

/*
 * Step: the chunk's heap, the towns whose ground reaches it, and which
 * block of its own pixel's town, if any, has its corner in it.
 */
static int start_chunk(int cx, int cz, StreamOut *out)
{
    Chunk *c;
    MemHeap *heap;
    MemHeap *was;
    TerrainFlat own;
    const Dungeon *dun;
    int px;
    int py;
    int f;

    heap = mem_heap_new();
    if (!heap) {
        return 0;
    }
    was = mem_heap_use(heap);
    c = (Chunk *)mem_alloc(sizeof(Chunk));
    mem_heap_use(was);
    if (!c) {
        mem_heap_delete(heap);
        return 0;
    }
    mem_zero(c, sizeof(Chunk));
    c->town_type = -1;
    c->heap = heap;
    c->far_slot = -1;
    c->land_slot = -1;
    out->data = c;
    px = cx / WORLDMAP_BLOCKS;
    py = WORLDMAP_H - 1 - cz / WORLDMAP_BLOCKS;
    c->nflats = gather_flats(cx, cz, c->flats);
    f = climate_family(worldmap_climate(px, py));
    c->land_family = f >= 0 ? f : TEX_WOODLAND;
    if (town_flat(px, py, &own, &dun)) {
        double lx;
        double lz;
        int b;

        /* The chunk's corner in the world. */
        lx = (double)cx * STREAM_CHUNK;
        lz = (double)cz * STREAM_CHUNK;
        for (b = 0; b < dun->nblocks; b++) {
            double bx;
            double bz;

            bx = own.x0 + (double)(dun->block[b].x * STREAM_CHUNK);
            bz = own.z0 + (double)(dun->block[b].z * STREAM_CHUNK);
            if (bx >= lx && bx < lx + STREAM_CHUNK && bz >= lz && bz < lz + STREAM_CHUNK) {
                c->has_block = 1;
                c->dun = dun;
                c->block = b;
                c->bx = (float)(bx - lx);
                c->bz = (float)(bz - lz);
                c->family = climate_family(dun->climate);
                c->town_y = terrain_y(own.s);
                c->town_type = worldmap_location_type(px, py);
                break;
            }
        }
    }
    return 1;
}

/*
 * The far ground (docs/streaming.md, Far ground). A chunk two rings out
 * or more draws a few quads instead of its tiles: its wilderness patches
 * merged into rectangles, its tile repeated across them, and its block's
 * ground as one quad under a picture baked from the block's tiles, each
 * tile FAR_TILE texels a side (FAR_SIDE for the block: 32 units a texel,
 * two or three pixels at the nearest a far chunk can be).
 */
#define FAR_SIDE 128
#define FAR_TILE 8
/* Ground archives: four families, summer and winter; records a ground archive can have. */
#define GROUND_ARCHIVES 8
#define GROUND_RECORDS 64

/* Each ground tile shrunk to FAR_TILE a side, made once for the session (a quarter of a ms each). */
static df_u8 g_tile_small[GROUND_ARCHIVES][GROUND_RECORDS][FAR_TILE * FAR_TILE];
static df_u8 g_tile_have[GROUND_ARCHIVES][GROUND_RECORDS];

static int ground_archive(int archive)
{
    int f;
    int k;

    f = family_index((archive / 100) * 100);
    k = archive % 100 - 2;
    return f < FAMILIES && (k == 0 || k == 1) ? f * 2 + k : -1;
}

/* A ground tile at FAR_TILE a side: each texel the mean of what it covers, nearest in the palette. */
static const df_u8 *tile_small(int archive, int record)
{
    df_u8 *out;
    MemHeap *h;
    MemHeap *was;
    Image *img;
    int a;

    a = ground_archive(archive);
    if (a < 0 || record < 0 || record >= GROUND_RECORDS) {
        return 0;
    }
    out = g_tile_small[a][record];
    if (g_tile_have[a][record]) {
        return g_tile_have[a][record] == 1 ? out : 0;
    }
    g_tile_have[a][record] = 2;
    /* Read into a heap of its own and thrown away: only the small copy stays. */
    h = mem_heap_new();
    if (!h) {
        return 0;
    }
    was = mem_heap_use(h);
    img = texture_load(archive, record);
    mem_heap_use(was);
    if (img && img->index && img->width >= FAR_TILE && img->height >= FAR_TILE) {
        int bw;
        int bh;
        int y;

        bw = img->width / FAR_TILE;
        bh = img->height / FAR_TILE;
        mip_palette(g_pal.r, g_pal.g, g_pal.b);
        for (y = 0; y < FAR_TILE; y++) {
            int x;

            for (x = 0; x < FAR_TILE; x++) {
                int r;
                int g;
                int b;
                int i;
                int j;

                r = g = b = 0;
                for (j = 0; j < bh; j++) {
                    const df_u8 *row;

                    row = img->index + (y * bh + j) * img->width + x * bw;
                    for (i = 0; i < bw; i++) {
                        r += g_pal.r[row[i]];
                        g += g_pal.g[row[i]];
                        b += g_pal.b[row[i]];
                    }
                }
                out[y * FAR_TILE + x] = mip_nearest(r / (bw * bh), g / (bw * bh), b / (bw * bh));
            }
        }
        g_tile_have[a][record] = 1;
    }
    mem_heap_delete(h);
    return g_tile_have[a][record] == 1 ? out : 0;
}

/*
 * Ground tiles as one picture of the chunk (a block's, or the land's):
 * column x east, row y from the north edge, as the quad's u and v run. A tile's rotate (0x40) and
 * flip (0x80) bits turn its small copy as rmb.c's corner_uv turns the
 * tile's corners: with a and b the column and row in the tile, none
 * reads (a, b); flip (7-a, 7-b); rotate (7-b, a); both (b, 7-a).
 */
static Image *bake_tiles(Chunk *c, const df_u8 *tiles, int family)
{
    Image *img;
    df_u8 *px;
    MemHeap *was;
    int t;

    was = mem_heap_use(c->heap);
    img = (Image *)mem_alloc(sizeof(Image));
    px = (df_u8 *)mem_alloc(FAR_SIDE * FAR_SIDE);
    mem_heap_use(was);
    if (!img || !px) {
        return 0;
    }
    mem_zero(img, sizeof(Image));
    for (t = 0; t < 256; t++) {
        const df_u8 *small;
        df_u8 *at;
        int bits;
        int record;
        int rot;
        int flip;
        int a;
        int b;

        bits = (int)tiles[t];
        record = bits & 0x3f;
        rot = (bits & 0x40) != 0;
        flip = (bits & 0x80) != 0;
        small = tile_small(swap_climate(family + 2, record, family, g_winter, texture_record_count),
                           record);
        /* Tile row t / 16 is its row from the north, as rmb.c places it. */
        at = px + (t / 16) * FAR_TILE * FAR_SIDE + (t % 16) * FAR_TILE;
        for (b = 0; b < FAR_TILE; b++) {
            for (a = 0; a < FAR_TILE; a++) {
                int sa;
                int sb;

                if (rot && flip) {
                    sa = b;
                    sb = FAR_TILE - 1 - a;
                } else if (rot) {
                    sa = FAR_TILE - 1 - b;
                    sb = a;
                } else if (flip) {
                    sa = FAR_TILE - 1 - a;
                    sb = FAR_TILE - 1 - b;
                } else {
                    sa = a;
                    sb = b;
                }
                at[b * FAR_SIDE + a] = small ? small[sb * FAR_TILE + sa] : 0;
            }
        }
    }
    img->width = img->height = FAR_SIDE;
    img->pot_w = img->pot_h = FAR_SIDE;
    img->span_w = img->span_h = FAR_SIDE;
    img->index = px;
    img->pot = px;
    return img;
}

static void add_model(StreamOut *out, Mesh *mesh, float x, float y, float z)
{
    Instance *in;

    if (!mesh || out->models_n >= CAP_MODELS) {
        return;
    }
    in = &out->models[out->models_n++];
    in->mesh = mesh;
    in->x = x;
    in->y = y;
    in->z = z;
    instance_set_rotation(in, 0.0f, 0.0f, 0.0f);
}

/* A tile record of the land: its picture in the land's family and the season, held for the chunk. */
static int land_slot_of(int record, void *user)
{
    Chunk *c;
    int slot;

    c = (Chunk *)user;
    slot = wres_slot(swap_climate(c->land_family + 2, record, c->land_family, g_winter,
                                  texture_record_count), record, WRES_WALL);
    hold(c, slot);
    return slot;
}

/* Step: the near ground: the land's tiles, then the block's ground raised to its level. */
static void place_near(Chunk *c, StreamOut *out)
{
    MemHeap *was;
    int i;
    int k;

    was = mem_heap_use(c->heap);
    add_model(out, terrain_near_mesh(&c->land, land_slot_of, c), 0.0f, 0.0f, 0.0f);
    mem_heap_use(was);
    for (i = 0; c->blk && i < c->blk->ninst && out->models_n < CAP_MODELS; i++) {
        Instance *in;

        if (c->blk->inst[i].mesh->id != 0) {
            continue;
        }
        in = &out->models[out->models_n++];
        *in = c->blk->inst[i];
        in->x += c->bx;
        in->y += c->town_y;
        in->z += c->bz;
        for (k = 0; k < in->mesh->nslot_keys; k++) {
            hold(c, (int)in->mesh->slot_keys[k]);
        }
    }
    out->near_n = out->models_n;
}

/*
 * Step: the far ground: the land as two by two quads under its picture,
 * then the block's ground as one quad under its own, at its level.
 */
static void place_far(Chunk *c, StreamOut *out)
{
    MemHeap *was;

    was = mem_heap_use(c->heap);
    if (c->land.covered != 0xffffu) {
        Image *img;

        img = bake_tiles(c, c->land.tiles, c->land_family);
        c->land_slot = img ? wres_dynamic(img, (float)FAR_SIDE / (float)STREAM_CHUNK) : -1;
        if (c->land_slot >= 0) {
            add_model(out, terrain_far_mesh(&c->land, c->land_slot, FAR_SIDE), 0.0f, 0.0f, 0.0f);
        }
    }
    if (c->blk) {
        Image *img;

        img = bake_tiles(c, c->tiles, c->family);
        c->far_slot = img ? wres_dynamic(img, (float)FAR_SIDE / (float)STREAM_CHUNK) : -1;
        if (c->far_slot >= 0) {
            add_model(out, make_quad(c->far_slot, 0, 0, (float)STREAM_CHUNK, (float)STREAM_CHUNK,
                                     (float)FAR_SIDE, (float)FAR_SIDE),
                      c->bx, c->town_y, c->bz);
        }
    }
    mem_heap_use(was);
    out->far_n = out->models_n - out->near_n;
}

/*
 * The block's bytes, in scratch, its missing ground tiles made grass.
 * Read again by each step that needs them: scratch is the next read's.
 */
static df_u8 *block_bytes(Chunk *c, df_u32 *size)
{
    const BsaRec *rec;
    df_u8 *bytes;
    int count;
    int i;

    rec = bsa_find_name(g_blocks, c->dun->block[c->block].name);
    if (!rec) {
        return 0;
    }
    bytes = bsa_read_scratch(g_blocks, rec, BSA_SCRATCH_BLOCK);
    if (!bytes || rec->size < RMB_GROUND + 256u) {
        return 0;
    }
    /*
     * A ground tile past the archive's records (63) is no tile: outside a
     * city's walls the land is the wilderness's. Those become its grass
     * here, so the block's ground covers its square whole and the
     * wilderness's patches under a location are never needed (nor drawn
     * twice, in the same plane). Scratch, so the bytes are ours to change.
     */
    count = texture_record_count(c->family + 2);
    for (i = 0; i < 256; i++) {
        if ((int)(bytes[RMB_GROUND + i] & 0x3f) >= count) {
            bytes[RMB_GROUND + i] = (df_u8)REC_GRASS;
        }
        c->tiles[i] = bytes[RMB_GROUND + i];
    }
    *size = rec->size;
    return bytes;
}

/*
 * A ground tile's walking weight, as Daggerfall Unity's CityNavigation
 * has it: roads best, then grass, then dirt, stone worst of what can be
 * walked; water and its edges never. By the tile's record.
 */
static int tile_weight(int record)
{
    switch (record) {
    case 0:
    case 5:
    case 6:
    case 20:
    case 21:
    case 30:
    case 31:
        return 0;
    case 3:
        return 4;
    case 1:
        return 6;
    case 2:
        return 12;
    case 46:
    case 47:
    case 55:
        return 15;
    default:
        return 7;
    }
}

/*
 * The block's walking cells: its automap (64 x 64 at +2251, row 0
 * north, 0 where nothing stands, a building or a prop elsewhere) and
 * under each cell its ground tile's weight. In the chunk's heap.
 */
static void make_nav(Chunk *c, const df_u8 *bytes, df_u32 size)
{
    MemHeap *was;
    int i;

    if (size < RMB_AUTOMAP + 64u * 64u) {
        return;
    }
    was = mem_heap_use(c->heap);
    c->nav = (df_u8 *)mem_alloc(64u * 64u);
    mem_heap_use(was);
    for (i = 0; c->nav && i < 64 * 64; i++) {
        int row;
        int col;

        row = i / 64;
        col = i % 64;
        c->nav[i] = bytes[RMB_AUTOMAP + i] != 0
            ? 0 : (df_u8)tile_weight((int)c->tiles[(row / 4) * 16 + col / 4] & 0x3f);
    }
}

/* Step: which meshes the block names, and where its townspeople may walk. */
static int list_meshes(Chunk *c)
{
    df_u8 *bytes;
    df_u32 size;

    bytes = block_bytes(c, &size);
    if (!bytes) {
        return 0;
    }
    make_nav(c, bytes, size);
    c->nids = rmb_model_ids(bytes, size, c->ids, CHUNK_MESHES);
    c->at = 0;
    return 1;
}

/*
 * Step, again and again: the next mesh not in the session's cache is
 * parsed into it and given its climate's pictures; one a call (a big
 * model is milliseconds on the host, a frame's budget many times over on
 * the 233). 1 when none is left.
 */
static int next_mesh(Chunk *c)
{
    ArchEnt *cache;
    int *ncache;

    cache = wres_meshes(c->family, g_winter, &ncache);
    if (!cache) {
        return 1;
    }
    while (c->at < c->nids) {
        MemHeap *was;
        int before;

        before = *ncache;
        was = mem_heap_use(wres_mesh_heap());
        arch_cached(cache, ncache, g_arch, c->ids[c->at++]);
        if (*ncache > before) {
            wres_paint(cache[*ncache - 1].mesh, c->family, g_winter);
            mem_heap_use(was);
            return 0;
        }
        mem_heap_use(was);
    }
    return 1;
}

/* Step: the block placed (its meshes all in the cache now) into the chunk's heap. */
static int read_block(Chunk *c)
{
    df_u8 *bytes;
    df_u32 size;
    ArchEnt *cache;
    int *ncache;
    int before;
    MemHeap *was;
    int i;

    bytes = block_bytes(c, &size);
    cache = wres_meshes(c->family, g_winter, &ncache);
    if (!bytes || !cache) {
        return 0;
    }
    before = *ncache;
    was = mem_heap_use(c->heap);
    arch_cache_heap(wres_mesh_heap());
    c->blk = rmb_load(bytes, size, g_arch, c->family + 2, cache, ncache);
    arch_cache_heap(0);
    /* The ground's patches are the chunk's own: their faces' pictures in its heap. */
    for (i = 0; c->blk && i < c->blk->ninst; i++) {
        if (c->blk->inst[i].mesh->id == 0) {
            wres_paint(c->blk->inst[i].mesh, c->family, g_winter);
        }
    }
    mem_heap_use(was);
    /* Any mesh the list missed: its pictures, once for the session. */
    was = mem_heap_use(wres_mesh_heap());
    for (i = before; i < *ncache; i++) {
        wres_paint(cache[i].mesh, c->family, g_winter);
    }
    mem_heap_use(was);
    return c->blk != 0;
}

/*
 * Step, again and again: the next ground tile the far pictures need
 * shrunk, one a call: the land's 256, then the block's. 1 when none is left.
 */
static int next_tile(Chunk *c)
{
    while (c->at < 512) {
        int record;
        int family;
        int archive;
        int a;

        if (c->at >= 256 && !c->blk) {
            break;
        }
        record = (int)(c->at < 256 ? c->land.tiles[c->at] : c->tiles[c->at - 256]) & 0x3f;
        family = c->at < 256 ? c->land_family : c->family;
        c->at++;
        archive = swap_climate(family + 2, record, family, g_winter, texture_record_count);
        a = ground_archive(archive);
        if (a >= 0 && record < GROUND_RECORDS && !g_tile_have[a][record]) {
            tile_small(archive, record);
            return 0;
        }
    }
    return 1;
}

/*
 * Step, again and again: the next part of the next held picture not in
 * RAM (its pixels, then its chain), one part a call. 1 when none is left.
 */
static int next_picture(Chunk *c)
{
    while (c->at < c->npics + c->nfolk) {
        if (wres_load((int)(c->at < c->npics ? c->pics[c->at] : c->folk[c->at - c->npics]))) {
            return 0;
        }
        c->at++;
    }
    return 1;
}

/* Step: the flats' pictures held, to be read before the flats are placed (they need their sizes). */
static void hold_flats(Chunk *c)
{
    int i;

    for (i = 0; i < c->blk->nflat; i++) {
        hold(c, wres_slot((int)c->blk->flat[i].file, (int)c->blk->flat[i].image, WRES_SPRITE));
    }
    /* A settlement's people, while the engine lets walkers come (docs/entities.md). */
    if (people_settle(c->town_type) && entity_population()) {
        c->nfolk = people_slots(c->dun->climate, c->folk, PEOPLE_SLOTS);
        for (i = 0; i < c->nfolk; i++) {
            wres_hold((int)c->folk[i]);
        }
    }
}


/* Step: the block's buildings and props, after the far ground; their pictures held. */
static void place_rest(Chunk *c, StreamOut *out)
{
    Block *b;
    int i;

    b = c->blk;
    c->rest = out->models_n;
    for (i = 0; i < b->ninst && out->models_n < CAP_MODELS; i++) {
        Instance *in;
        int k;

        if (b->inst[i].mesh->id == 0) {
            continue;
        }
        in = &out->models[out->models_n++];
        *in = b->inst[i];
        in->x += c->bx;
        in->y += c->town_y;
        in->z += c->bz;
        for (k = 0; k < in->mesh->nslot_keys; k++) {
            hold(c, (int)in->mesh->slot_keys[k]);
        }
    }
}

/* Step: the block's flats (their pictures read): sprites standing on their base, lamps, torches and animals heard. */
static void place_flats(Chunk *c, StreamOut *out)
{
    Block *b;
    int i;

    b = c->blk;
    for (i = 0; b && i < b->nflat && out->sprites_n < CAP_SPRITES; i++) {
        Flat *fl;
        const Image *img;
        int slot;
        int w;
        int h;

        fl = &out->sprites[out->sprites_n];
        *fl = b->flat[i];
        fl->x += c->bx;
        fl->y += c->town_y;
        fl->z += c->bz;
        slot = wres_slot((int)fl->file, (int)fl->image, WRES_SPRITE);
        hold(c, slot);
        img = wres_image(slot);
        if (!img) {
            continue;
        }
        w = texture_extent(img->width, img->xscale);
        h = texture_extent(img->height, img->yscale);
        if (w < 1 || h < 1) {
            continue;
        }
        fl->slot = (df_u16)slot;
        fl->w = (float)w;
        fl->h = (float)h;
        /* An RMB flat's point is its base (docs/sprites.md). */
        fl->y += (float)h * 0.5f;
        fl->rate = 0;
        fl->phase = 0;
        if (img->frames > 1) {
            fl->rate = (df_u8)(fl->file == LIGHT_FLATS ? FLAME_RATE : FLAT_RATE);
            fl->phase = (df_u8)(((unsigned int)df_iround(fl->x) * 7u
                                 + (unsigned int)df_iround(fl->z) * 13u) & 255u);
        }
        if (fl->glow && out->lights_n < CAP_LIGHTS) {
            Light *li;

            li = &out->lights[out->lights_n++];
            li->x = fl->x;
            li->y = fl->y + fl->h * 0.25f;
            li->z = fl->z;
            li->radius = LAMP_RADIUS;
            li->flicker = (df_u8)(img->frames > 1);
        }
        if (out->emitters_n < CAP_EMITTERS
            && sounds_flat((int)fl->file, (int)fl->image, fl->x, fl->y, fl->z,
                           &out->emitters[out->emitters_n])) {
            out->emitters_n++;
        }
        out->sprites_n++;
    }
}

/* The nature archive of a climate in the season (Daggerfall Unity's ClimateSwaps and climate settings). */
static int nature_archive(int climate, int winter)
{
    int archive;

    switch (climate) {
    case CLIMATE_DESERT:
    case CLIMATE_DESERT2:
        archive = 503;
        break;
    case CLIMATE_MOUNTAIN:
        archive = 510;
        break;
    case CLIMATE_RAINFOREST:
        archive = 500;
        break;
    case CLIMATE_SWAMP:
        archive = 502;
        break;
    case CLIMATE_SUBTROPICAL:
        archive = 501;
        break;
    case CLIMATE_MOUNTAIN_WOODS:
        archive = 506;
        break;
    case CLIMATE_HAUNTED:
        archive = 508;
        break;
    default:
        /* The woodlands, and the sea's few islands. */
        archive = 504;
        break;
    }
    /* Only the four woodland and mountain sets have snow. */
    if (winter && (archive == 504 || archive == 506 || archive == 508 || archive == 510)) {
        archive++;
    }
    return archive;
}

static df_u32 flora_hash(int x, int z)
{
    df_u32 h;

    h = (df_u32)x * 0x9e3779b1u ^ (df_u32)z * 0x85ebca77u ^ 0x27d4eb2fu;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    return h;
}

/*
 * Step: where the chunk's flora stands. Daggerfall Unity's rules: a
 * chance on each tile of dirt (0.2), grass (0.9) or stone (0.05), less
 * on low land and a quarter in the desert, none on transitions, the
 * beach, steep ground, or near a town; then FLORA_DENSITY of that. The
 * same for a tile every time (a hash of its place, not a random run).
 * Their pictures held, to be read before they stand.
 */
static void plan_flora(Chunk *c, int cx, int cz)
{
    float base[4] = { 0.0f, 0.2f, 0.9f, 0.05f };
    float scale;
    float beach;
    int climate;
    int px;
    int py;
    int t;

    px = cx / WORLDMAP_BLOCKS;
    py = WORLDMAP_H - 1 - cz / WORLDMAP_BLOCKS;
    climate = worldmap_climate(px, py);
    c->nature = nature_archive(climate, g_winter);
    scale = (float)worldmap_height(px, py) / 128.0f;
    scale = scale < 0.4f ? 0.4f : (scale > 1.0f ? 1.0f : scale);
    if (climate == CLIMATE_DESERT || climate == CLIMATE_DESERT2 || climate == CLIMATE_SUBTROPICAL) {
        scale *= 0.25f;
    }
    scale *= FLORA_DENSITY;
    beach = terrain_y(TERRAIN_BEACH);
    c->nflora = 0;
    for (t = 0; t < TERRAIN_TILES * TERRAIN_TILES && c->nflora < FLORA_MAX; t++) {
        int kind;
        int tx;
        int tz;
        df_u32 h;
        float lx;
        float lz;
        float y;
        float slope;
        double wx;
        double wz;
        int k;
        int clear;

        kind = c->land.tiles[t];
        /* A plain tile, not turned: dirt, grass or stone, not water, not a transition. */
        if (kind < 1 || kind > 3) {
            continue;
        }
        tx = cx * TERRAIN_TILES + t % TERRAIN_TILES;
        tz = cz * TERRAIN_TILES + (TERRAIN_TILES - 1 - t / TERRAIN_TILES);
        h = flora_hash(tx, tz);
        if ((float)(h & 0xffffu) * (1.0f / 65536.0f) >= base[kind] * scale) {
            continue;
        }
        /* Somewhere in the tile, not on its corner. */
        lx = ((float)(t % TERRAIN_TILES) + 0.15f + 0.7f * (float)((h >> 16) & 255u) / 255.0f)
            * (float)TERRAIN_TILE;
        lz = ((float)(TERRAIN_TILES - 1 - t / TERRAIN_TILES) + 0.15f
              + 0.7f * (float)((h >> 24) & 255u) / 255.0f) * (float)TERRAIN_TILE;
        if (c->land.covered & (1u << ((int)(lz / TERRAIN_CELL) * TERRAIN_CELLS + (int)(lx / TERRAIN_CELL)))) {
            continue;
        }
        wx = (double)cx * STREAM_CHUNK + lx;
        wz = (double)cz * STREAM_CHUNK + lz;
        clear = 1;
        for (k = 0; k < c->nflats; k++) {
            const TerrainFlat *f;

            f = &c->flats[k];
            if (wx > f->x0 - FLORA_CLEAR && wx < f->x1 + FLORA_CLEAR
                && wz > f->z0 - FLORA_CLEAR && wz < f->z1 + FLORA_CLEAR) {
                clear = 0;
            }
        }
        slope = terrain_slope_in(&c->land, lx, lz);
        y = terrain_height_in(&c->land, lx, lz);
        if (!clear || slope > FLORA_STEEP || y < beach) {
            continue;
        }
        /* Sunk a little, more on a slope, so a trunk does not float over the low side. */
        c->flora[c->nflora][0] = lx;
        c->flora[c->nflora][1] = y - 8.0f - slope * 64.0f;
        c->flora[c->nflora][2] = lz;
        /* Records 1 to 31 (0 is not a plant), as Daggerfall Unity picks them. */
        c->flora_rec[c->nflora] = (df_u8)(1u + ((h >> 8) ^ (h >> 20)) % 31u);
        /* Its rank for the console's density, from a hash of its own: even over 0..255. */
        c->flora_thin[c->nflora] = (df_u8)(flora_hash(tz, tx) >> 24);
        hold(c, wres_slot(c->nature, (int)c->flora_rec[c->nflora], WRES_SPRITE));
        c->nflora++;
    }
}

/* With the flats: the flora standing, drawn only within FLORA_SIGHT. */
static void place_flora(Chunk *c, StreamOut *out)
{
    int i;

    for (i = 0; i < c->nflora && out->sprites_n < CAP_SPRITES; i++) {
        Flat *fl;
        const Image *img;
        int slot;
        int w;
        int h;

        slot = wres_slot(c->nature, (int)c->flora_rec[i], WRES_SPRITE);
        img = wres_image(slot);
        if (!img) {
            continue;
        }
        w = texture_extent(img->width, img->xscale);
        h = texture_extent(img->height, img->yscale);
        if (w < 1 || h < 1) {
            continue;
        }
        fl = &out->sprites[out->sprites_n++];
        mem_zero(fl, sizeof(Flat));
        fl->x = c->flora[i][0];
        fl->y = c->flora[i][1] + (float)h * 0.5f;
        fl->z = c->flora[i][2];
        fl->w = (float)w;
        fl->h = (float)h;
        fl->file = (df_u16)c->nature;
        fl->image = (df_u16)c->flora_rec[i];
        fl->slot = (df_u16)slot;
        fl->sight = (df_u16)(FLORA_SIGHT / 16);
        fl->thin = c->flora_thin[i];
    }
}

/* Step: the block's doors (into its buildings, down to a dungeon) and its people. */
static void place_doors(Chunk *c, StreamOut *out)
{
    DoorBuild d;
    ModelAct *act;
    df_u8 *blk;
    MemHeap *was;
    Block *b;
    int n;
    int i;
    int k;

    b = c->blk;
    n = out->models_n - c->rest;
    if (n < 1) {
        return;
    }
    was = mem_heap_use(c->heap);
    act = (ModelAct *)mem_alloc((df_u32)n * (df_u32)sizeof(ModelAct));
    blk = (df_u8 *)mem_alloc((df_u32)n);
    if (act && blk) {
        /* What the file said of each building model, in the order they were placed. */
        k = 0;
        for (i = 0; i < b->ninst && k < n; i++) {
            if (b->inst[i].mesh->id != 0) {
                act[k] = b->act[i];
                blk[k] = (df_u8)c->block;
                k++;
            }
        }
        d.inst = out->models + c->rest;
        d.act = act;
        d.block = blk;
        d.ninst = k;
        d.flat = out->sprites;
        d.nflat = out->sprites_n;
        d.place = PLACE_TOWN;
        d.use = out->usables;
        d.use_cap = CAP_USABLES;
        d.mover = out->movers;
        d.mover_cap = CAP_MOVERS;
        if (doors_build(&d)) {
            /* Counted from the buildings: the stream counts from the slot. */
            for (i = 0; i < d.nuse; i++) {
                if (out->usables[i].model >= 0) {
                    out->usables[i].model += c->rest;
                }
            }
            for (i = 0; i < d.nmover; i++) {
                out->movers[i].model += c->rest;
            }
            out->usables_n = d.nuse;
            out->movers_n = d.nmover;
        }
    }
    mem_heap_use(was);
}

/*
 * A chunk, a step at a time, each a call: the stream checks its budget
 * between calls, so no step may be large (docs/streaming.md, A chunk's
 * contents). The repeated steps (meshes, tiles, pictures) do one piece
 * of real work a call and return 0 until none is left.
 */
enum {
    STEP_START,
    STEP_LAND,
    STEP_LIST,
    STEP_MESHES,
    STEP_BLOCK,
    STEP_TILES,
    STEP_NEAR,
    STEP_FAR,
    STEP_REST,
    STEP_FLORA,
    STEP_HOLD_FLATS,
    STEP_PICTURES,
    STEP_FLATS,
    STEP_DOORS
};

static int fill(void *user, int cx, int cz, StreamOut *out)
{
    Chunk *c;

    (void)user;
    if (out->step == STEP_START) {
        out->models_n = 0;
        out->near_n = 0;
        out->far_n = 0;
        if (cx < 0 || cz < 0 || cx >= WORLDMAP_W * WORLDMAP_BLOCKS || cz >= WORLDMAP_H * WORLDMAP_BLOCKS) {
            return 1;
        }
        if (!start_chunk(cx, cz, out)) {
            return 1;
        }
        out->step = STEP_LAND;
        return 0;
    }
    c = (Chunk *)out->data;
    switch (out->step) {
    case STEP_LAND:
        /* Heights and tiles: a few hundred samples and the finer heights' reads. */
        terrain_chunk(&c->land, cx, cz, c->flats, c->nflats);
        out->step = c->has_block ? STEP_LIST : STEP_TILES;
        c->at = 0;
        return 0;
    case STEP_LIST:
        if (!list_meshes(c)) {
            c->blk = 0;
            c->at = 0;
            out->step = STEP_TILES;
            return 0;
        }
        out->step = STEP_MESHES;
        return 0;
    case STEP_MESHES:
        if (next_mesh(c)) {
            out->step = STEP_BLOCK;
        }
        return 0;
    case STEP_BLOCK:
        if (!read_block(c)) {
            c->blk = 0;
        }
        c->at = 0;
        out->step = STEP_TILES;
        return 0;
    case STEP_TILES:
        if (next_tile(c)) {
            out->step = STEP_NEAR;
        }
        return 0;
    case STEP_NEAR:
        place_near(c, out);
        out->step = STEP_FAR;
        return 0;
    case STEP_FAR:
        place_far(c, out);
        out->step = c->blk ? STEP_REST : STEP_FLORA;
        return 0;
    case STEP_REST:
        place_rest(c, out);
        out->step = STEP_FLORA;
        return 0;
    case STEP_FLORA:
        plan_flora(c, cx, cz);
        out->step = c->blk ? STEP_HOLD_FLATS : STEP_PICTURES;
        c->at = 0;
        return 0;
    case STEP_HOLD_FLATS:
        hold_flats(c);
        c->at = 0;
        out->step = STEP_PICTURES;
        return 0;
    case STEP_PICTURES:
        if (next_picture(c)) {
            out->step = STEP_FLATS;
        }
        return 0;
    case STEP_FLATS:
        place_flats(c, out);
        place_flora(c, out);
        /* A chunk with no block has no doors. */
        if (!c->blk) {
            return 1;
        }
        out->step = STEP_DOORS;
        return 0;
    default:
        place_doors(c, out);
        return 1;
    }
}

/* A chunk goes: its pictures let go (freed later, past the cap), its heap with all it owned. */
static void drop(void *user, int cx, int cz, void *data)
{
    Chunk *c;
    MemHeap *heap;
    int i;

    (void)user;
    (void)cx;
    (void)cz;
    c = (Chunk *)data;
    if (!c) {
        return;
    }
    for (i = 0; i < c->npics; i++) {
        wres_release((int)c->pics[i]);
    }
    for (i = 0; i < c->nfolk; i++) {
        wres_release((int)c->folk[i]);
    }
    wres_forget(c->far_slot);
    wres_forget(c->land_slot);
    heap = c->heap;
    mem_heap_delete(heap);
}

/* A chunk came or went: its lamps' pools and its neighbours' (light.c cuts them a few a frame). */
static void changed(void *user, int slot, int loaded)
{
    (void)user;
    light_stream_changed(slot, loaded);
}

static void shift(void *user, float dx, float dz)
{
    (void)user;
    light_stream_shift(dx, dz);
    people_shift(dx, dz);
}

/* The land's height (a town's level on a town) at world point (wx, wz), its towns eased in. */
float wild_ground(double wx, double wz)
{
    TerrainFlat flats[CHUNK_FLATS];
    int n;

    n = gather_flats((int)(wx / (double)STREAM_CHUNK), (int)(wz / (double)STREAM_CHUNK), flats);
    return terrain_y(terrain_scaled(wx, wz, flats, n));
}

/* The sky and the weather's odds of climate, in the season: when it has a family (not the sea). */
static void set_climate(int climate)
{
    if (climate_family(climate) < 0 || !g_level) {
        return;
    }
    g_climate = climate;
    skyfile_open(climate, g_season, &g_sky);
    forecast_set(climate, g_season);
    g_level->sky = &g_sky;
    g_level->sky_serial++;
}

int wild_open(int px, int py, int radius, Level *level, float *x, float *y, float *z)
{
    /* The pixel's middle is the corner of its fifth chunk, east and north. */
    return wild_open_at((double)((px * WORLDMAP_BLOCKS + WORLDMAP_BLOCKS / 2) * STREAM_CHUNK),
                        (double)(((WORLDMAP_H - 1 - py) * WORLDMAP_BLOCKS + WORLDMAP_BLOCKS / 2)
                                 * STREAM_CHUNK),
                        radius, level, x, y, z);
}

int wild_open_at(double wx, double wz, int radius, Level *level, float *x, float *y, float *z)
{
    StreamSource src;
    int cx;
    int cz;
    int px;
    int py;
    df_u8 *pal_bytes;
    df_u32 pal_size;
    int climate;

    /* Once, not a frame: a cast will do. The world is all east and north of 0. */
    cx = wx > 0.0 ? (int)(wx / (double)STREAM_CHUNK) : 0;
    cz = wz > 0.0 ? (int)(wz / (double)STREAM_CHUNK) : 0;
    px = cx / WORLDMAP_BLOCKS;
    py = WORLDMAP_H - 1 - cz / WORLDMAP_BLOCKS;
    wild_close();
    dagger_bind_loaders();
    mem_zero(level, sizeof(Level));
    if (!worldmap_open()) {
        return 0;
    }
    pal_bytes = vfs_read(PAL_NAME, &pal_size);
    if (!pal_bytes || !loader_call("col", PAL_NAME, pal_bytes, pal_size, &g_pal)) {
        log_text(LOG_ERROR, "wild: " PAL_NAME " did not read");
        return 0;
    }
    g_blocks = bsa_open("BLOCKS.BSA");
    g_arch = bsa_open("ARCH3D.BSA");
    if (!g_blocks || !g_arch) {
        log_text(LOG_ERROR, "wild: BLOCKS.BSA or ARCH3D.BSA did not open");
        return 0;
    }
    /* Town after town of one region: MAPS.BSA open, the region's records read once. */
    exterior_keep(1);
    /* Files in texture.c's own heap, freed past a cap: never part of a picture's track. */
    texture_cache_limit(FILE_RAM);
    if (!wres_begin(&g_pal, level)) {
        return 0;
    }
    g_season = clock_season();
    g_winter = g_season == CLOCK_WINTER;
    level->palette = &g_pal;
    level->streamed = 1;
    climate = worldmap_climate(px, py);
    g_level = level;
    g_climate = -1;
    /* The start's sky and weather odds; wild_update makes them follow the camera. */
    set_climate(climate);
    level->sky_serial = 0;
    sounds_scape(&g_scape, SCAPE_TOWN, 0);
    level->sound = &g_scape;
    level->scene.outdoor = 1;
    mem_zero(&src, sizeof(src));
    src.fill = fill;
    src.drop = drop;
    src.changed = changed;
    src.shift = shift;
    src.caps.models = CAP_MODELS;
    src.caps.sprites = CAP_SPRITES;
    src.caps.lights = CAP_LIGHTS;
    src.caps.emitters = CAP_EMITTERS;
    src.caps.usables = CAP_USABLES;
    src.caps.movers = CAP_MOVERS;
    /* The point's chunk is the origin: the point is its offset in it. */
    if (!stream_open(&level->scene, &src, radius, cx, cz)) {
        return 0;
    }
    /* On the heap the stream's arrays are on; the decals in heaps of their own. */
    if (!light_stream_begin(&level->scene)) {
        log_text(LOG_WARN, "wild: the stream's lamps are unlit");
    }
    /* The townspeople, drawn from the scene (docs/entities.md). */
    people_begin(&level->scene);
    *x = (float)(wx - (double)cx * (double)STREAM_CHUNK);
    *y = wild_ground(wx, wz) + START_EYE;
    *z = (float)(wz - (double)cz * (double)STREAM_CHUNK);
    g_open = 1;
    log_print(LOG_INFO, "wild: open at pixel %d %d, climate %d, %s", px, py, climate,
              clock_season_name(g_season));
    return 1;
}

void wild_close(void)
{
    if (g_open) {
        people_end();
        light_stream_end();
        stream_close();
        exterior_keep(0);
        wres_end();
        texture_cache_limit(0);
        if (g_blocks) {
            bsa_close(g_blocks);
        }
        if (g_arch) {
            bsa_close(g_arch);
        }
    }
    g_blocks = 0;
    g_arch = 0;
    g_open = 0;
    g_level = 0;
}

void wild_update(Camera *cam, float budget_ms)
{
    if (g_open) {
        int px;
        int py;
        int climate;
        df_u32 start;
        float left;

        /* The camera's pixel's climate: its sky and its weather's odds (docs/streaming.md, Climate). */
        wild_here(cam, &px, &py);
        climate = worldmap_climate(px, py);
        if (climate != g_climate) {
            set_climate(climate);
        }
        start = prof_now();
        stream_update(cam, budget_ms);
        /*
         * The lamps' pools the chunks asked for, in what the budget has
         * left, a quarter of it at least so they are never starved.
         */
        left = prof_per_ms() ? budget_ms - (float)(prof_now() - start) / (float)prof_per_ms() : 0.0f;
        light_stream_service(left > budget_ms * 0.25f ? left : budget_ms * 0.25f);
        wres_trim(WRES_PICTURE_RAM);
    }
}

void wild_fill(Camera *cam)
{
    int k;

    for (k = 0; g_open && k < 4000 && (k == 0 || stream_missing(STREAM_RADIUS_MAX) > 0
                                       || light_stream_pending() > 0); k++) {
        stream_update(cam, 50.0f);
        light_stream_service(50.0f);
    }
    /* A town's people out on its streets before the first frame, not coming one a tick after it. */
    people_fill(cam);
}

int wild_location_at(double wx, double wz, int *region, int *index, double *ox, double *oz)
{
    TerrainFlat f;
    int px;
    int py;

    if (!g_open || wx < 0.0 || wz < 0.0) {
        return 0;
    }
    px = (int)(wx / (double)(STREAM_CHUNK * WORLDMAP_BLOCKS));
    py = WORLDMAP_H - 1 - (int)(wz / (double)(STREAM_CHUNK * WORLDMAP_BLOCKS));
    if (!worldmap_location(px, py, region, index) || !town_flat(px, py, &f, 0)) {
        return 0;
    }
    /* Its own frame's origin: the corner of its block 0, 0, as start_chunk centres it. */
    *ox = f.x0;
    *oz = f.z0;
    return 1;
}

int wild_crowded(void)
{
    return g_open && wres_full();
}

void wild_restart(Camera *cam)
{
    df_u32 had;

    if (!g_open) {
        return;
    }
    wres_stats(0, 0, 0, 0, 0, &had);
    /* Every chunk first: their models point into the meshes' heap. */
    stream_reload();
    wres_meshes_reset();
    wild_fill(cam);
    log_print(LOG_INFO, "wild: the meshes passed %u KB (%u KB): read again for what is round the camera",
              (unsigned int)(WRES_MESH_RAM / 1024u), (unsigned int)(had / 1024u));
}

int wild_town(double wx, double wz, double *x0, double *z0, double *x1, double *z1,
              int *blocks, int *climate, int *type)
{
    TerrainFlat f;
    const Dungeon *dun;
    int px;
    int py;

    if (!g_open || wx < 0.0 || wz < 0.0) {
        return 0;
    }
    px = (int)(wx / (double)(STREAM_CHUNK * WORLDMAP_BLOCKS));
    py = WORLDMAP_H - 1 - (int)(wz / (double)(STREAM_CHUNK * WORLDMAP_BLOCKS));
    if (!town_flat(px, py, &f, &dun)) {
        return 0;
    }
    *x0 = f.x0;
    *z0 = f.z0;
    *x1 = f.x1;
    *z1 = f.z1;
    *blocks = (int)((f.x1 - f.x0) / STREAM_CHUNK) * (int)((f.z1 - f.z0) / STREAM_CHUNK);
    *climate = dun->climate;
    *type = worldmap_location_type(px, py);
    return 1;
}

int wild_nav(double wx, double wz)
{
    double x0, z0, x1, z1;
    double bx;
    double bz;
    const Chunk *c;
    int blocks;
    int climate;
    int type;
    int col;
    int row;

    if (!wild_town(wx, wz, &x0, &z0, &x1, &z1, &blocks, &climate, &type)
        || wx < x0 || wx >= x1 || wz < z0 || wz >= z1) {
        return 0;
    }
    /* The block's corner, and the chunk that holds it (a block's corner is its chunk's). */
    bx = x0 + (double)((int)((wx - x0) / STREAM_CHUNK) * STREAM_CHUNK);
    bz = z0 + (double)((int)((wz - z0) / STREAM_CHUNK) * STREAM_CHUNK);
    c = (const Chunk *)stream_chunk_data((int)(bx / STREAM_CHUNK), (int)(bz / STREAM_CHUNK));
    if (!c || !c->nav) {
        return 0;
    }
    col = (int)((wx - bx) / 64.0);
    row = 63 - (int)((wz - bz) / 64.0);
    col = col < 0 ? 0 : (col > 63 ? 63 : col);
    row = row < 0 ? 0 : (row > 63 ? 63 : row);
    return c->nav[row * 64 + col];
}

int wild_season_turned(void)
{
    return g_open && clock_season() != g_season;
}

void wild_reseason(Camera *cam)
{
    int k;

    if (!g_open) {
        return;
    }
    g_season = clock_season();
    g_winter = g_season == CLOCK_WINTER;
    /* The meshes of the other season are another cache (wres_meshes); the land's tiles take it when made. */
    k = g_climate;
    g_climate = -1;
    set_climate(k);
    stream_reload();
    /* The ground round the camera before the next frame: the rest comes as it does on a walk. */
    wild_fill(cam);
    log_print(LOG_INFO, "wild: the season turned to %s, the chunks loaded again",
              clock_season_name(g_season));
}

void wild_here(const Camera *cam, int *px, int *py)
{
    int cx;
    int cz;

    stream_chunk_at(cam, &cx, &cz);
    *px = cx >= 0 ? cx / WORLDMAP_BLOCKS : -1;
    *py = cz >= 0 ? WORLDMAP_H - 1 - cz / WORLDMAP_BLOCKS : -1;
}
