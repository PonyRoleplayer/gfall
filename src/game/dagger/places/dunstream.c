#include "game/dagger/places/dunstream.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/wres.h"
#include "game/dagger/formats/bind.h"
#include "game/dagger/formats/bsa.h"
#include "game/dagger/formats/rdb.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/sounds.h"
#include "game/dagger/tables/swaps.h"
#include "engine/assets/loader.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/scene/entity.h"
#include "engine/scene/interact.h"
#include "engine/scene/light.h"
#include "engine/scene/stream.h"
#include "engine/scene/walker.h"

/* See dunstream.h and docs/streaming.md (Dungeons). */

#define PAL_NAME "ART_PAL.COL"
/* A block's side, the stream's chunk. */
#define BLOCK 2048
/* The ring: 3 x 3 blocks reach past the far clip from anywhere in the camera's. */
#define RADIUS 1
/* A slot's room: one RDB block's worth of each. */
#define CAP_MODELS 1024
#define CAP_SPRITES 512
#define CAP_LIGHTS 128
#define CAP_EMITTERS 128
#define CAP_USABLES 256
#define CAP_MOVERS 256
#define CAP_ENTITIES 64
#define CAP_WATER 64
/* Pictures a block holds: its walls and flats, and its creatures' views. */
#define CHUNK_PICS 512
/* Marker 10 is a dungeon's start (world.c's START_RECORD). */
#define START_RECORD 10
/* As world.c: the flames' frames, a flame's light flickering, the creatures' poses and floors. */
#define LIGHT_FLATS 210
#define FLAME_RATE 12
#define FLAT_RATE 5
#define FLAME_NEAR 64.0f
#define FOE_LEVEL 1
#define FOE_MOVE_RATE 6
#define FOE_FLY_RATE 10
#define FOE_IDLE_RATE 4
#define FOE_RAY_UP 8.0f
#define FOE_RAY_LEN 120.0f
/* A block's water in cells of 256 (world.c's), and its colour. */
#define WATER_CELL 256
#define WATER_CELLS (BLOCK / WATER_CELL)
#define DUNGEON_WATER 0x1c4652u

typedef struct Chunk {
    MemHeap *heap;
    /* The dungeon's block in this chunk: its index in the layout. */
    int b;
    Block *blk;
    /* The pictures it holds (wres), released when it goes. */
    df_u16 pics[CHUNK_PICS];
    int npics;
    int at;
    /* Its creatures before they are placed: the marker, the kind, a woman, passive. */
    int nfoes;
} Chunk;

static int g_open;
static const Dungeon *g_dun;
static Level *g_level;
static Palette g_pal;
static Bsa *g_blocks;
static Bsa *g_arch;
static SwapDungeon g_pics;
static df_u32 g_keep_place;
static Soundscape g_scape;

/*
 * The dungeon's creatures, drawn once at open in its blocks' order, as a
 * whole load draws them (world.c): a random marker's slot 0 and a
 * class's gender take classic's rand, so the order decides who is where.
 * Block b's are g_draw[g_draw_first[b] .. g_draw_first[b + 1]).
 */
typedef struct Draw {
    float x, y, z;
    df_u32 pos;
    df_i16 kind;
    df_u8 female;
    df_u8 passive;
} Draw;
#define DRAWS_MAX 4096
#define MARKS_MAX 1024
static Draw *g_draw;
static int g_draw_first[DUN_MAX_BLOCKS + 1];
static Marker g_marks[MARKS_MAX];

/* The dungeon's remap, as a rule for the registry's paint. */
static int face(void *user, int archive, int record)
{
    (void)user;
    return swap_dungeon(&g_pics, archive, record, texture_record_count);
}

/* The block at layout place (cx, cz), or -1. */
static int block_at(int cx, int cz)
{
    int b;

    for (b = 0; b < g_dun->nblocks; b++) {
        if (g_dun->block[b].x == cx && g_dun->block[b].z == cz) {
            return b;
        }
    }
    return -1;
}

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
        wres_hold(slot);
    }
}

