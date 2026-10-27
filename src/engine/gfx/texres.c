#include "engine/gfx/texres.h"
#include "engine/core/log.h"

/* See texres.h and docs/streaming.md. */

/* Blocks are 2^MIN_ORDER (256) to 2^MAX_ORDER (2 MB) bytes. */
#define MIN_ORDER 8
#define MAX_ORDER 21
#define ORDERS (MAX_ORDER - MIN_ORDER + 1)
#define ARENA_MAX 4
/* Bits a 2 MB arena needs at the smallest order, in 32-bit words. */
#define WORDS ((1u << (MAX_ORDER - MIN_ORDER)) / 32u)
/* Sharp copies sent a frame at most, whatever the budget: each is a download and a bind change. */
#define UPLOADS_A_FRAME 8
/*
 * Sharp copies of walls drawn this frame a sprite may take the room of,
 * a frame at most: a few, so a street of people never strips the walls
 * and the ground round them over and over.
 */
#define STEALS_A_FRAME 2

typedef struct Arena {
    df_u32 base;
    df_u32 size;
    /* free[o] bit i: the block of 2^(o + MIN_ORDER) bytes at i << (o + MIN_ORDER) from base is free. */
    df_u32 free[ORDERS][WORDS];
} Arena;

typedef struct Res {
    df_u8 used;
    df_u8 cutout;
    df_u8 tiny_order;
    df_u8 big_order;
    short side;
    short tiny_drop;
    /* The sharp copy's level, -1 for none. */
    short big_drop;
    float density;
    df_u32 tiny_addr;
    df_u32 big_addr;
    df_u32 tiny_h;
    df_u32 big_h;
    int note_frame;
    float note_dist;
    int last_frame;
} Res;

static TexResOps g_ops;
static int g_on;
static Arena g_arena[ARENA_MAX];
static int g_narena;
static Res g_res[TEXRES_SLOTS];
static int g_noted[TEXRES_SLOTS];
static int g_nnoted;
static int g_frame = 1;
static df_u32 g_used;
static df_u32 g_room;
static int g_added;
static int g_sharp;
static int g_uploads;
static int g_evictions;
/* Walls drawn this frame whose room a sprite took, this frame. */
static int g_steals;

static int bit(const Arena *a, int o, df_u32 i)
{
    return (int)((a->free[o][i >> 5] >> (i & 31u)) & 1u);
}

static void set_bit(Arena *a, int o, df_u32 i, int on)
{
    if (on) {
        a->free[o][i >> 5] |= 1u << (i & 31u);
    } else {
        a->free[o][i >> 5] &= ~(1u << (i & 31u));
    }
}

/* The order of a block that holds bytes: 0 is TEXRES_MIN. -1 past 2 MB. */
static int order_of(df_u32 bytes)
{
    int o;

    for (o = 0; o < ORDERS; o++) {
        if (bytes <= (TEXRES_MIN << o)) {
            return o;
        }
    }
    return -1;
}

/* An arena's room as the largest aligned blocks that fit, from its start. */
static void arena_init(Arena *a, df_u32 base, df_u32 size)
{
    df_u32 pos;
    int o;
    df_u32 w;

    a->base = base;
    a->size = size;
    for (o = 0; o < ORDERS; o++) {
        for (w = 0; w < WORDS; w++) {
            a->free[o][w] = 0;
        }
    }
    pos = 0;
    while (pos + TEXRES_MIN <= size) {
        for (o = ORDERS - 1; o >= 0; o--) {
            df_u32 b;

            b = TEXRES_MIN << o;
            if (pos % b == 0 && pos + b <= size) {
                set_bit(a, o, pos >> (o + MIN_ORDER), 1);
                pos += b;
                break;
            }
        }
    }
}

/*
 * Blocks under this order (16 KB: the 8 x 8 copies, small pictures) come
 * from the top of the card, the rest from the bottom. Taken from the
 * bottom alike, the small ones split block after block next to the big
 * ones and left the card with room to spare and no 128 KB block whole: a
 * sprite in front of the camera settled for half size (docs/streaming.md).
 */
#define HIGH_BELOW 6

/*
 * A free block of 2^order units: the smallest free order that holds it
 * (so no big block is split while a small one will do), at the lowest
 * address for a big block and the highest for a small one, split down
 * keeping the half on its own side and freeing the other.
 */
