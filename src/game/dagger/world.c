#include "game/dagger/world.h"
#include "game/dagger/bind.h"
#include "game/dagger/bsa.h"
#include "game/dagger/climate.h"
#include "game/dagger/doors.h"
#include "game/dagger/dungeon.h"
#include "game/dagger/enemies.h"
#include "game/dagger/rdb.h"
#include "game/dagger/rmb.h"
#include "game/dagger/skyfile.h"
#include "game/dagger/sounds.h"
#include "game/dagger/swaps.h"
#include "game/dagger/forecast.h"
#include "game/dagger/texture.h"
#include "engine/assets/loader.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/core/prof.h"
#include "engine/scene/entity.h"
#include "engine/scene/interact.h"
#include "engine/scene/walker.h"
#include "engine/gfx/texcache.h"
#include "engine/scene/clock.h"

/*
 * Privateer's Hold. MAPDITEM.017 places S0000999.RDB on the origin
 * tile and four border blocks around it. A tile is 2048 units. Model
 * and sprite positions in the RDB are relative to that tile. The camera
 * sits on editor marker 10, the new-game marker. File height is
 * mirrored so larger Y is up, matching the camera.
 */
#define PAL_NAME "ART_PAL.COL"
#define TILE 2048
#define RMB_TILE 4096
/* Family + 2 is the ground archive: 002, 102, 302, 402. */
#define TEX_SET_GROUND 2
#define START_RECORD 10
#define MAX_IMAGES 512
/* TEXTURE.210: the flats that give light, flames among them. */
#define LIGHT_FLATS 210
#define MAX_FILES 96
/* Buildings plus one ground patch per 512-unit square. 8192 is the cap. */
#define MAX_MODELS 8192
#define MAX_FLATS 8192
#define MAX_LIGHTS 8192
#define MAX_EMITTERS 2048
/* Doors, passages, levers and people; and what moves. */
#define MAX_USABLES 2048
#define MAX_MOVERS 1024
/* Editor marker 8 in a house: where a visitor stands on coming in. */
#define ENTER_RECORD 8
/* Interiors have no light records: each light flat is one, as outdoors (add_lamps). */
/* A dungeon's creature markers that place one; the hold has 4. */
#define MAX_FOES 1024
/* The player's level the random markers draw for: no player yet. */
#define FOE_LEVEL 1
/*
 * EnemyBasics' frame rates: walking 6 a second, flying 10, standing 4.
 * A creature with no standing pictures stands in its walking ones.
 */
#define FOE_MOVE_RATE 6
#define FOE_FLY_RATE 10
#define FOE_IDLE_RATE 4
/*
 * DFU's AlignControllerToGround: a ray from 0.2 m above the marker, 3 m
 * down (8 and 120 units), and the body's feet where it meets a floor.
 */
#define FOE_RAY_UP 8.0f
#define FOE_RAY_LEN 120.0f

static char g_why[96];
static char g_title[40];

static void why(const char *text)
{
    df_copy(g_why, text, (int)sizeof(g_why));
}

static void why_tex(int file, int image)
{
    char name[16];
    int n;

    texture_filename(file, name, (int)sizeof(name));
    n = 0;
    while (name[n] && n < 60) {
        g_why[n] = name[n];
        n++;
    }
    g_why[n++] = ' ';
    g_why[n++] = 'r';
    g_why[n++] = 'e';
    g_why[n++] = 'c';
    g_why[n++] = ' ';
    if (image >= 100) {
        g_why[n++] = (char)('0' + (image / 100) % 10);
    }
    if (image >= 10) {
        g_why[n++] = (char)('0' + (image / 10) % 10);
    }
    g_why[n++] = (char)('0' + image % 10);
    g_why[n] = 0;
}

struct World {
    Instance *inst;
    /* Beside inst: what the file said of each model, and the block it came from. */
    ModelAct *act;
    df_u8 *blk;
    Bound *bounds;
    Span *spans;
    int ninst;
    int nspans;
    int ntris;
    Flat *flat;
    int nflat;
    Light *light;
    int nlight;
    int neditor;
    Image *images[MAX_IMAGES];
    df_u16 key_file[MAX_IMAGES];
    df_u16 key_image[MAX_IMAGES];
    /* 1 for an animated record's sheet of frames: built padded, not padded again. */
    df_u8 sheet[MAX_IMAGES];
    /* cutout: a sprite uses it, so it uploads with alpha. wall: a model face does. */
    df_u8 cutout[MAX_IMAGES];
    df_u8 wall[MAX_IMAGES];
    /* A town's window pictures, lit at night (swap_window). */
    df_u8 glass[MAX_IMAGES];
    int nglass;
    int nimages;
    int files[MAX_FILES];
    int nfiles;
    Palette pal;
    float minx, miny, minz;
    float maxx, maxy, maxz;
    float start_x, start_y, start_z;
    float start_yaw, start_pitch;
    int have_start;
    float fall_x, fall_y, fall_z;
    int have_fall;
    /* Climate family, and the five archives for keys 119, 120, 122, 123, 124. */
    int tex_family;
    int tex_arch[5];
    /* An RMB place: under the sky, lit by the hour, its lamps the lights. */
    int outdoor;
    Sky sky;
    /* Its music and ambience, and the flats that make a sound (docs/sound.md). */
    Soundscape scape;
    Emitter *emit;
    int nemit;
    /* PLACE_*: a dungeon, a town, the inside of one of a town's buildings. */
    int place;
    /* The season it was built in (CLOCK_*): a town's pictures and sky are that season's. */
    int season;
    /* Flats stand on their file point (RMB, outside or in), not about it (RDB). */
    int base_flats;
    /* What can be used and what moves (docs/interact.md). */
    Usable *use;
    int nuse;
    Mover *mover;
    int nmover;
    /* Inside: the editor markers, and what the building is. */
    Marker *mark;
    int nmark;
    int building;
    int faction;
    /*
     * A dungeon's creatures: where each marker stands and its kind, as
     * the blocks are read; then, once their pictures are in the table,
     * an entity each (docs/entities.md).
     */
    struct Foe *foe;
    int nfoe;
    /*
     * A dungeon's water (docs/water.md): each block's level (world y),
     * read from its start marker as the blocks come; then the wet parts
     * of each block as rectangles.
     */
    float block_water[DUN_MAX_BLOCKS];
    df_u8 block_wet[DUN_MAX_BLOCKS];
    int block_gx[DUN_MAX_BLOCKS];
    int block_gz[DUN_MAX_BLOCKS];
    Water *water;
    int nwater;
    /* A dungeon's editor markers, every block's, and their blocks: teleports' targets. */
    Marker *dmark;
    df_u8 *dmark_blk;
    int ndmark;
    Entity *ent;
    df_i16 *ent_kind;
    df_u8 *ent_passive;
    /* FOE_*: its pictures still to read, read and drawn, or one did not read. */
    df_u8 *ent_state;
    int nent;
};

enum {
    FOE_PENDING,
    FOE_READY,
    FOE_FAILED
};

typedef struct Foe {
    float x, y, z;
    df_i16 kind;
    df_u8 female;
    df_u8 passive;
} Foe;

static int find_slot(const World *world, df_u16 file, df_u16 image)
{
    int i;

    for (i = 0; i < world->nimages; i++) {
        if (world->key_file[i] == file && world->key_image[i] == image) {
            return i;
        }
    }
    return -1;
}

static int add_file(World *world, int file)
{
    int i;

    for (i = 0; i < world->nfiles; i++) {
        if (world->files[i] == file) {
            return 1;
        }
    }
    if (world->nfiles >= MAX_FILES) {
        why("too many texture files");
        return 0;
    }
    world->files[world->nfiles++] = file;
    return 1;
}

/* DFU billboard size, in native units. Truncates toward zero, as a cast does. */
static int flat_extent(int pixels, int scale)
{
    return texture_extent(pixels, scale);
}

