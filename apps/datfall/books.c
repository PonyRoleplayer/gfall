#include "books.h"

#include "engine/assets/recfile.h"
#include "engine/core/mem.h"

/* See books.h. The merge is generic on purpose: rec_merge needs a schema. */

#define TAGS 48
#define MERGE_ROWS 1024
#define MERGE_ROW 1024

typedef struct MField {
    char name[REC_NAME];
    int type;
    int size;
    int count;
    int offset;
} MField;

typedef struct MTag {
    char tag[5];
    MField field[REC_FIELDS];
    int nfield;
    int row_size;
    int nrow;
    int cap;
    df_u8 *rows;
} MTag;

struct Books {
    MemHeap *heap;
    int n;
    int cut;
    char name[BOOKS_MAX][32];
    DatFile *file[BOOKS_MAX];
    DatFile *merged;
    char status[160];
};

static void copy_name(char *dst, int cap, const char *src)
{
    df_copy(dst, src ? src : "", cap);
}

static int fold(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

static int same_fold(const char *a, const char *b)
{
    int i;

    if (!a || !b) {
        return 0;
    }
    for (i = 0; a[i] || b[i]; i++) {
        if (fold(a[i]) != fold(b[i])) {
            return 0;
        }
    }
    return 1;
}

static void put16(df_u8 *p, unsigned int v)
{
    p[0] = (df_u8)v;
    p[1] = (df_u8)(v >> 8);
}

static void put32(df_u8 *p, df_u32 v)
{
    p[0] = (df_u8)v;
    p[1] = (df_u8)(v >> 8);
    p[2] = (df_u8)(v >> 16);
    p[3] = (df_u8)(v >> 24);
}

static void read_num(const df_u8 *p, int type, int size, df_i32 *iv, float *fv)
{
    df_u32 bits;

    *iv = 0;
    *fv = 0.0f;
    if (type == REC_FLOAT && size == 4) {
        bits = df_ru32(p);
        mem_copy(fv, &bits, 4);
        *iv = df_iround(*fv);
        return;
    }
    if (size == 1) {
        *iv = type == REC_INT ? (df_i32)(signed char)p[0] : (df_i32)p[0];
    } else if (size == 2) {
        *iv = type == REC_INT ? (df_i32)df_ri16(p) : (df_i32)df_ru16(p);
    } else if (size == 4) {
        *iv = (df_i32)df_ru32(p);
    }
    *fv = (float)*iv;
}

static void write_num(df_u8 *p, int type, int size, df_i32 iv, float fv, int from_float)
{
    float f;
    unsigned int u;

    if (from_float) {
        iv = df_iround(fv);
    }
    if (type == REC_FLOAT) {
        f = from_float ? fv : (float)iv;
        mem_copy(p, &f, 4);
        return;
    }
    if (size == 1) {
        if (type == REC_INT) {
            if (iv > 127) {
                iv = 127;
            }
            if (iv < -128) {
                iv = -128;
            }
        } else {
            if (iv < 0) {
                iv = 0;
            }
            if (iv > 255) {
                iv = 255;
            }
        }
        p[0] = (df_u8)iv;
        return;
    }
    if (size == 2) {
        if (type == REC_INT) {
            if (iv > 32767) {
                iv = 32767;
            }
            if (iv < -32768) {
                iv = -32768;
            }
        } else {
            if (iv < 0) {
                iv = 0;
            }
            if (iv > 65535) {
                iv = 65535;
            }
        }
        put16(p, (unsigned int)iv);
        return;
    }
    u = (unsigned int)iv;
    put32(p, u);
}

static int field_named(const MTag *t, const char *name)
{
    int i;

    for (i = 0; i < t->nfield; i++) {
        if (df_eq(t->field[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static int add_field(MTag *t, const char *name, int type, int size, int count)
{
    int have;
    int bytes;
    int ns;
    int i;
    df_u8 *rows;

    have = field_named(t, name);
    if (have >= 0) {
        return have;
    }
    if (t->nfield >= REC_FIELDS || size < 1 || count < 1) {
        return -1;
    }
    if (type == REC_TEXT) {
        if (size > 240) {
            size = 240;
        }
    } else if (type == REC_FLOAT) {
        size = 4;
    } else if (!(size == 1 || size == 2 || size == 4)) {
        return -1;
    }
    bytes = size * count;
    if (bytes < 1 || t->row_size > MERGE_ROW - bytes) {
        return -1;
    }
    ns = t->row_size + bytes;
    if (t->nrow > 0) {
        rows = (df_u8 *)mem_alloc((df_u32)t->cap * (df_u32)ns);
        if (!rows) {
            return -2;
        }
        mem_zero(rows, (df_u32)t->cap * (df_u32)ns);
        for (i = 0; i < t->nrow; i++) {
            mem_copy(rows + (df_u32)i * (df_u32)ns,
                     t->rows + (df_u32)i * (df_u32)t->row_size, (df_u32)t->row_size);
        }
        t->rows = rows;
    }
    copy_name(t->field[t->nfield].name, REC_NAME, name);
    t->field[t->nfield].type = type;
    t->field[t->nfield].size = size;
    t->field[t->nfield].count = count;
    t->field[t->nfield].offset = t->row_size;
    t->row_size = ns;
    t->nfield++;
    return t->nfield - 1;
}

static df_i32 row_id(const MTag *t, int i)
{
    const MField *f;
    df_i32 iv;
    float fv;

    if (t->nfield < 1 || i < 0 || i >= t->nrow) {
        return 0;
    }
    f = &t->field[0];
    read_num(t->rows + (df_u32)i * (df_u32)t->row_size + (df_u32)f->offset, f->type, f->size, &iv, &fv);
    return iv;
}

static int find_row(const MTag *t, df_i32 id)
{
    int i;

    for (i = 0; i < t->nrow; i++) {
        if (row_id(t, i) == id) {
            return i;
        }
    }
    return -1;
}

static int add_row(MTag *t, df_i32 id, int *cut)
{
    int i;
    int cap;
    df_u8 *rows;
    const MField *f;

    i = find_row(t, id);
    if (i >= 0) {
        return i;
    }
    if (t->nfield < 1 || t->row_size < 1) {
        return -1;
    }
    if (t->nrow >= MERGE_ROWS) {
        *cut = 1;
        return -1;
    }
    if (t->nrow >= t->cap) {
        cap = t->cap ? t->cap * 2 : 8;
        if (cap > MERGE_ROWS) {
            cap = MERGE_ROWS;
        }
        rows = (df_u8 *)mem_alloc((df_u32)cap * (df_u32)t->row_size);
        if (!rows) {
            return -1;
        }
        mem_zero(rows, (df_u32)cap * (df_u32)t->row_size);
        if (t->nrow > 0 && t->rows) {
            mem_copy(rows, t->rows, (df_u32)t->nrow * (df_u32)t->row_size);
        }
        t->rows = rows;
        t->cap = cap;
    }
    mem_zero(t->rows + (df_u32)t->nrow * (df_u32)t->row_size, (df_u32)t->row_size);
    f = &t->field[0];
    write_num(t->rows + (df_u32)t->nrow * (df_u32)t->row_size + (df_u32)f->offset,
              f->type, f->size, id, 0.0f, 0);
    t->nrow++;
    return t->nrow - 1;
}

static void copy_elem(df_u8 *dst, const MField *df, const RecView *v, int row, int src, int elem)
{
    df_i32 iv;
    float fv;
    char text[256];
    int type;
    int n;
    int k;

    iv = 0;
    fv = 0.0f;
    text[0] = 0;
    type = rec_view_value(v, row, src, elem, &iv, &fv, text, (int)sizeof(text));
    if (df->type == REC_TEXT) {
        if (type != REC_TEXT) {
            return;
        }
        mem_zero(dst, (df_u32)df->size);
        n = df->size;
        for (k = 0; k < n && text[k]; k++) {
            dst[k] = (df_u8)text[k];
        }
        return;
    }
    if (type == REC_TEXT) {
        return;
    }
    write_num(dst, df->type, df->size, iv, fv, type == REC_FLOAT);
}

static int take_chunk(MTag *tags, int *ntag, const RecChunk *chunk, int *cut)
{
    RecView view;
    MTag *t;
    int i;
    int f;
    int r;
    int e;
    int which;
    int type;
    int size;
    int count;
    char name[REC_NAME];
    df_i32 id;
    float fv;

    if (!rec_view(chunk, &view)) {
        return 1;
    }
    if (view.nfield < 1) {
        return 1;
    }
    rec_view_field(&view, 0, name, &type, &size, &count);
    if (type != REC_INT && type != REC_UINT) {
        return 1;
    }
    which = -1;
    for (i = 0; i < *ntag; i++) {
        if (df_eq(tags[i].tag, chunk->tag)) {
            which = i;
            break;
        }
    }
    if (which < 0) {
        if (*ntag >= TAGS) {
            *cut = 1;
            return 1;
        }
        which = *ntag;
        mem_zero(&tags[which], sizeof(tags[which]));
        copy_name(tags[which].tag, (int)sizeof(tags[which].tag), chunk->tag);
        (*ntag)++;
    }
    t = &tags[which];
    for (f = 0; f < view.nfield; f++) {
        int added;

        rec_view_field(&view, f, name, &type, &size, &count);
        added = add_field(t, name, type, size, count);
        if (added == -2) {
            return 0;
        }
    }
    for (r = 0; r < view.nrow; r++) {
        id = 0;
        fv = 0.0f;
        rec_view_value(&view, r, 0, 0, &id, &fv, name, (int)sizeof(name));
        (void)fv;
        i = add_row(t, id, cut);
        if (i < 0) {
            continue;
        }
        for (f = 0; f < view.nfield; f++) {
            int dest;
            int n;
            const MField *df;

            rec_view_field(&view, f, name, &type, &size, &count);
            dest = field_named(t, name);
            if (dest < 0) {
                continue;
            }
            df = &t->field[dest];
            n = count < df->count ? count : df->count;
            for (e = 0; e < n; e++) {
                copy_elem(t->rows + (df_u32)i * (df_u32)t->row_size + (df_u32)df->offset
                              + (df_u32)e * (df_u32)df->size,
                          df, &view, r, f, e);
            }
        }
    }
    return 1;
}

typedef struct Out {
    df_u8 *p;
    df_u32 n;
    df_u32 cap;
} Out;

static int out_put(Out *o, const void *src, df_u32 n)
{
    df_u8 *p;
    df_u32 cap;

    if (o->n + n > o->cap) {
        cap = o->cap ? o->cap : 4096u;
        while (cap < o->n + n) {
            if (cap > 8u * 1024u * 1024u) {
                return 0;
            }
            cap *= 2u;
        }
        p = (df_u8 *)mem_alloc(cap);
        if (!p) {
            return 0;
        }
        if (o->n && o->p) {
            mem_copy(p, o->p, o->n);
        }
        o->p = p;
        o->cap = cap;
    }
    if (n && src) {
        mem_copy(o->p + o->n, src, n);
    }
    o->n += n;
    return 1;
}

static int emit_tag(Out *o, const MTag *t)
{
    df_u8 head[8];
    df_u8 body[12];
    df_u8 fb[24];
    df_u32 size;
    int i;
    int r;

    if (t->nfield < 1) {
        return 1;
    }
    size = 12u + (df_u32)t->nfield * 24u + (df_u32)t->row_size * (df_u32)t->nrow;
    mem_zero(head, 8);
    mem_copy(head, t->tag, 4);
    put32(head + 4, size);
    if (!out_put(o, head, 8)) {
        return 0;
    }
    put16(body, 1);
    put16(body + 2, (unsigned int)t->nfield);
    put16(body + 4, (unsigned int)t->row_size);
    put16(body + 6, 0);
    put32(body + 8, (df_u32)t->nrow);
    if (!out_put(o, body, 12)) {
        return 0;
    }
    for (i = 0; i < t->nfield; i++) {
        mem_zero(fb, 24);
        mem_copy(fb, t->field[i].name, REC_NAME);
        fb[16] = (df_u8)t->field[i].type;
        fb[17] = (df_u8)t->field[i].size;
        put16(fb + 18, (unsigned int)t->field[i].count);
        put16(fb + 20, (unsigned int)t->field[i].offset);
        if (!out_put(o, fb, 24)) {
            return 0;
        }
    }
    for (r = 0; r < t->nrow; r++) {
        if (!out_put(o, t->rows + (df_u32)r * (df_u32)t->row_size, (df_u32)t->row_size)) {
            return 0;
        }
    }
    if (size & 1u) {
        head[0] = 0;
        if (!out_put(o, head, 1)) {
            return 0;
        }
    }
    return 1;
}

/*
 * The merge walks the caller's bytes, not the DatFile: rec_view needs
 * the chunk in place, and DatFile does not hand that pointer back.
 */
static int merge_bytes(Books *b, const df_u8 **bytes, const df_u32 *sizes)
{
    MemHeap *prev;
    MTag *tags;
    Out out;
    df_u8 magic[8];
    int ntag;
    int i;
    int cut;
    int ok;

    prev = mem_heap_use(b->heap);
    tags = (MTag *)mem_alloc((df_u32)sizeof(MTag) * TAGS);
    if (!tags) {
        mem_heap_use(prev);
        return 0;
    }
    mem_zero(tags, (df_u32)sizeof(MTag) * TAGS);
    ntag = 0;
    cut = 0;
    for (i = 0; i < b->n; i++) {
        df_u32 pos;
        RecChunk chunk;

        if (!bytes || !sizes || !bytes[i] || !dat_ok(b->file[i]) || !rec_is_file(bytes[i], sizes[i])) {
            continue;
        }
        pos = 8;
        while (rec_next(bytes[i], sizes[i], &pos, &chunk)) {
            if (!take_chunk(tags, &ntag, &chunk, &cut)) {
                mem_heap_use(prev);
                return 0;
            }
        }
    }
    mem_zero(&out, sizeof(out));
    mem_copy(magic, REC_MAGIC, 4);
    put32(magic + 4, REC_FORMAT);
    ok = out_put(&out, magic, 8);
    for (i = 0; i < ntag && ok; i++) {
        ok = emit_tag(&out, &tags[i]);
    }
    b->cut = cut;
    mem_heap_use(prev);
    if (!ok || !out.p) {
        return 0;
    }
    b->merged = dat_open(out.p, out.n);
    return b->merged != 0;
}

static void status_of(Books *b)
{
    int i;
    int n;

    n = 0;
    b->status[0] = 0;
    for (i = 0; i < b->n; i++) {
        int k;

        if (!dat_ok(b->file[i])) {
            continue;
        }
        if (n > 0 && n < (int)sizeof(b->status) - 2) {
            b->status[n++] = ',';
            b->status[n++] = ' ';
            b->status[n] = 0;
        }
        for (k = 0; b->name[i][k] && n < (int)sizeof(b->status) - 1; k++) {
            b->status[n++] = b->name[i][k];
        }
        b->status[n] = 0;
    }
    if (!b->status[0]) {
        copy_name(b->status, (int)sizeof(b->status), "no .DAT files");
    }
}

Books *books_open(const char **names, const df_u8 **bytes, const df_u32 *sizes, int n)
{
    MemHeap *heap;
    MemHeap *prev;
    Books *b;
    int i;

    mem_init();
    if (n < 0) {
        n = 0;
    }
    if (n > BOOKS_MAX) {
        n = BOOKS_MAX;
    }
    heap = mem_heap_new();
    if (!heap) {
        return 0;
    }
    prev = mem_heap_use(heap);
    b = (Books *)mem_alloc(sizeof(Books));
    mem_heap_use(prev);
    if (!b) {
        mem_heap_delete(heap);
        return 0;
    }
    mem_zero(b, sizeof(*b));
    b->heap = heap;
    b->n = n;
    for (i = 0; i < n; i++) {
        copy_name(b->name[i], (int)sizeof(b->name[i]), names && names[i] ? names[i] : "");
        b->file[i] = dat_open(bytes ? bytes[i] : 0, sizes ? sizes[i] : 0);
        if (!b->file[i]) {
            books_close(b);
            return 0;
        }
    }
    if (!merge_bytes(b, bytes, sizes)) {
        b->merged = dat_open(0, 0);
    }
    status_of(b);
    return b;
}

void books_close(Books *b)
{
    MemHeap *heap;
    int i;

    if (!b) {
        return;
    }
    for (i = 0; i < b->n; i++) {
        dat_close(b->file[i]);
        b->file[i] = 0;
    }
    dat_close(b->merged);
    heap = b->heap;
    mem_heap_delete(heap);
}

int books_count(const Books *b)
{
    return b ? b->n : 0;
}

const char *books_name(const Books *b, int i)
{
    if (!b || i < 0 || i >= b->n) {
        return "";
    }
    return b->name[i];
}

int books_ok(const Books *b, int i)
{
    if (!b || i < 0 || i >= b->n) {
        return 0;
    }
    return dat_ok(b->file[i]);
}

DatFile *books_file(Books *b, int i)
{
    if (!b || i < 0 || i >= b->n) {
        return 0;
    }
    return b->file[i];
}

DatFile *books_merged(Books *b)
{
    return b ? b->merged : 0;
}

const char *books_status(const Books *b)
{
    return b ? b->status : "";
}

int books_cut(const Books *b)
{
    return b ? b->cut : 0;
}

int books_order(const df_u8 *text, df_u32 n, char names[][BOOKS_NAME], int cap)
{
    df_u32 pos;
    int got;

    got = 0;
    pos = 0;
    if (!text || !names || cap < 1) {
        return 0;
    }
    while (pos < n && got < cap) {
        int k;
        df_u32 end;
        char name[BOOKS_NAME];

        end = pos;
        while (end < n && text[end] != '\n') {
            end++;
        }
        while (pos < end && (text[pos] == ' ' || text[pos] == '\t')) {
            pos++;
        }
        k = 0;
        name[0] = 0;
        if (pos < end && text[pos] != '#') {
            while (pos < end && k < BOOKS_NAME - 1 && text[pos] != '\r' && text[pos] != ' '
                   && text[pos] != '\t') {
                name[k++] = (char)text[pos++];
            }
            name[k] = 0;
        }
        pos = end < n ? end + 1 : end;
        if (k > 0 && !same_fold(name, "MAIN.DAT")) {
            copy_name(names[got], BOOKS_NAME, name);
            got++;
        }
    }
    return got;
}
