#include "game/dagger/formats/spells.h"

/* See spells.h. */

int stdspell_read(const df_u8 *bytes, df_u32 size, StdSpell *out, int cap)
{
    int n;
    int i;
    int k;

    n = 0;
    while ((df_u32)(n + 1) * STDSPELL_BYTES <= size && n < cap) {
        const df_u8 *b;
        StdSpell *s;

        b = bytes + n * STDSPELL_BYTES;
        s = &out[n];
        for (i = 0; i < STDSPELL_EFFECTS; i++) {
            s->group[i] = b[2 * i];
            s->sub[i] = b[2 * i + 1];
            for (k = 0; k < 3; k++) {
                s->duration[i][k] = b[14 + 3 * i + k];
                s->chance[i][k] = b[23 + 3 * i + k];
            }
            for (k = 0; k < 5; k++) {
                s->magnitude[i][k] = b[32 + 5 * i + k];
            }
        }
        s->element = b[6];
        s->target = b[7];
        s->price = (df_u16)(b[8] | (b[9] << 8));
        for (k = 0; k < STDSPELL_NAME - 1 && k < 25 && b[47 + k]; k++) {
            s->name[k] = (char)b[47 + k];
        }
        s->name[k] = 0;
        s->icon = b[72];
        s->number = b[73];
        n++;
    }
    return n;
}
