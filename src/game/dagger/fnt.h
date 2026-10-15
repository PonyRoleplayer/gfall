#ifndef DF_FNT_H
#define DF_FNT_H

/*
 * FONT0000.FNT. Builds a glyph sheet and hands it to the engine.
 * Writes font0000.bmp beside the exe when that file is missing.
 * A missing font is not fatal: the engine keeps the built-in face.
 */

int fnt_install(void);
const char *fnt_error(void);
int fnt_ready(void);
int fnt_generated(void);

#endif
