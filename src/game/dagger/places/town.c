#include "game/dagger/places/town.h"
#include "game/dagger/formats/bsa.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/rmb.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"

/* See town.h. */

/* A block's side, as rmb.c and every town's frame have it. */
#define BLOCK_SIDE 4096.0f

static TownBuilding *g_rows;
static int g_count;
static int g_region = -1;
static int g_index = -1;
static Dungeon g_town;

const TownBuilding *town_buildings(int region, int index, int *count)
{
    Bsa *blocks;
    int b;

    if (region == g_region && index == g_index) {
        *count = g_count;
        return g_rows;
    }
    *count = 0;
    if (!g_rows) {
        g_rows = (TownBuilding *)mem_keep((df_u32)TOWN_BUILDINGS * (df_u32)sizeof(TownBuilding));
    }
    if (!g_rows || !exterior_load_at(region, index, &g_town)) {
        return 0;
    }
    blocks = bsa_open("BLOCKS.BSA");
    if (!blocks) {
        return 0;
    }
    g_count = 0;
    for (b = 0; b < g_town.nblocks; b++) {
        const BsaRec *rec;
        const df_u8 *bytes;
        int r;

        rec = bsa_find_name(blocks, g_town.block[b].name);
        bytes = rec ? bsa_read_scratch(blocks, rec, BSA_SCRATCH_BLOCK) : 0;
        if (!bytes) {
            continue;
        }
        for (r = 0; g_count < TOWN_BUILDINGS; r++) {
            TownBuilding *t;
            int type;
            int faction;
            int quality;
            float x;
            float z;

            if (!rmb_building_at(bytes, rec->size, r, &type, &faction, &quality, &x, &z)) {
                break;
            }
            t = &g_rows[g_count++];
            t->block = (short)b;
            t->record = (short)r;
            t->type = (short)type;
            t->quality = (short)quality;
            t->name_seed = (unsigned short)rmb_building_seed(bytes, rec->size, r);
            t->faction = faction;
            t->x = (float)g_town.block[b].x * BLOCK_SIDE + x;
            t->z = (float)g_town.block[b].z * BLOCK_SIDE + z;
        }
    }
    bsa_close(blocks);
    g_region = region;
    g_index = index;
    *count = g_count;
    return g_rows;
}

/* Where a block file's 64 x 64 automap starts (wild.c's RMB_AUTOMAP too). */
#define RMB_AUTOMAP_AT 2251u
#define CELLS 64

int town_automap(int region, int index, unsigned char *out, int *w, int *h)
{
    static Dungeon town;
    Bsa *blocks;
    int across;
    int down;
    int b;

    *w = 0;
    *h = 0;
    if (!exterior_load_at(region, index, &town) || town.nblocks < 1) {
        return 0;
    }
    across = 0;
    down = 0;
    for (b = 0; b < town.nblocks; b++) {
        across = town.block[b].x + 1 > across ? town.block[b].x + 1 : across;
        down = town.block[b].z + 1 > down ? town.block[b].z + 1 : down;
    }
    if (across * CELLS > TOWN_MAP_SIDE || down * CELLS > TOWN_MAP_SIDE) {
        return 0;
    }
    *w = across * CELLS;
    *h = down * CELLS;
    mem_zero(out, (df_u32)(*w) * (df_u32)(*h));
    blocks = bsa_open("BLOCKS.BSA");
    if (!blocks) {
        return 0;
    }
    for (b = 0; b < town.nblocks; b++) {
        const BsaRec *rec;
        const unsigned char *bytes;
        int row;

        rec = bsa_find_name(blocks, town.block[b].name);
        bytes = rec ? bsa_read_scratch(blocks, rec, BSA_SCRATCH_BLOCK) : 0;
        if (!bytes || rec->size < RMB_AUTOMAP_AT + CELLS * CELLS) {
            continue;
        }
        /* A block's row 0 is its north edge; the town's north is its highest block row, drawn at the top. */
        for (row = 0; row < CELLS; row++) {
            int y;
            int col;

            y = (down - 1 - town.block[b].z) * CELLS + row;
            for (col = 0; col < CELLS; col++) {
                out[y * (*w) + town.block[b].x * CELLS + col] = bytes[RMB_AUTOMAP_AT + (df_u32)row * CELLS + (df_u32)col];
            }
        }
    }
    bsa_close(blocks);
    return 1;
}