/* A record's frame through the "texture" loader. 0 when it cannot be read. */
static Image *load_frame(int file, int image, int frame)
{
    TexLoad req;

    req.file = file;
    req.image = image;
    req.frame = frame;
    req.out = 0;
    if (!loader_call("texture", 0, 0, 0, &req)) {
        return 0;
    }
    return req.out;
}

static int place_image(World *world, int file, int image, int cutout, int *slot_out)
{
    int slot;
    Image *img;

    slot = find_slot(world, (df_u16)file, (df_u16)image);
    if (slot >= 0) {
        if (cutout) {
            world->cutout[slot] = 1;
        } else {
            world->wall[slot] = 1;
        }
        *slot_out = slot;
        return 1;
    }
    if (world->nimages >= MAX_IMAGES) {
        why("too many textures");
        return 0;
    }
    img = load_frame(file, image, 0);
    if (!img) {
        why_tex(file, image);
        return 0;
    }
    if (!add_file(world, file)) {
        return 0;
    }
    slot = world->nimages;
    world->images[slot] = img;
    world->key_file[slot] = (df_u16)file;
    world->key_image[slot] = (df_u16)image;
    world->cutout[slot] = cutout ? 1 : 0;
    world->wall[slot] = cutout ? 0 : 1;
    world->nimages++;
    *slot_out = slot;
    return 1;
}

static void shift_block(Block *block, int gx, int gz, int tile)
{
    float dx;
    float dz;
    int i;

    dx = (float)(gx * tile);
    dz = (float)(gz * tile);
    for (i = 0; i < block->ninst; i++) {
        block->inst[i].x += dx;
        block->inst[i].z += dz;
    }
    for (i = 0; i < block->nflat; i++) {
        block->flat[i].x += dx;
        block->flat[i].z += dz;
    }
    for (i = 0; i < block->nlight; i++) {
        block->light[i].x += dx;
        block->light[i].z += dz;
    }
    for (i = 0; i < block->nmarker; i++) {
        block->marker[i].x += dx;
        block->marker[i].z += dz;
    }
}

static void cover_span(World *world, int first, int count)
{
    Span *sp;
    float minx, miny, minz, maxx, maxy, maxz;
    int i;

    if (count < 1 || world->nspans >= DUN_MAX_BLOCKS) {
        return;
    }
    minx = world->bounds[first].cx - world->bounds[first].ex;
    miny = world->bounds[first].cy - world->bounds[first].ey;
    minz = world->bounds[first].cz - world->bounds[first].ez;
    maxx = world->bounds[first].cx + world->bounds[first].ex;
    maxy = world->bounds[first].cy + world->bounds[first].ey;
    maxz = world->bounds[first].cz + world->bounds[first].ez;
    for (i = 1; i < count; i++) {
        Bound *b;
        float x0, y0, z0, x1, y1, z1;

        b = &world->bounds[first + i];
        x0 = b->cx - b->ex;
        y0 = b->cy - b->ey;
        z0 = b->cz - b->ez;
        x1 = b->cx + b->ex;
        y1 = b->cy + b->ey;
        z1 = b->cz + b->ez;
        if (x0 < minx) {
            minx = x0;
        }
        if (y0 < miny) {
            miny = y0;
        }
        if (z0 < minz) {
            minz = z0;
        }
        if (x1 > maxx) {
            maxx = x1;
        }
        if (y1 > maxy) {
            maxy = y1;
        }
        if (z1 > maxz) {
            maxz = z1;
        }
    }
    sp = &world->spans[world->nspans];
    sp->first = first;
    sp->count = count;
    sp->cx = (minx + maxx) * 0.5f;
    sp->cy = (miny + maxy) * 0.5f;
    sp->cz = (minz + maxz) * 0.5f;
    sp->ex = (maxx - minx) * 0.5f;
    sp->ey = (maxy - miny) * 0.5f;
    sp->ez = (maxz - minz) * 0.5f;
    world->nspans++;
}

static int append_block(World *world, Block *block, int index)
{
    int i;
    int first;

    if (world->ninst + block->ninst > MAX_MODELS || world->nflat + block->nflat > MAX_FLATS) {
        why("dungeon is too large");
        return 0;
    }
    first = world->ninst;
    for (i = 0; i < block->ninst; i++) {
        int at;

        at = world->ninst;
        world->inst[at] = block->inst[i];
        world->act[at] = block->act[i];
        world->blk[at] = (df_u8)index;
        if (!instance_world_box(&world->inst[at], &world->bounds[at])) {
            world->bounds[at].cx = world->inst[at].x;
            world->bounds[at].cy = world->inst[at].y;
            world->bounds[at].cz = world->inst[at].z;
            world->bounds[at].ex = 0.0f;
            world->bounds[at].ey = 0.0f;
            world->bounds[at].ez = 0.0f;
        }
        world->ninst++;
        world->ntris += block->inst[i].mesh->ntris;
    }
    cover_span(world, first, block->ninst);
    for (i = 0; i < block->nflat; i++) {
        world->flat[world->nflat++] = block->flat[i];
    }
    for (i = 0; i < block->nlight && world->nlight < MAX_LIGHTS; i++) {
        world->light[world->nlight++] = block->light[i];
    }
    world->neditor += block->neditor;
    return 1;
}

static void note_start(World *world, const Block *block)
{
    int i;

    for (i = 0; i < block->nmarker; i++) {
        if ((int)block->marker[i].record == START_RECORD) {
            world->start_x = block->marker[i].x;
            world->start_y = block->marker[i].y;
            world->start_z = block->marker[i].z;
            world->start_yaw = 0.0f;
            world->start_pitch = 0.0f;
            world->have_start = 1;
            return;
        }
    }
}

/* Entrance block, used when marker 10 is not in this dungeon. */
static void note_fall(World *world, const Block *block)
{
    if (world->have_fall || block->nmarker < 1) {
        return;
    }
    world->fall_x = block->marker[0].x;
    world->fall_y = block->marker[0].y;
    world->fall_z = block->marker[0].z;
    world->have_fall = 1;
}

static int make_sheet(World *world, int slot, int file, int image);

/*
 * A dungeon block's creature markers (records 15 and 16), each with the
 * kind it places: the block's own water level decides a random one's
 * draw and whether a water dweller has water (enemies.c).
 */
/* A dungeon's markers, for doors_build's teleports: every block's, as placed. */
#define MAX_DMARKS 4096

static int note_marks(World *world, const Block *block, int b)
{
    int i;

    if (!world->dmark) {
        world->dmark = (Marker *)mem_alloc(MAX_DMARKS * (df_u32)sizeof(Marker));
        world->dmark_blk = (df_u8 *)mem_alloc(MAX_DMARKS);
        if (!world->dmark || !world->dmark_blk) {
            why("out of memory");
            return 0;
        }
    }
    for (i = 0; i < block->nmarker && world->ndmark < MAX_DMARKS; i++) {
        world->dmark[world->ndmark] = block->marker[i];
        world->dmark_blk[world->ndmark] = (df_u8)b;
        world->ndmark++;
    }
    return 1;
}

static int note_foes(World *world, const Block *block)
{
    int water;
    int i;

    water = enemy_water(block->marker, block->nmarker);
    for (i = 0; i < block->nmarker; i++) {
        const Marker *mk;
        Foe *f;
        int kind;
        int female;
        int passive;

        mk = &block->marker[i];
        kind = enemy_at(mk, water, &female, &passive);
        if (kind < 0) {
            continue;
        }
        if (world->nfoe >= MAX_FOES) {
            log_print(LOG_WARN, "world: more than %d creatures; the rest left out\n", MAX_FOES);
            return 1;
        }
        if (!world->foe) {
            world->foe = (Foe *)mem_alloc(MAX_FOES * (df_u32)sizeof(Foe));
            if (!world->foe) {
                why("out of memory");
                return 0;
            }
        }
        f = &world->foe[world->nfoe++];
        f->x = mk->x;
        f->y = mk->y;
        f->z = mk->z;
        f->kind = (df_i16)kind;
        f->female = (df_u8)female;
        f->passive = (df_u8)passive;
    }
    return 1;
}