/* Block b's file read and placed, its meshes painted once for the session; in the chunk's heap. */
static Block *read_block(int b, MemHeap *heap)
{
    const BsaRec *rec;
    df_u8 *bytes;
    ArchEnt *cache;
    int *ncache;
    int before;
    Block *blk;
    MemHeap *was;
    int i;

    rec = bsa_find_name(g_blocks, g_dun->block[b].name);
    bytes = rec ? bsa_read_scratch(g_blocks, rec, BSA_SCRATCH_BLOCK) : 0;
    cache = wres_meshes_dungeon(&ncache);
    if (!bytes || !cache) {
        return 0;
    }
    before = *ncache;
    was = mem_heap_use(heap);
    arch_cache_heap(wres_mesh_heap());
    blk = rdb_load(bytes, rec->size, g_arch, cache, ncache);
    arch_cache_heap(0);
    mem_heap_use(was);
    was = mem_heap_use(wres_mesh_heap());
    for (i = before; i < *ncache; i++) {
        wres_paint_by(cache[i].mesh, face, 0);
    }
    mem_heap_use(was);
    return blk;
}

/* Where a ray down from above (x, y, z) meets the block's models first; 0 for no floor in reach. */
static int floor_under(const Instance *inst, int n, float x, float y, float z, float *floor_y)
{
    float o[3];
    float d[3];
    float best;
    int i;

    o[0] = x;
    o[1] = y + FOE_RAY_UP;
    o[2] = z;
    d[0] = 0.0f;
    d[1] = -1.0f;
    d[2] = 0.0f;
    best = FOE_RAY_LEN;
    for (i = 0; i < n; i++) {
        Bound bd;
        float t;

        if (!instance_world_box(&inst[i], &bd)) {
            continue;
        }
        if (x < bd.cx - bd.ex || x > bd.cx + bd.ex || z < bd.cz - bd.ez || z > bd.cz + bd.ez
            || o[1] < bd.cy - bd.ey || o[1] - best > bd.cy + bd.ey) {
            continue;
        }
        if (instance_ray(&inst[i], o, d, best, &t) && t < best) {
            best = t;
        }
    }
    if (best >= FOE_RAY_LEN) {
        return 0;
    }
    *floor_y = o[1] - best;
    return 1;
}

/* A picture's size in world units by its slot; 0 when it is not read. */
static int size_of(int slot, df_u16 *w, df_u16 *h)
{
    const Image *img;
    int iw;
    int ih;

    img = wres_image(slot);
    if (!img) {
        return 0;
    }
    iw = texture_extent(img->width, img->xscale);
    ih = texture_extent(img->height, img->yscale);
    if (iw < 1 || ih < 1) {
        return 0;
    }
    *w = (df_u16)iw;
    *h = (df_u16)ih;
    return 1;
}

enum {
    STEP_START,
    STEP_BLOCK,
    STEP_MODELS,
    STEP_HOLD,
    STEP_PICTURES,
    STEP_FLATS,
    STEP_DOORS,
    STEP_FOES,
    STEP_WATER
};

static const int g_walk[ENTITY_VIEWS] = { 0, 1, 2, 3, 4 };
static const int g_idle[ENTITY_VIEWS] = { 15, 16, 17, 18, 19 };
static const int g_idle_483[ENTITY_VIEWS] = { 15, 11, 17, 18, 19 };

/* Step: the block's models into the slot, in its frame (an RDB block's own), their pictures held. */
static void place_models(Chunk *c, StreamOut *out)
{
    int i;
    int k;

    for (i = 0; i < c->blk->ninst && out->models_n < CAP_MODELS; i++) {
        Instance *in;

        in = &out->models[out->models_n++];
        *in = c->blk->inst[i];
        for (k = 0; k < in->mesh->nslot_keys; k++) {
            hold(c, (int)in->mesh->slot_keys[k]);
        }
    }
}

/*
 * At open: every block's markers (their bytes alone, no models), each
 * creature drawn in the blocks' order. 0 when out of memory.
 */
