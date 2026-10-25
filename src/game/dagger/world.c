#include "game/dagger/world.h"
#include "game/dagger/bind.h"
#include "game/dagger/bsa.h"
#include "game/dagger/climate.h"
#include "game/dagger/dungeon.h"
#include "game/dagger/rdb.h"
#include "game/dagger/rmb.h"
#include "game/dagger/skyfile.h"
#include "game/dagger/forecast.h"
#include "game/dagger/texture.h"
#include "engine/assets/loader.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

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
#define MAX_FILES 96
/* Buildings plus one ground patch per 512-unit square. 8192 is the cap. */
#define MAX_MODELS 8192
#define MAX_FLATS 8192
#define MAX_LIGHTS 8192

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
    /* cutout: a sprite uses it, so it uploads with alpha. wall: a model face does. */
    df_u8 cutout[MAX_IMAGES];
    df_u8 wall[MAX_IMAGES];
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
};

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
    int change;

    if (pixels < 1) {
        return 0;
    }
    change = (int)((float)pixels * ((float)scale / 256.0f));
    return pixels + change;
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
    {
        TexLoad req;

        req.file = file;
        req.image = image;
        req.out = 0;
        if (!loader_call("texture", 0, 0, 0, &req)) {
            req.out = 0;
        }
        img = req.out;
    }
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

static int append_block(World *world, Block *block)
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
            img = world->images[slot];
            extent_w = flat_extent(img->width, img->xscale);
            extent_h = flat_extent(img->height, img->yscale);
            if (extent_w < 1 || extent_h < 1) {
                continue;
            }
            fl->w = (float)extent_w;
            fl->h = (float)extent_h;
            /*
             * The quad is drawn from its centre. The file point is the
             * base, so the centre sits half a height above it. Without
             * this the picture is half under the floor.
             */
            fl->y += (float)extent_h * 0.5f;
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
        if (world->cutout[i] && !world->wall[i]) {
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
 * RMB blocks carry no light records. Outdoors the lights are the
 * TEXTURE.210 flats, the lamp posts, braziers and fires: one each, a
 * quarter of the flat's height above its centre, where the flame is.
 * After paint, which sizes the flats. See docs/daylight.md.
 */
#define LAMP_RADIUS 160.0f

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
        li->raw0 = 0;
        li->raw1 = 0;
    }
}

static World *assemble(const Dungeon *hold, int require_start)
{
    World *world;
    Bsa *arch;
    Bsa *blocks;
    ArchEnt *cache;
    int ncache;
    int b;
    int tile;

    why("load failed");
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
    world->inst = (Instance *)mem_alloc(MAX_MODELS * (df_u32)sizeof(Instance));
    world->bounds = (Bound *)mem_alloc(MAX_MODELS * (df_u32)sizeof(Bound));
    world->spans = (Span *)mem_alloc(DUN_MAX_BLOCKS * (df_u32)sizeof(Span));
    world->flat = (Flat *)mem_alloc(MAX_FLATS * (df_u32)sizeof(Flat));
    world->light = (Light *)mem_alloc(MAX_LIGHTS * (df_u32)sizeof(Light));
    if (!world->inst || !world->bounds || !world->spans || !world->flat || !world->light) {
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
    for (b = 0; b < hold->nblocks; b++) {
        const BsaRec *rec;
        df_u8 *rdb_bytes;
        Block *block;

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
        if (hold->outdoor) {
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
        if (hold->block[b].start) {
            note_start(world, block);
            if (!world->have_start) {
                note_fall(world, block);
            }
        }
        if (!append_block(world, block)) {
            bsa_close(blocks);
            bsa_close(arch);
            return 0;
        }
    }
    bsa_close(blocks);
    bsa_close(arch);
    if (world->ninst < 1 || world->ntris < 1) {
        why("dungeon has no models");
        return 0;
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
    if (!build_tex_table(world, hold)) {
        return 0;
    }
    if (!paint(world)) {
        return 0;
    }
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
    if (hold->outdoor) {
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
    finish_images(world);
    if (hold->outdoor) {
        world->outdoor = 1;
        skyfile_open(hold->climate, SEASON_SUMMER, &world->sky);
        /* The climate's odds; the engine's weather rolls them. */
        forecast_set(hold->climate, SEASON_SUMMER);
        add_lamps(world);
    }
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
    return assemble(&hold, 1);
}

World *world_load_dungeon(const Dungeon *dungeon)
{
    return assemble(dungeon, 0);
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
    level->images = 0;
    level->cutout = 0;
    level->nimages = 0;
    level->palette = 0;
    level->scene.models = 0;
    level->scene.bounds = 0;
    level->scene.nmodels = 0;
    level->scene.spans = 0;
    level->scene.nspans = 0;
    level->scene.sprites = 0;
    level->scene.nsprites = 0;
    level->scene.lights = 0;
    level->scene.nlights = 0;
    level->scene.outdoor = 0;
    level->sky = 0;
    if (!world) {
        return;
    }
    level->images = world->images;
    level->cutout = world->cutout;
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
