#include "engine/gfx/mip.h"

/*
 * The work buffer is RGBA, one level at a time. Each smaller level is
 * the 2 x 2 box mean of the one above, written over the front of the
 * same buffer: texel (x, y) of the half level is read from rows 2y and
 * 2y + 1, which are never before it, so the halving can run in place.
 * Alpha is kept as the mean, 0..255, and only thresholded on output,
 * so a half-covered texel two levels down is still half covered.
 * Words, so the halving sums four channels in two adds; its bytes (r,
 * g, b, a in memory) through g_px, which may alias anything.
 */
static df_u32 g_rgba[256 * 256];
#define g_px ((df_u8 *)g_rgba)
/* A row of the picture: nonzero where it or the rows above and below have an opaque texel. */
static df_u8 g_near[256];

/*
 * Nearest palette entry for a 5-bit-per-channel colour, filled the
 * first time that colour is asked for. A whole table would be 32768
 * searches of 256 entries, about a quarter of a second on a 233. A
 * level's chains ask for a few thousand distinct colours. g_inv_pal is
 * the palette the table was built for; a new one empties it.
 */
static df_u16 g_inv[32768];
static df_u8 g_inv_pal[768];
static int g_inv_ready;

static int level_count(int side)
{
    int n;

    n = 1;
    while (side > 1) {
        side >>= 1;
        n++;
    }
    return n;
}

static int pow2_side(int side)
{
    return side >= 1 && side <= 256 && (side & (side - 1)) == 0;
}

df_u32 mip_texels(int side, int drop, int chain)
{
    df_u32 n;
    int s;

    if (!pow2_side(side) || drop < 0 || (side >> drop) < 1) {
        return 0;
    }
    s = side >> drop;
    n = (df_u32)s * (df_u32)s;
    if (chain) {
        while (s > 1) {
            s >>= 1;
            n += (df_u32)s * (df_u32)s;
        }
    }
    return n;
}

void mip_palette(const df_u8 *pr, const df_u8 *pg, const df_u8 *pb)
{
    int i;
    int same;

    same = g_inv_ready;
    for (i = 0; same && i < 256; i++) {
        same = g_inv_pal[i] == pr[i] && g_inv_pal[256 + i] == pg[i]
            && g_inv_pal[512 + i] == pb[i];
    }
    if (same) {
        return;
    }
    for (i = 0; i < 256; i++) {
        g_inv_pal[i] = pr[i];
        g_inv_pal[256 + i] = pg[i];
        g_inv_pal[512 + i] = pb[i];
    }
    for (i = 0; i < 32768; i++) {
        g_inv[i] = 0xffff;
    }
    g_inv_ready = 1;
}

/* Squares by table: imul is 10 cycles on a Pentium and does not pair. */
static int g_sq[511];
static int g_sq_ready;

