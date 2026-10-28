#include "engine/gfx/texcache.h"
#include "engine/assets/level.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/mip.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/gfx/texres.h"
#include "engine/scene/entity.h"

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
/*
 * A place loaded whole keeps this much of the card back for the pictures
 * only its entities wear, which come and go by distance as a streamed
 * world's do (docs/memory.md): about four creatures near at full size.
 * Less when they need less.
 */
#define ENTITY_ROOM (512u * 1024u)

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
static int res_open(const Palette *pal);
static int chain_set(int slot, const Image *img, int cutout, float density);

/*
 * The pictures only the scene's entities wear, in g_dyn, with the
 * texels a world unit each is drawn at: not a flat's, not a face's.
 * Those go on the card by distance, not all at load: a dungeon's
 * creatures are ten sheets each, 8.7 MB of them in the hold.
 */
static df_u8 g_dyn[TEX_SLOTS];
static float g_dyn_density[TEX_SLOTS];

static int mark_entity_pictures(const Level *level, int n)
{
    const Scene *sc;
    int count;
    int i;
    int k;

    for (i = 0; i < TEX_SLOTS; i++) {
        g_dyn[i] = 0;
        g_dyn_density[i] = 0.0f;
    }
    sc = &level->scene;
    if (!sc->entities || sc->nentities < 1) {
        return 0;
    }
    for (i = 0; i < sc->nentities; i++) {
        const Entity *e;
        int p;

        e = &sc->entities[i];
        for (p = 0; p < 2; p++) {
            const EntityPose *pose;

            pose = p ? &e->idle : &e->walk;
            for (k = 0; k < ENTITY_VIEWS; k++) {
                const Image *img;
                int slot;
                float d;

                slot = (int)pose->slot[k];
                if (slot >= n) {
                    continue;
                }
                /*
                 * Marked even with no picture yet: the game reads a creature's
                 * pictures as it comes near, and adds them (texcache_resident_add).
                 */
                g_dyn[slot] = 1;
                img = level->images[slot];
                if (!img || pose->w[k] < 1) {
                    continue;
                }
                d = (float)img->width / (float)pose->w[k];
                if (d > g_dyn_density[slot]) {
                    g_dyn_density[slot] = d;
                }
            }
        }
    }
    /* Shared with a flat or a face: that one is drawn unnoted, so it stays with the place's. */
    for (i = 0; i < sc->nsprites; i++) {
        if (sc->sprites[i].slot < TEX_SLOTS) {
            g_dyn[sc->sprites[i].slot] = 0;
        }
    }
    for (i = 0; i < sc->nmodels; i++) {
        const Mesh *mesh;
        int t;

        mesh = sc->models[i].mesh;
        for (t = 0; mesh && t < mesh->ntris; t++) {
            if (mesh->tris[t].slot < TEX_SLOTS) {
                g_dyn[mesh->tris[t].slot] = 0;
            }
        }
    }
    count = 0;
    for (i = 0; i < n; i++) {
        count += g_dyn[i];
    }
    return count;
}

/* Texels a world unit of a wall in a place loaded whole: a 64 tile on a 256 face, as the stream's (wres.c). */
#define WALL_DENSITY 0.25f

static int g_all;

/*
 * Every picture of a place loaded whole by distance (texcache_upload, a
 * place past the card): a wall at WALL_DENSITY, a flat at its width in
 * texels over its width in the world, an entity's as marked.
 */
static void all_resident(const Level *level, int n, df_u32 need)
{
    const Scene *sc;
    int added;
    int i;

    sc = &level->scene;
    for (i = 0; i < sc->nsprites; i++) {
        const Flat *fl;
        const Image *img;
        float d;

        fl = &sc->sprites[i];
        if (fl->slot >= n || fl->w < 1.0f || g_dyn[fl->slot] || !(img = level->images[fl->slot])) {
            continue;
        }
        d = (float)img->width / fl->w;
        if (d > g_dyn_density[fl->slot]) {
            g_dyn_density[fl->slot] = d;
        }
    }
    added = 0;
    for (i = 0; i < n; i++) {
        int cut;
        float density;

        if (g_side[i] < 1 && !g_dyn[i]) {
            continue;
        }
        cut = g_dyn[i] ? 1 : g_cut[i];
        density = cut ? (g_dyn_density[i] > 0.0f ? g_dyn_density[i] : 1.0f) : WALL_DENSITY;
        added += chain_set(i, level->images[i], cut, density);
    }
    g_all = 1;
    g_n = n;
    g_nlive = added;
    log_print(LOG_INFO, "textures: %u KB at full size past the card: %d pictures by distance",
              (unsigned int)(need / 1024u), added);
}

int texcache_resident_all(void)
{
    return g_all;
}

