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
static df_u16 g_cap_px[SKY_CAP * SKY_CAP];
static df_u32 g_cap;
/*
 * A copy of the place's Sky, not a pointer into it: the place lives on
 * a viewer's temp heap, and a pointer would outlive it.
 */
static Sky g_sky_copy;
static const Sky *g_sky;
static int g_key = -1;
static int g_ready;
static int g_on = 1;
static int g_hr, g_hg, g_hb;
static int g_zr, g_zg, g_zb;
/*
 * A new picture comes in over frames (sky_update), not in one: its
 * pieces read a part a frame into g_pixels and g_rgb_next (g_part, the
 * next), then a tile a frame sent (g_tile_at), each drawn under the
 * palette of the picture it shows (g_tile_new), then the dome and the
 * horizon's colours. g_next is the key coming, -1 for none. On the 233
 * the whole change was 224 KB of disk and 256 KB to the card in a frame.
 */
static int g_next = -1;
static int g_part;
static int g_tile_at;
static df_u8 g_rgb_next[768];
static df_u8 g_tile_new[SKY_TILES];

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
    int i;

    g_ready = 0;
    g_sky = 0;
    g_key = -1;
    g_next = -1;
    for (i = 0; i < SKY_TILES; i++) {
        g_tile_new[i] = 0;
    }
}

int sky_ready(void)
{
    return g_ready;
}

df_u32 sky_tile(int i)
{
    return (i >= 0 && i < SKY_TILES) ? g_tiles[i] : 0;
}

df_u32 sky_cap(void)
{
    return g_cap;
}

static void mean_rows(int y0, int y1, int *r, int *g, int *b);

/* The rows the dome starts from: the top of the picture. */
#define CAP_ROWS 2

