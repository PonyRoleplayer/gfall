#ifndef DATFALL_BOOKS_H
#define DATFALL_BOOKS_H

#include "view.h"

/*
 * The .DAT files DATFALL shows (docs/data.md). One slot is one file as
 * it lies on disk. The merged book is built again from those bytes with
 * rec_view, not from the game's C structs: a mod's new table, or a new
 * field on an old one, shows up. Rows are keyed by field 0 (the id). A
 * later file overlays the fields it carries. A field it does not carry
 * stays. A number converts into the first file's type and size; text
 * and numbers do not cross. A new row is zeros where the file that
 * added it said nothing (the browser has no schema defaults).
 *
 * Call mem_init first on the target. books_open does that itself.
 */

#define BOOKS_MAX 16
/* A LOADORDR name, the same 15 characters maindat keeps. */
#define BOOKS_NAME 16

typedef struct Books Books;

/* names[i] may be null. bytes[i] null is a missing file: listed, not merged. */
Books *books_open(const char **names, const df_u8 **bytes, const df_u32 *sizes, int n);
void books_close(Books *b);

int books_count(const Books *b);
const char *books_name(const Books *b, int i);
/* 0 when that slot was missing or not a record file. */
int books_ok(const Books *b, int i);
DatFile *books_file(Books *b, int i);
DatFile *books_merged(Books *b);
/* The names that merged, for the status bar. */
const char *books_status(const Books *b);
/* 1 when a table had more rows than the merge keeps. */
int books_cut(const Books *b);

/*
 * LOADORDR.TXT: one name a line, blanks and # lines skipped, MAIN.DAT
 * skipped (it is always the first file). Returns how many were kept.
 */
int books_order(const df_u8 *text, df_u32 n, char names[][BOOKS_NAME], int cap);

#endif