static int draw_creatures(void)
{
    int b;
    int n;

    g_draw = (Draw *)mem_alloc(DRAWS_MAX * (df_u32)sizeof(Draw));
    if (!g_draw) {
        return 0;
    }
    n = 0;
    for (b = 0; b < g_dun->nblocks; b++) {
        const BsaRec *rec;
        const df_u8 *bytes;
        int nmark;
        int water;
        int i;

        g_draw_first[b] = n;
        rec = bsa_find_name(g_blocks, g_dun->block[b].name);
        bytes = rec ? bsa_read_scratch(g_blocks, rec, BSA_SCRATCH_BLOCK) : 0;
        nmark = bytes ? rdb_markers(bytes, rec->size, g_marks, MARKS_MAX) : 0;
        water = enemy_water(g_marks, nmark);
        for (i = 0; i < nmark && n < DRAWS_MAX; i++) {
            int kind;
            int female;
            int passive;

            kind = enemy_at(&g_marks[i], water, &female, &passive);
            if (kind < 0) {
                continue;
            }
            g_draw[n].x = g_marks[i].x;
            g_draw[n].y = g_marks[i].y;
            g_draw[n].z = g_marks[i].z;
            g_draw[n].pos = g_marks[i].pos;
            g_draw[n].kind = (df_i16)kind;
            g_draw[n].female = (df_u8)female;
            g_draw[n].passive = (df_u8)passive;
            n++;
        }
    }
    g_draw_first[g_dun->nblocks] = n;
    return 1;
}

/* Step: the flats' and the creatures' pictures held, read in the next step (they need their sizes). */
static void hold_pictures(Chunk *c)
{
    int i;
    int k;

    for (i = 0; i < c->blk->nflat; i++) {
        hold(c, wres_slot((int)c->blk->flat[i].file, (int)c->blk->flat[i].image, WRES_SPRITE));
    }
    for (i = g_draw_first[c->b]; i < g_draw_first[c->b + 1]; i++) {
        int kind;
        int female;
        int archive;

        kind = (int)g_draw[i].kind;
        female = (int)g_draw[i].female;
        archive = enemy_archive(kind, female);
        for (k = 0; k < ENTITY_VIEWS; k++) {
            hold(c, wres_slot(archive, g_walk[k], WRES_SPRITE));
            if (enemy_has_idle(kind)) {
                hold(c, wres_slot(archive, female && archive == 483 ? g_idle_483[k] : g_idle[k], WRES_SPRITE));
            }
        }
    }
}

/* Step, again and again: the next part of the next held picture, one a call. 1 when none is left. */
static int next_picture(Chunk *c)
{
    while (c->at < c->npics) {
        if (wres_load((int)c->pics[c->at])) {
            return 0;
        }
        c->at++;
    }
    return 1;
}

/* Step: the flats (an RDB flat's point is its centre), the lights by their flames, the sounds. */
static void place_flats(Chunk *c, StreamOut *out)
{
    int i;
    int f;

    for (i = 0; i < c->blk->nflat && out->sprites_n < CAP_SPRITES; i++) {
        Flat *fl;
        const Image *img;
        int slot;
        df_u16 w;
        df_u16 h;

        fl = &out->sprites[out->sprites_n];
        *fl = c->blk->flat[i];
        slot = wres_slot((int)fl->file, (int)fl->image, WRES_SPRITE);
        img = wres_image(slot);
        if (!img || !size_of(slot, &w, &h)) {
            continue;
        }
        fl->slot = (df_u16)slot;
        fl->w = (float)w;
        fl->h = (float)h;
        fl->rate = 0;
        fl->phase = 0;
        if (img->frames > 1) {
            fl->rate = (df_u8)(fl->file == LIGHT_FLATS ? FLAME_RATE : FLAT_RATE);
            fl->phase = (df_u8)(((unsigned int)df_iround(fl->x) * 7u + (unsigned int)df_iround(fl->z) * 13u) & 255u);
        }
        if (out->emitters_n < CAP_EMITTERS
            && sounds_flat((int)fl->file, (int)fl->image, fl->x, fl->y, fl->z, &out->emitters[out->emitters_n])) {
            out->emitters_n++;
        }
        out->sprites_n++;
    }
    /* The block's light records; one by a flaming flat flickers with it. */
    for (i = 0; i < c->blk->nlight && out->lights_n < CAP_LIGHTS; i++) {
        Light *li;
        float best;

        li = &out->lights[out->lights_n++];
        *li = c->blk->light[i];
        li->flicker = 0;
        best = FLAME_NEAR * FLAME_NEAR;
        for (f = 0; f < out->sprites_n; f++) {
            const Flat *fl;
            float dx, dy, dz, d2;

            fl = &out->sprites[f];
            if (fl->file != LIGHT_FLATS) {
                continue;
            }
            dx = fl->x - li->x;
            dy = fl->y - li->y;
            dz = fl->z - li->z;
            d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < best) {
                best = d2;
                li->flicker = (df_u8)(fl->rate != 0);
            }
        }
    }
}

