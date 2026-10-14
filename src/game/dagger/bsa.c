#include "game/dagger/bsa.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"

struct Bsa {
    VfsFile *file;
    int named;
    int count;
    BsaRec *recs;
};

static void rec_swap(BsaRec *a, BsaRec *b)
{
    BsaRec t;

    t = *a;
    *a = *b;
    *b = t;
}

static void heap_sift(BsaRec *a, int n, int root)
{
    for (;;) {
        int big;
        int left;
        int right;

        big = root;
        left = root * 2 + 1;
        right = left + 1;
        if (left < n && a[left].id > a[big].id) {
            big = left;
        }
        if (right < n && a[right].id > a[big].id) {
            big = right;
        }
        if (big == root) {
            break;
        }
        rec_swap(&a[root], &a[big]);
        root = big;
    }
}

static void heap_sort(BsaRec *a, int n)
{
    int i;

    if (n < 2) {
        return;
    }
    for (i = n / 2 - 1; i >= 0; i--) {
        heap_sift(a, n, i);
    }
    for (i = n - 1; i > 0; i--) {
        rec_swap(&a[0], &a[i]);
        heap_sift(a, i, 0);
    }
}

static void name_from(char *dst, const df_u8 *src)
{
    int n;
    int i;

    n = 12;
    while (n > 0 && (src[n - 1] == 0 || src[n - 1] == ' ')) {
        n--;
    }
    for (i = 0; i < n; i++) {
        dst[i] = (char)src[i];
    }
    dst[n] = 0;
}

Bsa *bsa_open(const char *rel)
{
    Bsa *bsa;
    df_u8 head[4];
    df_u16 count;
    df_u16 type;
    int entry;
    df_u32 file_size;
    df_u32 footer;
    df_u32 dir_bytes;
    df_u8 *dir;
    df_u32 cursor;
    int i;

    bsa = (Bsa *)mem_alloc(sizeof(Bsa));
    if (!bsa) {
        return 0;
    }
    bsa->file = vfs_open(rel);
    if (!bsa->file) {
        return 0;
    }
    file_size = vfs_size(bsa->file);
    if (file_size < 4 || !vfs_read_at(bsa->file, 0, head, 4)) {
        bsa_close(bsa);
        return 0;
    }
    count = df_ru16(head);
    type = df_ru16(head + 2);
    if (type == 0x0200) {
        entry = 8;
        bsa->named = 0;
    } else if (type == 0x0100) {
        entry = 18;
        bsa->named = 1;
    } else {
        bsa_close(bsa);
        return 0;
    }
    if (count == 0) {
        bsa_close(bsa);
        return 0;
    }
    dir_bytes = (df_u32)count * (df_u32)entry;
    if (dir_bytes > file_size - 4) {
        bsa_close(bsa);
        return 0;
    }
    footer = file_size - dir_bytes;
    dir = (df_u8 *)mem_alloc(dir_bytes);
    bsa->recs = (BsaRec *)mem_alloc((df_u32)count * (df_u32)sizeof(BsaRec));
    if (!dir || !bsa->recs || !vfs_read_at(bsa->file, footer, dir, dir_bytes)) {
        bsa_close(bsa);
        return 0;
    }
    cursor = 4;
    for (i = 0; i < (int)count; i++) {
        const df_u8 *e;
        df_u32 sz;

        e = dir + (df_u32)i * (df_u32)entry;
        if (bsa->named) {
            name_from(bsa->recs[i].name, e);
            bsa->recs[i].id = 0;
            sz = df_ru32(e + 14);
        } else {
            bsa->recs[i].name[0] = 0;
            bsa->recs[i].id = df_ru32(e);
            sz = df_ru32(e + 4);
        }
        if (sz > file_size || cursor > footer || sz > footer - cursor) {
            bsa_close(bsa);
            return 0;
        }
        bsa->recs[i].offset = cursor;
        bsa->recs[i].size = sz;
        cursor += sz;
    }
    if (cursor != footer) {
        bsa_close(bsa);
        return 0;
    }
    bsa->count = (int)count;
    if (!bsa->named) {
        int w;

        heap_sort(bsa->recs, bsa->count);
        w = 0;
        for (i = 1; i < bsa->count; i++) {
            if (bsa->recs[i].id == bsa->recs[w].id) {
                if (bsa->recs[i].offset < bsa->recs[w].offset) {
                    bsa->recs[w] = bsa->recs[i];
                }
            } else {
                w++;
                bsa->recs[w] = bsa->recs[i];
            }
        }
        bsa->count = w + 1;
    }
    return bsa;
}

void bsa_close(Bsa *bsa)
{
    if (bsa) {
        vfs_close(bsa->file);
        bsa->file = 0;
    }
}

int bsa_named(const Bsa *bsa)
{
    return bsa && bsa->named;
}

int bsa_count(const Bsa *bsa)
{
    return bsa ? bsa->count : 0;
}

const BsaRec *bsa_find_id(const Bsa *bsa, df_u32 id)
{
    int lo;
    int hi;

    if (!bsa || bsa->named) {
        return 0;
    }
    lo = 0;
    hi = bsa->count - 1;
    while (lo <= hi) {
        int mid;

        mid = (lo + hi) >> 1;
        if (bsa->recs[mid].id == id) {
            return &bsa->recs[mid];
        }
        if (bsa->recs[mid].id < id) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return 0;
}

const BsaRec *bsa_find_name(const Bsa *bsa, const char *name)
{
    int i;

    if (!bsa || !bsa->named || !name) {
        return 0;
    }
    for (i = 0; i < bsa->count; i++) {
        if (df_eq(bsa->recs[i].name, name)) {
            return &bsa->recs[i];
        }
    }
    return 0;
}

df_u8 *bsa_read(Bsa *bsa, const BsaRec *rec)
{
    df_u8 *bytes;

    if (!bsa || !rec || rec->size == 0) {
        return 0;
    }
    bytes = (df_u8 *)mem_alloc(rec->size);
    if (!bytes || !vfs_read_at(bsa->file, rec->offset, bytes, rec->size)) {
        return 0;
    }
    return bytes;
}
