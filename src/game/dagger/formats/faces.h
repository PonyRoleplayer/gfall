#ifndef DF_FACES_H
#define DF_FACES_H

#include "engine/core/df.h"

/*
 * The faces people talk with: TFAC00I0.RCI, 64 x 64 pictures one after another
 * under ART_PAL.COL with no header, and FLATS.CFG, which says which of them
 * goes with a flat. Read docs/game.md (People) before editing.
 *
 * FLATS.CFG is blocks of lines with a blank line between: "archive record",
 * a caption, a gender, two numbers the game has no use for, then the face.
 * The first of a flat wins, as in DFU's FlatsFile.
 *
 * Plain C: hostcheck runs it.
 */

#define FACE_W 64
#define FACE_H 64
/* The record classic's own art left as a note to its artist, drawn for a flat with no face: DFU falls back to it too. */
#define FACE_NONE 410

/* The face of a flat; FACE_NONE for one FLATS.CFG does not name. */
int faces_of_flat(int archive, int record);
/* Face record's pixels into out, FACE_W x FACE_H. 0 when it does not read. */
int faces_read(int record, df_u8 *out);

#endif
