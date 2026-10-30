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
