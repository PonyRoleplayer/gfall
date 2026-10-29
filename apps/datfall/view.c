#include "view.h"

#include "engine/assets/recfile.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/maindat.h"

/* See view.h and docs/data.md. The reader is recfile's. */

#define FIND_CAP 48

typedef struct DatKind {
    char tag[5];
    const char *table;
    df_u32 at;
    df_u32 size;
    RecView view;
    int damaged;
} DatKind;

struct DatFile {
    MemHeap *heap;
    df_u8 *bytes;
    df_u32 n;
    int ok;
    int version;
    char error[80];
    char warning[80];
    DatKind *kind;
    int nkind;
    int filt_kind;
    int filt_all;
    int nfilt;
    int *filt;
    char filt_q[FIND_CAP];
};

static void copy_cap(char *dst, int cap, const char *src)
{
    int i;

    if (!dst || cap < 1) {
        return;
    }
    if (!src) {
        src = "";
    }
    for (i = 0; i < cap - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

static int fold(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

static int contains(const char *hay, const char *needle)
{
    int i;
    int j;

    if (!needle || !needle[0]) {
        return 1;
    }
    if (!hay) {
        return 0;
    }
    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j]; j++) {
            if (!hay[i + j] || fold(hay[i + j]) != fold(needle[j])) {
                break;
            }
        }
        if (!needle[j]) {
            return 1;
        }
    }
    return 0;
}

static int starts(const char *hay, const char *needle)
{
    int i;

    if (!needle || !needle[0]) {
        return 1;
    }
    if (!hay) {
        return 0;
    }
    for (i = 0; needle[i]; i++) {
        if (!hay[i] || fold(hay[i]) != fold(needle[i])) {
            return 0;
        }
    }
    return 1;
}

static const DatKind *kind_at(const DatFile *f, int kind)
{
    if (!f || kind < 0 || kind >= f->nkind) {
        return 0;
    }
    return &f->kind[kind];
}

