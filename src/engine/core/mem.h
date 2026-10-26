#ifndef DF_MEM_H
#define DF_MEM_H

#include "engine/core/df.h"

/* One arena, live until process exit. No per-frame allocation. */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

void mem_init(void);
void mem_shutdown(void);
void *mem_alloc(df_u32 bytes);

/*
 * A second heap for one model. Allocations after mem_temp_begin die in
 * mem_temp_end. The directory and the palette stay on the main heap.
 * Host builds keep a single malloc heap; rewind is a no-op there.
 */
int mem_temp_begin(void);
void mem_temp_end(void);
/*
 * Changes whenever a temp heap begins or ends, on the host too. A
 * module that keeps a buffer from mem_alloc to reuse later records the
 * epoch with it, and allocates again when the epoch has moved: the
 * buffer may have died with a place's heap. light.c reused its decal
 * buffers into the next place's build and wrote through freed pages.
 */
df_u32 mem_epoch(void);

/*
 * Always the main heap, even between mem_temp_begin and mem_temp_end.
 * For a buffer allocated on first use and kept for the process, such
 * as the console's frame grab: LEVELFALL and RENDERFALL draw inside a
 * temp heap, and mem_alloc there would die with the place.
 */
void *mem_keep(df_u32 bytes);

/*
 * Bytes allocated so far on the main heap, and on the temp heap since
 * mem_temp_begin. A failed allocation logs an error with its size.
 */
void mem_stats(df_u32 *main_bytes, df_u32 *temp_bytes);

/*
 * Heaps that free (docs/streaming.md): a streamed world loads and drops
 * chunks for as long as it runs, so what a chunk alone owns lives in a
 * heap of its own, destroyed with it, and what chunks share (a picture,
 * a mesh) is freed piece by piece by a track of its allocations.
 *
 * mem_heap_use points mem_alloc at heap h (0 goes back to the temp or
 * main heap) and returns the heap it replaces, so calls nest. mem_keep
 * and mem_scratch never go there. Creating or destroying a heap does not
 * move mem_epoch: a chunk coming or going is not a new place.
 */
typedef struct MemHeap MemHeap;
MemHeap *mem_heap_new(void);
void mem_heap_delete(MemHeap *h);
MemHeap *mem_heap_use(MemHeap *h);
/* Free one allocation made in h. */
void mem_heap_free(MemHeap *h, void *p);
/* Bytes allocated in h and not freed. */
df_u32 mem_heap_bytes(const MemHeap *h);
/*
 * While a track is set, each allocation made in the heap mem_heap_use
 * had set when the track began (not in one a callee sets inside it) is
 * written to list, up to cap; *count counts them all, past cap
 * too (an overflowing track cannot free everything: its owner checks).
 * 0 for list ends it.
 */
void mem_track(void **list, int cap, int *count);

/*
 * A buffer for a record that is parsed and then discarded. slot is 0 or 1,
 * so a block file and a model record can be live together. The pointer is
 * invalid after the next call on that slot. On Win32 it also dies with the
 * temp heap. The old buffer is abandoned when the slot has to grow.
 */
void *mem_scratch(int slot, df_u32 bytes);

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
