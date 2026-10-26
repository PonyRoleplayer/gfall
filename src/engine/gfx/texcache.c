#include "engine/gfx/texcache.h"
#include "engine/assets/level.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/mip.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/gfx/texres.h"

/*
 * One upload per picture, at load, never during the frame. With mipmap
 * on, each is a chain down to 1 x 1. Before anything is sent, the
 * budget (mip_budget) picks how many top levels each picture loses so
 * the whole level fits: the picture that takes the most bytes loses
 * its top level first, until the total fits in the room left. The sky,
 * the light falloff and the font are on the card before it runs, so
 * that room is exact. It used to keep back 192 KB for a font sent
 * after it, a 128 KB sheet then and 64 KB now.
 * A small place loses nothing. A city's largest pictures go to half
 * size, and every picture is on the card instead of some drawing with
 * the last bound one. See docs/glide.md and docs/memory.md.
 */
#define TEX_SLOTS 512

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

int texcache_stream_begin(const Level *level);
void texcache_stream_end(void);

int texcache_upload(const Level *level)
{
    const Palette *pal;
    df_u32 room;
    df_u32 planned;
    int i;
    int n;

    /* A place loaded whole: whatever was streamed before is gone from the card with the reset. */
    texcache_stream_end();

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
            if (glide_tex_free() >= card_bytes(g_side[i] >> d, g_cut[i])
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

/* ---- A streamed world's pictures ---------------------------------- */

typedef struct Chain {
    df_u8 *levels;
    short side;
    df_u8 cutout;
} Chain;

static int g_stream;
static int g_stream_chain;
static Chain g_chains[TEXRES_SLOTS];
/* The chains' own heap: freed one by one as pictures go. */
static MemHeap *g_chain_heap;
static const df_u8 *g_pr;
static const df_u8 *g_pg;
static const df_u8 *g_pb;

static df_u32 res_bytes(int side, int cutout)
{
    df_u32 b;

    if (!glide_texture_bytes(side, cutout, g_stream_chain, &b)) {
        return 0x7fffffffu;
    }
    return b;
}

static int res_upload(int slot, int drop, df_u32 addr, df_u32 *handle)
{
    const Chain *c;

    c = &g_chains[slot];
    if (!c->levels) {
        return 0;
    }
    return glide_upload_chain(c->levels, c->side, c->cutout, g_stream_chain, drop, addr, handle);
}

static void res_release(df_u32 handle)
{
    glide_release(handle);
}

int texcache_stream_begin(const Level *level)
{
    TexResOps ops;
    df_u32 lo;
    df_u32 hi;
    int i;

    texcache_stream_end();
    if (!level || !level->palette || !glide_palette(level->palette->r, level->palette->g,
                                                     level->palette->b)) {
        g_why = "palette";
        return 0;
    }
    g_pr = level->palette->r;
    g_pg = level->palette->g;
    g_pb = level->palette->b;
    g_chain_heap = mem_heap_new();
    if (!g_chain_heap) {
        g_why = "out of memory";
        return 0;
    }
    for (i = 0; i < TEXRES_SLOTS; i++) {
        g_chains[i].levels = 0;
    }
    g_stream_chain = glide_mipmap();
    glide_mark();
    glide_tex_range(&lo, &hi);
    ops.bytes = res_bytes;
    ops.upload = res_upload;
    ops.release = res_release;
    texres_begin(lo, hi, &ops);
    g_stream = 1;
    g_n = 0;
    log_print(LOG_INFO, "textures: streamed, %u KB of the card", (unsigned int)((hi - lo) / 1024u));
    return 1;
}

void texcache_stream_end(void)
{
    if (!g_stream) {
        return;
    }
    texres_end();
    mem_heap_delete(g_chain_heap);
    g_chain_heap = 0;
    g_stream = 0;
}

int texcache_streamed(void)
{
    return g_stream;
}

int texcache_stream_set(int slot, const Image *img, int cutout, float density)
{
    Chain *c;
    MemHeap *was;
    df_u32 n;
    int side;

    if (!g_stream || slot < 0 || slot >= TEXRES_SLOTS || !img || !img->pot
        || img->pot_w != img->pot_h || img->pot_w < 1 || img->pot_w > 256
        || (img->pot_w & (img->pot_w - 1))) {
        return 0;
    }
    texcache_stream_drop(slot);
    side = img->pot_w;
    n = mip_texels(side, 0, 1) * (cutout ? 2u : 1u);
    c = &g_chains[slot];
    was = mem_heap_use(g_chain_heap);
    c->levels = (df_u8 *)mem_alloc(n);
    mem_heap_use(was);
    if (!c->levels) {
        return 0;
    }
    /* Built once, every level: a sharper copy is then only a download. */
    if (!mip_build(img->pot, side, cutout, 0, 1, g_pr, g_pg, g_pb, c->levels)) {
        texcache_stream_drop(slot);
        return 0;
    }
    c->side = (short)side;
    c->cutout = (df_u8)(cutout ? 1 : 0);
    if (!texres_add(slot, side, cutout, density)) {
        texcache_stream_drop(slot);
        return 0;
    }
    return 1;
}

void texcache_stream_drop(int slot)
{
    Chain *c;

    if (!g_stream || slot < 0 || slot >= TEXRES_SLOTS) {
        return;
    }
    texres_remove(slot);
    c = &g_chains[slot];
    if (c->levels) {
        mem_heap_free(g_chain_heap, c->levels);
        c->levels = 0;
    }
}

void texcache_note(int slot, float dist)
{
    if (g_stream) {
        texres_note(slot, dist);
    }
}

void texcache_stream_service(float focal, df_u32 budget)
{
    if (g_stream) {
        texres_service(focal, budget);
    }
}

df_u32 texcache_stream_ram(void)
{
    return g_stream ? mem_heap_bytes(g_chain_heap) : 0;
}

void texcache_bind(int slot)
{
    if (g_stream) {
        df_u32 h;

        if (texres_handle(slot, &h)) {
            glide_bind(h);
        }
        return;
    }
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
