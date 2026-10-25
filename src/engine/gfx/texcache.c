#include "engine/gfx/texcache.h"
#include "engine/assets/level.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/mip.h"
#include "engine/core/log.h"

/*
 * One upload per picture, at load, never during the frame. With mipmap
 * on, each is a chain down to 1 x 1. Before anything is sent, the
 * budget (mip_budget) picks how many top levels each picture loses so
 * the whole level fits: the picture that takes the most bytes loses
 * its top level first, until the total fits in the card less FONT_KEEP.
 * A small place loses nothing. A city's largest pictures go to half
 * size, and every picture is on the card instead of some drawing with
 * the last bound one. See docs/glide.md and docs/memory.md.
 */
#define TEX_SLOTS 384
#define FONT_KEEP (192u * 1024u)

/* glide_upload handles. glide_bind skips a repeat, so there is no cache here. */
static df_u32 g_handle[TEX_SLOTS];
static int g_live[TEX_SLOTS];
static int g_side[TEX_SLOTS];
static df_u8 g_cut[TEX_SLOTS];
static int g_drop[TEX_SLOTS];
static int g_n;
static int g_nlive;
static int g_unfit;
static int g_reduced;
static df_u32 g_bytes;
static int g_chain;
static const char *g_why = "texture upload failed";

const char *texcache_error(void)
{
    return g_why;
}

/* mip_budget's price list: the card's own figure for one texture. */
static df_u32 card_bytes(int side, int cutout)
{
    df_u32 b;

    if (!glide_texture_bytes(side, cutout, g_chain, &b)) {
        return 0;
    }
    return b;
}

int texcache_upload(const Level *level)
{
    const Palette *pal;
    df_u32 room;
    df_u32 planned;
    int i;
    int n;

    g_n = 0;
    g_nlive = 0;
    g_unfit = 0;
    g_reduced = 0;
    g_bytes = 0;
    for (i = 0; i < TEX_SLOTS; i++) {
        g_live[i] = 0;
        g_handle[i] = 0;
        g_side[i] = 0;
        g_cut[i] = 0;
        g_drop[i] = 0;
    }
    if (!level || !level->palette) {
        g_why = "texture upload failed";
        return 0;
    }
    pal = level->palette;
    if (!glide_palette(pal->r, pal->g, pal->b)) {
        g_why = glide_error();
        log_print(LOG_ERROR, "palette: %s", g_why);
        return 0;
    }
    n = level->nimages;
    if (n < 1 || n > TEX_SLOTS) {
        g_why = "texture count";
        log_print(LOG_ERROR, "textures: %d pictures, the cache holds 1..%d", n, TEX_SLOTS);
        return 0;
    }
    g_chain = glide_mipmap();
    /* Pictures the card can take: square, a power of two, 1..256. */
    for (i = 0; i < n; i++) {
        const Image *img;

        img = level->images[i];
        if (!img || !img->pot || img->pot_w < 1 || img->pot_w != img->pot_h
            || img->pot_w > 256 || (img->pot_w & (img->pot_w - 1))) {
            continue;
        }
        g_side[i] = img->pot_w;
        g_cut[i] = (df_u8)(level->cutout && level->cutout[i] ? 1 : 0);
    }
    room = glide_tex_free();
    room = room > FONT_KEEP ? room - FONT_KEEP : 0;
    planned = mip_budget(g_side, g_cut, n, room, card_bytes, g_drop);
    for (i = 0; i < n; i++) {
        const Image *img;
        int d;

        if (g_side[i] < 1) {
            continue;
        }
        img = level->images[i];
        /*
         * The budget used the card's sizes, but a 4MB TMU's 2MB edge
         * can push one upload past it and waste the gap. Then this
         * picture takes one more level off, and so on down to 1 x 1.
         */
        for (d = g_drop[i]; (g_side[i] >> d) >= 1; d++) {
            if (glide_tex_free() >= card_bytes(g_side[i] >> d, g_cut[i]) + FONT_KEEP
                && glide_upload(img->pot, g_side[i], g_cut[i], d, g_chain,
                                pal->r, pal->g, pal->b, &g_handle[i])) {
                break;
            }
        }
        if ((g_side[i] >> d) < 1) {
            g_unfit++;
            continue;
        }
        g_drop[i] = d;
        g_live[i] = 1;
        g_nlive++;
        g_bytes += card_bytes(g_side[i] >> d, g_cut[i]);
        if (d > 0) {
            g_reduced++;
        }
    }
    g_n = n;
    log_print(LOG_INFO, "textures: %d of %d on the card, %u KB of %u planned, mip chains %s",
              g_nlive, n, (unsigned int)(g_bytes / 1024u), (unsigned int)(planned / 1024u),
              g_chain ? "on" : "off");
    if (g_reduced > 0) {
        log_print(LOG_INFO, "textures: %d at half size or less to fit the card", g_reduced);
    }
    if (g_unfit > 0) {
        log_print(LOG_WARN, "textures: %d did not fit and draw with the last bound one",
                  g_unfit);
    }
    return 1;
}

int texcache_reduced(void)
{
    return g_reduced;
}

void texcache_bind(int slot)
{
    if (slot < 0 || slot >= g_n || !g_live[slot]) {
        return;
    }
    glide_bind(g_handle[slot]);
}

void texcache_stats(int *live, int *unfit, int *total, df_u32 *bytes)
{
    if (live) {
        *live = g_nlive;
    }
    if (unfit) {
        *unfit = g_unfit;
    }
    if (total) {
        *total = g_n;
    }
    if (bytes) {
        *bytes = g_bytes;
    }
}
