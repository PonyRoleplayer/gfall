#include "engine/core/mem.h"
#include "engine/core/log.h"

#define SCRATCH_SLOTS 2

/* Bytes handed out, for the console's mem command. Not freed bytes. */
static df_u32 g_main_bytes;
static df_u32 g_temp_bytes;
/* Moves on at every mem_temp_begin and mem_temp_end. See mem_epoch. */
static df_u32 g_epoch;

df_u32 mem_epoch(void)
{
    return g_epoch;
}

void mem_stats(df_u32 *main_bytes, df_u32 *temp_bytes)
{
    if (main_bytes) {
        *main_bytes = g_main_bytes;
    }
    if (temp_bytes) {
        *temp_bytes = g_temp_bytes;
    }
}

static void *counted(void *p, df_u32 bytes, int temp)
{
    if (!p) {
        log_print(LOG_ERROR, "out of memory: %u bytes on the %s heap",
                  (unsigned int)bytes, temp ? "temp" : "main");
        return 0;
    }
    if (temp) {
        g_temp_bytes += bytes;
    } else {
        g_main_bytes += bytes;
    }
    return p;
}

/* The heap mem_alloc is pointed at, 0 for the temp or main heap; and the track. */
static MemHeap *g_use;
static void **g_track;
static int g_track_cap;
static int *g_track_count;
/* The heap the track was set in: a nested heap's allocations are not its (texture.c's files). */
static MemHeap *g_track_heap;

/* mem_alloc as if no heap were set: the temp heap, else the main heap. */
static void *base_alloc(df_u32 bytes);

static void track(void *p)
{
    if (p && g_track && g_track_count && g_use == g_track_heap) {
        if (*g_track_count < g_track_cap) {
            g_track[*g_track_count] = p;
        }
        (*g_track_count)++;
    }
}

MemHeap *mem_heap_use(MemHeap *h)
{
    MemHeap *was;

    was = g_use;
    g_use = h;
    return was;
}

void mem_track(void **list, int cap, int *count)
{
    g_track = list;
    g_track_cap = list ? cap : 0;
    g_track_count = list ? count : 0;
    g_track_heap = g_use;
}

static void *g_scratch[SCRATCH_SLOTS];
static df_u32 g_scratch_cap[SCRATCH_SLOTS];

static void scratch_drop(void)
{
    int i;

    for (i = 0; i < SCRATCH_SLOTS; i++) {
        g_scratch[i] = 0;
        g_scratch_cap[i] = 0;
    }
}

void *mem_scratch(int slot, df_u32 bytes)
{
    df_u32 cap;
    void *p;

    if (slot < 0 || slot >= SCRATCH_SLOTS || bytes == 0) {
        return 0;
    }
    if (g_scratch[slot] && g_scratch_cap[slot] >= bytes) {
        return g_scratch[slot];
    }
    cap = 4096;
    while (cap < bytes) {
        if (cap >= 0x40000000u) {
            return 0;
        }
        cap <<= 1;
    }
    /* Never in a chunk's heap: the buffer outlives the chunk that grew it. */
    p = base_alloc(cap);
    if (!p) {
        return 0;
    }
    g_scratch[slot] = p;
    g_scratch_cap[slot] = cap;
    return p;
}

#ifdef DF_HOST

#include <stdlib.h>

/* Each allocation in a heap carries this before it, so the heap can free all of them. */
typedef struct HostBlock {
    struct HostBlock *prev;
    struct HostBlock *next;
    df_u32 size;
    df_u32 pad;
} HostBlock;

struct MemHeap {
    HostBlock *first;
    df_u32 bytes;
};

MemHeap *mem_heap_new(void)
{
    MemHeap *h;

    h = (MemHeap *)malloc(sizeof(MemHeap));
    if (h) {
        h->first = 0;
        h->bytes = 0;
    }
    return h;
}

void mem_heap_delete(MemHeap *h)
{
    HostBlock *b;

    if (!h) {
        return;
    }
    if (g_use == h) {
        g_use = 0;
    }
    b = h->first;
    while (b) {
        HostBlock *next;

        next = b->next;
        free(b);
        b = next;
    }
    free(h);
}

static void *heap_alloc(MemHeap *h, df_u32 bytes)
{
    HostBlock *b;

    b = (HostBlock *)malloc(sizeof(HostBlock) + bytes);
    if (!b) {
        log_print(LOG_ERROR, "out of memory: %u bytes in a heap", (unsigned int)bytes);
        return 0;
    }
    mem_zero(b + 1, bytes);
    b->size = bytes;
    b->prev = 0;
    b->next = h->first;
    if (h->first) {
        h->first->prev = b;
    }
    h->first = b;
    h->bytes += bytes;
    return b + 1;
}

void mem_heap_free(MemHeap *h, void *p)
{
    HostBlock *b;

    if (!h || !p) {
        return;
    }
    b = (HostBlock *)p - 1;
    if (b->prev) {
        b->prev->next = b->next;
    } else {
        h->first = b->next;
    }
    if (b->next) {
        b->next->prev = b->prev;
    }
    h->bytes -= b->size;
    free(b);
}

df_u32 mem_heap_bytes(const MemHeap *h)
{
    return h ? h->bytes : 0;
}

void mem_init(void)
{
}

