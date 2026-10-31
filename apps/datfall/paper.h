#ifndef DATFALL_PAPER_H
#define DATFALL_PAPER_H

#include "view.h"

/*
 * A race's paper doll, named by its RACE record (docs/data.md). The
 * seven parts are the record's own file fields, so a mod that points
 * a race at another picture shows that picture. A head file is a CIF;
 * paper_heads is how many frames it holds.
 */

#define PAPER_PARTS 7
#define PAPER_NAME 16

const char *paper_field(int part);
const char *paper_label(int part);
/* The part's file name, copied into dst. 0 when the row has no such field. */
int paper_file(const DatFile *f, int row, int part, char *dst, int cap);
/* Frames in that CIF, or 0 when it does not open. */
int paper_heads(const char *name);

#endif