/* Step: the doors, levers and teleports, keyed as a whole load's are; a teleport's target on its floor. */
static void place_doors(Chunk *c, StreamOut *out)
{
    DoorBuild d;
    df_u8 *blk;
    df_u8 *mark_blk;
    MemHeap *was;
    int i;

    was = mem_heap_use(c->heap);
    blk = (df_u8 *)mem_alloc((df_u32)(out->models_n > 0 ? out->models_n : 1));
    mark_blk = (df_u8 *)mem_alloc((df_u32)(c->blk->nmarker > 0 ? c->blk->nmarker : 1));
    mem_heap_use(was);
    if (!blk || !mark_blk) {
        return;
    }
    for (i = 0; i < out->models_n; i++) {
        blk[i] = (df_u8)c->b;
    }
    for (i = 0; i < c->blk->nmarker; i++) {
        mark_blk[i] = (df_u8)c->b;
    }
    mem_zero(&d, sizeof(d));
    d.inst = out->models;
    d.act = c->blk->act;
    d.block = blk;
    d.ninst = out->models_n;
    d.flat = out->sprites;
    d.nflat = out->sprites_n;
    d.mark = c->blk->marker;
    d.mark_block = mark_blk;
    d.nmark = c->blk->nmarker;
    d.place = PLACE_DUNGEON;
    d.keep_place = g_keep_place;
    d.use = out->usables;
    d.use_cap = CAP_USABLES;
    d.mover = out->movers;
    d.mover_cap = CAP_MOVERS;
    if (!doors_build(&d)) {
        return;
    }
    for (i = 0; i < d.nmover; i++) {
        Mover *m;
        float floor_y;

        m = &out->movers[i];
        if (m->teleport && floor_under(out->models, out->models_n, m->to_x, m->to_y, m->to_z, &floor_y)) {
            m->to_y = floor_y + WALK_EYE;
        }
    }
    out->usables_n = d.nuse;
    out->movers_n = d.nmover;
}

