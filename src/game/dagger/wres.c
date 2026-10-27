#include "game/dagger/wres.h"
#include "game/dagger/swaps.h"
#include "game/dagger/texture.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/gfx/texcache.h"

/* See wres.h and docs/streaming.md. */

#define HASH 8192
/* Allocations one picture may make and still be freed: an image, its pot, a sheet of frames. */
#define TRACK_MAX 160
/* Climate variants: four families, summer and winter. */
#define VARIANTS 8
/* Walls: about a texel for four world units (a 64 tile on a 256 face). */
#define WALL_DENSITY 0.25f

typedef struct Pic {
    short file;
    short record;
    df_u8 use;
    df_u8 failed;
    df_u8 card;
    /* A picture the game made (a far chunk's ground), not read: its image is the game's. */
    df_u8 dynamic;
    Image *img;
    /* Read and laid out (decode_pic), its chain and card copy not yet made (finish_pic). */
    Image *pending;
    void **track;
    int ntrack;
    df_u32 bytes;
    int refs;
    df_u32 released;
} Pic;

static int g_on;
static const Palette *g_pal;
static Level *g_level;
static Pic g_pic[WRES_PICTURES];
static int g_npic;
static int g_hash[HASH];
static Image *g_images[WRES_PICTURES];
static df_u8 g_cutout[WRES_PICTURES];
static df_u8 g_glass[WRES_PICTURES];
static MemHeap *g_pic_heap;
static MemHeap *g_mesh_heap;
static df_u32 g_pic_ram;
static df_u32 g_stamp;
static ArchEnt *g_mesh[VARIANTS];
static int g_nmesh[VARIANTS];
/* Dynamic slots given back, for the next dynamic picture: a long walk makes thousands. */
static df_u16 g_free[WRES_PICTURES];
static int g_nfree;

int wres_begin(const Palette *pal, Level *level)
{
    int i;

    wres_end();
    g_pic_heap = mem_heap_new();
    g_mesh_heap = mem_heap_new();
    if (!g_pic_heap || !g_mesh_heap) {
        return 0;
    }
    g_pal = pal;
    g_level = level;
    g_npic = 0;
    g_nfree = 0;
    g_pic_ram = 0;
    g_stamp = 0;
    for (i = 0; i < HASH; i++) {
        g_hash[i] = 0;
    }
    for (i = 0; i < WRES_PICTURES; i++) {
        g_images[i] = 0;
        g_cutout[i] = 0;
        g_glass[i] = 0;
    }
    for (i = 0; i < VARIANTS; i++) {
        g_mesh[i] = 0;
        g_nmesh[i] = 0;
    }
    level->images = g_images;
    level->cutout = g_cutout;
    level->glass = g_glass;
    level->nimages = 0;
    g_on = 1;
    return 1;
}

void wres_end(void)
{
    if (!g_on) {
        return;
    }
    /* The card's copies and the chains go with texcache's stream; the pixels and meshes here. */
    mem_heap_delete(g_pic_heap);
    mem_heap_delete(g_mesh_heap);
    g_pic_heap = 0;
    g_mesh_heap = 0;
    g_on = 0;
}

static int hash_of(int key)
{
    return (int)(((df_u32)key * 2654435761u) >> 19) & (HASH - 1);
}

int wres_slot(int file, int record, int use)
{
    int key;
    int h;
    Pic *p;

    if (!g_on || file < 0 || record < 0 || record > 255) {
        return -1;
    }
    key = (file * 256 + record) * 2 + use;
    for (h = hash_of(key); g_hash[h]; h = (h + 1) & (HASH - 1)) {
        p = &g_pic[g_hash[h] - 1];
        if (p->file == file && p->record == record && p->use == use) {
            return g_hash[h] - 1;
        }
    }
    if (g_npic >= WRES_PICTURES || g_npic >= HASH * 3 / 4) {
        return -1;
    }
    p = &g_pic[g_npic];
    mem_zero(p, sizeof(Pic));
    p->file = (short)file;
    p->record = (short)record;
    p->use = (df_u8)use;
    g_cutout[g_npic] = (df_u8)(use == WRES_SPRITE);
    g_glass[g_npic] = (df_u8)(use == WRES_WALL && swap_window(file, record));
    g_hash[h] = g_npic + 1;
    g_npic++;
    g_level->nimages = g_npic;
    return g_npic - 1;
}

/*
 * A picture is read in two parts, each a call (docs/streaming.md, A
 * chunk's contents): decode_pic reads the file and lays the picture out
 * (a wall's smooth stretch, a sprite's sheet of frames or its padding),
 * about 8 ms of a 256 sheet on the 233; finish_pic builds its mip chain
 * and puts its small copy on the card, about as much again. Whole, it
 * was a step past any frame's budget. Between the two it is not drawn:
 * its image is set only when it is finished.
 */
