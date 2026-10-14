#ifndef ENG_TEXTURE_H
#define ENG_TEXTURE_H

#include "engine/core/df.h"

/*
 * One image the rasterizer can upload.
 * index[] is the source picture, row 0 at the top, width*height bytes.
 * pot[] is that picture scaled to a power of two, then repeated along the
 * short side until the map is square. A Voodoo 1 samples a square, and a
 * coordinate in [0, 1] is one texel, so the repeat is what keeps the face
 * from collapsing to a solid colour.
 * span_w and span_h are the size before that repeat. One copy of the
 * picture occupies span_w/pot_w of s and span_h/pot_h of t.
 * pot_w and pot_h are equal, powers of two, and at most 256.
 * xscale and yscale are optional source scales. Billboards may use them.
 * The triangle path does not.
 */

typedef struct Image {
    int width;
    int height;
    int pot_w;
    int pot_h;
    int span_w;
    int span_h;
    int xscale;
    int yscale;
    df_u8 *index;
    df_u8 *pot;
} Image;

#endif
