#include "engine/ui/pic.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"

/*
 * Pictures on the screen. docs/pictures.md.
 *
 * Each picture is a list of tiles; each tile keeps its texels in card
 * format in this module's own heap (freed with the picture, never with
 * a place), and while it is on the card, its place in the room and its
 * handle. The room is a list of free extents, first fit, joined when
 * they meet.
 */

#define NO_TILE -1
#define EXTENTS 512
/* The card's alignment for a texture's start. */
#define ALIGN 8u

typedef struct Tile {
    /* Where it sits in its picture, and its side. */
    df_u16 x;
    df_u16 y;
    df_u16 side;
    df_u8 on;
    df_u8 used;
    df_u8 *texels;
    df_u32 addr;
    df_u32 bytes;
    df_u32 handle;
    int next;
} Tile;

typedef struct Pic {
    int used;
    int w;
    int h;
    int cutout;
    /* Its palette slot, -1 for a cutout (its colours are in its texels). */
    int pal;
    int first;
    /* Tiles of it not on the card: it is drawn only when 0. */
    int missing;
} Pic;

typedef struct Palette {
    int refs;
    df_u32 table[256];
} Palette;

typedef struct Extent {
    df_u32 at;
    df_u32 size;
} Extent;

static Pic g_pic[PIC_MAX + 1];
static Tile g_tile[PIC_TILES];
static Palette g_pal[PIC_PALETTES];
static MemHeap *g_heap;
static int g_room_kb;
static const PicOps *g_ops;
static df_u32 g_lo;
static df_u32 g_hi;
static Extent g_free[EXTENTS];
static int g_nfree;
static int g_warned;

void pic_set_room(int kb)
{
    g_room_kb = kb < 0 ? 0 : (kb > 1024 ? 1024 : kb);
}

int pic_room(void)
{
    return g_room_kb;
}

/* The room's free extents: first fit, the rest of the extent kept. */
static int room_take(df_u32 size, df_u32 *at)
{
    int i;

    size = (size + ALIGN - 1u) & ~(ALIGN - 1u);
    for (i = 0; i < g_nfree; i++) {
        if (g_free[i].size >= size) {
            *at = g_free[i].at;
            g_free[i].at += size;
            g_free[i].size -= size;
            if (g_free[i].size == 0) {
                for (; i + 1 < g_nfree; i++) {
                    g_free[i] = g_free[i + 1];
                }
                g_nfree--;
            }
            return 1;
        }
    }
    return 0;
}

/* Back into the list, in address order, joined to the extents it meets. */
static void room_give(df_u32 at, df_u32 size)
{
    int i;
    int k;

    size = (size + ALIGN - 1u) & ~(ALIGN - 1u);
    for (i = 0; i < g_nfree && g_free[i].at < at; i++) {
    }
    if (i > 0 && g_free[i - 1].at + g_free[i - 1].size == at) {
        g_free[i - 1].size += size;
        if (i < g_nfree && g_free[i - 1].at + g_free[i - 1].size == g_free[i].at) {
            g_free[i - 1].size += g_free[i].size;
            for (k = i; k + 1 < g_nfree; k++) {
                g_free[k] = g_free[k + 1];
            }
            g_nfree--;
        }
        return;
    }
    if (i < g_nfree && at + size == g_free[i].at) {
        g_free[i].at = at;
        g_free[i].size += size;
        return;
    }
    /* A full list loses the extent: the room is a little smaller until the next place. */
    if (g_nfree >= EXTENTS) {
        return;
    }
    for (k = g_nfree; k > i; k--) {
        g_free[k] = g_free[k - 1];
    }
    g_free[i].at = at;
    g_free[i].size = size;
    g_nfree++;
}

/* One tile onto the card: room, then the upload. 0 leaves it off. */
static int tile_send(Tile *t, int cutout)
{
    df_u32 at;

    if (!g_ops || t->on || !room_take(t->bytes, &at)) {
        return 0;
    }
    if (!g_ops->upload(t->texels, t->side, cutout, at, &t->handle)) {
        room_give(at, t->bytes);
        return 0;
    }
    t->addr = at;
    t->on = 1;
    return 1;
}

static void tile_drop(Tile *t)
{
    if (!t->on) {
        return;
    }
    if (g_ops) {
        g_ops->release(t->handle);
    }
    room_give(t->addr, t->bytes);
    t->on = 0;
}

/* Every tile of p onto the card, as many as fit; how many did not. */
static int pic_send(Pic *p)
{
    int i;
    int missing;

    missing = 0;
    for (i = p->first; i != NO_TILE; i = g_tile[i].next) {
        if (!g_tile[i].on && !tile_send(&g_tile[i], p->cutout)) {
            missing++;
        }
    }
    p->missing = missing;
    return missing;
}

