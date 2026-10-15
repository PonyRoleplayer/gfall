#include "game/dagger/dungeon.h"
#include "game/dagger/bsa.h"
#include "game/dagger/climate.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"

#define MAP_NAME "MAPS.BSA"
#define REGION_NAME "MAPDITEM.017"
#define TABLE_NAME "MAPTABLE.017"
#define PITEM_NAME "MAPPITEM.017"
#define NAMES_NAME "MAPNAMES.017"
#define CLIMATE_NAME "CLIMATE.PAK"
#define HOLD_NAME "Privateer's Hold"
#define LOC_NAME 71
#define LOC_ID 33
#define LOC_HAS_PARENT 39
#define LOC_PARENT 41
#define LOC_TAIL 112
#define DUN_HEAD 17
#define MAP_W 1000
#define MAP_H 500
#define MAP_DIV 128
#define TABLE_BYTES 17

static const char g_letters[] = "NWLSBM";
static char g_why[48];

static void why(const char *text)
{
    df_copy(g_why, text, (int)sizeof(g_why));
}

static int sign8(df_u8 v)
{
    if (v & 0x80) {
        return (int)v - 256;
    }
    return (int)v;
}

static void block_name(char *dst, int letter, int number)
{
    int i;

    dst[0] = g_letters[letter];
    for (i = 7; i >= 1; i--) {
        dst[i] = (char)('0' + (number % 10));
        number /= 10;
    }
    dst[8] = '.';
    dst[9] = 'R';
    dst[10] = 'D';
    dst[11] = 'B';
    dst[12] = 0;
}

int climate_family(int climate)
{
    switch (climate) {
    case CLIMATE_DESERT:
    case CLIMATE_DESERT2:
    case CLIMATE_SUBTROPICAL:
        return TEX_DESERT;
    case CLIMATE_MOUNTAIN:
        return TEX_MOUNTAIN;
    case CLIMATE_WOODLANDS:
    case CLIMATE_HAUNTED:
    case CLIMATE_MOUNTAIN_WOODS:
        return TEX_WOODLAND;
    case CLIMATE_OCEAN:
    case CLIMATE_RAINFOREST:
    case CLIMATE_SWAMP:
        return TEX_SWAMP;
    default:
        return -1;
    }
}

static void read_title(char *dst, const df_u8 *src)
{
    int i;

    for (i = 0; i < 31 && src[i]; i++) {
        dst[i] = (char)src[i];
    }
    dst[i] = 0;
}

static int read_climate(Bsa *maps, int parent, int self_id);

/* 1 = Privateer's Hold, 0 = some other record, -1 = the hold is unreadable. */
static int take_hold(const df_u8 *rec, df_u32 size, Dungeon *out, Bsa *maps)
{
    df_u32 doors;
    df_u32 at;
    df_u32 head;
    df_u16 count;
    int i;

    if (size < 4) {
        return 0;
    }
    doors = df_ru32(rec);
    if (doors > 4096) {
        return 0;
    }
    at = 4u + doors * 6u;
    if (at > size || size - at < (df_u32)LOC_TAIL + DUN_HEAD) {
        return 0;
    }
    read_title(out->title, rec + at + LOC_NAME);
    if (!df_eq(out->title, HOLD_NAME)) {
        return 0;
    }
    head = at + (df_u32)LOC_TAIL;
    count = df_ru16(rec + head + 10);
    if (count < 1 || count > DUN_MAX_BLOCKS) {
        why("block count");
        return -1;
    }
    if (size - (head + DUN_HEAD) < (df_u32)count * 4u) {
        why("block list");
        return -1;
    }
    out->nblocks = 0;
    for (i = 0; i < (int)count; i++) {
        const df_u8 *b;
        df_u16 bits;
        int letter;
        int number;
        DunBlock *blk;

        b = rec + head + DUN_HEAD + (df_u32)i * 4u;
        bits = df_ru16(b + 2);
        letter = (int)(bits >> 11);
        number = (int)(bits & 0x3ff);
        if (letter < 0 || letter >= (int)sizeof(g_letters) - 1) {
            why("block letter");
            return -1;
        }
        blk = &out->block[out->nblocks];
        block_name(blk->name, letter, number);
        blk->x = sign8(b[0]);
        blk->z = sign8(b[1]);
        blk->start = (bits & 0x400) ? 1 : 0;
        out->nblocks++;
    }
    out->location_id = (int)df_ru16(rec + at + LOC_ID);
    {
        int parent;

        parent = 0;
        if (df_ru16(rec + at + LOC_HAS_PARENT) != 0) {
            parent = (int)df_ru16(rec + at + LOC_PARENT);
        }
        out->climate = read_climate(maps, parent, out->location_id);
    }
    if (climate_family(out->climate) < 0) {
        why("climate");
        return -1;
    }
    return 1;
}

