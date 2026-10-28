#ifndef DF_FNT_H
#define DF_FNT_H

/*
 * FONT0000.FNT. Builds a glyph sheet and hands it to the engine,
 * replacing the OEM face the engine would use on its own.
 * Writes font0000.bmp beside the exe when that file is missing.
 * A missing font is not fatal: the engine keeps its own face.
 */

int fnt_install(void);
const char *fnt_error(void);
int fnt_ready(void);
int fnt_generated(void);

#endif
