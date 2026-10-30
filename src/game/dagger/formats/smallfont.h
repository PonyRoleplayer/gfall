#ifndef DF_SMALLFONT_H
#define DF_SMALLFONT_H

#include "engine/core/df.h"

/*
 * One of classic's FONT####.FNT faces drawn into a picture in memory,
 * not onto the card. A screen composed in software (play/bag.c) writes
 * its labels with this, at classic's own pixels, so they look as classic's
 * do and cost no draw call. fnt.c is the other reader: it builds the
 * glyph sheet the card's text uses, and it needs Win32; this one is plain
 * C, so hostcheck runs it.
 *
 * The file: a u16 cell width and a u16 height, then 240 records of a u16
 * offset and a u16 glyph width. A glyph is height rows of a little-endian
 * u16, bit 15 its leftmost pixel. Record 0 is '!'.
 */

typedef struct SmallFont {
    /* The file's bytes, in the heap current when it was loaded; 0 when none. */
    const df_u8 *bytes;
    df_u32 size;
    int height;
} SmallFont;

/* FONT0003.FNT, say. 0 when it is missing or too short. */
int smallfont_load(SmallFont *font, const char *name);
/* The width of text on one line, in pixels. */
int smallfont_width(const SmallFont *font, const char *text);
/*
 * Text with its top left at x, y in a picture of w x h palette indices,
 * clipped to it. ink is the palette index of the letters. shadow, when not
 * negative, is the index of a copy one pixel down and right, drawn first,
 * as classic's labels have.
 */
void smallfont_draw(const SmallFont *font, df_u8 *pixels, int w, int h, int x, int y, int ink, int shadow,
                    const char *text);

#endif
