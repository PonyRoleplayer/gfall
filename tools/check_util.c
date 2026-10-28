#include "check_util.h"

#include <stdio.h>

static int g_fail;

void check_expect(int ok, const char *msg)
{
    if (!ok) {
        fprintf(stderr, "fail: %s\n", msg);
        g_fail = 1;
    }
}

int check_near(float a, float b, float tol)
{
    float d;

    d = a - b;
    if (d < 0.0f) {
        d = -d;
    }
    return d <= tol;
}

int check_failed(void)
{
    return g_fail;
}

/*
 * The host has no card: a streamed world's pictures (wres.c) are put
 * on it through texcache, which talks to Glide and is not built here.
 */
struct Image;

int texcache_stream_set(int slot, const struct Image *img, int cutout, float density)
{
    (void)slot;
    (void)img;
    (void)cutout;
    (void)density;
    return 0;
}

void texcache_stream_drop(int slot)
{
    (void)slot;
}

/* No card on the host: a place's late pictures (a creature's) are only counted. */
static int g_resident_adds;

int texcache_resident_add(int slot, const struct Image *img, int cutout, float density)
{
    (void)slot;
    (void)cutout;
    (void)density;
    g_resident_adds += img != 0;
    return img != 0;
}
