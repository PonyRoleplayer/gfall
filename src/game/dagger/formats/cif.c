#include "game/dagger/formats/cif.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"

/* See cif.h. The swing RLE is the one in the public CIF notes:
 * a byte above 127 repeats the next byte (value - 127) times,
 * and any other byte is a run of (value + 1) literal pixels. */

static int rle(const df_u8 *src, int n, df_u8 *dst, int need)
{
    int i;
    int o;

    i = 0;
    o = 0;
    while (o < need && i < n) {
        int d;
        int count;

        d = src[i++];
        if (d > 127) {
            int pix;

            count = d - 127;
            if (i >= n || o + count > need) {
                return 0;
            }
            pix = src[i++];
            while (count--) {
                dst[o++] = (df_u8)pix;
            }
        } else {
            count = d + 1;
            if (i + count > n || o + count > need) {
                return 0;
            }
            while (count--) {
                dst[o++] = src[i++];
            }
        }
    }
    return o == need;
}

static int add_frame(CifSet *out, int w, int h, int x, int y, int group, const df_u8 *pixels)
{
    CifFrame *f;

    if (out->n >= CIF_MAX) {
        return 0;
    }
    f = &out->frame[out->n++];
    f->w = w;
    f->h = h;
    f->x = x;
    f->y = y;
    f->group = group;
    f->pixels = pixels;
    if (group + 1 > out->groups) {
        out->groups = group + 1;
    }
    return 1;
}

static int take_images(const df_u8 *b, int n, int *off, CifSet *out)
{
    while (*off + 12 <= n && out->n < CIF_MAX) {
        int w;
        int h;
        int comp;
        int len;

        w = df_ri16(b + *off + 4);
        h = df_ri16(b + *off + 6);
        comp = df_ru16(b + *off + 8);
        len = df_ru16(b + *off + 10);
        if (w < 1 || h < 1 || w > 512 || h > 512 || comp != 0 || len != w * h) {
            break;
        }
        if (*off + 12 + len > n) {
            break;
        }
        if (!add_frame(out, w, h, df_ri16(b + *off), df_ri16(b + *off + 2), 0, b + *off + 12)) {
            break;
        }
        *off += 12 + len;
    }
    return 1;
}

static int take_swings(const df_u8 *b, int n, int off, CifSet *out, MemHeap *heap)
{
    int group;

    group = 1;
    while (off + 76 <= n && out->n < CIF_MAX) {
        int w;
        int h;
        int total;
        int slot;
        int used;
        MemHeap *prev;

        w = df_ri16(b + off);
        h = df_ri16(b + off + 2);
        total = df_ru16(b + off + 74);
        if (w < 1 || h < 1 || w > 512 || h > 512 || total < 76 || off + total > n) {
            break;
        }
        used = 0;
        prev = mem_heap_use(heap);
        for (slot = 0; slot < 31 && out->n < CIF_MAX; slot++) {
            int at;
            int next;
            int span;
            int k;
            df_u8 *pix;

            at = df_ru16(b + off + 12 + slot * 2);
            if (at < 76 || at >= total) {
                continue;
            }
            next = total;
            for (k = 0; k < 31; k++) {
                int other;

                other = df_ru16(b + off + 12 + k * 2);
                if (other > at && other < next) {
                    next = other;
                }
            }
            span = next - at;
            pix = (df_u8 *)mem_alloc((df_u32)w * (df_u32)h);
            if (!pix || !rle(b + off + at, span, pix, w * h)) {
                continue;
            }
            add_frame(out, w, h, df_ri16(b + off + 6), df_ri16(b + off + 8), group, pix);
            used = 1;
        }
        mem_heap_use(prev);
        if (!used) {
            break;
        }
        off += total;
        group++;
    }
    return 1;
}

int cif_open(const char *name, CifSet *out)
{
    const df_u8 *b;
    df_u32 n;
    int off;
    MemHeap *heap;
    MemHeap *prev;

    if (!out) {
        return 0;
    }
    out->n = 0;
    out->groups = 0;
    out->heap = 0;
    heap = mem_heap_new();
    if (!heap) {
        return 0;
    }
    out->heap = heap;
    /* The file in the set's heap: the IMG frames point into it, and cif_close frees it. */
    prev = mem_heap_use(heap);
    b = vfs_read(name, &n);
    mem_heap_use(prev);
    if (!b || n < 12) {
        cif_close(out);
        return 0;
    }
    off = 0;
    take_images(b, (int)n, &off, out);
    take_swings(b, (int)n, off, out, heap);
    if (out->n < 1) {
        cif_close(out);
        return 0;
    }
    return 1;
}

