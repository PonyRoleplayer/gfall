#include "game/dagger/dungeon.h"
#include "game/dagger/bsa.h"
#include "game/dagger/climate.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"

#define MAP_NAME "MAPS.BSA"
#define REGION_NAME "MAPDITEM.017"
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

/* stem is "MAPDITEM." / "MAPPITEM." / "MAPTABLE." / "MAPNAMES." plus a dot. */
static void map_file(char *dst, const char *stem, int region)
{
    int i;

    i = 0;
    while (stem[i] && i < 12) {
        dst[i] = stem[i];
        i++;
    }
    dst[i++] = (char)('0' + region / 100);
    dst[i++] = (char)('0' + (region / 10) % 10);
    dst[i++] = (char)('0' + region % 10);
    dst[i] = 0;
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

static int read_climate(Bsa *maps, int region, int parent, int self_id, int *type);

/* Past the door list, or 0 when this is not a location record. */
static int location_at(const df_u8 *rec, df_u32 size, df_u32 *at_out)
{
    df_u32 doors;
    df_u32 at;

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
    *at_out = at;
    return 1;
}

/* 1 = blocks stored, 0 = the list is unusable and dungeon_error is set. */
static int read_blocks(const df_u8 *rec, df_u32 size, df_u32 at, Dungeon *out)
{
    df_u32 head;
    df_u16 count;
    int i;

    head = at + (df_u32)LOC_TAIL;
    count = df_ru16(rec + head + 10);
    if (count < 1 || count > DUN_MAX_BLOCKS) {
        why("block count");
        return 0;
    }
    if (size - (head + DUN_HEAD) < (df_u32)count * 4u) {
        why("block list");
        return 0;
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
            return 0;
        }
        blk = &out->block[out->nblocks];
        block_name(blk->name, letter, number);
        blk->x = sign8(b[0]);
        blk->z = sign8(b[1]);
        blk->start = (bits & 0x400) ? 1 : 0;
        out->nblocks++;
    }
    out->location_id = (int)df_ru16(rec + at + LOC_ID);
    /* The outside it is under: its parent location, or its own number when it has none. */
    out->exterior_id = out->location_id;
    if (df_ru16(rec + at + LOC_HAS_PARENT) != 0) {
        out->exterior_id = (int)df_ru16(rec + at + LOC_PARENT);
    }
    return 1;
}

static int apply_climate(const df_u8 *rec, df_u32 at, Dungeon *out, Bsa *maps, int region)
{
    int parent;

    parent = 0;
    if (df_ru16(rec + at + LOC_HAS_PARENT) != 0) {
        parent = (int)df_ru16(rec + at + LOC_PARENT);
    }
    out->climate = read_climate(maps, region, parent, out->location_id, &out->dungeon_type);
    if (climate_family(out->climate) < 0) {
        why("climate");
        return 0;
    }
    return 1;
}