df_u8 mip_nearest(int r, int g, int b)
{
    int key;
    int i;
    int best;
    int best_d;

    if (!g_inv_ready) {
        return 0;
    }
    key = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
    if (g_inv[key] != 0xffff) {
        return (df_u8)g_inv[key];
    }
    if (!g_sq_ready) {
        for (i = 0; i < 511; i++) {
            g_sq[i] = (i - 255) * (i - 255);
        }
        g_sq_ready = 1;
    }
    /* The cell's centre stands for every colour in it. */
    r = (r & ~7) + 4;
    g = (g & ~7) + 4;
    b = (b & ~7) + 4;
    best = 0;
    best_d = 0x7fffffff;
    for (i = 0; i < 256; i++) {
        int d;

        d = g_sq[g_inv_pal[i] - r + 255] + g_sq[g_inv_pal[256 + i] - g + 255]
            + g_sq[g_inv_pal[512 + i] - b + 255];
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    g_inv[key] = (df_u16)best;
    return (df_u8)best;
}

/*
 * A cutout's clear texels are most of a padded sprite, far from its
 * picture: they take no colour, and only those beside an opaque one
 * need the 3 x 3 gather that was most of a sheet's chain (20 ms of a
 * 256 on the 233, and the card's full-size sharpen ran it in the
 * frame). So each row's opaque span is found once (g_lo, g_hi, -1 for
 * none), a row is worked only within its and its two neighbours'
 * spans and a texel either side, wrapping as the texture does, and
 * g_near says, within that, where the rows above, at and below have
 * an opaque texel: a zero at x and its two neighbours, no gather.
 */
static short g_lo[256];
static short g_hi[256];

static void opaque_spans(const df_u8 *index, int side)
{
    int y;

    for (y = 0; y < side; y++) {
        const df_u8 *row;
        int lo;
        int hi;

        row = index + y * side;
        lo = 0;
        while (lo < side && row[lo] == 0) {
            lo++;
        }
        if (lo == side) {
            g_lo[y] = -1;
            g_hi[y] = -1;
            continue;
        }
        hi = side - 1;
        while (row[hi] == 0) {
            hi--;
        }
        g_lo[y] = (short)lo;
        g_hi[y] = (short)hi;
    }
}

/*
 * Row y's texels to work, from *from to *to (either may pass the row's
 * ends: take x & (side - 1)), with g_near set for them and their
 * neighbours. 0 when no opaque texel is in or beside the row: all clear.
 */
static int row_span(const df_u8 *index, int side, int y, int *from, int *to)
{
    const df_u8 *a;
    const df_u8 *b;
    const df_u8 *c;
    int m;
    int k;
    int lo;
    int hi;
    int r;

    m = side - 1;
    lo = side;
    hi = -1;
    for (k = -1; k <= 1; k++) {
        r = (y + k) & m;
        if (g_lo[r] >= 0) {
            lo = g_lo[r] < lo ? g_lo[r] : lo;
            hi = g_hi[r] > hi ? g_hi[r] : hi;
        }
    }
    if (hi < 0) {
        return 0;
    }
    a = index + ((y - 1) & m) * side;
    b = index + y * side;
    c = index + ((y + 1) & m) * side;
    if (hi - lo + 3 >= side) {
        /* The span and a texel either side reach round: the whole row, once. */
        *from = 0;
        *to = m;
        for (k = 0; k < side; k++) {
            g_near[k] = (df_u8)(a[k] | b[k] | c[k]);
        }
        return 1;
    }
    *from = lo - 1;
    *to = hi + 1;
    for (k = lo - 2; k <= hi + 2; k++) {
        r = k & m;
        g_near[r] = (df_u8)(a[r] | b[r] | c[r]);
    }
    return 1;
}

/* The mean colour of the opaque texels round clear texel (x, y), wrapping; n 0 for none. */
static void bleed(const df_u8 *index, int side, int x, int y,
                  const df_u8 *pr, const df_u8 *pg, const df_u8 *pb, int *r, int *g, int *b, int *n)
{
    int m;
    int dx;
    int dy;

    m = side - 1;
    *r = 0;
    *g = 0;
    *b = 0;
    *n = 0;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            df_u8 k;

            k = index[((y + dy) & m) * side + ((x + dx) & m)];
            if (k != 0) {
                *r += pr[k];
                *g += pg[k];
                *b += pb[k];
                (*n)++;
            }
        }
    }
}

/*
 * The picture into g_rgba. For a cutout, a transparent texel takes the
 * mean colour of its opaque neighbours, 8 around it, wrapping as the
 * texture does, and keeps alpha 0. One with no opaque neighbour stays
 * black: no filter tap from the sprite reaches it.
 */
static void top_level(const df_u8 *index, int side, int cutout,
                      const df_u8 *pr, const df_u8 *pg, const df_u8 *pb)
{
    int x;
    int y;
    int m;
    int from;
    int to;

    m = side - 1;
    if (cutout) {
        opaque_spans(index, side);
    }
    for (y = 0; y < side; y++) {
        const df_u8 *row;
        df_u8 *o;

        row = index + y * side;
        o = g_px + y * side * 4;
        if (!cutout) {
            /* An 8-bit picture's alpha is how much of the texel is glass, so halving keeps it. */
            for (x = 0; x < side; x++, o += 4) {
                df_u8 c;

                c = row[x];
                o[0] = pr[c];
                o[1] = pg[c];
                o[2] = pb[c];
                o[3] = (df_u8)(c == MIP_GLASS ? 255 : 0);
            }
            continue;
        }
        for (x = 0; x < side; x++) {
            g_rgba[y * side + x] = 0;
        }
        if (!row_span(index, side, y, &from, &to)) {
            continue;
        }
        for (x = from; x <= to; x++) {
            df_u8 c;
            int xm;
            int r;
            int g;
            int b;
            int n;

            xm = x & m;
            c = row[xm];
            o = g_px + (y * side + xm) * 4;
            if (c != 0) {
                o[0] = pr[c];
                o[1] = pg[c];
                o[2] = pb[c];
                o[3] = 255;
                continue;
            }
            if (!(g_near[(xm - 1) & m] | g_near[xm] | g_near[(xm + 1) & m])) {
                continue;
            }
            bleed(index, side, xm, y, pr, pg, pb, &r, &g, &b, &n);
            o[0] = (df_u8)(n ? r / n : 0);
            o[1] = (df_u8)(n ? g / n : 0);
            o[2] = (df_u8)(n ? b / n : 0);
        }
    }
}

