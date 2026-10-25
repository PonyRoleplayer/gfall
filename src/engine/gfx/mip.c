#include "engine/gfx/mip.h"

/*
 * The work buffer is RGBA, one level at a time. Each smaller level is
 * the 2 x 2 box mean of the one above, written over the front of the
 * same buffer: texel (x, y) of the half level is read from rows 2y and
 * 2y + 1, which are never before it, so the halving can run in place.
 * Alpha is kept as the mean, 0..255, and only thresholded on output,
 * so a half-covered texel two levels down is still half covered.
 */
static df_u8 g_rgba[256 * 256 * 4];

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

    m = side - 1;
    for (y = 0; y < side; y++) {
        for (x = 0; x < side; x++) {
            df_u8 *o;
            df_u8 c;

            o = &g_rgba[(y * side + x) * 4];
            c = index[y * side + x];
            if (!cutout || c != 0) {
                o[0] = pr[c];
                o[1] = pg[c];
                o[2] = pb[c];
                o[3] = 255;
                continue;
            }
            {
                int r;
                int g;
                int b;
                int n;
                int dx;
                int dy;

                r = 0;
                g = 0;
                b = 0;
                n = 0;
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        df_u8 k;

                        k = index[((y + dy) & m) * side + ((x + dx) & m)];
                        if (k != 0) {
                            r += pr[k];
                            g += pg[k];
                            b += pb[k];
                            n++;
                        }
                    }
                }
                o[0] = (df_u8)(n ? r / n : 0);
                o[1] = (df_u8)(n ? g / n : 0);
                o[2] = (df_u8)(n ? b / n : 0);
                o[3] = 0;
            }
        }
    }
}

/* g_rgba from side to side / 2, in place. */
static void halve(int side)
{
    int half;
    int x;
    int y;

    half = side >> 1;
    for (y = 0; y < half; y++) {
        for (x = 0; x < half; x++) {
            const df_u8 *a;
            const df_u8 *b;
            df_u8 *o;
            int k;

            a = &g_rgba[((2 * y) * side + 2 * x) * 4];
            b = a + side * 4;
            o = &g_rgba[(y * half + x) * 4];
            for (k = 0; k < 4; k++) {
                o[k] = (df_u8)((a[k] + a[4 + k] + b[k] + b[4 + k] + 2) >> 2);
            }
        }
    }
}

/* One level of g_rgba into out. Returns the bytes written. */
static df_u32 emit(int side, int cutout, df_u8 *out)
{
    df_u32 n;
    df_u32 i;

    n = (df_u32)side * (df_u32)side;
    for (i = 0; i < n; i++) {
        const df_u8 *p;

        p = &g_rgba[i * 4];
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
            out[i] = mip_nearest(p[0], p[1], p[2]);
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

        pick = -1;
        most = 0;
        for (i = 0; i < n; i++) {
            int s;
            df_u32 b;

            if (!pow2_side(side[i])) {
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
