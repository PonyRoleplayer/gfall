#ifndef DATFALL_PICS_H
#define DATFALL_PICS_H

#include "engine/core/df.h"

/*
 * TEXTURE.### archives in the mounted ARENA2 (docs/data.md). The count
 * is the file's own header, including TEXTURE.000 and TEXTURE.001,
 * which the engine's texture_record_count leaves out. out may be null
 * to count. A cap that is short still returns the full count.
 */

#define TEX_SCAN 512

typedef struct TexArc {
    int file;
    int records;
} TexArc;

int tex_list(TexArc *out, int cap);

#endif
