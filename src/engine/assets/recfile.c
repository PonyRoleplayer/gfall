#include "engine/assets/recfile.h"
#include "engine/core/mem.h"

/* See recfile.h and docs/data.md. */

#define CHUNK_HEAD 12
#define FIELD_BYTES 24

static const RecField g_head_fields[] = {
    REC_FIELD("id", REC_UINT, RecHead, id),
    REC_FIELD("role", REC_UINT, RecHead, role),
    REC_FIELD("size", REC_UINT, RecHead, size),
    REC_FIELD("name", REC_TEXT, RecHead, name)
};
const RecSchema rec_head_schema = {
    "HEAD", "head", g_head_fields, 4, (df_u16)sizeof(RecHead), 0
};

static void put16(df_u8 *p, df_u32 v)
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

int rec_put_begin(RecPut put, void *ctx)
{
    df_u8 b[8];

    mem_copy(b, REC_MAGIC, 4);
    put32(b + 4, REC_FORMAT);
    return put(ctx, b, 8);
}

/*
 * One element of a row struct's field as the file's bytes: the host and
 * the target are both little-endian x86, so it is the struct's bytes.
 */
int rec_put_rows(RecPut put, void *ctx, const RecSchema *s, const void *rows, int n,
                 const int *fields, int nfields)
{
    int all[REC_FIELDS];
    df_u8 head[CHUNK_HEAD];
    df_u8 fb[FIELD_BYTES];
    df_u8 row[512];
    df_u32 row_size;
    df_u32 size;
    int i;
    int r;

    if (!fields) {
        nfields = s->nfield;
        for (i = 0; i < nfields && i < REC_FIELDS; i++) {
            all[i] = i;
        }
        fields = all;
    }
    if (nfields < 1 || nfields > REC_FIELDS || fields[0] != 0) {
        return 0;
    }
    row_size = 0;
    for (i = 0; i < nfields; i++) {
        const RecField *f;

        f = &s->field[fields[i]];
        row_size += (df_u32)f->size * f->count;
    }
    if (row_size > sizeof(row)) {
        return 0;
    }
    size = CHUNK_HEAD + (df_u32)nfields * FIELD_BYTES + row_size * (df_u32)n;
    /* The chunk's own header: tag and size. */
    mem_copy(head, s->tag, 4);
    put32(head + 4, size);
    if (!put(ctx, head, 8)) {
        return 0;
    }
    put16(head, 1);
    put16(head + 2, (df_u32)nfields);
    put16(head + 4, row_size);
    put16(head + 6, 0);
    put32(head + 8, (df_u32)n);
    if (!put(ctx, head, CHUNK_HEAD)) {
        return 0;
    }
    row_size = 0;
    for (i = 0; i < nfields; i++) {
        const RecField *f;

        f = &s->field[fields[i]];
        mem_zero(fb, FIELD_BYTES);
        df_copy((char *)fb, f->name, REC_NAME);
        fb[16] = f->type;
        fb[17] = f->size;
        put16(fb + 18, f->count);
        put16(fb + 20, row_size);
        row_size += (df_u32)f->size * f->count;
        if (!put(ctx, fb, FIELD_BYTES)) {
            return 0;
        }
    }
    for (r = 0; r < n; r++) {
        const df_u8 *src;
        df_u32 at;

        src = (const df_u8 *)rows + (df_u32)r * s->row_size;
        at = 0;
        for (i = 0; i < nfields; i++) {
            const RecField *f;
            df_u32 bytes;

            f = &s->field[fields[i]];
            bytes = (df_u32)f->size * f->count;
            mem_copy(row + at, src + f->offset, bytes);
            at += bytes;
        }
        if (!put(ctx, row, at)) {
            return 0;
        }
    }
    if (size & 1u) {
        head[0] = 0;
        return put(ctx, head, 1);
    }
    return 1;
}

int rec_is_file(const df_u8 *bytes, df_u32 n)
{
    return bytes && n >= 8 && bytes[0] == 'G' && bytes[1] == 'D' && bytes[2] == 'A' && bytes[3] == 'T'
        && df_ru32(bytes + 4) == REC_FORMAT;
}

int rec_next(const df_u8 *bytes, df_u32 n, df_u32 *pos, RecChunk *out)
{
    df_u32 at;
    df_u32 size;

    at = *pos < 8 ? 8 : *pos;
    if (at + 8 > n) {
        return 0;
    }
    size = df_ru32(bytes + at + 4);
    if (size > n - at - 8) {
        return 0;
    }
    mem_copy(out->tag, bytes + at, 4);
    out->tag[4] = 0;
    out->data = bytes + at + 8;
    out->size = size;
    *pos = at + 8 + size + (size & 1u);
    return 1;
}

