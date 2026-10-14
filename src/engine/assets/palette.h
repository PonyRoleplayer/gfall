#ifndef ENG_PALETTE_H
#define ENG_PALETTE_H

#include "engine/core/df.h"

/* 256 RGB entries. Index 0 is the cutout colour when a texture asks for it. */

typedef struct Palette {
    df_u8 r[256];
    df_u8 g[256];
    df_u8 b[256];
} Palette;

#endif
