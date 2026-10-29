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
    int shade;

    if (index < 112 || index > 127) {
        return index;
    }
    shade = index - 112;
    if (metal < 0) {
        metal = 0;
    }
    /* Row starts in ART_PAL.COL. Grey stays on row 7 (112). Steel and
     * adamantium and ebony move within that grey row, lighter or darker.
     * The coloured metals are the rows that are that colour: silver 80,
     * a brighter silver for elven, yellow 144, blue 96, green 160, red 240. */
    if (metal == 1) {
        shade -= 2;
        if (shade < 0) {
            shade = 0;
        }
        return 112 + shade;
    }
    if (metal == 2) {
        return 80 + shade;
    }
    if (metal == 3) {
        shade -= 2;
        if (shade < 0) {
            shade = 0;
        }
        return 80 + shade;
    }
    if (metal == 4) {
        return 144 + shade;
    }
    if (metal == 5) {
        return 96 + shade;
    }
    if (metal == 6) {
        shade += 4;
        if (shade > 15) {
            shade = 15;
        }
        return 112 + shade;
    }
    if (metal == 7) {
        shade += 8;
        if (shade > 15) {
            shade = 15;
        }
        return 112 + shade;
    }
    if (metal == 8) {
        return 160 + shade;
    }
    if (metal == 9) {
        return 240 + shade;
    }
    return index;
}