const char *town_name(void)
{
    return g_town.title;
}

const TownBuilding *town_building(int region, int index, int block, int record)
{
    const TownBuilding *rows;
    int count;
    int i;

    rows = town_buildings(region, index, &count);
    for (i = 0; rows && i < count; i++) {
        if (rows[i].block == block && rows[i].record == record) {
            return &rows[i];
        }
    }
    return 0;
}

#define BLOCK_COPY_MOST 131072

const unsigned char *town_block_bytes(int region, int index, int block, unsigned *size)
{
    static unsigned char *copy;
    Bsa *blocks;
    const BsaRec *rec;
    const unsigned char *bytes;
    int count;

    *size = 0;
    if (!copy) {
        copy = (unsigned char *)mem_keep(BLOCK_COPY_MOST);
    }
    town_buildings(region, index, &count);
    if (!copy || block < 0 || block >= g_town.nblocks) {
        return 0;
    }
    blocks = bsa_open("BLOCKS.BSA");
    if (!blocks) {
        return 0;
    }
    rec = bsa_find_name(blocks, g_town.block[block].name);
    bytes = rec ? bsa_read_scratch(blocks, rec, BSA_SCRATCH_BLOCK) : 0;
    if (!bytes || rec->size > BLOCK_COPY_MOST) {
        bsa_close(blocks);
        return 0;
    }
    mem_copy(copy, bytes, rec->size);
    *size = rec->size;
    bsa_close(blocks);
    return copy;
}

/* The price of a house for sale: five times the largest model radius (the int at +12 of the ARCH3D record) among the models of its block (bank_add_house_for_sale, bank.c 218..241). */
#define HOUSE_PRICE_PER_RADIUS 5
#define ARCH_RADIUS_AT 12
#define HOUSE_MODELS_MOST 128

int town_house_price(int region, int index, int block)
{
    const df_u8 *bytes;
    unsigned size;
    df_u32 ids[HOUSE_MODELS_MOST];
    Bsa *arch;
    int n, i, best, radius;

    bytes = town_block_bytes(region, index, block, &size);
    if (!bytes) { return 0; }
    n = rmb_model_ids(bytes, size, ids, HOUSE_MODELS_MOST);
    arch = bsa_open("ARCH3D.BSA");
    if (!arch) { return 0; }
    best = 0;
    for (i = 0; i < n; i++) {
        const BsaRec *rec;
        const df_u8 *model;

        rec = bsa_find_id(arch, ids[i]);
        if (!rec || rec->size < ARCH_RADIUS_AT + 4) { continue; }
        model = bsa_read_scratch(arch, rec, BSA_SCRATCH_MODEL);
        radius = model ? df_ri32(model + ARCH_RADIUS_AT) : 0;
        if (radius > best) { best = radius; }
    }
    bsa_close(arch);
    return best * HOUSE_PRICE_PER_RADIUS;
}

df_u32 town_building_key(int region, int index, int block, int record)
{
    df_u32 h;
    int part[4], i, k;

    /* FNV-1a over the four numbers' bytes. */
    h = 2166136261u;
    part[0] = region; part[1] = index; part[2] = block; part[3] = record;
    for (i = 0; i < 4; i++) {
        for (k = 0; k < 4; k++) { h = (h ^ (df_u32)((part[i] >> (8 * k)) & 0xFF)) * 16777619u; }
    }
    return BUILDING_KEY_KIND | (h & ~BUILDING_KEY_KIND);
}

int town_marker(int region, int index, int record, float *x, float *z)
{
    unsigned size;
    const unsigned char *bytes = town_block_bytes(region, index, 0, &size);
    float mx, mz;

    if (!bytes || !rmb_editor_marker(bytes, size, record, &mx, &mz)) { return 0; }
    *x = (float)g_town.block[0].x * BLOCK_SIDE + mx;
    *z = (float)g_town.block[0].z * BLOCK_SIDE + mz;
    return 1;
}