int rec_view(const RecChunk *c, RecView *v)
{
    df_u32 need;
    df_u32 sum;
    int i;

    if (c->size < CHUNK_HEAD || df_ru16(c->data) != 1) {
        return 0;
    }
    v->nfield = df_ru16(c->data + 2);
    v->row_size = df_ru16(c->data + 4);
    v->nrow = (int)df_ru32(c->data + 8);
    if (v->nfield < 1 || v->nfield > REC_FIELDS) {
        return 0;
    }
    v->fields = c->data + CHUNK_HEAD;
    v->rows = v->fields + v->nfield * FIELD_BYTES;
    need = CHUNK_HEAD + (df_u32)v->nfield * FIELD_BYTES;
    if (need > c->size || (df_u32)v->nrow > (c->size - need) / (v->row_size ? (df_u32)v->row_size : 1u)) {
        return 0;
    }
    /* Every field within the row, and of a size its type allows. */
    sum = 0;
    for (i = 0; i < v->nfield; i++) {
        const df_u8 *f;
        int type;
        int size;

        f = v->fields + i * FIELD_BYTES;
        type = f[16];
        size = f[17];
        if (type == REC_TEXT ? size < 1
                             : !(size == 1 || size == 2 || size == 4) || (type == REC_FLOAT && size != 4)) {
            return 0;
        }
        if (type != REC_INT && type != REC_UINT && type != REC_FLOAT && type != REC_TEXT) {
            return 0;
        }
        if ((df_u32)df_ru16(f + 20) + (df_u32)size * df_ru16(f + 18) > (df_u32)v->row_size) {
            return 0;
        }
        sum += (df_u32)size * df_ru16(f + 18);
    }
    return sum <= (df_u32)v->row_size;
}

void rec_view_field(const RecView *v, int i, char *name, int *type, int *size, int *count)
{
    const df_u8 *f;
    int k;

    f = v->fields + i * FIELD_BYTES;
    for (k = 0; k < REC_NAME - 1 && f[k]; k++) {
        name[k] = (char)f[k];
    }
    name[k] = 0;
    *type = f[16];
    *size = f[17];
    *count = df_ru16(f + 18);
}

/* A number at p of a type and size, as an integer and as a float. */
static void read_number(const df_u8 *p, int type, int size, df_i32 *iv, float *fv)
{
    df_u32 bits;

    if (type == REC_FLOAT) {
        bits = df_ru32(p);
        mem_copy(fv, &bits, 4);
        *iv = df_iround(*fv);
        return;
    }
    if (size == 1) {
        *iv = type == REC_INT ? (df_i32)(signed char)p[0] : (df_i32)p[0];
    } else if (size == 2) {
        *iv = type == REC_INT ? (df_i32)df_ri16(p) : (df_i32)df_ru16(p);
    } else {
        *iv = (df_i32)df_ru32(p);
    }
    *fv = (float)*iv;
}

int rec_view_value(const RecView *v, int r, int i, int e, df_i32 *iv, float *fv, char *text, int cap)
{
    const df_u8 *f;
    const df_u8 *p;
    int type;
    int size;

    f = v->fields + i * FIELD_BYTES;
    type = f[16];
    size = f[17];
    p = v->rows + (df_u32)r * (df_u32)v->row_size + df_ru16(f + 20) + (df_u32)e * (df_u32)size;
    *iv = 0;
    *fv = 0.0f;
    if (type == REC_TEXT) {
        int k;

        for (k = 0; k < size && k < cap - 1 && p[k]; k++) {
            text[k] = (char)p[k];
        }
        if (cap > 0) {
            text[k] = 0;
        }
        return type;
    }
    read_number(p, type, size, iv, fv);
    return type;
}

int rec_table_init(RecTable *t, const RecSchema *s, int cap)
{
    t->schema = s;
    t->count = 0;
    t->cap = cap < 1 ? 1 : cap;
    t->rows = (df_u8 *)mem_keep((df_u32)t->cap * s->row_size);
    return t->rows != 0;
}

void rec_table_clear(RecTable *t)
{
    t->count = 0;
}

df_i32 rec_row_id(const RecTable *t, int i)
{
    const RecField *f;
    const df_u8 *row;
    df_i32 iv;
    float fv;

    f = &t->schema->field[0];
    row = t->rows + (df_u32)i * t->schema->row_size + f->offset;
    read_number(row, f->type, f->size, &iv, &fv);
    return iv;
}

void *rec_row(const RecTable *t, df_i32 id)
{
    int i;

    for (i = 0; i < t->count; i++) {
        if (rec_row_id(t, i) == id) {
            return t->rows + (df_u32)i * t->schema->row_size;
        }
    }
    return 0;
}

