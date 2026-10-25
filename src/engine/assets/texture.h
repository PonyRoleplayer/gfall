#ifndef ENG_TEXTURE_H
#define ENG_TEXTURE_H

#include "engine/core/df.h"

/*
 * One image the rasterizer can upload.
 * index[] is the source picture, row 0 at the top, width*height bytes.
 * pot[] is the square the card gets. For a wall it is the picture
 * stretched to powers of two, then repeated along the short side until
 * the map is square, so the face can tile it. For a sprite it is the
 * picture at its own size in the top-left corner, index 0 around it.
 * span_w and span_h are the size of one copy: the stretched size for a
 * wall, width and height for a sprite. One copy occupies span_w/pot_w
 * of s and span_h/pot_h of t.
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
    /*
     * An animated picture's frames, laid in pot as a sheet: frame k at
     * column k % frame_cols, row k / frame_cols, frame_dx by frame_dy
     * texels apart. width and height are one frame's. 0 or 1 is a still
     * picture. See docs/sprites.md.
     */
    int frames;
    int frame_cols;
    int frame_dx;
    int frame_dy;
} Image;

#endif