/*
 * A sprite picture's slot in the table, not read: a creature's, read
 * when it comes near (world_creatures_step). One already there is used
 * as it is. 0 when the table is full.
 */
static int reserve_image(World *world, int file, int image, int *slot_out)
{
    int slot;

    slot = find_slot(world, (df_u16)file, (df_u16)image);
    if (slot >= 0) {
        world->cutout[slot] = 1;
        *slot_out = slot;
        return 1;
    }
    if (world->nimages >= MAX_IMAGES || !add_file(world, file)) {
        return 0;
    }
    slot = world->nimages;
    world->images[slot] = 0;
    world->key_file[slot] = (df_u16)file;
    world->key_image[slot] = (df_u16)image;
    world->cutout[slot] = 1;
    world->wall[slot] = 0;
    world->nimages++;
    *slot_out = slot;
    return 1;
}

/*
 * One pose of a creature: records[k] of archive for view k, each a
 * sprite picture's slot, read later; its sizes come with its pictures.
 * 0 when the table is full: the creature is left out.
 */
static int foe_pose(World *world, int archive, const int *records, int rate, EntityPose *pose)
{
    int k;

    mem_zero(pose, sizeof(EntityPose));
    for (k = 0; k < ENTITY_VIEWS; k++) {
        int slot;

        if (!reserve_image(world, archive, records[k], &slot)) {
            return 0;
        }
        pose->slot[k] = (df_u16)slot;
    }
    pose->rate = (df_u8)rate;
    return 1;
}

/* The highest floor within FOE_RAY_LEN under (x, y + FOE_RAY_UP, z); 0 when none. */
static int foe_floor(const World *world, float x, float y, float z, float *floor_y)
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
    for (i = 0; i < world->ninst; i++) {
        const Bound *b;
        float t;

        b = &world->bounds[i];
        if (x < b->cx - b->ex || x > b->cx + b->ex || z < b->cz - b->ez || z > b->cz + b->ez
            || o[1] < b->cy - b->ey || o[1] - best > b->cy + b->ey) {
            continue;
        }
        if (instance_ray(&world->inst[i], o, d, best, &t) && t < best) {
            best = t;
        }
    }
    if (best >= FOE_RAY_LEN) {
        return 0;
    }
    *floor_y = o[1] - best;
    return 1;
}

/*
 * The creatures as entities, now the place's pictures are chosen: each
 * kind's walking records 0..4 and, if it has them, its standing 15..19,
 * with Daggerfall Unity's exceptions (MobileUnit's animation sets): a
 * woman of 483 (a thief, a bard, a burglar, an acrobat) stands turned a
 * little in record 11, as her 16 is broken; a rat's standing 16 to 18
 * and a ghost's or wraith's side-on 2 face the viewer's right. A flyer
 * or a ghost hangs where its marker is (the marker its middle); the
 * rest stand on the floor under it. Each faces one of eight ways, and
 * starts its frames, by where it stands: the same every load.
 */
static int add_foes(World *world)
{
    static const int walk[ENTITY_VIEWS] = { 0, 1, 2, 3, 4 };
    static const int idle[ENTITY_VIEWS] = { 15, 16, 17, 18, 19 };
    static const int idle_483[ENTITY_VIEWS] = { 15, 11, 17, 18, 19 };
    int i;

    if (world->nfoe < 1) {
        return 1;
    }
    world->ent = (Entity *)mem_alloc((df_u32)world->nfoe * (df_u32)sizeof(Entity));
    world->ent_kind = (df_i16 *)mem_alloc((df_u32)world->nfoe * (df_u32)sizeof(df_i16));
    world->ent_passive = (df_u8 *)mem_alloc((df_u32)world->nfoe);
    world->ent_state = (df_u8 *)mem_alloc((df_u32)world->nfoe);
    if (!world->ent || !world->ent_kind || !world->ent_passive || !world->ent_state) {
        why("out of memory");
        return 0;
    }
    for (i = 0; i < world->nfoe; i++) {
        const Foe *f;
        Entity *e;
        int archive;
        int kind;
        unsigned int hash;

        f = &world->foe[i];
        kind = (int)f->kind;
        archive = enemy_archive(kind, f->female);
        e = &world->ent[world->nent];
        mem_zero(e, sizeof(Entity));
        if (!foe_pose(world, archive, walk, enemy_flies(kind) ? FOE_FLY_RATE : FOE_MOVE_RATE, &e->walk)) {
            continue;
        }
        if (kind == ENEMY_GHOST || kind == ENEMY_WRAITH) {
            e->walk.right = 1u << 2;
        }
        e->idle.slot[0] = 0xffff;
        if (enemy_has_idle(kind)) {
            if (!foe_pose(world, archive, f->female && archive == 483 ? idle_483 : idle,
                          FOE_IDLE_RATE, &e->idle)) {
                continue;
            }
            if (kind == ENEMY_RAT) {
                e->idle.right = (1u << 1) | (1u << 2) | (1u << 3);
            }
        }
        e->x = f->x;
        e->y = f->y;
        e->z = f->z;
        hash = (unsigned int)df_iround(f->x) * 7u + (unsigned int)df_iround(f->z) * 13u;
        e->yaw = (float)(int)((hash >> 3) & 7u) * (DF_TAU / 8.0f);
        e->phase = (df_u8)(hash & 255u);
        e->moving = 0;
        /* Not drawn until its pictures are read (world_creatures_step). */
        e->live = 0;
        world->ent_kind[world->nent] = (df_i16)kind;
        world->ent_passive[world->nent] = f->passive;
        world->ent_state[world->nent] = FOE_PENDING;
        world->nent++;
    }
    return 1;
}

/* Every picture slot of entity e's two poses, in turn: k from 0; -1 past the last. */
static int foe_slot(const Entity *e, int k)
{
    const EntityPose *pose;

    if (k >= 2 * ENTITY_VIEWS) {
        return -1;
    }
    pose = k < ENTITY_VIEWS ? &e->walk : &e->idle;
    return pose->slot[k % ENTITY_VIEWS] == 0xffff ? -2 : (int)pose->slot[k % ENTITY_VIEWS];
}

/* Its pictures read: each view's size from its record, standing on its floor (a flyer at its marker), drawn. */
static void foe_finish(World *world, int n)
{
    Entity *e;
    int kind;
    int p;
    float ground;

    e = &world->ent[n];
    kind = (int)world->ent_kind[n];
    for (p = 0; p < 2; p++) {
        EntityPose *pose;
        int k;

        pose = p ? &e->idle : &e->walk;
        for (k = 0; k < ENTITY_VIEWS; k++) {
            const Image *img;

            if (pose->slot[k] == 0xffff) {
                continue;
            }
            img = world->images[pose->slot[k]];
            pose->w[k] = (df_u16)flat_extent(img->width, img->xscale);
            pose->h[k] = (df_u16)flat_extent(img->height, img->yscale);
        }
    }
    /* e->y is still its marker's point. */
    if (!(enemy_flies(kind) || enemy_spectral(kind)) && foe_floor(world, e->x, e->y, e->z, &ground)) {
        e->y = ground;
    } else {
        e->y -= (float)entity_pose(e)->h[0] * 0.5f;
    }
    e->live = 1;
    world->ent_state[n] = FOE_READY;
}