/* Room for one more row. Old rows stay where they were: the table is small and kept. */
static int grow(RecTable *t)
{
    df_u8 *rows;
    int cap;

    if (t->count < t->cap) {
        return 1;
    }
    cap = t->cap * 2;
    rows = (df_u8 *)mem_keep((df_u32)cap * t->schema->row_size);
    if (!rows) {
        return 0;
    }
    mem_copy(rows, t->rows, (df_u32)t->count * t->schema->row_size);
    t->rows = rows;
    t->cap = cap;
    return 1;
}

/* An integer into a field's element, clamped to its type's range. */
static void store_int(df_u8 *p, int type, int size, df_i32 v)
{
    df_i32 lo;
    df_i32 hi;

    if (size == 4) {
        if (type == REC_UINT && v < 0) {
            v = 0;
        }
        put32(p, (df_u32)v);
        return;
    }
    if (type == REC_INT) {
        lo = size == 1 ? -128 : -32768;
        hi = size == 1 ? 127 : 32767;
    } else {
        lo = 0;
        hi = size == 1 ? 255 : 65535;
    }
    v = v < lo ? lo : (v > hi ? hi : v);
    if (size == 1) {
        p[0] = (df_u8)v;
    } else {
        put16(p, (df_u32)v);
    }
}

/* Element e of the view's field fi in row r, into the schema's field tf of row dst. */
static void store(const RecView *v, int r, int fi, int e, const RecField *tf, df_u8 *dst)
{
    df_u8 *p;
    df_i32 iv;
    float fv;
    char text[256];
    int type;

    p = dst + tf->offset + (df_u32)e * tf->size;
    type = rec_view_value(v, r, fi, e, &iv, &fv, text, (int)sizeof(text));
    if (tf->type == REC_TEXT) {
        int k;

        for (k = 0; k < tf->size - 1 && text[k]; k++) {
            p[k] = (df_u8)text[k];
        }
        for (; k < tf->size; k++) {
            p[k] = 0;
        }
        return;
    }
    if (tf->type == REC_FLOAT) {
        mem_copy(p, &fv, 4);
        return;
    }
    (void)type;
    store_int(p, tf->type, tf->size, iv);
}

int rec_merge(RecTable *t, const RecChunk *c, RecMerge *st)
{
    const RecSchema *s;
    RecView v;
    int map[REC_FIELDS];
    int i;
    int r;

    s = t->schema;
    if (!rec_view(c, &v)) {
        return 0;
    }
    /* Each of the file's fields to the schema's field of that name and kind, or -1. */
    for (i = 0; i < v.nfield; i++) {
        char name[REC_NAME];
        int type;
        int size;
        int count;
        int k;

        rec_view_field(&v, i, name, &type, &size, &count);
        map[i] = -1;
        for (k = 0; k < s->nfield; k++) {
            if (df_eq(name, s->field[k].name)) {
                /* Numbers convert among themselves; text goes only to text. */
                if ((type == REC_TEXT) == (s->field[k].type == REC_TEXT)) {
                    map[i] = k;
                }
                break;
            }
        }
        if (map[i] < 0 && st) {
            st->skipped++;
        }
    }
    if (map[0] != 0 || s->field[0].type == REC_TEXT || s->field[0].type == REC_FLOAT) {
        return 0;
    }
    for (r = 0; r < v.nrow; r++) {
        df_u8 *row;
        df_i32 id;
        float fv;
        char none[1];

        rec_view_value(&v, r, 0, 0, &id, &fv, none, 1);
        row = (df_u8 *)rec_row(t, id);
        if (row) {
            if (st) {
                st->changed++;
            }
        } else {
            if (!grow(t)) {
                return 0;
            }
            row = t->rows + (df_u32)t->count * s->row_size;
            if (s->defaults) {
                mem_copy(row, s->defaults, s->row_size);
            } else {
                mem_zero(row, s->row_size);
            }
            t->count++;
            if (st) {
                st->added++;
            }
        }
        for (i = 0; i < v.nfield; i++) {
            const RecField *tf;
            char name[REC_NAME];
            int type;
            int size;
            int count;
            int e;

            if (map[i] < 0) {
                continue;
            }
            tf = &s->field[map[i]];
            rec_view_field(&v, i, name, &type, &size, &count);
            if (tf->type == REC_TEXT) {
                store(&v, r, i, 0, tf, row);
                continue;
            }
            /* An array: the elements the file carries, up to the schema's. */
            for (e = 0; e < count && e < tf->count; e++) {
                store(&v, r, i, e, tf, row);
            }
        }
    }
    return 1;
}
