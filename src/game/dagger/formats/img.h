#ifndef DF_IMG_H
#define DF_IMG_H

#include "engine/assets/palette.h"

/*
 * Daggerfall's IMG pictures: the screens and the HUD's pieces (the
 * compass, its box). Read through the VFS from ARENA2.
 *
 * Most start with a 12-byte header: x, y (where it sits on the 320 x 200
 * screen), width, height, compression, pixel bytes, then the pixels a
 * row at a time, top first. Some have no header and are known by their
 * size alone (COMPASS.IMG is 4508 bytes: 322 x 14), as Daggerfall
 * Unity's ImgFile lists them. Their palette is ART_PAL.COL but for a
 * few: six full screens carry their own after the pixels, 768 bytes of
 * 6-bit values (CHGN00I0, DIE_00I0, PICK02I0, PICK03I0, PRIS00I0,
 * TITL00I0: ImgPicture.palette), and FMAP*, NITE*, DANK02I0 and
 * TMAP00I0 use other COLs, not read here yet.
 *
 * Only uncompressed pictures are read for now; a compressed one fails
 * with a log line.
 */

typedef struct ImgPicture {
    int x;
    int y;
    int width;
    int height;
    /* width * height indices, row 0 at the top: the VFS read's bytes, live until exit. */
    const df_u8 *pixels;
    /* The palette it carries, 768 bytes of 0..63, or 0 for ART_PAL.COL's (img_palette). */
    const df_u8 *palette;
} ImgPicture;

/* An IMG from ARENA2 by name. 0 when it is missing, compressed, or short. */
int img_load(const char *name, ImgPicture *out);
/* ART_PAL.COL, the palette of most IMGs, as 768 RGB bytes. 0 when it does not read. */
int img_art_palette(df_u8 *rgb768);
/* A picture's own palette as 768 RGB bytes (its 6-bit values times 4), or ART_PAL.COL's. */
int img_palette(const ImgPicture *pic, df_u8 *rgb768);
/*
 * A RAW picture of known size with no header (ARROW.RAW, the pointer:
 * 10 x 10). 0 when it is missing or short.
 */
int img_load_raw(const char *name, int width, int height, ImgPicture *out);

#endif