/*
 * The next part of reading entity n's pictures: a 32 KB piece of a
 * TEXTURE file (texture_file_step), or one picture decoded, laid out as
 * a sheet of its frames or padded, and put on the card by distance
 * (texcache_resident_add); with every one read, the creature is
 * finished. A record that does not read leaves the creature out.
 */
static void foe_part(World *world, int n)
{
    const Entity *e;
    int k;

    e = &world->ent[n];
    for (k = 0;; k++) {
        Image *img;
        int slot;
        int file;
        int image;
        int extent;

        slot = foe_slot(e, k);
        if (slot == -1) {
            break;
        }
        if (slot < 0 || world->images[slot]) {
            continue;
        }
        file = (int)world->key_file[slot];
        image = (int)world->key_image[slot];
        if (!texture_file_step(file)) {
            return;
        }
        img = load_frame(file, image, 0);
        if (!img) {
            world->ent_state[n] = FOE_FAILED;
            log_print(LOG_WARN, "world: a creature's picture TEXTURE.%d record %d did not read", file, image);
            return;
        }
        world->images[slot] = img;
        if (!make_sheet(world, slot, file, image)) {
            texture_pad(img);
        }
        extent = flat_extent(img->width, img->xscale);
        texcache_resident_add(slot, img, 1, extent > 0 ? (float)img->width / (float)extent : 1.0f);
        return;
    }
    foe_finish(world, n);
}

int world_creatures_step(World *world, const Camera *cam, float radius, float budget_ms)
{
    df_u32 start;
    df_u32 budget;
    int left;

    if (!world || world->nent < 1) {
        return 0;
    }
    start = prof_now();
    budget = budget_ms > 0.0f && prof_per_ms() ? (df_u32)df_iround(budget_ms * (float)prof_per_ms()) : 0u;
    for (;;) {
        float best_d;
        int best;
        int i;

        /* The nearest still to read, within radius (0: any); every one with no camera. */
        best = -1;
        best_d = 0.0f;
        left = 0;
        for (i = 0; i < world->nent; i++) {
            float dx;
            float dz;
            float d;

            if (world->ent_state[i] != FOE_PENDING) {
                continue;
            }
            d = 0.0f;
            if (cam) {
                dx = world->ent[i].x - cam->x;
                dz = world->ent[i].z - cam->z;
                d = dx * dx + dz * dz;
                if (radius > 0.0f && d > radius * radius) {
                    continue;
                }
            }
            left++;
            if (best < 0 || d < best_d) {
                best = i;
                best_d = d;
            }
        }
        if (best < 0) {
            return 0;
        }
        foe_part(world, best);
        /* With a budget, until it is spent (a part at least); with none, until all are read. */
        if (budget_ms > 0.0f && (budget == 0u || prof_now() - start >= budget)) {
            return left;
        }
    }
}

/*
 * A block's water over the parts of it that have rooms under its level:
 * its square in cells of WATER_CELL, a cell wet when a model of the block
 * meets it and reaches below the level (a room or a passage it floods);
 * solid rock and rooms above the water have none, so the surface is not
 * drawn under every floor of the block, a full screen of fill on the
 * Voodoo. Each row's runs of wet cells are one rectangle.
 */
#define WATER_CELL 256
#define WATER_CELLS (TILE / WATER_CELL)
/* Dungeon water: a murky green-blue, its surface and the fog under it. */
#define DUNGEON_WATER 0x1c4652u
#define MAX_WATER 1024

static int add_water(World *world, int nblocks)
{
    int b;

    for (b = 0; b < nblocks; b++) {
        df_u8 wet[WATER_CELLS][WATER_CELLS];
        float x0;
        float z0;
        int i;
        int cx;
        int cz;

        if (!world->block_wet[b]) {
            continue;
        }
        x0 = (float)(world->block_gx[b] * TILE);
        z0 = (float)(world->block_gz[b] * TILE);
        for (cz = 0; cz < WATER_CELLS; cz++) {
            for (cx = 0; cx < WATER_CELLS; cx++) {
                wet[cz][cx] = 0;
            }
        }
        for (i = 0; i < world->ninst; i++) {
            const Bound *bd;
            int ax, az, bx, bz;

            bd = &world->bounds[i];
            if (world->blk[i] != b || bd->cy - bd->ey >= world->block_water[b]) {
                continue;
            }
            ax = df_ifloor((bd->cx - bd->ex - x0) / (float)WATER_CELL);
            bx = df_ifloor((bd->cx + bd->ex - x0) / (float)WATER_CELL);
            az = df_ifloor((bd->cz - bd->ez - z0) / (float)WATER_CELL);
            bz = df_ifloor((bd->cz + bd->ez - z0) / (float)WATER_CELL);
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
                if (!world->water) {
                    world->water = (Water *)mem_alloc(MAX_WATER * (df_u32)sizeof(Water));
                    if (!world->water) {
                        why("out of memory");
                        return 0;
                    }
                }
                if (world->nwater < MAX_WATER) {
                    w = &world->water[world->nwater++];
                    w->x0 = x0 + (float)(cx * WATER_CELL);
                    w->x1 = x0 + (float)(run * WATER_CELL);
                    w->z0 = z0 + (float)(cz * WATER_CELL);
                    w->z1 = w->z0 + (float)WATER_CELL;
                    w->y = world->block_water[b];
                    w->color = DUNGEON_WATER;
                }
                cx = run;
            }
        }
    }
    return 1;
}

static void measure(World *world)
{
    int i;
    int first;

    first = 1;
    for (i = 0; i < world->ninst; i++) {
        const Instance *in;
        int t;

        in = &world->inst[i];
        for (t = 0; t < in->mesh->ntris; t++) {
            const Tri *tri;
            int k;

            tri = &in->mesh->tris[t];
            for (k = 0; k < 3; k++) {
                float x, y, z;

                instance_point(in, tri->x[k], tri->y[k], tri->z[k], &x, &y, &z);
                if (first) {
                    world->minx = world->maxx = x;
                    world->miny = world->maxy = y;
                    world->minz = world->maxz = z;
                    first = 0;
                }
                if (x < world->minx) world->minx = x;
                if (y < world->miny) world->miny = y;
                if (z < world->minz) world->minz = z;
                if (x > world->maxx) world->maxx = x;
                if (y > world->maxy) world->maxy = y;
                if (z > world->maxz) world->maxz = z;
            }
        }
    }
}

/*
 * Watcom's rand: seed = seed * 0x41C64E6D + 0x3039, result is the high
 * 15 bits. One dungeon seeds it once with the location id.
 */
static df_u32 g_rng;

static int watcom(void)
{
    g_rng = g_rng * 0x41C64E6Du + 0x3039u;
    return (int)((g_rng >> 16) & 0x7fffu);
}

/* True when archive is that set in one of the four real families. */
static int same_set(int archive, int set)
{
    int family;

    if (archive < set) {
        return 0;
    }
    family = archive - set;
    return family == TEX_DESERT || family == TEX_MOUNTAIN
        || family == TEX_WOODLAND || family == TEX_SWAMP;
}

/*
 * Keys 119, 120, 122, 123, 124 each draw an offset 0..4. A draw of 2
 * becomes 4, because archive numbers skip one slot. The offset is added
 * to this climate's crypt base (family + 19). Doors, sewers, and caves
 * take the same family. A door record above 2 stays as stored. A cave
 * record the climate file does not contain stays on the archive the
 * face named, so paint does not abort.
 */
static int build_tex_table(World *world, const Dungeon *hold)
{
    int family;
    int i;

    family = climate_family(hold->climate);
    if (family < 0 || hold->location_id <= 0) {
        why("climate");
        return 0;
    }
    world->tex_family = family;
    g_rng = (df_u32)hold->location_id;
    for (i = 0; i < 5; i++) {
        int off;

        off = watcom() % 5;
        if (off == 2) {
            off = 4;
        }
        world->tex_arch[i] = family + TEX_SET_CRYPT + off;
    }
    return 1;
}

