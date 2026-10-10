#include "assets/palette.h"

int palette_load(const df_u8 *bytes, df_u32 size, Palette *out)
{
    int i;

    if (!bytes || !out || size < 8 + 768) {
        return 0;
    }
    if (df_ru32(bytes) != 776 || df_ru16(bytes + 4) != 0xb123) {
        return 0;
    }
    for (i = 0; i < 256; i++) {
        out->r[i] = bytes[8 + i * 3];
        out->g[i] = bytes[8 + i * 3 + 1];
        out->b[i] = bytes[8 + i * 3 + 2];
    }
    return 1;
}
