#ifndef DF_PALETTE_H
#define DF_PALETTE_H

#include "engine/assets/palette.h"

/* 776-byte COL: int32 length, uint16 0xB123, uint16 pad, then 256 RGB bytes. */

int palette_load(const df_u8 *bytes, df_u32 size, Palette *out);

#endif