static df_u16 pack565(int r, int g, int b)
{
    return (df_u16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/*
 * g_cap_px from g_pixels: see SKY_CAP. Once a picture, not per frame.
 * Each row of the dome blurs the ring of top-row colours a texel wider
 * on each side than the last, and fades to the zenith mean by row 48:
 * the top row's clouds soften into an even sky overhead. Pulling each
 * column straight to the apex instead drew rays.
 */
#define CAP_FADE 48

static void make_cap(void)
{
    static int ring[SKY_CAP][3];
    static int sum[3 * SKY_CAP + 1][3];
    int zr, zg, zb;
    int u;
    int v;
    int k;

    mean_rows(0, CAP_ROWS, &zr, &zg, &zb);
    for (u = 0; u < SKY_CAP; u++) {
        int acc[3];
        int y;

        acc[0] = acc[1] = acc[2] = 0;
        for (y = 0; y < CAP_ROWS; y++) {
            int x;

            for (x = u * (SKY_W / SKY_CAP); x < (u + 1) * (SKY_W / SKY_CAP); x++) {
                const df_u8 *c;

                c = &g_rgb[g_pixels[y * SKY_W + x] * 3];
                acc[0] += c[0];
                acc[1] += c[1];
                acc[2] += c[2];
            }
        }
        for (k = 0; k < 3; k++) {
            ring[u][k] = acc[k] / (CAP_ROWS * (SKY_W / SKY_CAP));
        }
    }
    /* Running sums round the ring three times, so no window has to wrap. */
    for (k = 0; k < 3; k++) {
        sum[0][k] = 0;
    }
    for (u = 0; u < 3 * SKY_CAP; u++) {
        for (k = 0; k < 3; k++) {
            sum[u + 1][k] = sum[u][k] + ring[u % SKY_CAP][k];
        }
    }
    for (v = 0; v < SKY_CAP; v++) {
        int rad;
        int t;

        rad = 1 + v;
        if (rad > SKY_CAP / 2 - 1) {
            rad = SKY_CAP / 2 - 1;
        }
        /* Smoothstep to the zenith, 0..256. */
        t = v >= CAP_FADE ? 256 : v * 256 / CAP_FADE;
        t = t * t * (768 - 2 * t) / 65536;
        for (u = 0; u < SKY_CAP; u++) {
            int lo;
            int c[3];

            /* The window u - rad .. u + rad, a ring on: lo >= 1, lo + 2 rad + 1 < 3 rings. */
            lo = u - rad + SKY_CAP;
            for (k = 0; k < 3; k++) {
                c[k] = (sum[lo + 2 * rad + 1][k] - sum[lo][k]) / (2 * rad + 1);
            }
            g_cap_px[v * SKY_CAP + u] = pack565(c[0] + (zr - c[0]) * t / 256,
                                                c[1] + (zg - c[1]) * t / 256,
                                                c[2] + (zb - c[2]) * t / 256);
        }
    }
}

const df_u8 *sky_tile_palette(int i)
{
    return i >= 0 && i < SKY_TILES && g_tile_new[i] ? g_rgb_next : g_rgb;
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

int sky_prepare(const Sky *sky, float minute, int weather)
{
    int i;
    int key;

    sky_reset();
    if (!sky || !sky->pick || !sky->read) {
        return 1;
    }
    g_sky_copy = *sky;
    g_sky = &g_sky_copy;
    key = g_sky->pick(g_sky, minute, weather);
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
    make_cap();
    if (!glide_upload_565(g_cap_px, SKY_CAP, &g_cap)) {
        log_text(LOG_WARN, "sky: no room for the sky on the card");
        g_sky = 0;
        return 0;
    }
    g_key = key;
    g_ready = 1;
    return 1;
}

void sky_change(const Sky *sky)
{
    if (!g_ready || !sky || !sky->pick || !sky->read) {
        return;
    }
    g_sky_copy = *sky;
    g_sky = &g_sky_copy;
    /* No key is -1: the next update reads the new sky's picture whatever its key. */
    g_key = -1;
}

/* Every tile sent: the new picture's palette is the sky's, and its dome and colours follow. */
static void finish_change(void)
{
    int i;

    for (i = 0; i < 768; i++) {
        g_rgb[i] = g_rgb_next[i];
    }
    for (i = 0; i < SKY_TILES; i++) {
        g_tile_new[i] = 0;
    }
    mean_rows(SKY_H - HORIZON_ROWS, SKY_H, &g_hr, &g_hg, &g_hb);
    mean_rows(0, 1, &g_zr, &g_zg, &g_zb);
    /* RGB 565, little-endian as Glide reads it, as df_u16 already is on x86. */
    make_cap();
    glide_replace(g_cap, (const df_u8 *)g_cap_px);
    g_key = g_next;
    g_next = -1;
}

void sky_update(float minute, int weather)
{
    int key;
    int r;

    if (!g_ready || !g_sky) {
        return;
    }
    if (g_next < 0) {
        key = g_sky->pick(g_sky, minute, weather);
        if (key == g_key) {
            return;
        }
        /* A change runs to its end before the next is asked: its tiles are half one picture. */
        g_next = key;
        g_part = 0;
        g_tile_at = -1;
    }
    if (g_tile_at < 0) {
        /* Reading: one piece a frame; a game with no pieces is read at once. */
        if (g_sky->read_part) {
            r = g_sky->read_part(g_sky, g_next, g_part++, g_pixels, g_rgb_next);
        } else {
            r = g_sky->read(g_sky, g_next, g_pixels, g_rgb_next) ? 0 : -1;
        }
        if (r < 0) {
            /* Marked as had: a picture that fails is not read again every frame. */
            log_print(LOG_WARN, "sky: picture %d could not be read", g_next);
            g_key = g_next;
            g_next = -1;
            return;
        }
        if (r == 0) {
            g_tile_at = 0;
        }
        return;
    }
    if (g_tile_at < SKY_TILES) {
        cut_tile(g_tile_at);
        glide_replace(g_tiles[g_tile_at], g_tile);
        g_tile_new[g_tile_at] = 1;
        g_tile_at++;
        return;
    }
    finish_change();
}
