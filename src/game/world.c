#include "game/world.h"
#include "assets/bsa.h"
#include "platform/fs.h"
#include "platform/mem.h"

/*
 * N0000012.RDB is a 4x4 block. The view keeps one room from it:
 * model 62022, tag R1C, at (896, -512, 1152). Two TEXTURE.210
 * record 6 torches stand on the far wall. Other models are dropped,
 * and a flat is kept only when its point sits inside that mesh.
 */
#define BLOCK_NAME "N0000012.RDB"
#define PAL_NAME "ART_PAL.COL"
#define MAX_IMAGES 256
#define MAX_FILES 64
#define CHAMBER_ID 62022u
#define CHAMBER_X 896
#define CHAMBER_Y (-512)
#define CHAMBER_Z 1152
/* This room's floor is the mesh minimum Y. The lift frames both torches. */
#define EYE_LIFT 158.0f
#define START_YAW 0.714f
#define START_PITCH 0.28f

static char g_why[96];

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
    Block *block;
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
    img = texture_load(file, image);
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

static void measure(World *world)
{
    int i;
    int first;

    first = 1;
    for (i = 0; i < world->block->ninst; i++) {
        const Instance *in;
        int t;

        in = &world->block->inst[i];
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
    if (first) {
        return;
    }
    /* Centre of the room, high enough to see the torches on the far wall. */
    world->start_x = (world->minx + world->maxx) * 0.5f;
    world->start_y = world->miny + EYE_LIFT;
    world->start_z = (world->minz + world->maxz) * 0.5f;
    world->start_yaw = START_YAW;
    world->start_pitch = START_PITCH;
}

/* One placed copy of the chamber model. Other pieces of the block stay out. */
static int keep_chamber(Block *block)
{
    int i;
    int kept;

    kept = 0;
    for (i = 0; i < block->ninst; i++) {
        Instance *in;

        in = &block->inst[i];
        if (!in->mesh || in->mesh->id != CHAMBER_ID) {
            continue;
        }
        if ((int)in->x != CHAMBER_X || (int)in->y != CHAMBER_Y || (int)in->z != CHAMBER_Z) {
            continue;
        }
        if (kept != i) {
            block->inst[kept] = *in;
        }
        kept++;
    }
    if (kept != 1) {
        return 0;
    }
    block->ninst = 1;
    block->ntris = block->inst[0].mesh->ntris;
    return 1;
}

World *world_load(void)
{
    World *world;
    Bsa *arch;
    Bsa *blocks;
    const BsaRec *rec;
    df_u8 *rdb_bytes;
    int i;
    int t;

    why("load failed");
    world = (World *)mem_alloc(sizeof(World));
    if (!world) {
        why("out of memory");
        return 0;
    }
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
    rec = bsa_find_name(blocks, BLOCK_NAME);
    if (!rec) {
        why(BLOCK_NAME);
        bsa_close(blocks);
        bsa_close(arch);
        return 0;
    }
    rdb_bytes = bsa_read(blocks, rec);
    bsa_close(blocks);
    if (!rdb_bytes) {
        why(BLOCK_NAME);
        bsa_close(arch);
        return 0;
    }
    world->block = rdb_load(rdb_bytes, rec->size, arch);
    bsa_close(arch);
    if (!world->block || world->block->ninst < 1 || world->block->ntris < 1) {
        why("block has no models");
        return 0;
    }
    if (!keep_chamber(world->block)) {
        why("chamber missing");
        return 0;
    }
    for (i = 0; i < world->block->ninst; i++) {
        Mesh *mesh;

        mesh = world->block->inst[i].mesh;
        for (t = 0; t < mesh->ntris; t++) {
            Tri *tri;
            int slot;

            tri = &mesh->tris[t];
            if (!place_image(world, (int)tri->tex_file, (int)tri->tex_image, 0, &slot)) {
                return 0;
            }
            tri->slot = (df_u16)slot;
        }
    }
    measure(world);
    if (world->maxx <= world->minx || world->maxy <= world->miny) {
        why("chamber has no extent");
        return 0;
    }
    {
        int f;
        int kept;

        kept = 0;
        for (f = 0; f < world->block->nflat; f++) {
            Flat *fl;
            int slot;
            const Image *img;
            int extent_w;
            int extent_h;

            fl = &world->block->flat[f];
            if (fl->x < world->minx || fl->x > world->maxx ||
                fl->y < world->miny || fl->y > world->maxy ||
                fl->z < world->minz || fl->z > world->maxz) {
                continue;
            }
            if (!place_image(world, (int)fl->file, (int)fl->image, 1, &slot)) {
                /* A missing decoration is dropped. A full table is fatal. */
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
                world->block->flat[kept] = *fl;
            }
            kept++;
        }
        world->block->nflat = kept;
    }
    {
        df_u8 *pal_bytes;
        df_u32 pal_size;

        pal_bytes = fs_read_all(PAL_NAME, &pal_size);
        if (!pal_bytes || !palette_load(pal_bytes, pal_size, &world->pal)) {
            why(PAL_NAME);
            return 0;
        }
    }
    why("");
    return world;
}

const char *world_error(void)
{
    return g_why;
}

const char *world_block_name(void)
{
    return BLOCK_NAME;
}

int world_model_count(const World *world)
{
    return world && world->block ? world->block->ninst : 0;
}

int world_triangle_count(const World *world)
{
    return world && world->block ? world->block->ntris : 0;
}

const Instance *world_instance(const World *world, int index)
{
    if (!world || !world->block || index < 0 || index >= world->block->ninst) {
        return 0;
    }
    return &world->block->inst[index];
}

int world_flat_count(const World *world)
{
    return world && world->block ? world->block->nflat : 0;
}

int world_editor_count(const World *world)
{
    return world && world->block ? world->block->neditor : 0;
}

const Flat *world_flat(const World *world, int index)
{
    if (!world || !world->block || index < 0 || index >= world->block->nflat) {
        return 0;
    }
    return &world->block->flat[index];
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
