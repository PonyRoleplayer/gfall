#include "engine/gfx/sky.h"
#include "engine/core/log.h"
#include "engine/gfx/glide.h"

/*
 * The picture as the game hands it over, then one tile at a time. Both
 * static: 220 KB and 64 KB, kept for the process, so no heap (the
 * viewers' temp heap dies with each place).
 */
static df_u8 g_pixels[SKY_W * SKY_H];
static df_u8 g_tile[SKY_TILE * SKY_TILE];
static df_u8 g_rgb[768];
static df_u32 g_tiles[SKY_TILES];
static const Sky *g_sky;
static int g_key = -1;
static int g_ready;
static int g_on = 1;
static int g_hr, g_hg, g_hb;
static int g_zr, g_zg, g_zb;

/* The rows the fog colour is the mean of: the last few above the horizon. */
#define HORIZON_ROWS 6

void sky_set_on(int on)
{
    g_on = on ? 1 : 0;
}

int sky_on(void)
{
    return g_on;
}

void sky_reset(void)
{
    g_ready = 0;
    g_sky = 0;
    g_key = -1;
}

int sky_ready(void)
{
    return g_ready;
}

df_u32 sky_tile(int i)
{
    return (i >= 0 && i < SKY_TILES) ? g_tiles[i] : 0;
}

const df_u8 *sky_palette(void)
{
    return g_rgb;
}

void sky_horizon(int *r, int *g, int *b)
{
    *r = g_hr;
    *g = g_hg;
    *b = g_hb;
}

void sky_zenith(int *r, int *g, int *b)
{
    *r = g_zr;
    *g = g_zg;
    *b = g_zb;
}

/* Tile i of g_pixels into g_tile: 256 columns, the horizon row repeated below. */
static void cut_tile(int i)
{
    int y;

    for (y = 0; y < SKY_TILE; y++) {
        const df_u8 *src;
        df_u8 *dst;
        int x;

        src = &g_pixels[(y < SKY_H ? y : SKY_H - 1) * SKY_W + i * SKY_TILE];
        dst = &g_tile[y * SKY_TILE];
        for (x = 0; x < SKY_TILE; x++) {
            dst[x] = src[x];
        }
    }
}

static void mean_rows(int y0, int y1, int *r, int *g, int *b)
{
    df_u32 sr;
    df_u32 sg;
    df_u32 sb;
    df_u32 n;
    int y;

    sr = 0;
    sg = 0;
    sb = 0;
    for (y = y0; y < y1; y++) {
        int x;

        for (x = 0; x < SKY_W; x++) {
            const df_u8 *c;

            c = &g_rgb[g_pixels[y * SKY_W + x] * 3];
            sr += c[0];
            sg += c[1];
            sb += c[2];
        }
    }
    n = (df_u32)(y1 - y0) * (df_u32)SKY_W;
    *r = (int)(sr / n);
    *g = (int)(sg / n);
    *b = (int)(sb / n);
}

/* Read key's picture and work out its colours. 0 when the game could not. */
static int load_key(int key)
{
    if (!g_sky || !g_sky->read || !g_sky->read(g_sky, key, g_pixels, g_rgb)) {
        log_print(LOG_WARN, "sky: picture %d could not be read", key);
        return 0;
    }
    mean_rows(SKY_H - HORIZON_ROWS, SKY_H, &g_hr, &g_hg, &g_hb);
    mean_rows(0, 1, &g_zr, &g_zg, &g_zb);
    return 1;
}

int sky_prepare(const Sky *sky, float minute)
{
    int i;
    int key;

    sky_reset();
    if (!sky || !sky->pick || !sky->read) {
        return 1;
    }
    g_sky = sky;
    key = sky->pick(sky, minute);
    if (!load_key(key)) {
        g_sky = 0;
        return 1;
    }
    for (i = 0; i < SKY_TILES; i++) {
        cut_tile(i);
        if (!glide_upload_p8(g_tile, SKY_TILE, &g_tiles[i])) {
            log_text(LOG_WARN, "sky: no room for the sky on the card");
            g_sky = 0;
            return 0;
        }
    }
    g_key = key;
    g_ready = 1;
    return 1;
}

void sky_update(float minute)
{
    int key;
    int i;

    if (!g_ready || !g_sky) {
        return;
    }
    key = g_sky->pick(g_sky, minute);
    if (key == g_key) {
        return;
    }
    /* Mark it first: a picture that fails is not read again every frame. */
    g_key = key;
    if (!load_key(key)) {
        return;
    }
    for (i = 0; i < SKY_TILES; i++) {
        cut_tile(i);
        glide_replace(g_tiles[i], g_tile);
    }
}