static int block_alloc(int order, df_u32 *addr)
{
    int high;
    int o;

    high = order < HIGH_BELOW;
    for (o = order; o < ORDERS; o++) {
        int found;
        int fk;
        df_u32 fi;
        int k;

        found = 0;
        fk = 0;
        fi = 0;
        for (k = 0; k < g_narena; k++) {
            Arena *a;
            df_u32 words;
            df_u32 w;

            a = &g_arena[k];
            words = ((a->size >> (o + MIN_ORDER)) + 31u) >> 5;
            words = words < WORDS ? words : WORDS;
            for (w = 0; w < words; w++) {
                df_u32 bits;
                df_u32 j;

                bits = a->free[o][w];
                for (j = 0; bits && j < 32u; j++) {
                    df_u32 at;

                    if (!(bits & (1u << j))) {
                        continue;
                    }
                    at = a->base + (((w << 5) + j) << (o + MIN_ORDER));
                    if (!found || (high ? at > g_arena[fk].base + (fi << (o + MIN_ORDER))
                                        : at < g_arena[fk].base + (fi << (o + MIN_ORDER)))) {
                        found = 1;
                        fk = k;
                        fi = (w << 5) + j;
                    }
                    if (!high) {
                        break;
                    }
                }
                if (found && !high) {
                    break;
                }
            }
        }
        if (found) {
            Arena *a;
            df_u32 i;

            a = &g_arena[fk];
            i = fi;
            set_bit(a, o, i, 0);
            while (o > order) {
                o--;
                i <<= 1;
                if (high) {
                    set_bit(a, o, i, 1);
                    i++;
                } else {
                    set_bit(a, o, i + 1u, 1);
                }
            }
            *addr = a->base + (i << (order + MIN_ORDER));
            g_used += TEXRES_MIN << order;
            return 1;
        }
    }
    return 0;
}

static void block_free(df_u32 addr, int order)
{
    int k;

    for (k = 0; k < g_narena; k++) {
        Arena *a;
        df_u32 i;
        int o;

        a = &g_arena[k];
        if (addr < a->base || addr >= a->base + a->size) {
            continue;
        }
        g_used -= TEXRES_MIN << order;
        o = order;
        i = (addr - a->base) >> (o + MIN_ORDER);
        /* Join with the buddy while it is free. */
        while (o < ORDERS - 1 && bit(a, o, i ^ 1u)) {
            set_bit(a, o, i ^ 1u, 0);
            i >>= 1;
            o++;
        }
        set_bit(a, o, i, 1);
        return;
    }
}

void texres_begin(df_u32 base, df_u32 end, const TexResOps *ops)
{
    int i;

    texres_end();
    g_ops = *ops;
    g_narena = 0;
    g_room = 0;
    base = (base + TEXRES_MIN - 1u) & ~(TEXRES_MIN - 1u);
    /* One arena a 2 MB half, so a block never crosses the edge. */
    while (base + TEXRES_MIN <= end && g_narena < ARENA_MAX) {
        df_u32 edge;
        df_u32 stop;

        edge = (base & ~0x1fffffu) + 0x200000u;
        stop = end < edge ? end : edge;
        arena_init(&g_arena[g_narena++], base, stop - base);
        g_room += stop - base;
        base = stop;
    }
    for (i = 0; i < TEXRES_SLOTS; i++) {
        g_res[i].used = 0;
    }
    g_used = 0;
    g_nnoted = 0;
    g_added = 0;
    g_sharp = 0;
    g_uploads = 0;
    g_evictions = 0;
    g_on = 1;
}

void texres_end(void)
{
    g_on = 0;
    g_narena = 0;
}

/*
 * Free a sharp copy to make room (docs/streaming.md, The card). Of the
 * pictures not drawn this frame, a wall's first, then a sprite's, the
 * least recently drawn of each: a hand-drawn sprite turned back to is
 * worth having still sharp. For a sprite drawn sprite_dist away (-1 for
 * a wall), once none of those is left: a wall drawn this frame farther
 * than the sprite, the farthest first, STEALS_A_FRAME at most; it shows
 * its 8 x 8 copy until a smaller sharp one fits round the sprites. The
 * ground under the camera, nearer than any sprite on it, is never taken.
 * 0 when none can go.
 */
