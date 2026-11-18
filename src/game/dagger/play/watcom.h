#ifndef GFALL_WATCOM_H
#define GFALL_WATCOM_H

#include "engine/core/df.h"

/* Watcom's rand, code at 0x9DC25: seed * MUL + ADD, the high word's low 15 bits; one stream for creation and play. */
/* rand() is 0..RAND_MAX_WATCOM. */
#define RAND_MAX_WATCOM 32767
#define WATCOM_RAND_MUL 1103515245u
#define WATCOM_RAND_ADD 12345u
/* One step of Watcom's rand on a seed of its own: the new seed, and the value (0..RAND_MAX_WATCOM) it gives. */
static __inline__ unsigned watcom_next(df_u32 *seed)
{
    *seed = *seed * WATCOM_RAND_MUL + WATCOM_RAND_ADD;
    return (*seed >> 16) & (df_u32)RAND_MAX_WATCOM;
}

#endif
