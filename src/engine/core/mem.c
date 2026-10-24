#include "engine/core/mem.h"
#include "engine/core/log.h"

#define SCRATCH_SLOTS 2

/* Bytes handed out, for the console's mem command. Not freed bytes. */
static df_u32 g_main_bytes;
static df_u32 g_temp_bytes;

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
    p = mem_alloc(cap);
    if (!p) {
        return 0;
    }
    g_scratch[slot] = p;
    g_scratch_cap[slot] = cap;
    return p;
}

#ifdef DF_HOST

#include <stdlib.h>

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

void *mem_alloc(df_u32 bytes)
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

int mem_temp_begin(void)
{
    return 1;
}

void mem_temp_end(void)
{
}

#else

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE g_heap;
static HANDLE g_temp;

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
    }
}

void *mem_alloc(df_u32 bytes)
{
    HANDLE heap;

    heap = g_temp ? g_temp : g_heap;
    if (!heap || bytes == 0) {
        return 0;
    }
    return counted(HeapAlloc(heap, HEAP_ZERO_MEMORY, bytes), bytes, g_temp != 0);
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
        for (i = 0; i < n; i++) {
            d[i] = s[i];
        }
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
    df_u8 *d;
    df_u32 i;

    d = (df_u8 *)dst;
    for (i = 0; i < n; i++) {
        d[i] = (df_u8)value;
    }
    return dst;
}

#endif