void mem_shutdown(void)
{
    int i;

    for (i = 0; i < SCRATCH_SLOTS; i++) {
        free(g_scratch[i]);
    }
    scratch_drop();
}

static void *base_alloc(df_u32 bytes)
{
    void *p;

    if (bytes == 0) {
        return 0;
    }
    p = counted(malloc(bytes), bytes, 0);
    if (!p) {
        return 0;
    }
    mem_zero(p, bytes);
    return p;
}

void *mem_alloc(df_u32 bytes)
{
    void *p;

    if (g_use) {
        if (bytes == 0) {
            return 0;
        }
        p = heap_alloc(g_use, bytes);
        track(p);
        return p;
    }
    return base_alloc(bytes);
}

int mem_temp_begin(void)
{
    g_epoch++;
    return 1;
}

/* The host has one heap, so kept and temp bytes are the same. */
void *mem_keep(df_u32 bytes)
{
    return mem_alloc(bytes);
}

void mem_temp_end(void)
{
    g_epoch++;
}

#else

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE g_heap;
static HANDLE g_temp;

struct MemHeap {
    HANDLE h;
    df_u32 bytes;
};

MemHeap *mem_heap_new(void)
{
    MemHeap *h;

    if (!g_heap) {
        return 0;
    }
    h = (MemHeap *)HeapAlloc(g_heap, HEAP_ZERO_MEMORY, sizeof(MemHeap));
    if (!h) {
        return 0;
    }
    h->h = HeapCreate(0, 64 * 1024, 0);
    if (!h->h) {
        HeapFree(g_heap, 0, h);
        return 0;
    }
    return h;
}

void mem_heap_delete(MemHeap *h)
{
    if (!h) {
        return;
    }
    if (g_use == h) {
        g_use = 0;
    }
    HeapDestroy(h->h);
    HeapFree(g_heap, 0, h);
}

void mem_heap_free(MemHeap *h, void *p)
{
    DWORD n;

    if (!h || !p) {
        return;
    }
    n = HeapSize(h->h, 0, p);
    if (n != (DWORD)-1 && n <= h->bytes) {
        h->bytes -= (df_u32)n;
    }
    HeapFree(h->h, 0, p);
}

df_u32 mem_heap_bytes(const MemHeap *h)
{
    return h ? h->bytes : 0;
}

void mem_init(void)
{
    if (!g_heap) {
        g_heap = HeapCreate(0, 1024 * 1024, 0);
    }
}

void mem_shutdown(void)
{
    mem_temp_end();
    scratch_drop();
    if (g_heap) {
        HeapDestroy(g_heap);
        g_heap = 0;
    }
}

int mem_temp_begin(void)
{
    if (g_temp) {
        return 1;
    }
    g_epoch++;
    g_temp = HeapCreate(0, 1024 * 1024, 0);
    return g_temp != 0;
}

void mem_temp_end(void)
{
    if (g_temp) {
        /* Scratch bytes came from this heap. Drop the pointers first. */
        scratch_drop();
        HeapDestroy(g_temp);
        g_temp = 0;
        g_temp_bytes = 0;
        g_epoch++;
    }
}

static void *base_alloc(df_u32 bytes)
{
    HANDLE heap;

    heap = g_temp ? g_temp : g_heap;
    if (!heap || bytes == 0) {
        return 0;
    }
    return counted(HeapAlloc(heap, HEAP_ZERO_MEMORY, bytes), bytes, g_temp != 0);
}

void *mem_alloc(df_u32 bytes)
{
    void *p;

    if (g_use) {
        if (bytes == 0) {
            return 0;
        }
        p = HeapAlloc(g_use->h, HEAP_ZERO_MEMORY, bytes);
        if (!p) {
            log_print(LOG_ERROR, "out of memory: %u bytes in a heap", (unsigned int)bytes);
            return 0;
        }
        g_use->bytes += bytes;
        track(p);
        return p;
    }
    return base_alloc(bytes);
}

void *mem_keep(df_u32 bytes)
{
    if (!g_heap || bytes == 0) {
        return 0;
    }
    return counted(HeapAlloc(g_heap, HEAP_ZERO_MEMORY, bytes), bytes, 0);
}

/* GCC emits these for block copies. The exe has no C runtime. */

void *memcpy(void *dst, const void *src, unsigned int n)
{
    mem_copy(dst, src, n);
    return dst;
}

void *memmove(void *dst, const void *src, unsigned int n)
{
    df_u8 *d;
    const df_u8 *s;
    df_u32 i;

    d = (df_u8 *)dst;
    s = (const df_u8 *)src;
    if (d < s) {
        /* Forward, a word at a time: a lower destination never overwrites source not yet read. */
        mem_copy(d, s, n);
    } else if (d > s) {
        i = n;
        while (i > 0) {
            i--;
            d[i] = s[i];
        }
    }
    return dst;
}

/*
 * GCC turns a plain length loop into a call. The body reads through a
 * volatile pointer so this function is not replaced with a call to itself.
 */
unsigned int strlen(const char *s)
{
    const volatile unsigned char *p;
    unsigned int n;

    n = 0;
    p = (const volatile unsigned char *)s;
    if (!p) {
        return 0;
    }
    while (p[n] != 0) {
        n++;
    }
    return n;
}

void *memset(void *dst, int value, unsigned int n)
{
    mem_fill(dst, (df_u8)value, n);
    return dst;
}

#endif
