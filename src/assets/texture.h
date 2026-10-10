#ifndef DF_TEXTURE_H
#define DF_TEXTURE_H

#include "df.h"

/*
 * One TEXTURE.### image, frame 0.
 * index[] is the file image, row 0 at the top, width*height bytes.
 * pot[] is that image nearest-scaled to a power of two, then repeated
 * along the short side until the map is square. A 1x1 upload samples
 * correctly. A 64x32 wall, uploaded as 2x1, comes back as vertical bars.
 * span_w and span_h are the size before that repeat, so one copy of the
 * picture occupies span_w/pot_w of s and span_h/pot_h of t.
 * pot_w and pot_h are equal, powers of two, and at most 256.
 * TEXTURE.000 and TEXTURE.001 are solid colour sets, 32x32.
 */

typedef struct Image {
    int width;
    int height;
    int pot_w;
    int pot_h;
    int span_w;
    int span_h;
    /* Signed texture scales. A flat's world size is pixels + pixels * scale / 256. */
    int xscale;
    int yscale;
    df_u8 *index;
    df_u8 *pot;
} Image;

void texture_filename(int file, char *dst, int n);
Image *texture_load(int file, int image);

#endif
