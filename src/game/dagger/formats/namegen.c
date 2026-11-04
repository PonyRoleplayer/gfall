#include "game/dagger/formats/namegen.h"

/* See namegen.h. */

#define BANK_BYTES (NAMEGEN_SETS * 8)

int namegen_read(const df_u8 *bytes, df_u32 n, NamegenPiece *out, int cap)
{
    int count;
    int bank;

    if (!bytes || n < (df_u32)(NAMEGEN_BANKS * BANK_BYTES)) {
        return 0;
    }
    count = 0;
    for (bank = 0; bank < NAMEGEN_BANKS; bank++) {
        const df_u8 *head;
        int set;

        head = bytes + bank * BANK_BYTES;
        for (set = 0; set < NAMEGEN_SETS; set++) {
            df_u32 offset;
            df_u32 pieces;
            df_u32 i;

            offset = df_ru32(head + set * 4);
            pieces = df_ru32(head + NAMEGEN_SETS * 4 + set * 4);
            if (!pieces || offset + pieces * NAMEGEN_PIECE > n) {
                continue;
            }
            for (i = 0; i < pieces && count < cap; i++) {
                const df_u8 *text;
                int k;

                text = bytes + offset + i * NAMEGEN_PIECE;
                out[count].bank = bank;
                out[count].set = set;
                out[count].index = (int)i;
                for (k = 0; k < NAMEGEN_PIECE && text[k]; k++) {
                    out[count].text[k] = (char)text[k];
                }
                out[count].text[k] = 0;
                count++;
            }
        }
    }
    return count;
}
