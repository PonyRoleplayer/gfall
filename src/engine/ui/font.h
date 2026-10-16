#ifndef ENG_FONT_H
#define ENG_FONT_H

#include "engine/core/df.h"

/*
 * Screen text. A sheet is a square index image, row 0 at the top.
 * Glyph i (character code first+i) sits in cell i of a grid of
 * cell-pixel squares. widths[i] is the ink width. gap is the advance
 * for a missing glyph. pixels stays valid until shutdown.
 * Call font_set before font_upload to replace the engine face.
 * With no sheet, font_upload rasterizes the 8x8 Terminal strike
 * (OEM charset, not the 8x12 stock OEM face). The built-in 8x8 face
 * draws when that raster fails. A null sheet clears back to that path.
 */

#define FONT_MAX 256

typedef struct FontSheet {
    const df_u8 *pixels;
    int side;
    int cell;
    int height;
    const df_u8 *widths;
    int nwidths;
    int first;
    int gap;
} FontSheet;

int font_set(const FontSheet *sheet);

/* Uploads the sheet. No sheet installs the 8x8 Terminal face, then uploads it. */
void font_upload(const df_u8 *r, const df_u8 *g, const df_u8 *b);

int ui_line_height(void);
int ui_string_width(const char *s);
/* top is the top of the line. Origin is the lower left of the screen. */
void ui_string(int x, int top, const char *s);

#endif