static void warn_missing(int id, const Pic *p)
{
    if (p->missing && !g_warned) {
        g_warned = 1;
        log_print(LOG_WARN, "pictures: %d x %d (%d) is not on the card: %d KB of room (pic_set_room)",
                  p->w, p->h, id, g_room_kb);
    }
}

void pic_begin(df_u32 lo, df_u32 hi, const PicOps *ops)
{
    int i;

    /* The card was emptied: every handle is gone with it, nothing to release. */
    for (i = 0; i < PIC_TILES; i++) {
        g_tile[i].on = 0;
    }
    g_ops = ops;
    g_lo = lo;
    g_hi = hi > lo ? hi : lo;
    g_nfree = 0;
    if (g_hi > g_lo) {
        g_free[0].at = (g_lo + ALIGN - 1u) & ~(ALIGN - 1u);
        g_free[0].size = g_hi > g_free[0].at ? g_hi - g_free[0].at : 0;
        g_nfree = g_free[0].size ? 1 : 0;
    }
    g_warned = 0;
    for (i = 1; i <= PIC_MAX; i++) {
        if (g_pic[i].used) {
            pic_send(&g_pic[i]);
            warn_missing(i, &g_pic[i]);
        }
    }
}

void pic_end(void)
{
    int i;

    for (i = 0; i < PIC_TILES; i++) {
        g_tile[i].on = 0;
    }
    g_ops = 0;
    g_lo = 0;
    g_hi = 0;
    g_nfree = 0;
}

static int pot_floor(int n)
{
    int p;

    for (p = 1; p * 2 <= n; p *= 2) {
    }
    return p;
}

/* A palette slot holding table: one already there, or a free one. -1 when all are taken. */
static int palette_find(const df_u32 *table)
{
    int i;
    int k;
    int free_slot;

    free_slot = -1;
    for (i = 0; i < PIC_PALETTES; i++) {
        if (!g_pal[i].refs) {
            if (free_slot < 0) {
                free_slot = i;
            }
            continue;
        }
        for (k = 0; k < 256 && g_pal[i].table[k] == table[k]; k++) {
        }
        if (k == 256) {
            g_pal[i].refs++;
            return i;
        }
    }
    if (free_slot >= 0) {
        for (k = 0; k < 256; k++) {
            g_pal[free_slot].table[k] = table[k];
        }
        g_pal[free_slot].refs = 1;
    }
    return free_slot;
}

/* The loading picture's source, for tile_fill. */
static const df_u8 *g_src;
static const df_u8 *g_rgb;
static int g_src_w;
static int g_src_h;
static int g_src_cut;

/*
 * One tile's texels from the source. Past the picture's edge, a cutout
 * is clear and an opaque one repeats its edge: never drawn, but a point
 * on a tile's edge never samples a stray colour.
 */
static int tile_fill(Tile *t)
{
    int bpp;
    int row;
    int col;
    df_u8 *out;

    bpp = g_src_cut ? 2 : 1;
    t->texels = (df_u8 *)mem_alloc((df_u32)t->side * (df_u32)t->side * (df_u32)bpp);
    if (!t->texels) {
        return 0;
    }
    out = t->texels;
    for (row = 0; row < t->side; row++) {
        int sy;

        sy = t->y + row;
        if (sy >= g_src_h) {
            sy = g_src_h - 1;
        }
        for (col = 0; col < t->side; col++) {
            int sx;
            int past;
            df_u8 c;

            sx = t->x + col;
            past = sx >= g_src_w || t->y + row >= g_src_h;
            if (sx >= g_src_w) {
                sx = g_src_w - 1;
            }
            c = g_src[sy * g_src_w + sx];
            if (!g_src_cut) {
                *out++ = c;
                continue;
            }
            if (past || c == 0) {
                *out++ = 0;
                *out++ = 0;
                continue;
            }
            {
                df_u32 v;

                /* ARGB 1555 little-endian, alpha set. */
                v = 0x8000u | ((df_u32)(g_rgb[c * 3] >> 3) << 10)
                    | ((df_u32)(g_rgb[c * 3 + 1] >> 3) << 5) | (df_u32)(g_rgb[c * 3 + 2] >> 3);
                *out++ = (df_u8)(v & 0xffu);
                *out++ = (df_u8)(v >> 8);
            }
        }
    }
    return 1;
}