/*
 * g_rgba from side to side / 2, in place: each channel the rounded mean
 * of its four, (a + b + c + d + 2) >> 2, two channels a word at a time
 * (16 bits each hold the sum, 1022 at most), the same bit for bit.
 */
static void halve(int side)
{
    int half;
    int x;
    int y;

    half = side >> 1;
    for (y = 0; y < half; y++) {
        const df_u32 *a;
        const df_u32 *b;
        df_u32 *o;

        a = &g_rgba[(2 * y) * side];
        b = a + side;
        o = &g_rgba[y * half];
        for (x = 0; x < half; x++) {
            df_u32 p0;
            df_u32 p1;
            df_u32 q0;
            df_u32 q1;
            df_u32 lo;
            df_u32 hi;

            p0 = a[2 * x];
            p1 = a[2 * x + 1];
            q0 = b[2 * x];
            q1 = b[2 * x + 1];
            lo = (p0 & 0x00ff00ffu) + (p1 & 0x00ff00ffu) + (q0 & 0x00ff00ffu)
                + (q1 & 0x00ff00ffu) + 0x00020002u;
            hi = ((p0 >> 8) & 0x00ff00ffu) + ((p1 >> 8) & 0x00ff00ffu) + ((q0 >> 8) & 0x00ff00ffu)
                + ((q1 >> 8) & 0x00ff00ffu) + 0x00020002u;
            o[x] = ((lo >> 2) & 0x00ff00ffu) | (((hi >> 2) & 0x00ff00ffu) << 8);
        }
    }
}

/*
 * A cutout's full level alone, straight to out (the card's full-size
 * sharpen of a sprite, docs/streaming.md, run in the frame): each texel
 * as emit would write it from top_level's, without the RGBA pass.
 */
static df_u32 cutout_top(const df_u8 *index, int side,
                         const df_u8 *pr, const df_u8 *pg, const df_u8 *pb, df_u8 *out)
{
    df_u16 pal[256];
    int x;
    int y;
    int i;
    int m;
    int from;
    int to;

    for (i = 0; i < 256; i++) {
        pal[i] = (df_u16)(0x8000u | ((df_u16)(pr[i] >> 3) << 10) | ((df_u16)(pg[i] >> 3) << 5)
                          | (df_u16)(pb[i] >> 3));
    }
    m = side - 1;
    opaque_spans(index, side);
    for (y = 0; y < side; y++) {
        const df_u8 *row;
        df_u8 *o;

        row = index + y * side;
        o = out + y * side * 2;
        for (x = 0; x < side * 2; x++) {
            o[x] = 0;
        }
        if (!row_span(index, side, y, &from, &to)) {
            continue;
        }
        for (x = from; x <= to; x++) {
            df_u16 pix;
            df_u8 c;
            int xm;
            int r;
            int g;
            int b;
            int n;

            xm = x & m;
            c = row[xm];
            if (c != 0) {
                pix = pal[c];
            } else if (!(g_near[(xm - 1) & m] | g_near[xm] | g_near[(xm + 1) & m])) {
                continue;
            } else {
                bleed(index, side, xm, y, pr, pg, pb, &r, &g, &b, &n);
                pix = n ? (df_u16)(((df_u16)((r / n) >> 3) << 10) | ((df_u16)((g / n) >> 3) << 5)
                                   | (df_u16)((b / n) >> 3))
                        : 0;
            }
            o[xm * 2] = (df_u8)(pix & 0xff);
            o[xm * 2 + 1] = (df_u8)(pix >> 8);
        }
    }
    return (df_u32)side * (df_u32)side * 2u;
}

