#ifndef ENG_FONT_H
#define ENG_FONT_H

#include "engine/core/df.h"

/*
 * Looks for FONT0000.FNT. When those glyphs are present, builds a
 * 256x256 sheet and writes font0000.bmp beside the exe if it is missing.
 * Text then uses that sheet. With no glyphs, text uses the built-in 8x8.
 */

int font_ensure(void);
const char *font_error(void);
int font_ready(void);
int font_generated(void);

/* After the level palette is on the TMU. Missing glyphs are not an error. */
void font_upload(const df_u8 *r, const df_u8 *g, const df_u8 *b);

int ui_line_height(void);
int ui_string_width(const char *s);
/* top is the top of the line. Origin is the lower left of the screen. */
void ui_string(int x, int top, const char *s);

#endif