static int evict_one(float sprite_dist)
{
    int best;
    int pass;
    int i;

    best = -1;
    for (pass = 0; pass < 2 && best < 0; pass++) {
        for (i = 0; i < TEXRES_SLOTS; i++) {
            Res *r;

            r = &g_res[i];
            if (!r->used || r->big_drop < 0 || r->last_frame >= g_frame || (int)r->cutout != pass) {
                continue;
            }
            if (best < 0 || r->last_frame < g_res[best].last_frame) {
                best = i;
            }
        }
    }
    if (sprite_dist >= 0.0f && best < 0 && g_steals < STEALS_A_FRAME) {
        for (i = 0; i < TEXRES_SLOTS; i++) {
            Res *r;

            r = &g_res[i];
            if (!r->used || r->big_drop < 0 || r->cutout || r->last_frame < g_frame
                || r->note_dist <= sprite_dist) {
                continue;
            }
            if (best < 0 || r->note_dist > g_res[best].note_dist) {
                best = i;
            }
        }
        if (best >= 0) {
            g_steals++;
        }
    }
    if (best < 0) {
        return 0;
    }
    g_ops.release(g_res[best].big_h);
    block_free(g_res[best].big_addr, g_res[best].big_order);
    g_res[best].big_drop = -1;
    g_sharp--;
    g_evictions++;
    return 1;
}

/* Free slot i's sharp copy: its handle released and its block given back. */
static void drop_big(int i)
{
    g_ops.release(g_res[i].big_h);
    block_free(g_res[i].big_addr, g_res[i].big_order);
    g_res[i].big_drop = -1;
    g_sharp--;
    g_evictions++;
}

/*
 * A whole block of 2^o units made by freeing one aligned region of the
 * card of that size: the region that holds no small copy and only sharp
 * copies that may go (not drawn this frame; or, for a sprite drawn
 * sprite_dist away, walls drawn this frame farther than it, within this
 * frame's STEALS_A_FRAME), the fewest drawn ones first, then the fewest
 * copies. Freed least recently drawn first one by one, copies land all
 * over the card and a large block may never join; freed by region, it
 * does at once. 0 when no region will do.
 */
/* Regions a free_region pass weighs at most: 128 an arena (16 KB ones of 2 MB). */
#define REGIONS (ARENA_MAX * 128)

static int free_region(int o, float sprite_dist)
{
    static int cost[REGIONS];
    static df_u8 drawn[REGIONS];
    static df_u8 bad[REGIONS];
    df_u32 size;
    int per;
    int best;
    int freed;
    int n;
    int i;

    size = TEXRES_MIN << o;
    per = (int)((2u * 1024u * 1024u) / size);
    per = per > REGIONS / ARENA_MAX ? REGIONS / ARENA_MAX : per;
    n = g_narena * per;
    for (i = 0; i < n; i++) {
        cost[i] = 0;
        drawn[i] = 0;
        bad[i] = 0;
    }
    /* One pass over the pictures: each copy marks the region it lies in. */
    for (i = 0; i < TEXRES_SLOTS; i++) {
        const Res *r;
        int k;

        r = &g_res[i];
        if (!r->used) {
            continue;
        }
        for (k = 0; k < g_narena; k++) {
            const Arena *a;
            int g;

            a = &g_arena[k];
            if (r->tiny_addr >= a->base && r->tiny_addr < a->base + a->size) {
                g = k * per + (int)((r->tiny_addr - a->base) / size);
                if (g < n) {
                    bad[g] = 1;
                }
            }
            if (r->big_drop >= 0 && r->big_addr >= a->base && r->big_addr < a->base + a->size) {
                g = k * per + (int)((r->big_addr - a->base) / size);
                if (g >= n) {
                    continue;
                }
                if (r->last_frame >= g_frame) {
                    if (sprite_dist < 0.0f || r->cutout || r->note_dist <= sprite_dist) {
                        bad[g] = 1;
                    } else if (drawn[g] < 255) {
                        drawn[g]++;
                    }
                }
                cost[g]++;
            }
        }
    }
    best = -1;
    for (i = 0; i < n; i++) {
        int k;
        df_u32 at;

        k = i / per;
        at = (df_u32)(i % per) * size;
        if (bad[i] || cost[i] == 0 || at + size > g_arena[k].size || g_steals + drawn[i] > STEALS_A_FRAME) {
            continue;
        }
        if (best < 0 || drawn[i] * 1000 + cost[i] < drawn[best] * 1000 + cost[best]) {
            best = i;
        }
    }
    if (best < 0) {
        return 0;
    }
    /* Every sharp copy in it goes; the region then joins into one block. */
    freed = 0;
    for (i = 0; i < TEXRES_SLOTS; i++) {
        const Res *r;
        df_u32 lo;

        r = &g_res[i];
        lo = g_arena[best / per].base + (df_u32)(best % per) * size;
        if (r->used && r->big_drop >= 0 && r->big_addr >= lo && r->big_addr < lo + size) {
            if (r->last_frame >= g_frame) {
                g_steals++;
            }
            drop_big(i);
            freed++;
        }
    }
    return freed;
}

