#include "engine/assets/loader.h"

#define LOADER_SLOTS 8
#define LOADER_NAME 16

static struct {
    char kind[LOADER_NAME];
    LoaderFn fn;
} g_slot[LOADER_SLOTS];

static int g_nslots;

static int kind_eq(const char *a, const char *b)
{
    int i;

    i = 0;
    while (a[i] && a[i] == b[i] && i < LOADER_NAME - 1) {
        i++;
    }
    return a[i] == b[i];
}

static void kind_copy(char *dst, const char *src)
{
    int i;

    for (i = 0; i < LOADER_NAME - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

int loader_register(const char *kind, LoaderFn fn)
{
    int i;

    if (!kind || !kind[0] || !fn) {
        return 0;
    }
    for (i = 0; i < g_nslots; i++) {
        if (kind_eq(g_slot[i].kind, kind)) {
            g_slot[i].fn = fn;
            return 1;
        }
    }
    if (g_nslots >= LOADER_SLOTS) {
        return 0;
    }
    kind_copy(g_slot[g_nslots].kind, kind);
    g_slot[g_nslots].fn = fn;
    g_nslots++;
    return 1;
}

int loader_call(const char *kind, const char *name, const df_u8 *bytes, df_u32 size, void *out)
{
    int i;

    if (!kind) {
        return 0;
    }
    for (i = 0; i < g_nslots; i++) {
        if (kind_eq(g_slot[i].kind, kind)) {
            return g_slot[i].fn(name, bytes, size, out);
        }
    }
    return 0;
}
