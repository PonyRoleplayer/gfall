#ifndef DATFALL_VIEW_H
#define DATFALL_VIEW_H

#include "engine/core/df.h"

/*
 * One parsed copy of a record file, for DATFALL (docs/data.md).
 * Chunks, fields and values come from recfile's reader. A kind's
 * table name comes from maindat when the tag is one of its tables,
 * and from rec_head_schema for HEAD. The bytes are copied into a
 * heap dat_close frees. Call mem_init before dat_open on the target;
 * dat_open does that itself.
 *
 * A list is not built here. dat_filter remembers which rows match,
 * and the window draws the ones on screen.
 */

/* A file larger than this is refused. */
#define DAT_MAX_BYTES (16u * 1024u * 1024u)

typedef struct DatFile DatFile;

/* 0 when there is no memory. A bad file still returns a DatFile: dat_ok is 0. */
DatFile *dat_open(const df_u8 *bytes, df_u32 n);
void dat_close(DatFile *f);

int dat_ok(const DatFile *f);
/* Why dat_ok is 0. Empty when the file is a record file. */
const char *dat_error(const DatFile *f);
/* A chunk past the end, after the kinds that did read. Empty when the file is whole. */
const char *dat_warning(const DatFile *f);
df_u32 dat_size(const DatFile *f);
/* REC_FORMAT when the bytes are a record file, else 0. */
int dat_version(const DatFile *f);

int dat_kinds(const DatFile *f);
/* The kind whose tag is this, or -1. */
int dat_find_tag(const DatFile *f, const char *tag);
void dat_kind(const DatFile *f, int kind, char *tag, int tagcap, char *table, int tablecap);
int dat_rows(const DatFile *f, int kind);
int dat_fields(const DatFile *f, int kind);
int dat_row_bytes(const DatFile *f, int kind);
df_u32 dat_chunk_at(const DatFile *f, int kind);
df_u32 dat_chunk_size(const DatFile *f, int kind);
/* 1 when the chunk is not a record chunk rec_view accepts. */
int dat_damaged(const DatFile *f, int kind);

/* Field i: name, type ('i','u','f','s'), one element's bytes, and the count. 0 if out of range. */
int dat_field(const DatFile *f, int kind, int field, char *name, int namecap,
              int *type, int *size, int *count);
/* Element e of that field, as text. 0 if out of range. */
int dat_value(const DatFile *f, int kind, int row, int field, int elem, char *dst, int cap);
/* The same elements joined by ", ", cut to cap. */
void dat_values(const DatFile *f, int kind, int row, int field, char *dst, int cap);
/* Field 0 as text, and the first text field (a row's label). */
void dat_row_label(const DatFile *f, int kind, int row, char *id, int idcap, char *label, int labelcap);

/*
 * Rows of this kind whose id or any text field contains q, case folded.
 * An empty q is every row. dat_filter_row is the file's row index.
 * One index for the file, replaced by the next call.
 */
int dat_filter(DatFile *f, int kind, const char *q);
int dat_filter_count(const DatFile *f);
int dat_filter_row(const DatFile *f, int i);

/*
 * The kind to select for a typed prefix: a tag or table name that
 * starts with q, or else one that contains it. Empty q is kind 0.
 * -1 when nothing matches.
 */
int dat_kind_pick(const DatFile *f, const char *q);

#endif