/*
 * A block for bytes, freeing sharp copies until one fits: for a large
 * block (HIGH_BELOW and up) a region freed whole (free_region), for a
 * small one copies one by one (evict_one; sprite_dist as it takes it).
 * 0 when nothing more can go.
 */
static int room_for(df_u32 bytes, df_u32 *addr, int *order, float sprite_dist)
{
    int o;

    o = order_of(bytes);
    if (o < 0) {
        return 0;
    }
    for (;;) {
        if (block_alloc(o, addr)) {
            *order = o;
            return 1;
        }
        if (o >= HIGH_BELOW) {
            if (!free_region(o, sprite_dist)) {
                return 0;
            }
            continue;
        }
        if (!evict_one(sprite_dist)) {
            return 0;
        }
    }
}

int texres_add(int slot, int side, int cutout, float density)
{
    Res *r;
    df_u32 addr;
    int order;
    int drop;

    if (!g_on || slot < 0 || slot >= TEXRES_SLOTS || side < 1 || side > 256) {
        return 0;
    }
    if (g_res[slot].used) {
        texres_remove(slot);
    }
    drop = 0;
    while ((side >> drop) > TEXRES_TINY) {
        drop++;
    }
    if (!room_for(g_ops.bytes(side >> drop, cutout), &addr, &order, -1.0f)) {
        return 0;
    }
    r = &g_res[slot];
    if (!g_ops.upload(slot, drop, addr, &r->tiny_h)) {
        block_free(addr, order);
        return 0;
    }
    r->used = 1;
    r->cutout = (df_u8)(cutout ? 1 : 0);
    r->side = (short)side;
    r->density = density > 0.0f ? density : 0.25f;
    r->tiny_drop = (short)drop;
    r->tiny_addr = addr;
    r->tiny_order = (df_u8)order;
    r->big_drop = -1;
    r->note_frame = 0;
    r->last_frame = g_frame;
    g_added++;
    return 1;
}

void texres_remove(int slot)
{
    Res *r;

    if (slot < 0 || slot >= TEXRES_SLOTS || !g_res[slot].used) {
        return;
    }
    r = &g_res[slot];
    if (r->big_drop >= 0) {
        g_ops.release(r->big_h);
        block_free(r->big_addr, r->big_order);
        g_sharp--;
    }
    g_ops.release(r->tiny_h);
    block_free(r->tiny_addr, r->tiny_order);
    r->used = 0;
    g_added--;
}

void texres_note(int slot, float dist)
{
    Res *r;

    if (slot < 0 || slot >= TEXRES_SLOTS || !g_res[slot].used) {
        return;
    }
    r = &g_res[slot];
    if (r->note_frame != g_frame) {
        r->note_frame = g_frame;
        r->note_dist = dist;
        r->last_frame = g_frame;
        if (g_nnoted < TEXRES_SLOTS) {
            g_noted[g_nnoted++] = slot;
        }
    } else if (dist < r->note_dist) {
        r->note_dist = dist;
    }
}

/* The level a picture needs at its distance: a texel a pixel or less. */
static int want_drop(const Res *r, float focal)
{
    float lim;
    int d;

    lim = focal / r->density;
    d = 0;
    while (r->note_dist > lim && d < r->tiny_drop) {
        lim *= 2.0f;
        d++;
    }
    return d;
}