static int decode_pic(int slot)
{
    void *list[TRACK_MAX];
    Pic *p;
    MemHeap *was;
    Image *img;
    df_u32 before;
    int n;

    p = &g_pic[slot];
    before = mem_heap_bytes(g_pic_heap);
    was = mem_heap_use(g_pic_heap);
    n = 0;
    mem_track(list, TRACK_MAX, &n);
    img = texture_load(p->file, p->record);
    if (img && p->use == WRES_WALL) {
        /* A wall tiles: its stretch to powers of two is redone smoothly (world.c's finish_images). */
        if (img->span_w != img->width || img->span_h != img->height) {
            texture_smooth(img, g_pal->r, g_pal->g, g_pal->b);
        }
    } else if (img) {
        /* A sprite: a sheet of its frames when it has them, else padded at its own size. */
        if (!texture_sheet(img, p->file, p->record)) {
            texture_pad(img);
        }
    }
    mem_track(0, 0, 0);
    if (img && n <= TRACK_MAX) {
        int k;

        p->track = (void **)mem_alloc((df_u32)(n > 0 ? n : 1) * (df_u32)sizeof(void *));
        if (p->track) {
            for (k = 0; k < n; k++) {
                p->track[k] = list[k];
            }
            p->ntrack = n;
        }
    }
    mem_heap_use(was);
    if (!img || n > TRACK_MAX || !p->track) {
        /* Never freed piece by piece if its allocations overflowed the track: it stays. */
        p->failed = (df_u8)(img == 0);
        if (!img) {
            log_print(LOG_WARN, "wres: TEXTURE.%d record %d did not load", p->file, p->record);
            return 0;
        }
    }
    p->pending = img;
    p->bytes = mem_heap_bytes(g_pic_heap) - before;
    g_pic_ram += p->bytes;
    return 1;
}

static void finish_pic(int slot)
{
    Pic *p;
    Image *img;
    float density;

    p = &g_pic[slot];
    img = p->pending;
    p->pending = 0;
    p->img = img;
    g_images[slot] = img;
    density = WALL_DENSITY;
    if (p->use == WRES_SPRITE) {
        int extent;

        extent = texture_extent(img->width, img->xscale);
        density = extent > 0 ? (float)img->width / (float)extent : 1.0f;
    }
    p->card = (df_u8)texcache_stream_set(slot, img, p->use == WRES_SPRITE, density);
}

void wres_hold(int slot)
{
    if (g_on && slot >= 0 && slot < g_npic) {
        g_pic[slot].refs++;
    }
}

int wres_load(int slot)
{
    Pic *p;

    if (!g_on || slot < 0 || slot >= g_npic) {
        return 0;
    }
    p = &g_pic[slot];
    if (p->pending) {
        finish_pic(slot);
        return 1;
    }
    if (p->img || p->failed || p->dynamic) {
        return 0;
    }
    /* Its file first, a piece a call; then its pixels; then its chain. */
    if (!texture_file_step(p->file)) {
        return 1;
    }
    decode_pic(slot);
    return 1;
}

int wres_acquire(int slot)
{
    if (!g_on || slot < 0 || slot >= g_npic) {
        return 0;
    }
    wres_hold(slot);
    while (wres_load(slot)) {
    }
    return g_pic[slot].img != 0;
}

void wres_release(int slot)
{
    Pic *p;

    if (!g_on || slot < 0 || slot >= g_npic) {
        return;
    }
    p = &g_pic[slot];
    if (p->refs > 0 && --p->refs == 0) {
        p->released = ++g_stamp;
    }
}

int wres_dynamic(Image *img, float density)
{
    Pic *p;
    int slot;

    if (!g_on || !img) {
        return -1;
    }
    if (g_nfree > 0) {
        slot = (int)g_free[--g_nfree];
    } else if (g_npic < WRES_PICTURES) {
        slot = g_npic++;
        g_level->nimages = g_npic;
    } else {
        return -1;
    }
    p = &g_pic[slot];
    mem_zero(p, sizeof(Pic));
    p->file = -1;
    p->record = -1;
    p->dynamic = 1;
    p->img = img;
    g_images[slot] = img;
    g_cutout[slot] = 0;
    g_glass[slot] = 0;
    p->card = (df_u8)texcache_stream_set(slot, img, 0, density);
    return slot;
}

void wres_forget(int slot)
{
    Pic *p;

    if (!g_on || slot < 0 || slot >= g_npic || !g_pic[slot].dynamic) {
        return;
    }
    p = &g_pic[slot];
    texcache_stream_drop(slot);
    g_images[slot] = 0;
    p->img = 0;
    p->dynamic = 0;
    g_free[g_nfree++] = (df_u16)slot;
}

int wres_picture(int slot, int *file, int *record)
{
    if (!g_on || slot < 0 || slot >= g_npic || g_pic[slot].dynamic || g_pic[slot].file < 0) {
        return 0;
    }
    *file = g_pic[slot].file;
    *record = g_pic[slot].record;
    return 1;
}

