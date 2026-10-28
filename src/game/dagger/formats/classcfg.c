#include "game/dagger/formats/classcfg.h"

/* See classcfg.h. */

int classcfg_read(const df_u8 *b, df_u32 n, ClassCfg *out)
{
    int i;

    if (!b || n < CLASSCFG_BYTES) {
        return 0;
    }
    out->resist = b[0];
    out->immune = b[1];
    out->low_tolerance = b[2];
    out->weakness = b[3];
    out->abilities = df_ru16(b + 4);
    out->rapid_healing = b[6];
    out->regeneration = b[7];
    out->absorption = b[9];
    out->attack_modifier = b[10];
    out->forbidden_materials = df_ru16(b + 11);
    out->weapons = ((df_u32)b[13] << 16) | ((df_u32)b[15] << 8) | (df_u32)b[14];
    for (i = 0; i < 3; i++) {
        out->primary[i] = b[16 + i];
        out->major[i] = b[19 + i];
    }
    for (i = 0; i < 6; i++) {
        out->minor[i] = b[22 + i];
    }
    for (i = 0; i < 15 && b[28 + i]; i++) {
        out->name[i] = (char)b[28 + i];
    }
    for (; i < 16; i++) {
        out->name[i] = 0;
    }
    out->hp_per_level = df_ru16(b + 52);
    out->advancement = df_ru32(b + 54);
    for (i = 0; i < 8; i++) {
        out->attribute[i] = df_ru16(b + 58 + i * 2);
    }
    return 1;
}

float classcfg_magicka(df_u16 abilities)
{
    switch ((abilities & 0x1C00u) >> 8) {
    case 4:
        return 2.0f;
    case 8:
        return 1.75f;
    case 12:
        return 1.5f;
    case 16:
        return 1.0f;
    case 20:
        return 0.5f;
    default:
        return 3.0f;
    }
}

void classcfg_name(int index, char *dst)
{
    static const char base[] = "CLASS00.CFG";
    int i;

    for (i = 0; base[i]; i++) {
        dst[i] = base[i];
    }
    dst[i] = 0;
    dst[5] = (char)('0' + index / 10);
    dst[6] = (char)('0' + index % 10);
}
