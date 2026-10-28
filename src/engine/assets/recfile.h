#ifndef ENG_RECFILE_H
#define ENG_RECFILE_H

#include "engine/core/df.h"

/*
 * Record files: a game's tables as small binary files, read and merged
 * in a load order. Read docs/data.md before editing.
 *
 * A file is "GDAT", a u32 format version, then chunks to its end: a
 * 4-character tag, a u32 size, the data, and a pad byte to an even
 * size, as RIFF lays them out. A record chunk describes itself: a
 * header, its fields by name, type and size, then fixed-size rows. So a
 * tool dumps any chunk without knowing the game, a newer program reads
 * an older file (a field the file lacks keeps its value), and a file
 * may carry only some of a table's fields: a mod's rat that sets its
 * health alone.
 *
 * The game describes each table once, as a RecSchema over its own row
 * struct. Its first field is the id the rows are keyed by. Writing,
 * merging and a tool's text all go through that schema.
 *
 * Record chunk:
 *   u16 1, u16 fields, u16 row size, u16 0, u32 rows
 *   per field: char name[16], u8 type, u8 size, u16 count, u16 offset, u16 0
 *   rows, each row size bytes, little-endian
 *
 * Plain C, no I/O: bytes come in from memory and go out through a
 * RecPut. hostcheck runs it.
 */

#define REC_MAGIC "GDAT"
#define REC_FORMAT 1
#define REC_NAME 16
/* A chunk's fields at most. */
#define REC_FIELDS 48

/* A field's type: a signed or unsigned integer, a float, or text. */
enum {
    REC_INT = 'i',
    REC_UINT = 'u',
    REC_FLOAT = 'f',
    REC_TEXT = 's'
};

typedef struct RecField {
    /* Under REC_NAME characters, as a tool's text spells it. */
    const char *name;
    df_u8 type;
    /* One element's bytes: 1, 2 or 4 for a number (4 for a float), the char array's for text. */
    df_u8 size;
    /* Elements: 1 for a scalar, more for an array of numbers. */
    df_u16 count;
    /* The first element's offset in the row struct. */
    df_u16 offset;
} RecField;

/* Builds a RecField over a member of a row struct. */
#define REC_FIELD(name, type, row, member) \
    { name, type, (df_u8)sizeof(((row *)0)->member), 1, (df_u16)DF_OFFSETOF(row, member) }
#define REC_ARRAY(name, type, row, member) \
    { name, type, (df_u8)sizeof(((row *)0)->member[0]), \
      (df_u16)(sizeof(((row *)0)->member) / sizeof(((row *)0)->member[0])), (df_u16)DF_OFFSETOF(row, member) }
#define DF_OFFSETOF(row, member) ((unsigned long)&((row *)0)->member)

typedef struct RecSchema {
    /* The chunk's tag, four characters. */
    const char *tag;
    /* The table's name in a tool's text: [enemy 200]. */
    const char *table;
    /* field[0] is the id: an integer, count 1. */
    const RecField *field;
    int nfield;
    df_u16 row_size;
    /* A new id's row before a file's fields: these values, or zeros. */
    const void *defaults;
} RecSchema;

/* Rows of one schema, kept for the process (mem_keep). */
typedef struct RecTable {
    const RecSchema *schema;
    df_u8 *rows;
    int count;
    int cap;
} RecTable;

/* Writes n bytes; 0 on failure. */
typedef int (*RecPut)(void *ctx, const void *src, df_u32 n);

/* A file's header. */
int rec_put_begin(RecPut put, void *ctx);
/*
 * One record chunk of n rows laid out as the schema's struct. fields
 * lists the schema's field indices to carry (field 0, the id, always
 * first), or 0 for all of them.
 */
int rec_put_rows(RecPut put, void *ctx, const RecSchema *s, const void *rows, int n,
                 const int *fields, int nfields);

/* A chunk as it lies in the bytes. */
typedef struct RecChunk {
    char tag[5];
    const df_u8 *data;
    df_u32 size;
} RecChunk;

/* 1 when the bytes start as a record file this code reads. */
int rec_is_file(const df_u8 *bytes, df_u32 n);
/*
 * The chunk at *pos (start at 0), and *pos moved past it. 0 at the end
 * or when a chunk runs past the bytes.
 */
int rec_next(const df_u8 *bytes, df_u32 n, df_u32 *pos, RecChunk *out);

/* A record chunk's header, read and checked. */
typedef struct RecView {
    int nfield;
    int row_size;
    int nrow;
    const df_u8 *fields;
    const df_u8 *rows;
} RecView;
int rec_view(const RecChunk *c, RecView *v);
/* Field i of a view: its name (copied, terminated), type, size and count. */
void rec_view_field(const RecView *v, int i, char *name, int *type, int *size, int *count);
/*
 * Element e of field i in row r: an integer in *iv (a float rounded), a
 * float in *fv, or text in text (cap bytes). Returns the field's type.
 */
int rec_view_value(const RecView *v, int r, int i, int e, df_i32 *iv, float *fv, char *text, int cap);

/* Statistics of the merges into a table. */
typedef struct RecMerge {
    int added;
    int changed;
    /* Fields a file carried that the schema does not have, or of another kind. */
    int skipped;
} RecMerge;

/* An empty table with room for cap rows. 0 when there is no memory. */
int rec_table_init(RecTable *t, const RecSchema *s, int cap);
void rec_table_clear(RecTable *t);
/*
 * A record chunk of t's tag into t, by id: a new id adds a row (the
 * schema's defaults, then the file's fields), an existing one takes the
 * file's fields alone. Numbers are converted to the schema's type and
 * clamped to its range; text is cut to its size. 0 for a chunk that is
 * not a valid record chunk, or whose first field is not an integer id.
 */
int rec_merge(RecTable *t, const RecChunk *c, RecMerge *st);
/* The row with this id, or 0. */
void *rec_row(const RecTable *t, df_i32 id);
/* Row i's id. */
df_i32 rec_row_id(const RecTable *t, int i);

/*
 * The header chunk every file carries, HEAD: who the file is, what it
 * was made from, and what must load before it.
 */
enum {
    /* This file: name, and size is the version of what made it. */
    REC_HEAD_SELF = 0,
    /* A file it was made from, and that file's size then. */
    REC_HEAD_SOURCE = 1,
    /* A file that must be merged before this one. */
    REC_HEAD_NEEDS = 2
};
typedef struct RecHead {
    df_u16 id;
    df_u8 role;
    df_u32 size;
    char name[REC_NAME];
} RecHead;
extern const RecSchema rec_head_schema;

#endif