/* One level of g_rgba into out. Returns the bytes written. */
static df_u32 emit(int side, int cutout, df_u8 *out)
{
    df_u32 n;
    df_u32 i;

    n = (df_u32)side * (df_u32)side;
    for (i = 0; i < n; i++) {
        const df_u8 *p;

        p = g_px + i * 4;
        if (cutout) {
            df_u16 pix;

            pix = (df_u16)(((df_u16)(p[0] >> 3) << 10) | ((df_u16)(p[1] >> 3) << 5)
                           | (df_u16)(p[2] >> 3));
            if (p[3] >= 128) {
                pix |= 0x8000u;
            }
            out[i * 2] = (df_u8)(pix & 0xff);
            out[i * 2 + 1] = (df_u8)(pix >> 8);
        } else {
            out[i] = p[3] >= 128 ? (df_u8)MIP_GLASS : mip_nearest(p[0], p[1], p[2]);
        }
    }
    return cutout ? n * 2u : n;
}

df_u32 mip_build(const df_u8 *index, int side, int cutout, int drop, int chain,
                 const df_u8 *pr, const df_u8 *pg, const df_u8 *pb, df_u8 *out)
{
    df_u32 at;
    int s;
    int level;
    int levels;

    if (!index || !out || !pr || !pg || !pb || !pow2_side(side)
        || drop < 0 || (side >> drop) < 1) {
        return 0;
    }
    at = 0;
    s = side;
    level = 0;
    levels = chain ? level_count(side) : drop + 1;
    if (!cutout && drop == 0) {
        /* The full level is the picture: no colour is remapped. */
        df_u32 n;
        df_u32 i;

        n = (df_u32)side * (df_u32)side;
        for (i = 0; i < n; i++) {
            out[i] = index[i];
        }
        at = n;
        if (!chain || side == 1) {
            return at;
        }
    }
    if (cutout && drop == 0 && levels == 1) {
        return cutout_top(index, side, pr, pg, pb, out);
    }
    if (!cutout) {
        mip_palette(pr, pg, pb);
    }
    top_level(index, side, cutout, pr, pg, pb);
    for (;;) {
        if (level >= drop && !(level == 0 && !cutout)) {
            at += emit(s, cutout, out + at);
        }
        level++;
        if (level >= levels || s == 1) {
            break;
        }
        halve(s);
        s >>= 1;
    }
    return at;
}

df_u32 mip_budget(const int *side, const df_u8 *cutout, int n, df_u32 room,
                  df_u32 (*bytes)(int side, int cutout), int *drop)
{
    /* bytes for each side 1..256 (by its level count) and format. */
    df_u32 table[2][10];
    df_u32 total;
    int i;
    int k;

    for (k = 0; k < 9; k++) {
        table[0][k] = bytes(1 << k, 0);
        table[1][k] = bytes(1 << k, 1);
    }
    total = 0;
    for (i = 0; i < n; i++) {
        drop[i] = 0;
        if (pow2_side(side[i])) {
            total += table[cutout && cutout[i] ? 1 : 0][level_count(side[i]) - 1];
        }
    }
    while (total > room) {
        int pick;
        df_u32 most;
        int pass;

        /*
         * Sprites last (MIP_WALL_GRACE): first only the walls that have
         * given fewer than that many levels; then anything, the largest.
         */
        pick = -1;
        most = 0;
        for (pass = 0; pass < 2 && pick < 0; pass++) {
            for (i = 0; i < n; i++) {
                int s;
                df_u32 b;

                if (!pow2_side(side[i])) {
                    continue;
                }
                if (pass == 0 && ((cutout && cutout[i]) || drop[i] >= MIP_WALL_GRACE)) {
                    continue;
                }
                s = side[i] >> drop[i];
                if (s <= MIP_MIN_SIDE) {
                    continue;
                }
                b = table[cutout && cutout[i] ? 1 : 0][level_count(s) - 1];
                if (b > most) {
                    most = b;
                    pick = i;
                }
            }
        }
        if (pick < 0) {
            break;
        }
        {
            int c;
            int s;

            c = cutout && cutout[pick] ? 1 : 0;
            s = side[pick] >> drop[pick];
            total -= table[c][level_count(s) - 1] - table[c][level_count(s >> 1) - 1];
            drop[pick]++;
        }
    }
    return total;
}
