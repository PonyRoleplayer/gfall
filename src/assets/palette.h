#ifndef DF_PALETTE_H
#define DF_PALETTE_H

#include "df.h"

/* 776-byte COL: int32 length, uint16 0xB123, uint16 pad, then 256 RGB bytes. */

typedef struct Palette {
    df_u8 r[256];
    df_u8 g[256];
    df_u8 b[256];
} Palette;

int palette_load(const df_u8 *bytes, df_u32 size, Palette *out);

#endif