/* One row of CLIMATE.PAK. Runs are a uint16 count and one climate byte. */
static int climate_pixel(int x, int y)
{
    df_u8 *file;
    df_u32 size;
    df_u32 row;
    df_u32 next;
    df_u32 p;
    int cx;

    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) {
        return -1;
    }
    file = vfs_read(CLIMATE_NAME, &size);
    if (!file || size < (df_u32)MAP_H * 4u) {
        return -1;
    }
    row = df_ru32(file + (df_u32)y * 4u);
    if (y + 1 < MAP_H) {
        next = df_ru32(file + (df_u32)(y + 1) * 4u);
    } else {
        next = size;
    }
    if (row >= size || next > size || next < row) {
        return -1;
    }
    p = row;
    cx = 0;
    while (cx <= x && p + 3u <= next) {
        int n;
        int value;

        n = (int)df_ru16(file + p);
        value = (int)file[p + 2u];
        p += 3u;
        if (n < 1) {
            return -1;
        }
        if (cx + n > x) {
            return value;
        }
        cx += n;
    }
    return -1;
}

/*
 * MAPTABLE is one 17-byte record per MAPNAMES entry, same index as
 * MAPPITEM. Longitude is bits 8..24 of the first bitfield, latitude
 * bits 8..23 of the next word. A pixel is those values divided by 128.
 */
static int climate_of_index(Bsa *maps, int index)
{
    const BsaRec *rec;
    df_u8 *table;
    df_u32 bitfield;
    df_u32 latword;
    int longitude;
    int latitude;
    int x;
    int y;

    rec = bsa_find_name(maps, TABLE_NAME);
    if (!rec) {
        return -1;
    }
    table = bsa_read(maps, rec);
    if (!table) {
        return -1;
    }
    if ((df_u32)index * TABLE_BYTES + TABLE_BYTES > rec->size) {
        return -1;
    }
    bitfield = df_ru32(table + (df_u32)index * TABLE_BYTES + 4u);
    latword = df_ru32(table + (df_u32)index * TABLE_BYTES + 8u);
    longitude = (int)((bitfield & 0x1ffffffu) >> 8);
    latitude = (int)((latword & 0xffffffu) >> 8);
    x = longitude / MAP_DIV;
    y = (MAP_H - 1) - (latitude / MAP_DIV);
    return climate_pixel(x, y);
}

/* Index of the exterior whose LocationNum is `which`, or -1. */
static int find_exterior(Bsa *maps, int which)
{
    const BsaRec *names;
    const BsaRec *items;
    df_u8 *name_bytes;
    df_u8 *item_bytes;
    df_u32 count;
    df_u32 base;
    df_u32 i;

    names = bsa_find_name(maps, NAMES_NAME);
    items = bsa_find_name(maps, PITEM_NAME);
    if (!names || !items) {
        return -1;
    }
    name_bytes = bsa_read(maps, names);
    item_bytes = bsa_read(maps, items);
    if (!name_bytes || names->size < 4 || !item_bytes) {
        return -1;
    }
    count = df_ru32(name_bytes);
    base = count * 4u;
    if (count < 1 || count > 4096 || items->size < base) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        df_u32 rel;
        df_u32 pos;
        df_u32 doors;
        df_u32 at;

        rel = df_ru32(item_bytes + i * 4u);
        if (rel > items->size - base) {
            continue;
        }
        pos = base + rel;
        if (pos + 4u > items->size) {
            continue;
        }
        doors = df_ru32(item_bytes + pos);
        if (doors > 4096) {
            continue;
        }
        at = pos + 4u + doors * 6u;
        if (at + LOC_ID + 2u > items->size) {
            continue;
        }
        if ((int)df_ru16(item_bytes + at + LOC_ID) == which) {
            return (int)i;
        }
    }
    return -1;
}

static int read_climate(Bsa *maps, int parent, int self_id)
{
    int which;
    int index;

    which = parent ? parent : self_id;
    index = find_exterior(maps, which);
    if (index < 0 && parent && self_id != parent) {
        index = find_exterior(maps, self_id);
    }
    if (index < 0) {
        return -1;
    }
    return climate_of_index(maps, index);
}

int dungeon_load(Dungeon *out)
{
    Bsa *maps;
    const BsaRec *rec;
    df_u8 *bytes;
    df_u32 count;
    df_u32 base;
    df_u32 i;

    why("load failed");
    if (!out) {
        return 0;
    }
    out->nblocks = 0;
    out->title[0] = 0;
    out->location_id = 0;
    out->climate = 0;
    maps = bsa_open(MAP_NAME);
    if (!maps) {
        why(MAP_NAME);
        return 0;
    }
    rec = bsa_find_name(maps, REGION_NAME);
    if (!rec) {
        why(REGION_NAME);
        bsa_close(maps);
        return 0;
    }
    bytes = bsa_read(maps, rec);
    if (!bytes || rec->size < 12) {
        bsa_close(maps);
        why(REGION_NAME);
        return 0;
    }
    count = df_ru32(bytes);
    if (count < 1 || count > 4096 || rec->size < 4u + count * 8u) {
        why(REGION_NAME);
        bsa_close(maps);
        return 0;
    }
    base = 4u + count * 8u;
    for (i = 0; i < count; i++) {
        df_u32 rel;
        df_u32 pos;

        rel = df_ru32(bytes + 4u + i * 8u);
        if (rel > rec->size - base) {
            continue;
        }
        pos = base + rel;
        {
            int got;

            got = take_hold(bytes + pos, rec->size - pos, out, maps);
            if (got < 0) {
                bsa_close(maps);
                return 0;
            }
            if (got > 0) {
                bsa_close(maps);
                why("");
                return 1;
            }
        }
    }
    bsa_close(maps);
    why(HOLD_NAME);
    return 0;
}

const char *dungeon_error(void)
{
    return g_why;
}
