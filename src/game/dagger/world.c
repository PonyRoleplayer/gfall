#include "game/dagger/world.h"
#include "game/dagger/bind.h"
#include "game/dagger/bsa.h"
#include "game/dagger/climate.h"
#include "game/dagger/dungeon.h"
#include "game/dagger/rdb.h"
#include "game/dagger/rmb.h"
#include "game/dagger/texture.h"
#include "engine/assets/loader.h"
#include "engine/assets/vfs.h"
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
/* A city is one mesh per RMB block plus its buildings. 4096 fills up. */
#define MAX_MODELS 8192
#define MAX_FLATS 8192

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
    int ninst;
    int ntris;
    Flat *flat;
    int nflat;
    int neditor;
    Image *images[MAX_IMAGES];
    df_u16 key_file[MAX_IMAGES];
    df_u16 key_image[MAX_IMAGES];
    df_u8 cutout[MAX_IMAGES];
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
    for (i = 0; i < block->nmarker; i++) {
        block->marker[i].x += dx;
        block->marker[i].z += dz;
    }
}

static int append_block(World *world, Block *block)
{
    int i;

    if (world->ninst + block->ninst > MAX_MODELS || world->nflat + block->nflat > MAX_FLATS) {
        why("dungeon is too large");
        return 0;
    }
    for (i = 0; i < block->ninst; i++) {
        world->inst[world->ninst++] = block->inst[i];
        world->ntris += block->inst[i].mesh->ntris;
    }
    for (i = 0; i < block->nflat; i++) {
        world->flat[world->nflat++] = block->flat[i];
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

static World *assemble(const Dungeon *hold, int require_start)
{
    World *world;
    Bsa *arch;
    Bsa *blocks;
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
    world->flat = (Flat *)mem_alloc(MAX_FLATS * (df_u32)sizeof(Flat));
    if (!world->inst || !world->flat) {
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
        rdb_bytes = bsa_read(blocks, rec);
        if (!rdb_bytes) {
            why(hold->block[b].name);
            bsa_close(blocks);
            bsa_close(arch);
            return 0;
        }
        if (hold->outdoor) {
            block = rmb_load(rdb_bytes, rec->size, arch,
                             climate_family(hold->climate) + TEX_SET_GROUND);
        } else {
            block = rdb_load(rdb_bytes, rec->size, arch);
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
    level->scene.nmodels = 0;
    level->scene.sprites = 0;
    level->scene.nsprites = 0;
    if (!world) {
        return;
    }
    level->images = world->images;
    level->cutout = world->cutout;
    level->nimages = world->nimages;
    level->palette = &world->pal;
    level->scene.models = world->inst;
    level->scene.nmodels = world->ninst;
    level->scene.sprites = world->flat;
    level->scene.nsprites = world->nflat;
}
