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
    /* 1: full colour, its tiles RGB 565 (pic_load_565); no palette. */
    int rgb;
    /* Its palette slot; a cutout's has index 0 the key (PIC_KEY). */
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

/*
 * A cutout on a card without the chroma key: its tile as ARGB 1555,
 * little-endian, index 0 clear, the rest opaque in its palette's colour.
 */
static df_u8 g_wide[PIC_TILE_MAX * PIC_TILE_MAX * 2];

static const df_u8 *widen(const Tile *t, const df_u32 *table)
{
    int i;
    int n;

    n = t->side * t->side;
    for (i = 0; i < n; i++) {
        df_u32 c;
        df_u32 v;

        if (!t->texels[i]) {
            g_wide[i * 2] = 0;
            g_wide[i * 2 + 1] = 0;
            continue;
        }
        c = table[t->texels[i]];
        v = 0x8000u | (((c >> 19) & 0x1fu) << 10) | (((c >> 11) & 0x1fu) << 5) | ((c >> 3) & 0x1fu);
        g_wide[i * 2] = (df_u8)(v & 0xffu);
        g_wide[i * 2 + 1] = (df_u8)(v >> 8);
    }
    return g_wide;
}

/*
 * One tile onto the card: room, then the upload. Its texels are P_8; a
 * cutout's go as they are under its keyed palette when the card has the
 * chroma key, else widened to 1555. A full colour picture's are 565
 * already, and go as they are. 0 leaves it off.
 */