/* Step: the block's creatures, keyed, in their poses, standing on its floors (world.c's rules). */
static void place_foes(Chunk *c, StreamOut *out)
{
    int i;

    for (i = g_draw_first[c->b]; i < g_draw_first[c->b + 1] && out->entities_n < CAP_ENTITIES; i++) {
        const Draw *mk;
        Entity *e;
        int kind;
        int female;
        int archive;
        int k;
        int whole;
        unsigned int hash;
        float ground;

        mk = &g_draw[i];
        kind = (int)mk->kind;
        female = (int)mk->female;
        archive = enemy_archive(kind, female);
        e = &out->entities[out->entities_n];
        entity_init(e);
        whole = 1;
        for (k = 0; k < ENTITY_VIEWS; k++) {
            e->pose[ENTITY_WALK].slot[k] = (df_u16)wres_slot(archive, g_walk[k], WRES_SPRITE);
            whole = whole && size_of(e->pose[ENTITY_WALK].slot[k], &e->pose[ENTITY_WALK].w[k],
                                     &e->pose[ENTITY_WALK].h[k]);
        }
        e->pose[ENTITY_WALK].rate = (df_u8)(enemy_flies(kind) ? FOE_FLY_RATE : FOE_MOVE_RATE);
        if (kind == ENEMY_GHOST || kind == ENEMY_WRAITH) {
            e->pose[ENTITY_WALK].right = 1u << 2;
        }
        if (enemy_has_idle(kind)) {
            for (k = 0; k < ENTITY_VIEWS; k++) {
                int rec;

                rec = female && archive == 483 ? g_idle_483[k] : g_idle[k];
                e->pose[ENTITY_IDLE].slot[k] = (df_u16)wres_slot(archive, rec, WRES_SPRITE);
                whole = whole && size_of(e->pose[ENTITY_IDLE].slot[k], &e->pose[ENTITY_IDLE].w[k],
                                         &e->pose[ENTITY_IDLE].h[k]);
            }
            e->pose[ENTITY_IDLE].rate = FOE_IDLE_RATE;
            if (kind == ENEMY_RAT) {
                e->pose[ENTITY_IDLE].right = (1u << 1) | (1u << 2) | (1u << 3);
            }
        }
        if (!whole) {
            continue;
        }
        e->x = mk->x;
        e->y = mk->y;
        e->z = mk->z;
        hash = (unsigned int)df_iround(mk->x) * 7u + (unsigned int)df_iround(mk->z) * 13u;
        e->yaw = (float)(int)((hash >> 3) & 7u) * (DF_TAU / 8.0f);
        e->phase = (df_u8)(hash & 255u);
        /* A marker's point is the flat's centre: a walker stands on the floor under it, a flyer hangs there. */
        if (!(enemy_flies(kind) || enemy_spectral(kind))
            && floor_under(out->models, out->models_n, e->x, e->y, e->z, &ground)) {
            e->y = ground;
        } else {
            e->y -= (float)entity_pose(e)->h[0] * 0.5f;
        }
        if (g_keep_place) {
            e->key.place = g_keep_place;
            e->key.thing = ((df_u32)c->b << 24) | (mk->pos & 0xFFFFFFu);
        }
        e->live = 1;
        out->entities_n++;
    }
}

/* Step: the block's water: its start marker's level over its models that reach below it (world.c's). */
static void place_water(Chunk *c, StreamOut *out)
{
    df_u8 wet[WATER_CELLS][WATER_CELLS];
    float level;
    int lv;
    int i;
    int cx;
    int cz;

    lv = enemy_water(c->blk->marker, c->blk->nmarker);
    if (lv == ENEMY_DRY) {
        return;
    }
    level = (float)-lv;
    mem_zero(wet, sizeof(wet));
    for (i = 0; i < out->models_n; i++) {
        Bound bd;
        int ax, az, bx, bz;

        if (!instance_world_box(&out->models[i], &bd) || bd.cy - bd.ey >= level) {
            continue;
        }
        ax = df_ifloor((bd.cx - bd.ex) / (float)WATER_CELL);
        bx = df_ifloor((bd.cx + bd.ex) / (float)WATER_CELL);
        az = df_ifloor((bd.cz - bd.ez) / (float)WATER_CELL);
        bz = df_ifloor((bd.cz + bd.ez) / (float)WATER_CELL);
        ax = ax < 0 ? 0 : ax;
        az = az < 0 ? 0 : az;
        bx = bx >= WATER_CELLS ? WATER_CELLS - 1 : bx;
        bz = bz >= WATER_CELLS ? WATER_CELLS - 1 : bz;
        for (cz = az; cz <= bz; cz++) {
            for (cx = ax; cx <= bx; cx++) {
                wet[cz][cx] = 1;
            }
        }
    }
    for (cz = 0; cz < WATER_CELLS; cz++) {
        for (cx = 0; cx < WATER_CELLS; cx++) {
            int run;
            Water *w;

            if (!wet[cz][cx]) {
                continue;
            }
            for (run = cx; run < WATER_CELLS && wet[cz][run]; run++) {
            }
            if (out->water_n < CAP_WATER) {
                w = &out->water[out->water_n++];
                w->x0 = (float)(cx * WATER_CELL);
                w->x1 = (float)(run * WATER_CELL);
                w->z0 = (float)(cz * WATER_CELL);
                w->z1 = w->z0 + (float)WATER_CELL;
                w->y = level;
                w->color = DUNGEON_WATER;
            }
            cx = run;
        }
    }
}