static int face_archive(const World *world, int archive, int record)
{
    static const int keys[5] = {119, 120, 122, 123, 124};
    int i;
    int set;

    /* Towns and houses: the place's climate, and a town's season (swaps.c). */
    if (world->place != PLACE_DUNGEON) {
        return swap_climate(archive, record, world->tex_family,
                            world->place == PLACE_TOWN && world->season == CLOCK_WINTER,
                            texture_record_count);
    }

    if (same_set(archive, TEX_SET_DOOR)) {
        if (record > 2) {
            return archive;
        }
        return world->tex_family + TEX_SET_DOOR;
    }
    for (i = 0; i < 5; i++) {
        if (archive == keys[i]) {
            return world->tex_arch[i];
        }
    }
    if (same_set(archive, TEX_SET_SEWER)) {
        return world->tex_family + TEX_SET_SEWER;
    }
    if (same_set(archive, TEX_SET_CAVE)) {
        set = world->tex_family + TEX_SET_CAVE;
        if (record >= 0 && record < texture_record_count(set)) {
            return set;
        }
    }
    return archive;
}

/* Unique texture slots, so a camera step does not rescan every triangle. */
static void note_keys(Mesh *mesh)
{
    df_u8 seen[256];
    df_u16 tmp[256];
    int t;
    int n;

    if (!mesh || mesh->have_keys) {
        return;
    }
    n = 0;
    for (t = 0; t < 256; t++) {
        seen[t] = 0;
    }
    for (t = 0; t < mesh->ntris; t++) {
        int slot;

        slot = (int)mesh->tris[t].slot;
        if (slot < 0 || slot >= 256 || seen[slot]) {
            continue;
        }
        seen[slot] = 1;
        tmp[n++] = (df_u16)slot;
    }
    if (n > 0) {
        mesh->slot_keys = (df_u16 *)mem_alloc((df_u32)n * (df_u32)sizeof(df_u16));
        if (!mesh->slot_keys) {
            return;
        }
        for (t = 0; t < n; t++) {
            mesh->slot_keys[t] = tmp[t];
        }
    }
    mesh->nslot_keys = n;
    mesh->have_keys = 1;
}

/*
 * Daggerfall's animated flats are records of several frames: flames,
 * waterfalls, townsfolk, animals. A record's frames go into one sheet:
 * side by side, SHEET_GAP texels apart, in the smallest power-of-two
 * square that holds them, which for a narrow flame is the square its
 * first frame was padded into anyway. The engine shows frame k by moving
 * the texture coordinates (Image.frames, frame_cols, frame_dx, frame_dy).
 * A slot per frame cost the hold 36 more pictures and 440 KB, the
 * padding of each frame's square; the sheet costs none for most.
 *
 * Flames (TEXTURE.210) run at 12 frames a second and everything else at
 * 5, as Daggerfall Unity runs them; each flat starts at its own frame, so
 * the candles in a room do not move in step. A record whose frames do not
 * all load, or do not fit a 256 sheet, stays still.
 */
#define FLAME_RATE 12
#define FLAT_RATE 5

static int make_sheet(World *world, int slot, int file, int image)
{
    Image *img;

    img = world->images[slot];
    if (world->sheet[slot]) {
        return img->frames > 1;
    }
    world->sheet[slot] = (df_u8)texture_sheet(img, file, image);
    return world->sheet[slot];
}

static void animate(World *world, Flat *fl, int slot)
{
    fl->rate = 0;
    fl->phase = 0;
    if (!make_sheet(world, slot, (int)fl->file, (int)fl->image)) {
        return;
    }
    fl->rate = (df_u8)(fl->file == LIGHT_FLATS ? FLAME_RATE : FLAT_RATE);
    /* Where it stands picks where it starts: the same every load. */
    fl->phase = (df_u8)(((unsigned int)df_iround(fl->x) * 7u + (unsigned int)df_iround(fl->z) * 13u) & 255u);
}

static int paint(World *world)
{
    int i;
    int t;

    for (i = 0; i < world->ninst; i++) {
        Mesh *mesh;

        mesh = world->inst[i].mesh;
        for (t = 0; t < mesh->ntris; t++) {
            Tri *tri;
            int slot;
            int archive;

            tri = &mesh->tris[t];
            archive = face_archive(world, (int)tri->tex_file, (int)tri->tex_image);
            if (!place_image(world, archive, (int)tri->tex_image, 0, &slot)) {
                return 0;
            }
            tri->slot = (df_u16)slot;
        }
    }
    for (i = 0; i < world->ninst; i++) {
        note_keys(world->inst[i].mesh);
    }
    {
        int f;
        int kept;

        kept = 0;
        for (f = 0; f < world->nflat; f++) {
            Flat *fl;
            int slot;
            const Image *img;
            int extent_w;
            int extent_h;

            fl = &world->flat[f];
            if (!place_image(world, (int)fl->file, (int)fl->image, 1, &slot)) {
                if (world->nimages >= MAX_IMAGES || world->nfiles >= MAX_FILES) {
                    return 0;
                }
                continue;
            }
            animate(world, fl, slot);
            img = world->images[slot];
            extent_w = flat_extent(img->width, img->xscale);
            extent_h = flat_extent(img->height, img->yscale);
            if (extent_w < 1 || extent_h < 1) {
                continue;
            }
            fl->w = (float)extent_w;
            fl->h = (float)extent_h;
            /*
             * The quad is drawn from its centre. An RMB flat's file point
             * is its base, so outdoors the centre sits half a height above
             * it; without this a lamp post is half under the street. An
             * RDB flat's point is already its centre, as Daggerfall Unity
             * places them (only RMBLayout calls AlignBillboardToBase):
             * lifting those too set every dungeon sprite half its height
             * in the air. The data agrees: a dungeon flat standing on a
             * floor has its point at mid-height, a town's at its base.
             */
            if (world->base_flats) {
                fl->y += (float)extent_h * 0.5f;
            }
            fl->slot = (df_u16)slot;
            if (kept != f) {
                world->flat[kept] = *fl;
            }
            kept++;
        }
        world->nflat = kept;
    }
    return 1;
}

/*
 * Now that each image's use is known, and the palette is loaded: a
 * picture only sprites use is padded at its own size, not stretched
 * and repeated (texture_pad). A wall picture whose sides are not
 * powers of two gets its stretch redone smoothly (texture_smooth). A
 * picture both use stays a repeating square, since the wall needs it
 * to tile; its sprites draw from the first copy as before.
 */
static void finish_images(World *world)
{
    int i;
    int padded;
    int smoothed;
    int shared;

    padded = 0;
    smoothed = 0;
    shared = 0;
    for (i = 0; i < world->nimages; i++) {
        Image *img;

        img = world->images[i];
        if (!img) {
            continue;
        }
        if (world->cutout[i] && world->wall[i]) {
            shared++;
        }
        if (world->sheet[i]) {
            padded++;
        } else if (world->cutout[i] && !world->wall[i]) {
            texture_pad(img);
            padded++;
        } else if (img->span_w != img->width || img->span_h != img->height) {
            texture_smooth(img, world->pal.r, world->pal.g, world->pal.b);
            smoothed++;
        }
    }
    log_print(LOG_DEBUG, "textures: %d sprites padded, %d walls smoothed, %d used as both",
              padded, smoothed, shared);
}

/*
 * Which of a dungeon's lights flicker. The records do not say; the
 * pictures do. TEXTURE.210's flames, the torches, candles, braziers and
 * fires, are records of several frames, and its lanterns, chandeliers
 * and lamp posts a single one. So a light flickers when the light flat
 * nearest it, within FLAME_NEAR, is a moving flame; a light with none
 * near lights its room steadily. After paint, which builds the sheets.
 */
#define FLAME_NEAR 64.0f