static int tile_send(Tile *t, int format, const df_u32 *table)
{
    df_u32 at;
    int wide;

    if (!g_ops || t->on) {
        return 0;
    }
    wide = format != PIC_FMT_P8;
    t->bytes = g_ops->bytes ? g_ops->bytes(t->side, wide)
                            : (df_u32)t->side * (df_u32)t->side * (wide ? 2u : 1u);
    if (!room_take(t->bytes, &at)) {
        return 0;
    }
    if (!g_ops->upload(format == PIC_FMT_1555 ? widen(t, table) : t->texels, t->side, format, at,
                       &t->handle)) {
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

/* 1 when p goes on the card as 1555: a cutout, and no chroma key. */
static int pic_wide(const Pic *p)
{
    return p->cutout && !(g_ops && g_ops->chroma);
}

/* p's tiles' format on the card. */
static int pic_format(const Pic *p)
{
    return p->rgb ? PIC_FMT_565 : (pic_wide(p) ? PIC_FMT_1555 : PIC_FMT_P8);
}

/* Every tile of p onto the card, as many as fit; how many did not. */
static int pic_send(Pic *p)
{
    int i;
    int missing;

    missing = 0;
    for (i = p->first; i != NO_TILE; i = g_tile[i].next) {
        /* A full colour picture has no palette: widen is never asked for its table. */
        if (!g_tile[i].on && !tile_send(&g_tile[i], pic_format(p), p->pal >= 0 ? g_pal[p->pal].table : 0)) {
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
/* A full colour picture's source instead (pic_load_565), with g_src 0. */
static const df_u16 *g_src16;
static int g_src_w;
static int g_src_h;
static int g_src_cut;

/*
 * One tile's texels from the source, P_8 whatever goes on the card. Past
 * the picture's edge a cutout is clear (index 0) and an opaque one
 * repeats its edge: never drawn, but a point on a tile's edge never
 * samples a stray colour.
 */
static void tile_cut(const Tile *t, df_u8 *out)
{
    int row;
    int col;

    if (g_src16) {
        /* 565, little-endian as the card reads it, the edge repeated past the picture. */
        for (row = 0; row < t->side; row++) {
            int sy;

            sy = t->y + row < g_src_h ? t->y + row : g_src_h - 1;
            for (col = 0; col < t->side; col++) {
                int sx;
                df_u16 v;

                sx = t->x + col < g_src_w ? t->x + col : g_src_w - 1;
                v = g_src16[sy * g_src_w + sx];
                *out++ = (df_u8)(v & 0xffu);
                *out++ = (df_u8)(v >> 8);
            }
        }
        return;
    }
    for (row = 0; row < t->side; row++) {
        int sy;

        sy = t->y + row;
        if (sy >= g_src_h) {
            sy = g_src_h - 1;
        }
        for (col = 0; col < t->side; col++) {
            int sx;
            int past;

            sx = t->x + col;
            past = sx >= g_src_w || t->y + row >= g_src_h;
            if (sx >= g_src_w) {
                sx = g_src_w - 1;
            }
            *out++ = past && g_src_cut ? 0 : g_src[sy * g_src_w + sx];
        }
    }
}

static int tile_fill(Tile *t)
{
    t->texels = (df_u8 *)mem_alloc((df_u32)t->side * (df_u32)t->side * (g_src16 ? 2u : 1u));
    if (!t->texels) {
        return 0;
    }
    tile_cut(t, t->texels);
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
    /* Its card bytes are known when it is sent: P_8, or 1555 for a cutout on a card with no key. */
    t->bytes = 0;
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

static int pic_tile(int id);

int pic_load(const df_u8 *pixels, int w, int h, const df_u8 *rgb, int cutout)
{
    int id;
    Pic *p;

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
    p->rgb = 0;
    p->pal = -1;
    p->first = NO_TILE;
    p->missing = 0;
    {
        df_u32 table[256];
        int i;

        /* 0xRRGGBB, as grTexDownloadTable takes it (glide_palette). */
        for (i = 0; i < 256; i++) {
            table[i] = ((df_u32)rgb[i * 3] << 16) | ((df_u32)rgb[i * 3 + 1] << 8)
                | (df_u32)rgb[i * 3 + 2];
        }
        if (p->cutout) {
            /*
             * Index 0 is the key, and no other entry may land on it, in 24
             * bits or in the card's 565: one that would gets a step of green
             * (a 64th), unseen, so it stays drawn.
             */
            table[0] = PIC_KEY;
            for (i = 1; i < 256; i++) {
                if (((table[i] >> 16) & 0xffu) >= 0xf8u && ((table[i] >> 8) & 0xffu) < 0x04u
                    && (table[i] & 0xffu) >= 0xf8u) {
                    table[i] = (table[i] & 0xff00ffu) | 0x000400u;
                }
            }
        }
        p->pal = palette_find(table);
        if (p->pal < 0) {
            log_text(LOG_WARN, "pictures: no palette slot free");
            return 0;
        }
    }
    g_src = pixels;
    g_src16 = 0;
    g_src_w = w;
    g_src_h = h;
    g_src_cut = p->cutout;
    return pic_tile(id);
}

/* Picture id's tiles cut from the source set up (g_src or g_src16), and sent if there is room. */
static int pic_tile(int id)
{
    Pic *p;
    MemHeap *was;

    p = &g_pic[id];
    g_tile_fail = 0;
    was = mem_heap_use(g_heap);
    cover(p, 0, 0, p->w, p->h);
    mem_heap_use(was);
    p->used = 1;
    if (g_tile_fail) {
        log_print(LOG_WARN, "pictures: %d x %d did not fit in memory or tile records", p->w, p->h);
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

int pic_load_565(const df_u16 *pixels, int w, int h)
{
    int id;
    Pic *p;

    if (!pixels || w < 1 || h < 1 || w > PIC_SIDE_MAX || h > PIC_SIDE_MAX) {
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
    p->cutout = 0;
    p->rgb = 1;
    p->pal = -1;
    p->first = NO_TILE;
    p->missing = 0;
    g_src = 0;
    g_src16 = pixels;
    g_src_w = w;
    g_src_h = h;
    g_src_cut = 0;
    id = pic_tile(id);
    g_src16 = 0;
    return id;
}

/* A tile's texels cut fresh for pic_update, to compare with what it holds. */
static df_u8 g_cut[PIC_TILE_MAX * PIC_TILE_MAX * 2];

int pic_update(int id, const df_u8 *pixels)
{
    Pic *p;
    int sent;
    int i;

    if (id < 1 || id > PIC_MAX || !g_pic[id].used || !pixels || g_pic[id].rgb) {
        return -1;
    }
    p = &g_pic[id];
    g_src = pixels;
    g_src16 = 0;
    g_src_w = p->w;
    g_src_h = p->h;
    g_src_cut = p->cutout;
    sent = 0;
    for (i = p->first; i != NO_TILE; i = g_tile[i].next) {
        Tile *t;
        int n;
        int k;

        t = &g_tile[i];
        n = t->side * t->side;
        tile_cut(t, g_cut);
        for (k = 0; k < n && g_cut[k] == t->texels[k]; k++) {
        }
        if (k == n) {
            continue;
        }
        mem_copy(t->texels, g_cut, (df_u32)n);
        /*
         * On the card: sent again into its own room, its old handle given
         * back first so the record is the one reused. Off it, the new
         * texels go up when it does.
         */
        if (t->on && g_ops) {
            g_ops->release(t->handle);
            if (!g_ops->upload(pic_format(p) == PIC_FMT_1555 ? widen(t, g_pal[p->pal].table) : t->texels,
                               t->side, pic_format(p), t->addr, &t->handle)) {
                room_give(t->addr, t->bytes);
                t->on = 0;
                p->missing++;
            }
        }
        sent++;
    }
    g_src = 0;
    return sent;
}

int pic_box_565(const df_u8 *src, int sw, int sh, int stride, df_u16 *out, int w, int h)
{
    int x;
    int y;

    if (!src || !out || w < 1 || h < 1 || w > sw || h > sh || stride < sw * 2) {
        return 0;
    }
    for (y = 0; y < h; y++) {
        int y0;
        int y1;

        y0 = y * sh / h;
        y1 = (y + 1) * sh / h;
        for (x = 0; x < w; x++) {
            df_u32 r;
            df_u32 g;
            df_u32 b;
            df_u32 n;
            int x0;
            int x1;
            int sy;

            x0 = x * sw / w;
            x1 = (x + 1) * sw / w;
            r = 0;
            g = 0;
            b = 0;
            for (sy = y0; sy < y1; sy++) {
                const df_u8 *row;
                int sx;

                row = src + sy * stride;
                for (sx = x0; sx < x1; sx++) {
                    df_u32 v;

                    v = (df_u32)row[sx * 2] | ((df_u32)row[sx * 2 + 1] << 8);
                    r += (v >> 11) & 31u;
                    g += (v >> 5) & 63u;
                    b += v & 31u;
                }
            }
            /* Rounded to the nearest: half the count added before the divide. */
            n = (df_u32)((x1 - x0) * (y1 - y0));
            out[y * w + x] = (df_u16)((((r + n / 2u) / n) << 11) | (((g + n / 2u) / n) << 5)
                                      | ((b + n / 2u) / n));
        }
    }
    return 1;
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
    g_ops->begin(!p->cutout ? PIC_OPAQUE : (pic_wide(p) ? PIC_ALPHA : PIC_KEYED), alpha);
    /* P_8 is drawn through its palette; 1555 and 565 carry their colour. */
    if (pic_format(p) == PIC_FMT_P8) {
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