/* A free tile record. NO_TILE when all are taken. */
static int tile_new(void)
{
    int i;

    for (i = 0; i < PIC_TILES && g_tile[i].used; i++) {
    }
    return i < PIC_TILES ? i : NO_TILE;
}

static int g_tile_fail;

/* A tile of side at x, y, made and put at the front of p's list. */
static void tile_add(Pic *p, int x, int y, int side)
{
    int i;
    Tile *t;

    if (g_tile_fail) {
        return;
    }
    i = tile_new();
    if (i == NO_TILE) {
        g_tile_fail = 1;
        return;
    }
    t = &g_tile[i];
    t->x = (df_u16)x;
    t->y = (df_u16)y;
    t->side = (df_u16)side;
    t->on = 0;
    t->used = 1;
    t->texels = 0;
    t->bytes = g_ops && g_ops->bytes ? g_ops->bytes(side, g_src_cut)
                                     : (df_u32)side * (df_u32)side * (g_src_cut ? 2u : 1u);
    t->next = p->first;
    p->first = i;
    if (!tile_fill(t)) {
        g_tile_fail = 1;
    }
}

/*
 * Cover w x h at x, y: a grid of the largest square that fits the
 * shorter side (PIC_TILE_MAX at most), then the strip to its right and
 * the strip under it the same way. A strip thinner than PIC_TILE_MIN is
 * covered by PIC_TILE_MIN squares, padded past the picture.
 */
static void cover(Pic *p, int x, int y, int w, int h)
{
    int m;
    int s;
    int nx;
    int ny;
    int i;
    int k;

    if (w <= 0 || h <= 0 || g_tile_fail) {
        return;
    }
    m = w < h ? w : h;
    if (m < PIC_TILE_MIN) {
        s = PIC_TILE_MIN;
        for (k = 0; k < h; k += s) {
            for (i = 0; i < w; i += s) {
                tile_add(p, x + i, y + k, s);
            }
        }
        return;
    }
    s = pot_floor(m);
    if (s > PIC_TILE_MAX) {
        s = PIC_TILE_MAX;
    }
    nx = w / s;
    ny = h / s;
    for (k = 0; k < ny; k++) {
        for (i = 0; i < nx; i++) {
            tile_add(p, x + i * s, y + k * s, s);
        }
    }
    cover(p, x + nx * s, y, w - nx * s, ny * s);
    cover(p, x, y + ny * s, w, h - ny * s);
}

static void tiles_free(Pic *p)
{
    MemHeap *was;
    int i;
    int next;

    was = mem_heap_use(g_heap);
    for (i = p->first; i != NO_TILE; i = next) {
        next = g_tile[i].next;
        tile_drop(&g_tile[i]);
        if (g_tile[i].texels) {
            mem_heap_free(g_heap, g_tile[i].texels);
        }
        g_tile[i].texels = 0;
        g_tile[i].used = 0;
    }
    mem_heap_use(was);
    p->first = NO_TILE;
}

int pic_load(const df_u8 *pixels, int w, int h, const df_u8 *rgb, int cutout)
{
    int id;
    Pic *p;
    MemHeap *was;

    if (!pixels || !rgb || w < 1 || h < 1 || w > PIC_SIDE_MAX || h > PIC_SIDE_MAX) {
        return 0;
    }
    for (id = 1; id <= PIC_MAX && g_pic[id].used; id++) {
    }
    if (id > PIC_MAX) {
        log_text(LOG_WARN, "pictures: no slot free");
        return 0;
    }
    if (!g_heap) {
        g_heap = mem_heap_new();
        if (!g_heap) {
            return 0;
        }
    }
    p = &g_pic[id];
    p->w = w;
    p->h = h;
    p->cutout = cutout ? 1 : 0;
    p->pal = -1;
    p->first = NO_TILE;
    p->missing = 0;
    if (!p->cutout) {
        df_u32 table[256];
        int i;

        /* 0xRRGGBB, as grTexDownloadTable takes it (glide_palette). */
        for (i = 0; i < 256; i++) {
            table[i] = ((df_u32)rgb[i * 3] << 16) | ((df_u32)rgb[i * 3 + 1] << 8)
                | (df_u32)rgb[i * 3 + 2];
        }
        p->pal = palette_find(table);
        if (p->pal < 0) {
            log_text(LOG_WARN, "pictures: no palette slot free");
            return 0;
        }
    }
    g_src = pixels;
    g_rgb = rgb;
    g_src_w = w;
    g_src_h = h;
    g_src_cut = p->cutout;
    g_tile_fail = 0;
    was = mem_heap_use(g_heap);
    cover(p, 0, 0, w, h);
    mem_heap_use(was);
    p->used = 1;
    if (g_tile_fail) {
        log_print(LOG_WARN, "pictures: %d x %d did not fit in memory or tile records", w, h);
        pic_free(id);
        return 0;
    }
    if (g_hi > g_lo) {
        pic_send(p);
        warn_missing(id, p);
    } else {
        /* No room yet: it goes on the card with the next place. */
        p->missing = 1;
    }
    return id;
}