static void put_u(char *dst, int cap, int *n, unsigned int v)
{
    char tmp[16];
    int k;
    int i;

    k = 0;
    do {
        tmp[k++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v && k < 16);
    for (i = k - 1; i >= 0 && *n < cap - 1; i--) {
        dst[(*n)++] = tmp[i];
    }
    dst[*n] = 0;
}

static void put_i(char *dst, int cap, df_i32 v)
{
    int n;
    unsigned int u;

    n = 0;
    dst[0] = 0;
    if (cap < 2) {
        return;
    }
    if (v < 0) {
        dst[n++] = '-';
        dst[n] = 0;
        if (v == (df_i32)0x80000000) {
            u = 2147483648u;
        } else {
            u = (unsigned int)(-v);
        }
    } else {
        u = (unsigned int)v;
    }
    put_u(dst, cap, &n, u);
}

static void put_f(char *dst, int cap, float v)
{
    int n;
    int neg;
    int whole;
    int dec;
    int i;

    n = 0;
    dst[0] = 0;
    if (cap < 2) {
        return;
    }
    neg = v < 0.0f;
    if (neg) {
        v = -v;
    }
    whole = df_ifloor(v);
    dec = df_iround((v - (float)whole) * 1000.0f);
    if (dec >= 1000) {
        whole += 1;
        dec = 0;
    }
    if (dec < 0) {
        dec = 0;
    }
    if (neg && whole == 0 && dec == 0) {
        neg = 0;
    }
    if (neg) {
        dst[n++] = '-';
        dst[n] = 0;
    }
    put_u(dst, cap, &n, (unsigned int)whole);
    if (n < cap - 1) {
        dst[n++] = '.';
        dst[n] = 0;
    }
    /* Three digits, leading zeros kept, so 1.050 does not become 1.5. */
    for (i = 100; i >= 1 && n < cap - 1; i /= 10) {
        dst[n++] = (char)('0' + (dec / i) % 10);
        dst[n] = 0;
    }
}

DatFile *dat_open(const df_u8 *bytes, df_u32 n)
{
    MemHeap *heap;
    MemHeap *prev;
    DatFile *f;
    df_u32 pos;
    int count;
    RecChunk chunk;

    mem_init();
    heap = mem_heap_new();
    if (!heap) {
        return 0;
    }
    prev = mem_heap_use(heap);
    f = (DatFile *)mem_alloc(sizeof(DatFile));
    if (!f) {
        mem_heap_use(prev);
        mem_heap_delete(heap);
        return 0;
    }
    f->heap = heap;
    f->filt_kind = -1;
    f->filt_all = 1;
    if (!bytes || n > DAT_MAX_BYTES || n < 8) {
        copy_cap(f->error, (int)sizeof(f->error),
                 !bytes || n < 8 ? "not a record file" : "file is larger than 16 MB");
        mem_heap_use(prev);
        return f;
    }
    f->bytes = (df_u8 *)mem_alloc(n);
    if (!f->bytes) {
        mem_heap_use(prev);
        mem_heap_delete(heap);
        return 0;
    }
    mem_copy(f->bytes, bytes, n);
    f->n = n;
    if (!rec_is_file(f->bytes, n)) {
        copy_cap(f->error, (int)sizeof(f->error), "not a record file (GDAT version 1)");
        mem_heap_use(prev);
        return f;
    }
    f->ok = 1;
    f->version = REC_FORMAT;
    pos = 8;
    count = 0;
    while (rec_next(f->bytes, n, &pos, &chunk)) {
        count++;
    }
    if (pos + 8 <= n) {
        copy_cap(f->warning, (int)sizeof(f->warning), "a chunk runs past the end of the file");
    }
    if (count > 0) {
        f->kind = (DatKind *)mem_alloc((df_u32)count * (df_u32)sizeof(DatKind));
        if (!f->kind) {
            mem_heap_use(prev);
            mem_heap_delete(heap);
            return 0;
        }
    }
    pos = 8;
    while (f->nkind < count && rec_next(f->bytes, n, &pos, &chunk)) {
        DatKind *k;
        df_u32 at;

        k = &f->kind[f->nkind];
        /* rec_next leaves pos just past this chunk, pad included. */
        at = pos - 8u - chunk.size - (chunk.size & 1u);
        k->at = at;
        k->size = chunk.size;
        copy_cap(k->tag, (int)sizeof(k->tag), chunk.tag);
        k->table = k->tag;
        if (df_eq(k->tag, rec_head_schema.tag)) {
            k->table = rec_head_schema.table;
        } else {
            RecTable *t;

            /*
             * On the host, mem_keep follows the heap in use. The tables
             * must outlive this file, so the lookup runs on the caller's heap.
             */
            mem_heap_use(prev);
            t = maindat_find(k->tag);
            mem_heap_use(heap);
            if (t && t->schema && t->schema->table) {
                k->table = t->schema->table;
            }
        }
        if (!rec_view(&chunk, &k->view)) {
            k->damaged = 1;
            mem_zero(&k->view, sizeof(k->view));
        }
        f->nkind++;
    }
    mem_heap_use(prev);
    return f;
}

void dat_close(DatFile *f)
{
    MemHeap *heap;

    if (!f) {
        return;
    }
    heap = f->heap;
    mem_heap_delete(heap);
}

int dat_ok(const DatFile *f)
{
    return f && f->ok;
}

const char *dat_error(const DatFile *f)
{
    return f ? f->error : "not a record file";
}

const char *dat_warning(const DatFile *f)
{
    return f ? f->warning : "";
}

df_u32 dat_size(const DatFile *f)
{
    return f ? f->n : 0;
}

int dat_version(const DatFile *f)
{
    return f ? f->version : 0;
}

int dat_kinds(const DatFile *f)
{
    return f ? f->nkind : 0;
}

int dat_find_tag(const DatFile *f, const char *tag)
{
    int i;

    if (!f || !tag) {
        return -1;
    }
    for (i = 0; i < f->nkind; i++) {
        if (df_eq(f->kind[i].tag, tag)) {
            return i;
        }
    }
    return -1;
}

void dat_kind(const DatFile *f, int kind, char *tag, int tagcap, char *table, int tablecap)
{
    const DatKind *k;

    k = kind_at(f, kind);
    copy_cap(tag, tagcap, k ? k->tag : "");
    copy_cap(table, tablecap, k && k->table ? k->table : "");
}

int dat_rows(const DatFile *f, int kind)
{
    const DatKind *k;

    k = kind_at(f, kind);
    if (!k || k->damaged) {
        return 0;
    }
    return k->view.nrow;
}

int dat_fields(const DatFile *f, int kind)
{
    const DatKind *k;

    k = kind_at(f, kind);
    if (!k || k->damaged) {
        return 0;
    }
    return k->view.nfield;
}

int dat_row_bytes(const DatFile *f, int kind)
{
    const DatKind *k;

    k = kind_at(f, kind);
    if (!k || k->damaged) {
        return 0;
    }
    return k->view.row_size;
}

df_u32 dat_chunk_at(const DatFile *f, int kind)
{
    const DatKind *k;

    k = kind_at(f, kind);
    return k ? k->at : 0;
}

df_u32 dat_chunk_size(const DatFile *f, int kind)
{
    const DatKind *k;

    k = kind_at(f, kind);
    return k ? k->size : 0;
}

int dat_damaged(const DatFile *f, int kind)
{
    const DatKind *k;

    k = kind_at(f, kind);
    return k ? k->damaged : 1;
}

int dat_field(const DatFile *f, int kind, int field, char *name, int namecap,
              int *type, int *size, int *count)
{
    const DatKind *k;
    char got[REC_NAME];
    int t;
    int s;
    int c;

    k = kind_at(f, kind);
    if (!k || k->damaged || field < 0 || field >= k->view.nfield) {
        copy_cap(name, namecap, "");
        return 0;
    }
    rec_view_field(&k->view, field, got, &t, &s, &c);
    copy_cap(name, namecap, got);
    if (type) {
        *type = t;
    }
    if (size) {
        *size = s;
    }
    if (count) {
        *count = c;
    }
    return 1;
}

int dat_value(const DatFile *f, int kind, int row, int field, int elem, char *dst, int cap)
{
    const DatKind *k;
    char name[REC_NAME];
    char text[128];
    int type;
    int size;
    int count;
    df_i32 iv;
    float fv;

    if (!dst || cap < 1) {
        return 0;
    }
    dst[0] = 0;
    k = kind_at(f, kind);
    if (!k || k->damaged || !dat_field(f, kind, field, name, (int)sizeof(name), &type, &size, &count)) {
        return 0;
    }
    if (row < 0 || row >= k->view.nrow || elem < 0 || elem >= count) {
        return 0;
    }
    rec_view_value(&k->view, row, field, elem, &iv, &fv, text, (int)sizeof(text));
    if (type == REC_TEXT) {
        copy_cap(dst, cap, text);
    } else if (type == REC_FLOAT) {
        put_f(dst, cap, fv);
    } else if (type == REC_UINT) {
        int n;

        n = 0;
        put_u(dst, cap, &n, (unsigned int)iv);
    } else {
        put_i(dst, cap, iv);
    }
    return 1;
}

void dat_values(const DatFile *f, int kind, int row, int field, char *dst, int cap)
{
    char name[REC_NAME];
    char one[64];
    int type;
    int size;
    int count;
    int e;
    int n;
    int i;

    if (!dst || cap < 1) {
        return;
    }
    dst[0] = 0;
    if (!dat_field(f, kind, field, name, (int)sizeof(name), &type, &size, &count)) {
        return;
    }
    n = 0;
    for (e = 0; e < count; e++) {
        if (e > 0) {
            if (n < cap - 1) {
                dst[n++] = ',';
            }
            if (n < cap - 1) {
                dst[n++] = ' ';
            }
        }
        if (!dat_value(f, kind, row, field, e, one, (int)sizeof(one))) {
            break;
        }
        for (i = 0; one[i] && n < cap - 1; i++) {
            dst[n++] = one[i];
        }
        if (n >= cap - 1) {
            break;
        }
    }
    dst[n] = 0;
}

void dat_row_label(const DatFile *f, int kind, int row, char *id, int idcap, char *label, int labelcap)
{
    const DatKind *k;
    int i;

    copy_cap(id, idcap, "");
    copy_cap(label, labelcap, "");
    k = kind_at(f, kind);
    if (!k || k->damaged || row < 0 || row >= k->view.nrow || k->view.nfield < 1) {
        return;
    }
    dat_value(f, kind, row, 0, 0, id, idcap);
    for (i = 1; i < k->view.nfield; i++) {
        char name[REC_NAME];
        int type;
        int size;
        int count;

        if (!dat_field(f, kind, i, name, (int)sizeof(name), &type, &size, &count)) {
            break;
        }
        if (type == REC_TEXT) {
            dat_value(f, kind, row, i, 0, label, labelcap);
            break;
        }
    }
}

static int row_match(const DatFile *f, int kind, int row, const char *q)
{
    const DatKind *k;
    char text[128];
    int i;

    if (!q || !q[0]) {
        return 1;
    }
    k = kind_at(f, kind);
    if (!k || k->damaged) {
        return 0;
    }
    if (!dat_value(f, kind, row, 0, 0, text, (int)sizeof(text))) {
        return 0;
    }
    if (contains(text, q)) {
        return 1;
    }
    for (i = 1; i < k->view.nfield; i++) {
        char name[REC_NAME];
        int type;
        int size;
        int count;

        if (!dat_field(f, kind, i, name, (int)sizeof(name), &type, &size, &count)) {
            break;
        }
        if (type != REC_TEXT) {
            continue;
        }
        if (!dat_value(f, kind, row, i, 0, text, (int)sizeof(text))) {
            continue;
        }
        if (contains(text, q)) {
            return 1;
        }
    }
    return 0;
}

int dat_filter(DatFile *f, int kind, const char *q)
{
    const DatKind *k;
    MemHeap *prev;
    int rows;
    int n;
    int i;

    if (!f) {
        return 0;
    }
    if (!q) {
        q = "";
    }
    k = kind_at(f, kind);
    rows = k && !k->damaged ? k->view.nrow : 0;
    if (f->filt) {
        prev = mem_heap_use(f->heap);
        mem_heap_free(f->heap, f->filt);
        mem_heap_use(prev);
        f->filt = 0;
    }
    f->filt_kind = kind;
    f->nfilt = 0;
    copy_cap(f->filt_q, FIND_CAP, q);
    if (!q[0]) {
        f->filt_all = 1;
        f->nfilt = rows;
        return rows;
    }
    f->filt_all = 0;
    n = 0;
    for (i = 0; i < rows; i++) {
        if (row_match(f, kind, i, q)) {
            n++;
        }
    }
    if (n > 0) {
        prev = mem_heap_use(f->heap);
        f->filt = (int *)mem_alloc((df_u32)n * (df_u32)sizeof(int));
        mem_heap_use(prev);
        if (!f->filt) {
            f->nfilt = 0;
            return 0;
        }
        n = 0;
        for (i = 0; i < rows; i++) {
            if (row_match(f, kind, i, q)) {
                f->filt[n++] = i;
            }
        }
    }
    f->nfilt = n;
    return n;
}

int dat_filter_count(const DatFile *f)
{
    return f ? f->nfilt : 0;
}

int dat_filter_row(const DatFile *f, int i)
{
    if (!f || i < 0 || i >= f->nfilt) {
        return 0;
    }
    if (f->filt_all) {
        return i;
    }
    return f->filt[i];
}

int dat_kind_pick(const DatFile *f, const char *q)
{
    int i;

    if (!f || f->nkind < 1) {
        return -1;
    }
    if (!q || !q[0]) {
        return 0;
    }
    for (i = 0; i < f->nkind; i++) {
        if (starts(f->kind[i].tag, q) || starts(f->kind[i].table, q)) {
            return i;
        }
    }
    for (i = 0; i < f->nkind; i++) {
        if (contains(f->kind[i].tag, q) || contains(f->kind[i].table, q)) {
            return i;
        }
    }
    return -1;
}