static void flame_lights(World *world)
{
    int i;

    for (i = 0; i < world->nlight; i++) {
        Light *li;
        float best;
        int f;

        li = &world->light[i];
        li->flicker = 0;
        best = FLAME_NEAR * FLAME_NEAR;
        for (f = 0; f < world->nflat; f++) {
            const Flat *fl;
            float dx, dy, dz, d2;

            fl = &world->flat[f];
            if (fl->file != LIGHT_FLATS) {
                continue;
            }
            dx = fl->x - li->x;
            dy = fl->y - li->y;
            dz = fl->z - li->z;
            d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < best) {
                best = d2;
                li->flicker = (df_u8)(world->images[fl->slot]->frames > 1);
            }
        }
    }
}

/*
 * RMB blocks carry no light records. Outdoors the lights are the
 * TEXTURE.210 flats, the lamp posts, braziers and fires: one each, a
 * quarter of the flat's height above its centre, where the flame is.
 * After paint, which sizes the flats. See docs/daylight.md.
 */
#define LAMP_RADIUS 160.0f

/*
 * A sound from each flat that makes one: a flaming torch's fire, an
 * animal's call (sounds_flat). At the flat's centre, where it is drawn.
 */
static void add_emitters(World *world)
{
    int i;

    world->nemit = 0;
    world->emit = (Emitter *)mem_alloc(MAX_EMITTERS * (df_u32)sizeof(Emitter));
    if (!world->emit) {
        return;
    }
    for (i = 0; i < world->nflat && world->nemit < MAX_EMITTERS; i++) {
        const Flat *fl;

        fl = &world->flat[i];
        if (sounds_flat((int)fl->file, (int)fl->image, fl->x, fl->y, fl->z,
                        &world->emit[world->nemit])) {
            world->nemit++;
        }
    }
}

static void add_lamps(World *world)
{
    int i;

    for (i = 0; i < world->nflat && world->nlight < MAX_LIGHTS; i++) {
        const Flat *fl;
        Light *li;

        fl = &world->flat[i];
        if (!fl->glow) {
            continue;
        }
        li = &world->light[world->nlight++];
        li->x = fl->x;
        li->y = fl->y + fl->h * 0.25f;
        li->z = fl->z;
        li->radius = LAMP_RADIUS;
        /* A brazier or a fire flickers; a lantern on its post burns steady. */
        li->flicker = (df_u8)(world->images[fl->slot]->frames > 1);
    }
}

/*
 * Every mover's model box covers its whole sweep, for the cull and the
 * collision grid, and its block's span grows to hold it.
 */
static void sweep_movers(World *world)
{
    int i;

    for (i = 0; i < world->nmover; i++) {
        const Mover *m;
        Bound b;
        int s;

        m = &world->mover[i];
        if (!mover_sweep(m, &b)) {
            continue;
        }
        world->bounds[m->model] = b;
        for (s = 0; s < world->nspans; s++) {
            Span *sp;
            float lo, hi;

            sp = &world->spans[s];
            if (m->model < sp->first || m->model >= sp->first + sp->count) {
                continue;
            }
            lo = sp->cx - sp->ex < b.cx - b.ex ? sp->cx - sp->ex : b.cx - b.ex;
            hi = sp->cx + sp->ex > b.cx + b.ex ? sp->cx + sp->ex : b.cx + b.ex;
            sp->cx = (lo + hi) * 0.5f;
            sp->ex = (hi - lo) * 0.5f;
            lo = sp->cy - sp->ey < b.cy - b.ey ? sp->cy - sp->ey : b.cy - b.ey;
            hi = sp->cy + sp->ey > b.cy + b.ey ? sp->cy + sp->ey : b.cy + b.ey;
            sp->cy = (lo + hi) * 0.5f;
            sp->ey = (hi - lo) * 0.5f;
            lo = sp->cz - sp->ez < b.cz - b.ez ? sp->cz - sp->ez : b.cz - b.ez;
            hi = sp->cz + sp->ez > b.cz + b.ez ? sp->cz + sp->ez : b.cz + b.ez;
            sp->cz = (lo + hi) * 0.5f;
            sp->ez = (hi - lo) * 0.5f;
        }
    }
}

/* The doors, passages, levers and people, once the flats are final. */
static int add_doors(World *world)
{
    DoorBuild b;

    world->use = (Usable *)mem_alloc(MAX_USABLES * (df_u32)sizeof(Usable));
    world->mover = (Mover *)mem_alloc(MAX_MOVERS * (df_u32)sizeof(Mover));
    if (!world->use || !world->mover) {
        why("out of memory");
        return 0;
    }
    b.inst = world->inst;
    b.act = world->act;
    b.block = world->blk;
    b.ninst = world->ninst;
    b.flat = world->flat;
    b.nflat = world->nflat;
    b.mark = world->dmark;
    b.mark_block = world->dmark_blk;
    b.nmark = world->ndmark;
    b.place = world->place;
    b.use = world->use;
    b.use_cap = MAX_USABLES;
    b.mover = world->mover;
    b.mover_cap = MAX_MOVERS;
    if (!doors_build(&b)) {
        why("out of memory");
        return 0;
    }
    world->nuse = b.nuse;
    world->nmover = b.nmover;
    /*
     * A teleport's target is a marker's point, mid-height of its flat:
     * the eye goes a body's eye above the floor under it, as a walker
     * stands there (FOE_RAY's reach; with no floor, the point itself).
     */
    {
        int i;

        for (i = 0; i < world->nmover; i++) {
            Mover *m;
            float floor_y;

            m = &world->mover[i];
            if (m->teleport && foe_floor(world, m->to_x, m->to_y, m->to_z, &floor_y)) {
                m->to_y = floor_y + WALK_EYE;
            }
        }
    }
    sweep_movers(world);
    log_print(LOG_DEBUG, "doors: %d to use, %d that move", world->nuse, world->nmover);
    return 1;
}

/*
 * A house's start: its enter marker (199 record 8; any marker if it has
 * none), a body's eye above the floor point.
 */
#define INSIDE_EYE 64.0f

static void note_inside_start(World *world)
{
    int i;
    int pick;

    pick = -1;
    for (i = 0; i < world->nmark; i++) {
        if (world->mark[i].record == ENTER_RECORD) {
            pick = i;
            break;
        }
    }
    if (pick < 0 && world->nmark > 0) {
        pick = 0;
    }
    if (pick < 0) {
        return;
    }
    world->start_x = world->mark[pick].x;
    world->start_y = world->mark[pick].y + INSIDE_EYE;
    world->start_z = world->mark[pick].z;
    world->start_yaw = 0.0f;
    world->start_pitch = 0.0f;
    world->have_start = 1;
}

/*
 * One place: every block of hold, or with inside_block >= 0, the inside
 * of building inside_record of that one RMB block, placed where the
 * building stands in the town.
 */
/*
 * A town's window pictures: their glass is lit after dark (draw.c's
 * night pass). Not a house's: from inside, Daggerfall Unity leaves them.
 */
static void mark_glass(World *world)
{
    int i;

    world->nglass = 0;
    for (i = 0; i < world->nimages; i++) {
        world->glass[i] = (df_u8)(world->wall[i] && !world->cutout[i]
                                  && swap_window((int)world->key_file[i], (int)world->key_image[i]));
        world->nglass += world->glass[i];
    }
}

/*
 * The last load's stages in cycles (world_load_times): each lap adds the
 * cycles since the one before to its stage. LEVELFALL logs them.
 */
static WorldLoadTimes g_times;
static df_u32 g_lap;

static void lap(df_u32 *stage)
{
    df_u32 now;

    now = prof_now();
    *stage += now - g_lap;
    g_lap = now;
}

void world_load_times(WorldLoadTimes *out)
{
    *out = g_times;
}