int texcache_upload(const Level *level)
{
    const Palette *pal;
    df_u32 room;
    df_u32 planned;
    df_u32 reserve;
    int ndyn;
    int to_come;
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
    /*
     * The entities' pictures out of the budget, and room kept for them:
     * as much as they take at full size, one level each, up to
     * ENTITY_ROOM; never less than every one's smallest copy.
     */
    reserve = 0;
    ndyn = mark_entity_pictures(level, n);
    if (ndyn > 0) {
        df_u32 full;
        df_u32 tiny;

        full = 0;
        tiny = 0;
        to_come = 0;
        for (i = 0; i < n; i++) {
            df_u32 b;

            if (!g_dyn[i]) {
                continue;
            }
            tiny += TEXRES_MIN;
            if (g_side[i] < 1) {
                /* No picture yet: one to come, read when its creature comes near. */
                to_come++;
                continue;
            }
            if (glide_texture_bytes(g_side[i], g_cut[i], 0, &b)) {
                full += b;
            }
            g_side[i] = 0;
        }
        /* With pictures to come their sizes are not known: the whole room for them. */
        reserve = full < ENTITY_ROOM && !to_come ? full : ENTITY_ROOM;
        reserve = reserve < tiny ? tiny : reserve;
        room = room > reserve ? room - reserve : 0;
    }
    /*
     * A place too large for the card at full size (Daggerfall's castle
     * needs 5 MB, its flats 3.8 of it): every picture on the card by
     * distance, as a streamed world's (docs/memory.md), sharp near and
     * small far. Budgeted at load instead, its walls went to a quarter
     * size everywhere, before any sprite gave a level.
     */
    {
        df_u32 need;

        need = 0;
        for (i = 0; i < n; i++) {
            if (g_side[i] > 0) {
                need += card_bytes(g_side[i], g_cut[i]);
            }
        }
        if (need > room && res_open(pal)) {
            all_resident(level, n, need);
            return 1;
        }
    }
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
    /* The rest of the card: the entities' pictures, by distance. */
    if (reserve > 0 && res_open(pal)) {
        int added;

        added = 0;
        for (i = 0; i < n; i++) {
            if (g_dyn[i] && level->images[i] && chain_set(i, level->images[i], 1, g_dyn_density[i])) {
                added++;
            }
        }
        log_print(LOG_INFO, "textures: %d entity pictures by distance, %u KB kept for them",
                  added, (unsigned int)(reserve / 1024u));
    }
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

/*
 * A picture's levels in RAM, from level first down. A cutout keeps them
 * from half size (first 1): at 16 bits a texel its full level was three
 * quarters of its chain, 128 KB of a 256 sheet's 171, for a level drawn
 * only near. That level is made again from the picture (pot, its own
 * eight bits, kept by its owner while it is set) when it is sent.
 */
typedef struct Chain {
    df_u8 *levels;
    const df_u8 *pot;
    short side;
    df_u8 cutout;
    df_u8 first;
} Chain;

/*
 * g_stream: a streamed world, every picture by distance. g_res: a
 * residency session is open, the stream's, or a whole place's for its
 * entities' pictures (g_dyn).
 */
static int g_stream;
static int g_res;
static int g_stream_chain;
static Chain g_chains[TEXRES_SLOTS];
/* The chains' own heap: freed one by one as pictures go. */
static MemHeap *g_chain_heap;
static const df_u8 *g_pr;
static const df_u8 *g_pg;
static const df_u8 *g_pb;

/*
 * Whether a picture goes to the card with its smaller levels. A sprite
 * does not: it is small on the glass, and the residency already sends
 * the level its distance needs, so one level is all it uses. A chain is
 * a third more than its top level, and the card's blocks are powers of
 * two: a 256 sprite's chain (171 KB) took a 256 KB block, its one level
 * takes 128. Walls keep their chains: one wall spans near and far.
 */
static int card_chain(int cutout)
{
    return g_stream_chain && !cutout;
}

static df_u32 res_bytes(int side, int cutout)
{
    df_u32 b;

    if (!glide_texture_bytes(side, cutout, card_chain(cutout), &b)) {
        return 0x7fffffffu;
    }
    return b;
}

/* The full level and the stored ones after it, for a cutout's full-size upload: kept for the process. */
static df_u8 *g_top;

static int res_upload(int slot, int drop, df_u32 addr, df_u32 *handle)
{
    const Chain *c;
    df_u32 top;

    c = &g_chains[slot];
    if (!c->levels) {
        return 0;
    }
    if (drop >= c->first) {
        /* The stored levels are a whole chain of a picture first times halved. */
        return glide_upload_chain(c->levels, c->side >> c->first, c->cutout, card_chain(c->cutout),
                                  drop - c->first, addr, handle);
    }
    /* The full level, made now from the picture, with the stored ones after it. */
    if (!g_top) {
        g_top = (df_u8 *)mem_keep(MIP_MAX_BYTES);
    }
    if (!g_top || !c->pot) {
        return 0;
    }
    top = mip_build(c->pot, c->side, c->cutout, 0, 0, g_pr, g_pg, g_pb, g_top);
    if (!top) {
        return 0;
    }
    if (card_chain(c->cutout)) {
        mem_copy(g_top + top, c->levels, mip_texels(c->side, 1, 1) * (c->cutout ? 2u : 1u));
    }
    return glide_upload_chain(g_top, c->side, c->cutout, card_chain(c->cutout), 0, addr, handle);
}

static void res_release(df_u32 handle)
{
    glide_release(handle);
}

/* A residency session over the card from here to its end, under pal. */
static int res_open(const Palette *pal)
{
    TexResOps ops;
    df_u32 lo;
    df_u32 hi;
    int i;

    g_pr = pal->r;
    g_pg = pal->g;
    g_pb = pal->b;
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
    g_res = 1;
    return 1;
}

int texcache_stream_begin(const Level *level)
{
    df_u32 lo;
    df_u32 hi;

    texcache_stream_end();
    if (!level || !level->palette || !glide_palette(level->palette->r, level->palette->g,
                                                     level->palette->b)) {
        g_why = "palette";
        return 0;
    }
    if (!res_open(level->palette)) {
        return 0;
    }
    glide_tex_range(&lo, &hi);
    g_stream = 1;
    g_n = 0;
    log_print(LOG_INFO, "textures: streamed, %u KB of the card", (unsigned int)((hi - lo) / 1024u));
    return 1;
}

void texcache_stream_end(void)
{
    if (!g_res) {
        return;
    }
    texres_end();
    mem_heap_delete(g_chain_heap);
    g_chain_heap = 0;
    g_stream = 0;
    g_res = 0;
    g_all = 0;
}

int texcache_streamed(void)
{
    return g_stream;
}

int texcache_resident(void)
{
    return g_res;
}

int texcache_resident_add(int slot, const Image *img, int cutout, float density)
{
    if (!g_res || g_stream || slot < 0 || slot >= TEX_SLOTS || !img) {
        return 0;
    }
    g_dyn[slot] = 1;
    return chain_set(slot, img, cutout, density);
}

int texcache_stream_set(int slot, const Image *img, int cutout, float density)
{
    if (!g_stream) {
        return 0;
    }
    return chain_set(slot, img, cutout, density);
}

static int chain_set(int slot, const Image *img, int cutout, float density)
{
    Chain *c;
    MemHeap *was;
    df_u32 n;
    int side;
    int first;

    if (!g_res || slot < 0 || slot >= TEXRES_SLOTS || !img || !img->pot
        || img->pot_w != img->pot_h || img->pot_w < 1 || img->pot_w > 256
        || (img->pot_w & (img->pot_w - 1))) {
        return 0;
    }
    texcache_stream_drop(slot);
    side = img->pot_w;
    /* A cutout of more than one texel keeps its levels from half size (Chain). */
    first = cutout && side > 1 ? 1 : 0;
    n = mip_texels(side, first, 1) * (cutout ? 2u : 1u);
    c = &g_chains[slot];
    was = mem_heap_use(g_chain_heap);
    c->levels = (df_u8 *)mem_alloc(n);
    mem_heap_use(was);
    if (!c->levels) {
        return 0;
    }
    /* Built once, every level kept: a sharper copy is then only a download. */
    if (!mip_build(img->pot, side, cutout, first, 1, g_pr, g_pg, g_pb, c->levels)) {
        texcache_stream_drop(slot);
        return 0;
    }
    c->pot = img->pot;
    c->side = (short)side;
    c->cutout = (df_u8)(cutout ? 1 : 0);
    c->first = (df_u8)first;
    if (!texres_add(slot, side, cutout, density)) {
        texcache_stream_drop(slot);
        return 0;
    }
    return 1;
}

void texcache_stream_drop(int slot)
{
    Chain *c;

    if (!g_res || slot < 0 || slot >= TEXRES_SLOTS) {
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
    if (g_res) {
        texres_note(slot, dist);
    }
}

void texcache_stream_service(float focal, df_u32 budget)
{
    if (g_res) {
        texres_service(focal, budget);
    }
}

df_u32 texcache_stream_ram(void)
{
    return g_res ? mem_heap_bytes(g_chain_heap) : 0;
}

void texcache_bind(int slot)
{
    if (g_stream || g_all || (g_res && slot >= 0 && slot < TEX_SLOTS && g_dyn[slot])) {
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
