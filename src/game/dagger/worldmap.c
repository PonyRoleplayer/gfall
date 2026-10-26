#include "game/dagger/worldmap.h"
#include "game/dagger/bsa.h"
#include "game/dagger/climate.h"
#include "game/dagger/dungeon.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/*
 * See worldmap.h. WOODS.WLD: a 144-byte header (its words: the offset
 * table's bytes, width, height, 0, the next section, 1, 22, the height
 * map's offset), 500000 record offsets, a section, then the height map
 * at 2001168. CLIMATE.PAK: 500 row offsets (u32), each row runs of a
 * u16 count and a climate byte. MAPTABLE: 17 bytes a location, its
 * longitude in bits 8..24 of the word at 4, latitude in bits 8..23 of
 * the word at 8; a pixel is each over 128, latitude counted from the
 * south (as dungeon.c reads it for climates).
 */

#define WOODS_NAME "WOODS.WLD"
#define WOODS_MAP_AT 7
#define CLIMATE_NAME "CLIMATE.PAK"
#define MAPS_NAME "MAPS.BSA"
#define TABLE_BYTES 17
#define LONLAT_DIV 128

static int g_tried;
static int g_ok;
static df_u8 *g_height;
static df_u8 *g_climate;
/* Per pixel: 1 + the location's number in g_region / g_index, 0 for none. */
static df_u16 *g_loc;
static df_u8 *g_region;
static df_u16 *g_index;
static int g_nloc;

static int clampx(int x)
{
    return x < 0 ? 0 : (x >= WORLDMAP_W ? WORLDMAP_W - 1 : x);
}

static int clampy(int y)
{
    return y < 0 ? 0 : (y >= WORLDMAP_H ? WORLDMAP_H - 1 : y);
}

static int read_heights(void)
{
    VfsFile *f;
    df_u8 head[32];
    df_u32 at;
    int ok;

    f = vfs_open(WOODS_NAME);
    if (!f) {
        return 0;
    }
    ok = vfs_read_at(f, 0, head, 32);
    at = ok ? df_ru32(head + WOODS_MAP_AT * 4) : 0;
    ok = ok && df_ru32(head + 4) == WORLDMAP_W && df_ru32(head + 8) == WORLDMAP_H
        && vfs_read_at(f, at, g_height, (df_u32)WORLDMAP_W * WORLDMAP_H);
    vfs_close(f);
    return ok;
}

static int read_climates(void)
{
    df_u8 *file;
    df_u32 size;
    int y;

    file = vfs_read(CLIMATE_NAME, &size);
    if (!file || size < (df_u32)WORLDMAP_H * 4u) {
        return 0;
    }
    for (y = 0; y < WORLDMAP_H; y++) {
        df_u32 p;
        df_u32 end;
        int x;

        p = df_ru32(file + (df_u32)y * 4u);
        end = y + 1 < WORLDMAP_H ? df_ru32(file + (df_u32)(y + 1) * 4u) : size;
        if (p > size || end > size) {
            return 0;
        }
        x = 0;
        while (x < WORLDMAP_W && p + 3u <= end) {
            int n;
            df_u8 v;

            n = (int)df_ru16(file + p);
            v = file[p + 2u];
            p += 3u;
            while (n-- > 0 && x < WORLDMAP_W) {
                g_climate[y * WORLDMAP_W + x++] = v;
            }
        }
        while (x < WORLDMAP_W) {
            g_climate[y * WORLDMAP_W + x++] = (df_u8)CLIMATE_OCEAN;
        }
    }
    return 1;
}

static int read_locations(void)
{
    Bsa *maps;
    int region;
    int cap;

    maps = bsa_open(MAPS_NAME);
    if (!maps) {
        return 0;
    }
    /* Two passes: count, then fill, so the lists are sized to the world. */
    cap = 0;
    for (region = 0; region < DUN_REGIONS; region++) {
        char name[16];
        const BsaRec *rec;

        name[0] = 0;
        df_copy(name, "MAPTABLE.000", (int)sizeof(name));
        name[9] = (char)('0' + region / 100);
        name[10] = (char)('0' + (region / 10) % 10);
        name[11] = (char)('0' + region % 10);
        rec = bsa_find_name(maps, name);
        cap += rec ? (int)(rec->size / TABLE_BYTES) : 0;
    }
    g_region = (df_u8 *)mem_keep((df_u32)(cap > 0 ? cap : 1));
    g_index = (df_u16 *)mem_keep((df_u32)(cap > 0 ? cap : 1) * 2u);
    if (!g_region || !g_index || cap > 65535) {
        bsa_close(maps);
        return 0;
    }
    g_nloc = 0;
    for (region = 0; region < DUN_REGIONS; region++) {
        char name[16];
        const BsaRec *rec;
        const df_u8 *t;
        int n;
        int i;

        df_copy(name, "MAPTABLE.000", (int)sizeof(name));
        name[9] = (char)('0' + region / 100);
        name[10] = (char)('0' + (region / 10) % 10);
        name[11] = (char)('0' + region % 10);
        rec = bsa_find_name(maps, name);
        if (!rec) {
            continue;
        }
        t = bsa_read_scratch(maps, rec, BSA_SCRATCH_BLOCK);
        if (!t) {
            continue;
        }
        n = (int)(rec->size / TABLE_BYTES);
        for (i = 0; i < n && g_nloc < cap; i++) {
            df_u32 lon;
            df_u32 lat;
            int x;
            int y;

            lon = (df_ru32(t + (df_u32)i * TABLE_BYTES + 4u) & 0x1ffffffu) >> 8;
            lat = (df_ru32(t + (df_u32)i * TABLE_BYTES + 8u) & 0xffffffu) >> 8;
            x = (int)(lon / LONLAT_DIV);
            y = WORLDMAP_H - 1 - (int)(lat / LONLAT_DIV);
            if (x < 0 || x >= WORLDMAP_W || y < 0 || y >= WORLDMAP_H) {
                continue;
            }
            g_region[g_nloc] = (df_u8)region;
            g_index[g_nloc] = (df_u16)i;
            g_loc[y * WORLDMAP_W + x] = (df_u16)(g_nloc + 1);
            g_nloc++;
        }
    }
    bsa_close(maps);
    return g_nloc > 0;
}

int worldmap_open(void)
{
    df_u32 n;

    if (g_tried) {
        return g_ok;
    }
    g_tried = 1;
    n = (df_u32)WORLDMAP_W * WORLDMAP_H;
    g_height = (df_u8 *)mem_keep(n);
    g_climate = (df_u8 *)mem_keep(n);
    g_loc = (df_u16 *)mem_keep(n * 2u);
    if (!g_height || !g_climate || !g_loc) {
        log_text(LOG_ERROR, "worldmap: out of memory");
        return 0;
    }
    mem_zero(g_loc, n * 2u);
    if (!read_heights()) {
        log_text(LOG_ERROR, "worldmap: " WOODS_NAME " did not read");
        return 0;
    }
    if (!read_climates()) {
        log_text(LOG_ERROR, "worldmap: " CLIMATE_NAME " did not read");
        return 0;
    }
    if (!read_locations()) {
        log_text(LOG_ERROR, "worldmap: the locations did not read");
        return 0;
    }
    log_print(LOG_INFO, "worldmap: %d locations on 1000 x 500 pixels", g_nloc);
    g_ok = 1;
    return 1;
}

/*
 * WOODS.WLD's finer heights: each pixel's record holds a 5 x 5 block at
 * +22, row 0 north, of which the inner 3 x 3 is a pixel's heights in
 * thirds (the outer ring repeats the neighbours'). Read on demand, two
 * small reads a pixel, the file never whole, into a direct-mapped cache
 * of DETAIL_CACHE pixels (13 KB, static): a chunk needs the few round
 * it, and its neighbours need the same.
 */
#define DETAIL_CACHE 1024
#define WOODS_OFFSETS 144
#define WOODS_DETAIL 22

static int g_detail_key[DETAIL_CACHE];
static df_u8 g_detail[DETAIL_CACHE][9];
static int g_detail_init;

int worldmap_detail(int px, int py, df_u8 *out)
{
    int key;
    int slot;
    int k;

    px = clampx(px);
    py = clampy(py);
    key = py * WORLDMAP_W + px;
    if (!g_detail_init) {
        for (k = 0; k < DETAIL_CACHE; k++) {
            g_detail_key[k] = -1;
        }
        g_detail_init = 1;
    }
    slot = (int)(((df_u32)key * 2654435761u) >> 22) & (DETAIL_CACHE - 1);
    if (g_detail_key[slot] != key) {
        VfsFile *f;
        df_u8 word[4];
        df_u8 block[25];
        int ok;

        f = vfs_open(WOODS_NAME);
        ok = f && vfs_read_at(f, WOODS_OFFSETS + (df_u32)key * 4u, word, 4)
            && vfs_read_at(f, df_ru32(word) + WOODS_DETAIL, block, 25);
        if (f) {
            vfs_close(f);
        }
        if (!ok) {
            /* The pixel's own height in every third: the map, without its detail. */
            for (k = 0; k < 9; k++) {
                out[k] = (df_u8)worldmap_height(px, py);
            }
            return 0;
        }
        /* South row first: out[j * 3 + i] is third i east, j north, from file row 3 - j. */
        for (k = 0; k < 9; k++) {
            g_detail[slot][k] = block[(3 - k / 3) * 5 + 1 + k % 3];
        }
        g_detail_key[slot] = key;
    }
    for (k = 0; k < 9; k++) {
        out[k] = g_detail[slot][k];
    }
    return 1;
}

int worldmap_height(int px, int py)
{
    if (!g_ok) {
        return 0;
    }
    return (int)g_height[clampy(py) * WORLDMAP_W + clampx(px)];
}

int worldmap_climate(int px, int py)
{
    if (!g_ok || px < 0 || py < 0 || px >= WORLDMAP_W || py >= WORLDMAP_H) {
        return CLIMATE_OCEAN;
    }
    return (int)g_climate[py * WORLDMAP_W + px];
}

int worldmap_location(int px, int py, int *region, int *index)
{
    int k;

    if (!g_ok || px < 0 || py < 0 || px >= WORLDMAP_W || py >= WORLDMAP_H) {
        return 0;
    }
    k = (int)g_loc[py * WORLDMAP_W + px];
    if (!k) {
        return 0;
    }
    if (region) {
        *region = (int)g_region[k - 1];
    }
    if (index) {
        *index = (int)g_index[k - 1];
    }
    return 1;
}

int worldmap_pixel_of(int region, int index, int *px, int *py)
{
    int p;

    if (!g_ok) {
        return 0;
    }
    /* Once, from a menu: a pass over the map is fine. */
    for (p = 0; p < WORLDMAP_W * WORLDMAP_H; p++) {
        int k;

        k = (int)g_loc[p];
        if (k && (int)g_region[k - 1] == region && (int)g_index[k - 1] == index) {
            *px = p % WORLDMAP_W;
            *py = p / WORLDMAP_W;
            return 1;
        }
    }
    return 0;
}

int worldmap_locations(void)
{
    return g_ok ? g_nloc : 0;
}