/* A block's chunk, a step a call: the stream checks its budget between them. */
static int fill(void *user, int cx, int cz, StreamOut *out)
{
    Chunk *c;

    (void)user;
    if (out->step == STEP_START) {
        MemHeap *heap;
        MemHeap *was;
        int b;

        b = block_at(cx, cz);
        if (b < 0) {
            /* A place in the layout with no block: an empty chunk. */
            return 1;
        }
        heap = mem_heap_new();
        if (!heap) {
            return 1;
        }
        was = mem_heap_use(heap);
        c = (Chunk *)mem_alloc(sizeof(Chunk));
        mem_heap_use(was);
        if (!c) {
            mem_heap_delete(heap);
            return 1;
        }
        mem_zero(c, sizeof(Chunk));
        c->heap = heap;
        c->b = b;
        out->data = c;
        out->step = STEP_BLOCK;
        return 0;
    }
    c = (Chunk *)out->data;
    switch (out->step) {
    case STEP_BLOCK:
        c->blk = read_block(c->b, c->heap);
        if (!c->blk) {
            log_print(LOG_WARN, "dunstream: block %s did not read", g_dun->block[c->b].name);
            return 1;
        }
        out->step = STEP_MODELS;
        return 0;
    case STEP_MODELS:
        place_models(c, out);
        out->step = STEP_HOLD;
        return 0;
    case STEP_HOLD:
        hold_pictures(c);
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
        out->step = STEP_DOORS;
        return 0;
    case STEP_DOORS:
        place_doors(c, out);
        out->step = STEP_FOES;
        return 0;
    case STEP_FOES:
        place_foes(c, out);
        out->step = STEP_WATER;
        return 0;
    default:
        place_water(c, out);
        return 1;
    }
}

static void drop(void *user, int cx, int cz, void *data)
{
    Chunk *c;
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
    mem_heap_delete(c->heap);
}

/*
 * A chunk came or went: its lights' pools (light.c), and, come, a
 * creature the stream's kept state says is dead is not drawn (live is
 * "drawn"; dead, the game's, is what keep put back).
 */
static void changed(void *user, int slot, int loaded)
{
    (void)user;
    light_stream_changed(slot, loaded);
    if (loaded) {
        Entity *e;
        int first;
        int n;
        int i;

        e = stream_slot_entities(slot, &first, &n);
        for (i = 0; e && i < n; i++) {
            if (e[i].dead) {
                e[i].live = 0;
            }
        }
    }
}

static void shift(void *user, float dx, float dz)
{
    (void)user;
    light_stream_shift(dx, dz);
}

/* The start block's marker 10, its first marker, or its middle: in its own frame (the stream's origin's). */
static int start_point(int b, float *x, float *y, float *z)
{
    MemHeap *heap;
    Block *blk;
    int i;

    heap = mem_heap_new();
    if (!heap) {
        return 0;
    }
    blk = read_block(b, heap);
    *x = BLOCK * 0.5f;
    *y = 0.0f;
    *z = BLOCK * 0.5f;
    if (blk && blk->nmarker > 0) {
        *x = blk->marker[0].x;
        *y = blk->marker[0].y;
        *z = blk->marker[0].z;
        for (i = 0; i < blk->nmarker; i++) {
            if ((int)blk->marker[i].record == START_RECORD) {
                *x = blk->marker[i].x;
                *y = blk->marker[i].y;
                *z = blk->marker[i].z;
                break;
            }
        }
    }
    mem_heap_delete(heap);
    return blk != 0;
}