void cif_close(CifSet *set)
{
    if (!set) {
        return;
    }
    if (set->heap) {
        mem_heap_delete((MemHeap *)set->heap);
    }
    set->heap = 0;
    set->n = 0;
    set->groups = 0;
}

const CifFrame *cif_frame(const CifSet *set, int index)
{
    if (!set || index < 0 || index >= set->n) {
        return 0;
    }
    return &set->frame[index];
}

int cif_bounds(const CifFrame *f, int *x, int *y, int *w, int *h)
{
    int x0;
    int y0;
    int x1;
    int y1;
    int row;

    *x = *y = *w = *h = 0;
    if (!f || !f->pixels) {
        return 0;
    }
    x0 = f->w;
    y0 = f->h;
    x1 = -1;
    y1 = -1;
    for (row = 0; row < f->h; row++) {
        const df_u8 *p;
        int a;
        int b;

        p = f->pixels + row * f->w;
        /* The row's first and last pixel drawn; a clear row is passed over. */
        for (a = 0; a < f->w && !p[a]; a++) {
        }
        if (a == f->w) {
            continue;
        }
        for (b = f->w - 1; !p[b]; b--) {
        }
        if (a < x0) {
            x0 = a;
        }
        if (b > x1) {
            x1 = b;
        }
        if (y1 < 0) {
            y0 = row;
        }
        y1 = row;
    }
    if (x1 < 0) {
        return 0;
    }
    *x = x0;
    *y = y0;
    *w = x1 - x0 + 1;
    *h = y1 - y0 + 1;
    return 1;
}

int cif_group_span(const CifSet *set, int group, int *first)
{
    int i;
    int n;
    int at;

    if (first) {
        *first = 0;
    }
    if (!set) {
        return 0;
    }
    n = 0;
    at = -1;
    for (i = 0; i < set->n; i++) {
        if (set->frame[i].group != group) {
            continue;
        }
        if (at < 0) {
            at = i;
        }
        n++;
    }
    if (first && at >= 0) {
        *first = at;
    }
    return n;
}

const char *cif_metal_name(int metal)
{
    static const char *names[CIF_METALS] = {
        "Iron", "Steel", "Silver", "Elven", "Dwarven",
        "Mithril", "Adamantium", "Ebony", "Orcish", "Daedric"
    };

    if (metal < 0 || metal >= CIF_METALS) {
        return "Iron";
    }
    return names[metal];
}

int cif_metal(int index, int metal)
{
    /*
     * Classic's swap tables (DFU's ImageProcessing.GetMetalColorTable): what
     * each of the sixteen indices from CIF_RAMP becomes, by metal. Steel is
     * the pictures as drawn; the others are not simply another row, they
     * step through several of the palette's ramps.
     */
    static const df_u8 swap[CIF_METALS][16] = {
        { 0x77, 0x78, 0x57, 0x79, 0x58, 0x59, 0x7A, 0x5A, 0x7B, 0x5B, 0x7C, 0x5C, 0x7D, 0x5D, 0x5E, 0x5F },
        { 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F },
        { 0xE0, 0x70, 0x50, 0x71, 0x51, 0x72, 0x73, 0x52, 0x74, 0x53, 0x75, 0x54, 0x55, 0x56, 0x57, 0x58 },
        { 0xE0, 0x70, 0x50, 0x71, 0x51, 0x72, 0x73, 0x52, 0x74, 0x53, 0x75, 0x54, 0x76, 0x56, 0x77, 0x78 },
        { 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F },
        { 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE },
        { 0x5A, 0x5B, 0x7C, 0x5C, 0x7D, 0x5D, 0x7E, 0x5E, 0x7F, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE },
        { 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE },
        { 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD },
        { 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE }
    };

    if (index < CIF_RAMP || index >= CIF_RAMP + 16 || metal < 0 || metal >= CIF_METALS) {
        return index;
    }
    return swap[metal][index - CIF_RAMP];
}