const Image *wres_image(int slot)
{
    return g_on && slot >= 0 && slot < g_npic ? g_pic[slot].img : 0;
}

static void free_pic(int slot)
{
    Pic *p;
    int k;

    p = &g_pic[slot];
    texcache_stream_drop(slot);
    for (k = p->ntrack - 1; k >= 0; k--) {
        mem_heap_free(g_pic_heap, p->track[k]);
    }
    mem_heap_free(g_pic_heap, p->track);
    p->track = 0;
    p->ntrack = 0;
    p->img = 0;
    p->pending = 0;
    p->card = 0;
    g_images[slot] = 0;
    g_pic_ram -= p->bytes;
    p->bytes = 0;
}

void wres_trim(df_u32 cap)
{
    while (g_on && g_pic_ram > cap) {
        int best;
        int i;

        best = -1;
        for (i = 0; i < g_npic; i++) {
            const Pic *p;

            p = &g_pic[i];
            if ((p->img || p->pending) && p->refs == 0 && p->track
                && (best < 0 || p->released < g_pic[best].released)) {
                best = i;
            }
        }
        if (best < 0) {
            return;
        }
        free_pic(best);
    }
}

static int variant_of(int family, int winter)
{
    int f;

    f = family == 0 ? 0 : (family == 100 ? 1 : (family == 300 ? 2 : 3));
    return f * 2 + (winter ? 1 : 0);
}

ArchEnt *wres_meshes(int family, int winter, int **count)
{
    int v;

    v = variant_of(family, winter);
    if (!g_mesh[v]) {
        MemHeap *was;

        was = mem_heap_use(g_mesh_heap);
        g_mesh[v] = (ArchEnt *)mem_alloc((df_u32)ARCH_CACHE * (df_u32)sizeof(ArchEnt));
        mem_heap_use(was);
    }
    *count = &g_nmesh[v];
    return g_mesh[v];
}

MemHeap *wres_mesh_heap(void)
{
    return g_mesh_heap;
}

void wres_paint(Mesh *mesh, int family, int winter)
{
    static df_u8 seen[WRES_PICTURES];
    df_u16 keys[256];
    int nkeys;
    int t;

    if (!mesh || mesh->have_keys) {
        return;
    }
    nkeys = 0;
    for (t = 0; t < mesh->ntris; t++) {
        Tri *tri;
        int archive;
        int slot;

        tri = &mesh->tris[t];
        archive = swap_climate((int)tri->tex_file, (int)tri->tex_image, family, winter,
                               texture_record_count);
        slot = wres_slot(archive, (int)tri->tex_image, WRES_WALL);
        tri->slot = (df_u16)(slot >= 0 ? slot : 0xffff);
        if (slot >= 0 && !seen[slot] && nkeys < 256) {
            seen[slot] = 1;
            keys[nkeys++] = (df_u16)slot;
        }
    }
    for (t = 0; t < nkeys; t++) {
        seen[keys[t]] = 0;
    }
    mesh->slot_keys = nkeys ? (df_u16 *)mem_alloc((df_u32)nkeys * (df_u32)sizeof(df_u16)) : 0;
    if (nkeys && mesh->slot_keys) {
        for (t = 0; t < nkeys; t++) {
            mesh->slot_keys[t] = keys[t];
        }
    }
    mesh->nslot_keys = mesh->slot_keys ? nkeys : 0;
    mesh->have_keys = 1;
}

void wres_meshes_reset(void)
{
    int i;

    if (!g_on) {
        return;
    }
    mem_heap_delete(g_mesh_heap);
    g_mesh_heap = mem_heap_new();
    for (i = 0; i < VARIANTS; i++) {
        g_mesh[i] = 0;
        g_nmesh[i] = 0;
    }
}

int wres_full(void)
{
    return g_on && mem_heap_bytes(g_mesh_heap) > WRES_MESH_RAM;
}

void wres_stats(int *slots, int *in_ram, df_u32 *ram, int *held, int *meshes, df_u32 *mesh_ram)
{
    int i;
    int r;
    int h;
    int m;

    r = 0;
    h = 0;
    for (i = 0; i < g_npic; i++) {
        r += g_pic[i].img != 0;
        h += g_pic[i].refs > 0;
    }
    m = 0;
    for (i = 0; i < VARIANTS; i++) {
        m += g_nmesh[i];
    }
    if (slots) {
        *slots = g_npic;
    }
    if (in_ram) {
        *in_ram = r;
    }
    if (ram) {
        *ram = g_pic_ram;
    }
    if (held) {
        *held = h;
    }
    if (meshes) {
        *meshes = m;
    }
    if (mesh_ram) {
        *mesh_ram = g_on ? mem_heap_bytes(g_mesh_heap) : 0;
    }
}