void texres_service(float focal, df_u32 budget)
{
    int sent;
    int pass;
    df_u32 spent;

    if (!g_on) {
        return;
    }
    if (focal < 1.0f) {
        focal = 1.0f;
    }
    sent = 0;
    spent = 0;
    g_steals = 0;
    /*
     * Sprites, then walls (pass 1, then 0): a sprite is the hand-drawn
     * art, a blurry one breaks the picture where a blurry wall reads as
     * distance. Each class's first copy goes even past the byte budget
     * (a 256 sprite is 128 KB alone): so the walls always get a turn,
     * however many people want sharpening.
     */
    for (pass = 1; pass >= 0; pass--) {
        int class_sent;

        class_sent = 0;
        while (sent < UPLOADS_A_FRAME) {
            int best;
            int best_short;
            int i;
            Res *r;
            int want;
            df_u32 addr;
            df_u32 bytes;
            df_u32 h;
            int order;

            /* Of this class, the picture most levels short of its need; between equals, the nearest. */
            best = -1;
            best_short = 0;
            for (i = 0; i < g_nnoted; i++) {
                int s;
                int have;
                int lack;

                s = g_noted[i];
                r = &g_res[s];
                if (!r->used || (int)r->cutout != pass) {
                    continue;
                }
                have = r->big_drop >= 0 ? r->big_drop : r->tiny_drop;
                lack = have - want_drop(r, focal);
                if (lack <= 0) {
                    continue;
                }
                if (best < 0 || lack > best_short
                    || (lack == best_short && r->note_dist < g_res[best].note_dist)) {
                    best = s;
                    best_short = lack;
                }
            }
            if (best < 0) {
                break;
            }
            r = &g_res[best];
            want = (r->big_drop >= 0 ? r->big_drop : r->tiny_drop) - best_short;
            /* Short of room, settle for a level down; at the small copy's level there is nothing to send. */
            for (;;) {
                bytes = g_ops.bytes(r->side >> want, r->cutout);
                if (class_sent > 0 && spent + bytes > budget) {
                    want = -1;
                    break;
                }
                if (room_for(bytes, &addr, &order, r->cutout ? r->note_dist : -1.0f)) {
                    break;
                }
                want++;
                if (want >= r->tiny_drop || (r->big_drop >= 0 && want >= r->big_drop)) {
                    want = -1;
                    break;
                }
            }
            if (want < 0) {
                /* Its need cannot be met now: leave it until the next frame. */
                r->note_frame = 0;
                for (i = 0; i < g_nnoted; i++) {
                    if (g_noted[i] == best) {
                        g_noted[i] = g_noted[--g_nnoted];
                        break;
                    }
                }
                if (class_sent > 0 && spent + bytes > budget) {
                    break;
                }
                continue;
            }
            if (!g_ops.upload(best, want, addr, &h)) {
                block_free(addr, order);
                break;
            }
            if (r->big_drop >= 0) {
                g_ops.release(r->big_h);
                block_free(r->big_addr, r->big_order);
            } else {
                g_sharp++;
            }
            r->big_drop = (short)want;
            r->big_addr = addr;
            r->big_order = (df_u8)order;
            r->big_h = h;
            g_uploads++;
            sent++;
            class_sent++;
            spent += bytes;
        }
    }
    g_nnoted = 0;
    g_frame++;
}

int texres_handle(int slot, df_u32 *handle)
{
    const Res *r;

    if (slot < 0 || slot >= TEXRES_SLOTS || !g_res[slot].used) {
        return 0;
    }
    r = &g_res[slot];
    *handle = r->big_drop >= 0 ? r->big_h : r->tiny_h;
    return 1;
}

int texres_drop(int slot)
{
    const Res *r;

    if (slot < 0 || slot >= TEXRES_SLOTS || !g_res[slot].used) {
        return -1;
    }
    r = &g_res[slot];
    return r->big_drop >= 0 ? r->big_drop : r->tiny_drop;
}

void texres_stats(int *added, int *sharp, df_u32 *used, df_u32 *free_bytes,
                  int *uploads, int *evictions)
{
    if (added) {
        *added = g_added;
    }
    if (sharp) {
        *sharp = g_sharp;
    }
    if (used) {
        *used = g_used;
    }
    if (free_bytes) {
        *free_bytes = g_room > g_used ? g_room - g_used : 0;
    }
    if (uploads) {
        *uploads = g_uploads;
    }
    if (evictions) {
        *evictions = g_evictions;
    }
}

df_u32 texres_largest_free(void)
{
    int o;
    int k;

    for (o = ORDERS - 1; o >= 0; o--) {
        for (k = 0; k < g_narena; k++) {
            df_u32 w;

            for (w = 0; w < WORDS; w++) {
                if (g_arena[k].free[o][w]) {
                    return TEXRES_MIN << o;
                }
            }
        }
    }
    return 0;
}