void pic_free(int id)
{
    Pic *p;

    if (id < 1 || id > PIC_MAX || !g_pic[id].used) {
        return;
    }
    p = &g_pic[id];
    tiles_free(p);
    if (p->pal >= 0 && g_pal[p->pal].refs > 0) {
        g_pal[p->pal].refs--;
    }
    p->used = 0;
}

int pic_size(int id, int *w, int *h)
{
    if (id < 1 || id > PIC_MAX || !g_pic[id].used) {
        return 0;
    }
    if (w) {
        *w = g_pic[id].w;
    }
    if (h) {
        *h = g_pic[id].h;
    }
    return 1;
}

void pic_draw(int id, int x, int top, int w, int h)
{
    if (id < 1 || id > PIC_MAX || !g_pic[id].used) {
        return;
    }
    pic_draw_part(id, 0, 0, g_pic[id].w, g_pic[id].h, x, top, w, h, 255);
}

void pic_draw_part(int id, int sx, int sy, int sw, int sh, int x, int top, int w, int h,
                   int alpha)
{
    Pic *p;
    float kx;
    float ky;
    int i;

    if (id < 1 || id > PIC_MAX || !g_pic[id].used || !g_ops || alpha < 1) {
        return;
    }
    p = &g_pic[id];
    /* A picture with a tile off the card is not drawn at all, never with a hole in it. */
    if (p->missing) {
        return;
    }
    if (sx < 0) {
        sx = 0;
    }
    if (sy < 0) {
        sy = 0;
    }
    if (sx + sw > p->w) {
        sw = p->w - sx;
    }
    if (sy + sh > p->h) {
        sh = p->h - sy;
    }
    if (sw < 1 || sh < 1 || w < 1 || h < 1) {
        return;
    }
    if (alpha > 255) {
        alpha = 255;
    }
    kx = (float)w / (float)sw;
    ky = (float)h / (float)sh;
    g_ops->begin(p->cutout, alpha);
    if (p->pal >= 0) {
        g_ops->palette(p->pal, g_pal[p->pal].table);
    }
    for (i = p->first; i != NO_TILE; i = g_tile[i].next) {
        const Tile *t;
        int x0;
        int y0;
        int x1;
        int y1;
        float texel;

        t = &g_tile[i];
        /* The tile's part inside the picture and the source rectangle, in picture pixels. */
        x0 = t->x > sx ? t->x : sx;
        y0 = t->y > sy ? t->y : sy;
        x1 = t->x + t->side;
        y1 = t->y + t->side;
        if (x1 > sx + sw) {
            x1 = sx + sw;
        }
        if (y1 > sy + sh) {
            y1 = sy + sh;
        }
        if (x0 >= x1 || y0 >= y1) {
            continue;
        }
        /*
         * Each edge from its integer pixel by the same sum, so two tiles
         * that meet put their shared edge on the same screen point. t
         * grows down the picture, as a font sheet's rows do.
         */
        texel = 256.0f / (float)t->side;
        g_ops->quad(t->handle,
                    (float)x + (float)(x0 - sx) * kx, (float)top - (float)(y0 - sy) * ky,
                    (float)x + (float)(x1 - sx) * kx, (float)top - (float)(y1 - sy) * ky,
                    (float)(x0 - t->x) * texel, (float)(y0 - t->y) * texel,
                    (float)(x1 - t->x) * texel, (float)(y1 - t->y) * texel);
    }
    g_ops->end();
}

void pic_stats(int *pics, int *tiles_on, df_u32 *used, df_u32 *room)
{
    int i;
    int n;
    int on;
    df_u32 bytes;

    n = 0;
    for (i = 1; i <= PIC_MAX; i++) {
        n += g_pic[i].used;
    }
    on = 0;
    bytes = 0;
    for (i = 0; i < PIC_TILES; i++) {
        if (g_tile[i].used && g_tile[i].on) {
            on++;
            bytes += (g_tile[i].bytes + ALIGN - 1u) & ~(ALIGN - 1u);
        }
    }
    if (pics) {
        *pics = n;
    }
    if (tiles_on) {
        *tiles_on = on;
    }
    if (used) {
        *used = bytes;
    }
    if (room) {
        *room = g_hi - g_lo;
    }
}