static World *assemble(const Dungeon *hold, int require_start, int inside_block, int inside_record)
{
    World *world;
    Bsa *arch;
    Bsa *blocks;
    ArchEnt *cache;
    int ncache;
    int b;
    int tile;

    why("load failed");
    mem_zero(&g_times, sizeof(g_times));
    g_lap = prof_now();
    dagger_bind_loaders();
    if (!hold || hold->nblocks < 1) {
        why("dungeon");
        return 0;
    }
    df_copy(g_title, hold->title, (int)sizeof(g_title));
    world = (World *)mem_alloc(sizeof(World));
    if (!world) {
        why("out of memory");
        return 0;
    }
    mem_zero(world, sizeof(World));
    world->place = inside_block >= 0 ? PLACE_INSIDE
        : (hold->outdoor ? PLACE_TOWN : PLACE_DUNGEON);
    world->season = clock_season();
    world->outdoor = world->place == PLACE_TOWN;
    /* Set now: paint places flats by it. */
    world->base_flats = hold->outdoor;
    world->inst = (Instance *)mem_alloc(MAX_MODELS * (df_u32)sizeof(Instance));
    world->bounds = (Bound *)mem_alloc(MAX_MODELS * (df_u32)sizeof(Bound));
    world->spans = (Span *)mem_alloc(DUN_MAX_BLOCKS * (df_u32)sizeof(Span));
    world->flat = (Flat *)mem_alloc(MAX_FLATS * (df_u32)sizeof(Flat));
    world->light = (Light *)mem_alloc(MAX_LIGHTS * (df_u32)sizeof(Light));
    world->act = (ModelAct *)mem_alloc(MAX_MODELS * (df_u32)sizeof(ModelAct));
    world->blk = (df_u8 *)mem_alloc(MAX_MODELS);
    if (!world->inst || !world->bounds || !world->spans || !world->flat || !world->light
        || !world->act || !world->blk) {
        why("out of memory");
        return 0;
    }
    tile = hold->outdoor ? RMB_TILE : TILE;
    arch = bsa_open("ARCH3D.BSA");
    if (!arch) {
        why("ARCH3D.BSA");
        return 0;
    }
    blocks = bsa_open("BLOCKS.BSA");
    if (!blocks) {
        why("BLOCKS.BSA");
        bsa_close(arch);
        return 0;
    }
    cache = (ArchEnt *)mem_alloc((df_u32)ARCH_CACHE * (df_u32)sizeof(ArchEnt));
    if (!cache) {
        why("out of memory");
        bsa_close(blocks);
        bsa_close(arch);
        return 0;
    }
    ncache = 0;
    if (world->place == PLACE_DUNGEON) {
        /* Classic's draws for the random markers, before any block is read. */
        enemy_dungeon(hold->location_id, hold->dungeon_type, FOE_LEVEL);
    }
    for (b = 0; b < hold->nblocks; b++) {
        const BsaRec *rec;
        df_u8 *rdb_bytes;
        Block *block;

        if (inside_block >= 0 && b != inside_block) {
            continue;
        }
        rec = bsa_find_name(blocks, hold->block[b].name);
        if (!rec) {
            why(hold->block[b].name);
            bsa_close(blocks);
            bsa_close(arch);
            return 0;
        }
        rdb_bytes = bsa_read_scratch(blocks, rec, BSA_SCRATCH_BLOCK);
        if (!rdb_bytes) {
            why(hold->block[b].name);
            bsa_close(blocks);
            bsa_close(arch);
            return 0;
        }
        if (inside_block >= 0) {
            block = rmb_load_interior(rdb_bytes, rec->size, arch, inside_record, cache, &ncache);
            if (!rmb_building(rdb_bytes, rec->size, inside_record,
                              &world->building, &world->faction)) {
                world->building = -1;
                world->faction = 0;
            }
        } else if (hold->outdoor) {
            block = rmb_load(rdb_bytes, rec->size, arch,
                             climate_family(hold->climate) + TEX_SET_GROUND,
                             cache, &ncache);
        } else {
            block = rdb_load(rdb_bytes, rec->size, arch, cache, &ncache);
        }
        if (!block) {
            why(hold->block[b].name);
            bsa_close(blocks);
            bsa_close(arch);
            return 0;
        }
        shift_block(block, hold->block[b].x, hold->block[b].z, tile);
        if (inside_block >= 0) {
            world->mark = block->marker;
            world->nmark = block->nmarker;
        }
        if (world->place == PLACE_DUNGEON) {
            int level;

            /* Its start marker's water: classic's height, down positive, so the world's y is its negative. */
            level = enemy_water(block->marker, block->nmarker);
            world->block_wet[b] = (df_u8)(level != ENEMY_DRY);
            world->block_water[b] = (float)-level;
            world->block_gx[b] = hold->block[b].x;
            world->block_gz[b] = hold->block[b].z;
        }
        if (world->place == PLACE_DUNGEON && (!note_foes(world, block) || !note_marks(world, block, b))) {
            bsa_close(blocks);
            bsa_close(arch);
            return 0;
        }
        if (hold->block[b].start) {
            note_start(world, block);
            if (!world->have_start) {
                note_fall(world, block);
            }
        }
        if (!append_block(world, block, b)) {
            bsa_close(blocks);
            bsa_close(arch);
            return 0;
        }
    }
    bsa_close(blocks);
    bsa_close(arch);
    lap(&g_times.blocks);
    if (world->ninst < 1 || world->ntris < 1) {
        why("dungeon has no models");
        return 0;
    }
    if (world->place == PLACE_INSIDE) {
        note_inside_start(world);
    }
    if (!world->have_start) {
        if (require_start) {
            why("start marker");
            return 0;
        }
        if (world->have_fall) {
            world->start_x = world->fall_x;
            world->start_y = world->fall_y;
            world->start_z = world->fall_z;
            world->start_yaw = 0.0f;
            world->start_pitch = 0.0f;
            world->have_start = 1;
        }
    }
    lap(&g_times.rest);
    if (!build_tex_table(world, hold)) {
        return 0;
    }
    if (!paint(world)) {
        return 0;
    }
    lap(&g_times.pictures);
    if (!add_foes(world)) {
        return 0;
    }
    lap(&g_times.creatures);
    if (world->place == PLACE_DUNGEON && !add_water(world, hold->nblocks)) {
        return 0;
    }
    lap(&g_times.water);
    measure(world);
    if (world->maxx <= world->minx || world->maxy <= world->miny) {
        why("dungeon has no extent");
        return 0;
    }
    if (!world->have_start) {
        world->start_x = (world->minx + world->maxx) * 0.5f;
        world->start_y = (world->miny + world->maxy) * 0.5f;
        world->start_z = (world->minz + world->maxz) * 0.5f;
        world->start_yaw = 0.0f;
        world->start_pitch = 0.0f;
        world->have_start = 1;
    }
    /* Above the roofs, looking down, so a town opens as a map rather than a wall. */
    if (world->place == PLACE_TOWN) {
        world->start_x = (world->minx + world->maxx) * 0.5f;
        world->start_y = world->maxy + 256.0f;
        world->start_z = (world->minz + world->maxz) * 0.5f;
        world->start_yaw = 0.0f;
        world->start_pitch = -0.6f;
        world->have_start = 1;
    }
    {
        df_u8 *pal_bytes;
        df_u32 pal_size;

        pal_bytes = vfs_read(PAL_NAME, &pal_size);
        if (!pal_bytes || !loader_call("col", PAL_NAME, pal_bytes, pal_size, &world->pal)) {
            why(PAL_NAME);
            return 0;
        }
    }
    lap(&g_times.rest);
    finish_images(world);
    lap(&g_times.pictures);
    flame_lights(world);
    add_emitters(world);
    lap(&g_times.rest);
    if (!add_doors(world)) {
        return 0;
    }
    lap(&g_times.doors);
    if (world->place == PLACE_INSIDE) {
        sounds_house(&world->scape, world->building, world->faction, hold->location_id);
        /* Indoors, so always lit; a house has no light records, only its candles. */
        add_lamps(world);
    } else {
        sounds_scape(&world->scape, hold->outdoor ? SCAPE_TOWN : SCAPE_DUNGEON, hold->location_id);
    }
    if (world->place == PLACE_TOWN) {
        /* The clock's season: CLOCK_* and SEASON_* are Daggerfall's one order. */
        skyfile_open(hold->climate, world->season, &world->sky);
        /* The climate's odds; the engine's weather rolls them. */
        forecast_set(hold->climate, world->season);
        add_lamps(world);
        mark_glass(world);
    }
    lap(&g_times.rest);
    why("");
    return world;
}