/* 1 = Privateer's Hold, 0 = some other record, -1 = the hold is unreadable. */
static int take_hold(const df_u8 *rec, df_u32 size, Dungeon *out, Bsa *maps)
{
    df_u32 at;

    if (!location_at(rec, size, &at)) {
        return 0;
    }
    read_title(out->title, rec + at + LOC_NAME);
    if (!df_eq(out->title, HOLD_NAME)) {
        return 0;
    }
    if (!read_blocks(rec, size, at, out) || !apply_climate(rec, at, out, maps, 17)) {
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
static int climate_of_index(Bsa *maps, int region, int index, int *type)
{
    const BsaRec *rec;
    df_u8 *table;
    df_u32 bitfield;
    df_u32 latword;
    int longitude;
    int latitude;
    int x;
    int y;
    char name[16];

    map_file(name, "MAPTABLE.", region);
    rec = bsa_find_name(maps, name);
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
    /*
     * Byte +12, after the type bits and the latitude: the kind of the
     * location's dungeon (DUN_TYPE_*), 255 for none. A dungeon's row is
     * the outside it lies under, as its climate is.
     */
    if (type) {
        *type = (int)table[(df_u32)index * TABLE_BYTES + 12u];
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
static int find_exterior(Bsa *maps, int region, int which)
{
    const BsaRec *names;
    const BsaRec *items;
    df_u8 *name_bytes;
    df_u8 *item_bytes;
    df_u32 count;
    df_u32 base;
    df_u32 i;
    char name[16];

    map_file(name, "MAPNAMES.", region);
    names = bsa_find_name(maps, name);
    map_file(name, "MAPPITEM.", region);
    items = bsa_find_name(maps, name);
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

static int read_climate(Bsa *maps, int region, int parent, int self_id, int *type)
{
    int which;
    int index;

    which = parent ? parent : self_id;
    index = find_exterior(maps, region, which);
    if (index < 0 && parent && self_id != parent) {
        index = find_exterior(maps, region, self_id);
    }
    if (index < 0) {
        return -1;
    }
    return climate_of_index(maps, region, index, type);
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
    out->exterior_id = 0;
    out->climate = 0;
    out->dungeon_type = DUN_TYPE_NONE;
    out->outdoor = 0;
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

static void region_name(char *dst, int region)
{
    map_file(dst, "MAPDITEM.", region);
}

/* Walk one MAPDITEM. visit returns 0 to stop. Returns 0 if the file is missing. */
static int each_record(Bsa *maps, int region,
                       int (*visit)(const df_u8 *rec, df_u32 size, int index, void *user),
                       void *user)
{
    char name[16];
    const BsaRec *file;
    df_u8 *bytes;
    df_u32 count;
    df_u32 base;
    df_u32 i;

    region_name(name, region);
    file = bsa_find_name(maps, name);
    if (!file) {
        return 1;
    }
    if (file->size < 12) {
        return 1;
    }
    bytes = bsa_read(maps, file);
    if (!bytes) {
        why(name);
        return 0;
    }
    count = df_ru32(bytes);
    if (count < 1 || count > 4096 || file->size < 4u + count * 8u) {
        return 1;
    }
    base = 4u + count * 8u;
    for (i = 0; i < count; i++) {
        df_u32 rel;
        df_u32 pos;

        rel = df_ru32(bytes + 4u + i * 8u);
        if (rel > file->size - base) {
            continue;
        }
        pos = base + rel;
        if (!visit(bytes + pos, file->size - pos, (int)i, user)) {
            return 1;
        }
    }
    return 1;
}

struct ListWalk {
    DunRef *out;
    int cap;
    int n;
    int region;
};

static int list_visit(const df_u8 *rec, df_u32 size, int index, void *user)
{
    struct ListWalk *walk;
    Dungeon dun;
    df_u32 at;
    DunRef *row;

    walk = (struct ListWalk *)user;
    if (!location_at(rec, size, &at)) {
        return 1;
    }
    read_title(dun.title, rec + at + LOC_NAME);
    if (!read_blocks(rec, size, at, &dun)) {
        return 1;
    }
    if (walk->out && walk->n < walk->cap) {
        row = &walk->out[walk->n];
        df_copy(row->name, dun.title, (int)sizeof(row->name));
        row->region = walk->region;
        row->index = index;
        row->nblocks = dun.nblocks;
        row->location_id = dun.location_id;
        row->exterior_id = dun.exterior_id;
        row->outdoor = 0;
    }
    walk->n++;
    return 1;
}

int dungeon_list(DunRef *out, int cap)
{
    Bsa *maps;
    struct ListWalk walk;
    int region;

    why("load failed");
    maps = bsa_open(MAP_NAME);
    if (!maps) {
        why(MAP_NAME);
        return 0;
    }
    walk.out = out;
    walk.cap = cap;
    walk.n = 0;
    for (region = 0; region < DUN_REGIONS; region++) {
        walk.region = region;
        if (!each_record(maps, region, list_visit, &walk)) {
            bsa_close(maps);
            return 0;
        }
    }
    bsa_close(maps);
    why("");
    return walk.n;
}

struct OneWalk {
    int want;
    int region;
    Dungeon *out;
    Bsa *maps;
    int found;
    int bad;
};

static int one_visit(const df_u8 *rec, df_u32 size, int index, void *user)
{
    struct OneWalk *walk;
    df_u32 at;

    walk = (struct OneWalk *)user;
    if (index != walk->want) {
        return 1;
    }
    walk->found = 1;
    if (!location_at(rec, size, &at)) {
        why("dungeon record");
        walk->bad = 1;
        return 0;
    }
    read_title(walk->out->title, rec + at + LOC_NAME);
    if (!read_blocks(rec, size, at, walk->out)
        || !apply_climate(rec, at, walk->out, walk->maps, walk->region)) {
        walk->bad = 1;
        return 0;
    }
    return 0;
}

int dungeon_load_at(int region, int index, Dungeon *out)
{
    Bsa *maps;
    struct OneWalk walk;
    char name[16];

    why("load failed");
    if (!out || region < 0 || region >= DUN_REGIONS || index < 0) {
        return 0;
    }
    out->nblocks = 0;
    out->title[0] = 0;
    out->location_id = 0;
    out->exterior_id = 0;
    out->climate = 0;
    out->dungeon_type = DUN_TYPE_NONE;
    out->outdoor = 0;
    maps = bsa_open(MAP_NAME);
    if (!maps) {
        why(MAP_NAME);
        return 0;
    }
    walk.want = index;
    walk.region = region;
    walk.out = out;
    walk.maps = maps;
    walk.found = 0;
    walk.bad = 0;
    if (!each_record(maps, region, one_visit, &walk)) {
        bsa_close(maps);
        return 0;
    }
    bsa_close(maps);
    if (walk.bad || !walk.found || out->nblocks < 1) {
        if (!walk.bad) {
            region_name(name, region);
            why(name);
        }
        return 0;
    }
    why("");
    return 1;
}

/*
 * RMB block file. Prefixes are the building kind. Letter 1 is the
 * location's letter when bit 0x10 is set, otherwise A. Letter 2 is
 * ALMS from the top two bits of (character * 2) masked to a byte.
 * Temples (index 13 and 14) use a letter plus the number. Everything
 * else uses a two-digit number, three when the number is 100 or more.
 */
static const char *const g_rmb_prefix[45] = {
    "TVRN", "GENR", "RESI", "WEAP", "ARMR", "ALCH", "BANK", "BOOK",
    "CLOT", "FURN", "GEMS", "LIBR", "PAWN", "TEMP", "TEMP", "PALA",
    "FARM", "DUNG", "CAST", "MANR", "SHRI", "RUIN", "SHCK", "GRVE",
    "FILL", "KRAV", "KDRA", "KOWL", "KMOO", "KCAN", "KFLA", "KHOR",
    "KROS", "KWHE", "KSCA", "KHAW", "MAGE", "THIE", "DARK", "FIGH",
    "CUST", "WALL", "MARK", "SHIP", "WITC"
};

static const char g_letter2[] = "ALMS";

static int emit_char(char *dst, int n, char c)
{
    if (n < 0 || n >= DUN_NAME_MAX - 1) {
        return -1;
    }
    dst[n] = c;
    return n + 1;
}

static int emit_str(char *dst, int n, const char *s)
{
    int i;

    for (i = 0; s[i]; i++) {
        n = emit_char(dst, n, s[i]);
        if (n < 0) {
            return -1;
        }
    }
    return n;
}

static int emit_dec(char *dst, int n, int value, int width)
{
    char tmp[8];
    int k;

    if (value < 0) {
        value = 0;
    }
    k = 0;
    do {
        tmp[k++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value && k < (int)sizeof(tmp));
    while (k < width && k < (int)sizeof(tmp)) {
        tmp[k++] = '0';
    }
    while (k > 0) {
        n = emit_char(dst, n, tmp[--k]);
        if (n < 0) {
            return -1;
        }
    }
    return n;
}

static int rmb_block_name(char *dst, int letter1, int block_index, int block_number, int block_character)
{
    int n;
    int letter;

    if (block_index < 0 || block_index >= (int)(sizeof(g_rmb_prefix) / sizeof(g_rmb_prefix[0]))) {
        return 0;
    }
    n = emit_str(dst, 0, g_rmb_prefix[block_index]);
    if ((block_character & 0x10) && letter1 >= 32 && letter1 < 127) {
        letter = letter1;
    } else {
        letter = 'A';
    }
    n = emit_char(dst, n, (char)letter);
    n = emit_char(dst, n, g_letter2[((block_character * 2) & 0xff) >> 6]);
    if (n < 0) {
        return 0;
    }
    if (block_index == 13 || block_index == 14) {
        n = emit_char(dst, n, (char)((block_character & 0x0f) + 65));
        n = emit_dec(dst, n, block_number, 1);
    } else if (block_number >= 100) {
        n = emit_dec(dst, n, block_number, 3);
    } else {
        n = emit_dec(dst, n, block_number, 2);
    }
    n = emit_str(dst, n, ".RMB");
    if (n < 0) {
        return 0;
    }
    dst[n] = 0;
    return 1;
}

static void read_map_name(char *dst, const df_u8 *src)
{
    int i;

    for (i = 0; i < 32 && src[i]; i++) {
        dst[i] = (char)src[i];
    }
    dst[i] = 0;
}

/* Past the doors and the shared header. 0 when the record is short. */
static int exterior_at(const df_u8 *rec, df_u32 size, df_u32 *ext_out, int *width, int *height, int *letter1)
{
    df_u32 doors;
    df_u32 at;
    int buildings;
    df_u32 ext;

    if (size < 4) {
        return 0;
    }
    doors = df_ru32(rec);
    if (doors > 4096) {
        return 0;
    }
    at = 4u + doors * 6u;
    if (at > size || size - at < (df_u32)LOC_TAIL + 2u) {
        return 0;
    }
    buildings = (int)df_ru16(rec + at + LOC_TAIL);
    if (buildings < 0 || buildings > 4096) {
        return 0;
    }
    ext = at + (df_u32)LOC_TAIL + 7u + (df_u32)buildings * 26u;
    if (ext > size || size - ext < 241u) {
        return 0;
    }
    *width = (int)rec[ext + 40];
    *height = (int)rec[ext + 41];
    *letter1 = (int)rec[ext + 46];
    if (*width < 1 || *height < 1 || (*width * *height) > DUN_MAX_BLOCKS) {
        return 0;
    }
    *ext_out = ext;
    return 1;
}

static int fill_rmb_blocks(const df_u8 *rec, df_u32 ext, int width, int height, int letter1, Dungeon *out)
{
    int n;
    int i;

    n = width * height;
    out->nblocks = 0;
    for (i = 0; i < n; i++) {
        int block_index;
        int block_number;
        int block_character;
        DunBlock *blk;

        block_index = (int)rec[ext + 49u + (df_u32)i];
        block_number = (int)rec[ext + 113u + (df_u32)i];
        block_character = (int)rec[ext + 177u + (df_u32)i];
        blk = &out->block[out->nblocks];
        if (!rmb_block_name(blk->name, letter1, block_index, block_number, block_character)) {
            why("block name");
            return 0;
        }
        blk->x = i % width;
        blk->z = i / width;
        blk->start = 0;
        out->nblocks++;
    }
    return 1;
}

typedef struct ExtNames {
    const df_u8 *names;
    df_u32 name_bytes;
    int region;
} ExtNames;

struct ExtWalk {
    ExtNames src;
    DunRef *out;
    int cap;
    int n;
};

/*
 * exterior_keep: while on, MAPS.BSA stays open and the last region's
 * MAPNAMES and MAPPITEM stay read, in a heap of their own. A streamed
 * world asks for town after town of one region; opening the archive and
 * reading the region again for each was a millisecond on the host, a
 * dropped frame or two on the 233 (docs/streaming.md).
 */
static int g_keep;
static MemHeap *g_keep_heap;
static Bsa *g_keep_maps;
static int g_keep_region = -1;
static df_u8 *g_keep_names;
static df_u8 *g_keep_items;
static df_u32 g_keep_nsize;
static df_u32 g_keep_isize;

void exterior_keep(int on)
{
    if (g_keep_maps) {
        bsa_close(g_keep_maps);
    }
    mem_heap_delete(g_keep_heap);
    g_keep_maps = 0;
    g_keep_heap = 0;
    g_keep_region = -1;
    g_keep = on ? 1 : 0;
    /* Opened now, on the caller's heap: a load may run in a heap thrown away after it. */
    if (g_keep) {
        g_keep_maps = bsa_open(MAP_NAME);
        g_keep = g_keep_maps != 0;
    }
}

/* The region's names and items: 1 read, -1 when the region has none, 0 when a read failed. */
static int region_bytes(Bsa *maps, int region, df_u8 **names, df_u32 *nsize,
                        df_u8 **items, df_u32 *isize)
{
    char name[16];
    const BsaRec *names_rec;
    const BsaRec *items_rec;
    MemHeap *was;

    if (g_keep && region == g_keep_region) {
        *names = g_keep_names;
        *nsize = g_keep_nsize;
        *items = g_keep_items;
        *isize = g_keep_isize;
        return 1;
    }
    map_file(name, "MAPNAMES.", region);
    names_rec = bsa_find_name(maps, name);
    map_file(name, "MAPPITEM.", region);
    items_rec = bsa_find_name(maps, name);
    if (!names_rec || !items_rec || names_rec->size < 4 || items_rec->size < 4) {
        return -1;
    }
    was = 0;
    if (g_keep) {
        /* The last region's bytes go; this one's are kept instead. */
        mem_heap_delete(g_keep_heap);
        g_keep_region = -1;
        g_keep_heap = mem_heap_new();
        was = mem_heap_use(g_keep_heap);
    }
    *names = bsa_read(maps, names_rec);
    *items = bsa_read(maps, items_rec);
    if (g_keep) {
        mem_heap_use(was);
    }
    if (!*names || !*items) {
        why(name);
        return 0;
    }
    *nsize = names_rec->size;
    *isize = items_rec->size;
    if (g_keep) {
        g_keep_region = region;
        g_keep_names = *names;
        g_keep_items = *items;
        g_keep_nsize = *nsize;
        g_keep_isize = *isize;
    }
    return 1;
}

static int each_exterior(Bsa *maps, int region, ExtNames *src,
                         int (*visit)(const df_u8 *rec, df_u32 size, int index, void *user),
                         void *user)
{
    df_u8 *names;
    df_u8 *items;
    df_u32 nsize;
    df_u32 isize;
    df_u32 count;
    df_u32 base;
    df_u32 i;
    int got;

    got = region_bytes(maps, region, &names, &nsize, &items, &isize);
    if (got <= 0) {
        return got < 0;
    }
    count = df_ru32(names);
    base = count * 4u;
    if (count < 1 || count > 4096 || isize < base) {
        return 1;
    }
    if (4u + count * 32u > nsize) {
        return 1;
    }
    src->names = names;
    src->name_bytes = nsize;
    src->region = region;
    for (i = 0; i < count; i++) {
        df_u32 rel;
        df_u32 pos;

        rel = df_ru32(items + i * 4u);
        if (rel > isize - base) {
            continue;
        }
        pos = base + rel;
        if (!visit(items + pos, isize - pos, (int)i, user)) {
            return 1;
        }
    }
    return 1;
}

static int ext_list_visit(const df_u8 *rec, df_u32 size, int index, void *user)
{
    struct ExtWalk *walk;
    df_u32 ext;
    int width;
    int height;
    int letter1;
    DunRef *row;

    walk = (struct ExtWalk *)user;
    if (!exterior_at(rec, size, &ext, &width, &height, &letter1)) {
        return 1;
    }
    if (walk->out && walk->n < walk->cap) {
        row = &walk->out[walk->n];
        if (4u + (df_u32)index * 32u + 32u <= walk->src.name_bytes) {
            read_map_name(row->name, walk->src.names + 4u + (df_u32)index * 32u);
        } else {
            row->name[0] = 0;
        }
        row->region = walk->src.region;
        row->index = index;
        row->nblocks = width * height;
        row->location_id = 0;
        row->exterior_id = 0;
        if (size >= 4) {
            df_u32 doors;
            df_u32 at;

            doors = df_ru32(rec);
            at = 4u + doors * 6u;
            if (doors <= 4096 && at + LOC_ID + 2u <= size) {
                row->location_id = (int)df_ru16(rec + at + LOC_ID);
            }
        }
        row->exterior_id = row->location_id;
        row->outdoor = 1;
    }
    walk->n++;
    return 1;
}

int exterior_list(DunRef *out, int cap)
{
    Bsa *maps;
    struct ExtWalk walk;
    int region;

    why("load failed");
    maps = bsa_open(MAP_NAME);
    if (!maps) {
        why(MAP_NAME);
        return 0;
    }
    walk.out = out;
    walk.cap = cap;
    walk.n = 0;
    walk.src.names = 0;
    walk.src.name_bytes = 0;
    walk.src.region = 0;
    for (region = 0; region < DUN_REGIONS; region++) {
        if (!each_exterior(maps, region, &walk.src, ext_list_visit, &walk)) {
            bsa_close(maps);
            return 0;
        }
    }
    bsa_close(maps);
    why("");
    return walk.n;
}

struct ExtOne {
    ExtNames src;
    int want;
    Dungeon *out;
    Bsa *maps;
    int found;
    int bad;
};

static int ext_one_visit(const df_u8 *rec, df_u32 size, int index, void *user)
{
    struct ExtOne *one;
    df_u32 ext;
    int width;
    int height;
    int letter1;
    df_u32 doors;
    df_u32 at;

    one = (struct ExtOne *)user;
    if (index != one->want) {
        return 1;
    }
    one->found = 1;
    if (!exterior_at(rec, size, &ext, &width, &height, &letter1)) {
        why("exterior record");
        one->bad = 1;
        return 0;
    }
    if (4u + (df_u32)index * 32u + 32u > one->src.name_bytes || !one->src.names) {
        why("map name");
        one->bad = 1;
        return 0;
    }
    read_map_name(one->out->title, one->src.names + 4u + (df_u32)index * 32u);
    if (!fill_rmb_blocks(rec, ext, width, height, letter1, one->out)) {
        one->bad = 1;
        return 0;
    }
    doors = df_ru32(rec);
    at = 4u + doors * 6u;
    one->out->location_id = (int)df_ru16(rec + at + LOC_ID);
    one->out->exterior_id = one->out->location_id;
    one->out->climate = climate_of_index(one->maps, one->src.region, index, 0);
    if (climate_family(one->out->climate) < 0) {
        why("climate");
        one->bad = 1;
        return 0;
    }
    one->out->outdoor = 1;
    return 0;
}

int exterior_load_at(int region, int index, Dungeon *out)
{
    Bsa *maps;
    struct ExtOne one;
    char name[16];

    why("load failed");
    if (!out || region < 0 || region >= DUN_REGIONS || index < 0) {
        return 0;
    }
    out->nblocks = 0;
    out->title[0] = 0;
    out->location_id = 0;
    out->exterior_id = 0;
    out->climate = 0;
    out->dungeon_type = DUN_TYPE_NONE;
    out->outdoor = 0;
    maps = g_keep ? g_keep_maps : bsa_open(MAP_NAME);
    if (!maps) {
        why(MAP_NAME);
        return 0;
    }
    one.src.names = 0;
    one.src.name_bytes = 0;
    one.src.region = region;
    one.want = index;
    one.out = out;
    one.maps = maps;
    one.found = 0;
    one.bad = 0;
    if (!each_exterior(maps, region, &one.src, ext_one_visit, &one)) {
        if (!g_keep) {
            bsa_close(maps);
        }
        return 0;
    }
    if (!g_keep) {
        bsa_close(maps);
    }
    if (one.bad || !one.found || out->nblocks < 1) {
        if (!one.bad) {
            map_file(name, "MAPPITEM.", region);
            why(name);
        }
        return 0;
    }
    why("");
    return 1;
}

const char *dungeon_error(void)
{
    return g_why;
}