int dunstream_open(const Dungeon *dungeon, Level *level, float *x, float *y, float *z, float *yaw, float *pitch)
{
    StreamSource src;
    const df_u8 *pal_bytes;
    df_u32 pal_size;
    int start;
    int b;

    dunstream_close();
    if (!dungeon || dungeon->nblocks < 1) {
        return 0;
    }
    start = -1;
    for (b = 0; b < dungeon->nblocks; b++) {
        if (dungeon->block[b].start) {
            start = b;
            break;
        }
    }
    if (start < 0) {
        start = 0;
    }
    g_dun = dungeon;
    dagger_bind_loaders();
    mem_zero(level, sizeof(Level));
    if (!swap_dungeon_init(&g_pics, dungeon->climate, dungeon->location_id)) {
        log_text(LOG_ERROR, "dunstream: no climate or location for the dungeon's pictures");
        return 0;
    }
    pal_bytes = vfs_read(PAL_NAME, &pal_size);
    if (!pal_bytes || !loader_call("col", PAL_NAME, pal_bytes, pal_size, &g_pal)) {
        log_text(LOG_ERROR, "dunstream: " PAL_NAME " did not read");
        return 0;
    }
    g_blocks = bsa_open("BLOCKS.BSA");
    g_arch = bsa_open("ARCH3D.BSA");
    if (!g_blocks || !g_arch) {
        log_text(LOG_ERROR, "dunstream: BLOCKS.BSA or ARCH3D.BSA did not open");
        return 0;
    }
    if (!wres_begin(&g_pal, level)) {
        return 0;
    }
    /* Classic's draws for the dungeon's random markers, once, by its location id and type; then every creature, in its blocks' order. */
    enemy_dungeon(dungeon->location_id, dungeon->dungeon_type, FOE_LEVEL);
    if (!draw_creatures()) {
        log_text(LOG_ERROR, "dunstream: out of memory for the creatures");
        return 0;
    }
    /* Its key's place word, as a whole load's (world.c): a dungeon, its location id. */
    g_keep_place = dungeon->location_id > 0 ? (1u << 30) | ((df_u32)dungeon->location_id & 0xFFFFFFu) : 0;
    level->palette = &g_pal;
    level->streamed = 1;
    sounds_scape(&g_scape, SCAPE_DUNGEON, dungeon->location_id);
    level->sound = &g_scape;
    g_level = level;
    if (!start_point(start, x, y, z)) {
        log_print(LOG_WARN, "dunstream: start block %s did not read", dungeon->block[start].name);
    }
    *yaw = 0.0f;
    *pitch = 0.0f;
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
    src.caps.entities = CAP_ENTITIES;
    src.caps.water = CAP_WATER;
    /* The start block's place in the layout is the origin chunk: the start is in its frame. */
    if (!stream_open_sized(&level->scene, &src, RADIUS, dungeon->block[start].x, dungeon->block[start].z, BLOCK)) {
        return 0;
    }
    if (!light_stream_begin(&level->scene)) {
        log_text(LOG_WARN, "dunstream: the dungeon's lights are unlit");
    }
    g_open = 1;
    log_print(LOG_INFO, "dunstream: %s open, %d blocks, at block %s", dungeon->title, dungeon->nblocks,
              dungeon->block[start].name);
    return 1;
}

void dunstream_fill(Camera *cam)
{
    int k;

    for (k = 0; g_open && k < 4000 && (k == 0 || stream_missing(RADIUS) > 0 || light_stream_pending() > 0); k++) {
        stream_update(cam, 50.0f);
        light_stream_service(50.0f);
    }
}

void dunstream_update(Camera *cam, float budget_ms)
{
    if (!g_open) {
        return;
    }
    stream_update(cam, budget_ms);
    light_stream_service(budget_ms);
}

void dunstream_close(void)
{
    if (!g_open) {
        return;
    }
    light_stream_end();
    stream_close();
    wres_end();
    if (g_blocks) {
        bsa_close(g_blocks);
        g_blocks = 0;
    }
    if (g_arch) {
        bsa_close(g_arch);
        g_arch = 0;
    }
    g_open = 0;
}

int dunstream_active(void)
{
    return g_open;
}
