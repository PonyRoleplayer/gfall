#ifndef DF_MEM_H
#define DF_MEM_H

#include "engine/core/df.h"

/* One arena, live until process exit. No per-frame allocation. */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

void mem_init(void);
void mem_shutdown(void);
void *mem_alloc(df_u32 bytes);

static void mem_copy(void *dst, const void *src, df_u32 n)
{
    df_u32 i;
    df_u8 *d;
    const df_u8 *s;

    d = (df_u8 *)dst;
    s = (const df_u8 *)src;
    for (i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static void mem_zero(void *dst, df_u32 n)
{
    df_u32 i;
    df_u8 *d;

    d = (df_u8 *)dst;
    for (i = 0; i < n; i++) {
        d[i] = 0;
    }
}

#pragma GCC diagnostic pop

#endif