World *world_load(void)
{
    Dungeon hold;

    if (!dungeon_load(&hold)) {
        why(dungeon_error());
        return 0;
    }
    return assemble(&hold, 1, -1, -1);
}

World *world_load_dungeon(const Dungeon *dungeon)
{
    return assemble(dungeon, 0, -1, -1);
}

World *world_load_inside(const Dungeon *town, int block, int record)
{
    if (!town || !town->outdoor || block < 0 || block >= town->nblocks || record < 0) {
        why("no such building");
        return 0;
    }
    return assemble(town, 0, block, record);
}

const char *world_error(void)
{
    return g_why;
}

const char *world_block_name(void)
{
    return g_title[0] ? g_title : "Privateer's Hold";
}

int world_model_count(const World *world)
{
    return world ? world->ninst : 0;
}

int world_triangle_count(const World *world)
{
    return world ? world->ntris : 0;
}

const Instance *world_instance(const World *world, int index)
{
    if (!world || index < 0 || index >= world->ninst) {
        return 0;
    }
    return &world->inst[index];
}

int world_flat_count(const World *world)
{
    return world ? world->nflat : 0;
}

int world_editor_count(const World *world)
{
    return world ? world->neditor : 0;
}

const Flat *world_flat(const World *world, int index)
{
    if (!world || index < 0 || index >= world->nflat) {
        return 0;
    }
    return &world->flat[index];
}

int world_image_count(const World *world)
{
    return world ? world->nimages : 0;
}

const Image *world_image(const World *world, int slot)
{
    if (!world || slot < 0 || slot >= world->nimages) {
        return 0;
    }
    return world->images[slot];
}

int world_image_cutout(const World *world, int slot)
{
    if (!world || slot < 0 || slot >= world->nimages) {
        return 0;
    }
    return world->cutout[slot] ? 1 : 0;
}

const Palette *world_palette(const World *world)
{
    return world ? &world->pal : 0;
}

void world_start(const World *world, float *x, float *y, float *z, float *yaw, float *pitch)
{
    *x = world->start_x;
    *y = world->start_y;
    *z = world->start_z;
    *yaw = world->start_yaw;
    *pitch = world->start_pitch;
}

void world_bounds(const World *world,
                  float *minx, float *miny, float *minz,
                  float *maxx, float *maxy, float *maxz)
{
    *minx = world->minx;
    *miny = world->miny;
    *minz = world->minz;
    *maxx = world->maxx;
    *maxy = world->maxy;
    *maxz = world->maxz;
}

int world_texture_file_count(const World *world)
{
    return world ? world->nfiles : 0;
}

int world_texture_file(const World *world, int index)
{
    if (!world || index < 0 || index >= world->nfiles) {
        return -1;
    }
    return world->files[index];
}

void world_as_level(const World *world, Level *level)
{
    /* Every field, those added since too (a stream's sprite_spans, sky_serial): 0 unless set here. */
    mem_zero(level, sizeof(Level));
    if (!world) {
        return;
    }
    level->images = world->images;
    level->cutout = world->cutout;
    level->glass = world->nglass > 0 ? world->glass : 0;
    level->nimages = world->nimages;
    level->palette = &world->pal;
    level->scene.models = world->inst;
    level->scene.bounds = world->bounds;
    level->scene.nmodels = world->ninst;
    level->scene.spans = world->spans;
    level->scene.nspans = world->nspans;
    level->scene.sprites = world->flat;
    level->scene.nsprites = world->nflat;
    level->scene.lights = world->light;
    level->scene.nlights = world->nlight;
    level->scene.outdoor = world->outdoor;
    level->sky = world->outdoor ? &world->sky : 0;
    level->scene.emitters = world->emit;
    level->scene.nemitters = world->nemit;
    level->scene.usables = world->use;
    level->scene.nusables = world->nuse;
    level->scene.movers = world->mover;
    level->scene.nmovers = world->nmover;
    level->sound = &world->scape;
    level->scene.entities = world->nent > 0 ? world->ent : 0;
    level->scene.nentities = world->nent;
    level->scene.water = world->nwater > 0 ? world->water : 0;
    level->scene.nwater = world->nwater;
}

int world_water_count(const World *world)
{
    return world ? world->nwater : 0;
}

const Water *world_water(const World *world, int index)
{
    if (!world || index < 0 || index >= world->nwater) {
        return 0;
    }
    return &world->water[index];
}

int world_entity_count(const World *world)
{
    return world ? world->nent : 0;
}

const Entity *world_entity(const World *world, int index)
{
    if (!world || index < 0 || index >= world->nent) {
        return 0;
    }
    return &world->ent[index];
}

int world_entity_kind(const World *world, int index, int *passive)
{
    if (!world || index < 0 || index >= world->nent) {
        return -1;
    }
    if (passive) {
        *passive = world->ent_passive[index];
    }
    return (int)world->ent_kind[index];
}

int world_season(const World *world)
{
    return world ? world->season : CLOCK_SUMMER;
}

int world_glass_count(const World *world)
{
    return world ? world->nglass : 0;
}

int world_place(const World *world)
{
    return world ? world->place : PLACE_DUNGEON;
}

int world_usable_count(const World *world)
{
    return world ? world->nuse : 0;
}

const Usable *world_usable(const World *world, int index)
{
    if (!world || index < 0 || index >= world->nuse) {
        return 0;
    }
    return &world->use[index];
}

int world_mover_count(const World *world)
{
    return world ? world->nmover : 0;
}

const Mover *world_mover(const World *world, int index)
{
    if (!world || index < 0 || index >= world->nmover) {
        return 0;
    }
    return &world->mover[index];
}

int world_passage(const World *world, int what, float x, float y, float z)
{
    float best;
    int pick;
    int i;

    pick = -1;
    best = 0.0f;
    for (i = 0; world && i < world->nuse; i++) {
        const Usable *u;
        float dx, dy, dz, d2;

        u = &world->use[i];
        if (u->kind != USE_PASSAGE || u->what != what) {
            continue;
        }
        dx = u->cx - x;
        dy = u->cy - y;
        dz = u->cz - z;
        d2 = dx * dx + dy * dy + dz * dz;
        if (pick < 0 || d2 < best) {
            pick = i;
            best = d2;
        }
    }
    return pick;
}

int world_building(const World *world, int *faction)
{
    if (faction) {
        *faction = world ? world->faction : 0;
    }
    return world && world->place == PLACE_INSIDE ? world->building : -1;
}

int world_emitter_count(const World *world)
{
    return world ? world->nemit : 0;
}

const Emitter *world_emitter_at(const World *world, int index)
{
    if (!world || index < 0 || index >= world->nemit) {
        return 0;
    }
    return &world->emit[index];
}

const Soundscape *world_soundscape(const World *world)
{
    return world ? &world->scape : 0;
}

int world_light_count(const World *world)
{
    return world ? world->nlight : 0;
}

int world_outdoor(const World *world)
{
    return world ? world->outdoor : 0;
}

const Sky *world_sky(const World *world)
{
    return world && world->outdoor ? &world->sky : 0;
}
