#ifndef DF_H
#define DF_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

/*
 * Engine integers and x87 helpers. unsigned int is 32 bits on the Win32
 * exe and on the host. Names stay short because every file uses them.
 */

typedef unsigned int df_u32;
typedef int df_i32;
typedef unsigned short df_u16;
typedef short df_i16;
typedef unsigned char df_u8;

#define DF_VIEW_W 640
#define DF_VIEW_H 480
#define DF_FOCAL 400.0f
#define DF_NEAR 4.0f
/* Fog is solid at DF_FOG_FAR. The clip is a little farther, so the cut is black. */
#define DF_FOG_NEAR 280.0f
#define DF_FOG_FAR 1100.0f
#define DF_FAR 1400.0f
#define DF_TAU 6.28318530718f

static df_u16 df_ru16(const df_u8 *p)
{
    return (df_u16)(p[0] | (p[1] << 8));
}

static df_i16 df_ri16(const df_u8 *p)
{
    return (df_i16)df_ru16(p);
}

static df_u32 df_ru32(const df_u8 *p)
{
    return (df_u32)p[0]
        | ((df_u32)p[1] << 8)
        | ((df_u32)p[2] << 16)
        | ((df_u32)p[3] << 24);
}

static df_i32 df_ri32(const df_u8 *p)
{
    return (df_i32)df_ru32(p);
}

static int df_eq(const char *a, const char *b)
{
    int i;

    i = 0;
    while (a[i] && a[i] == b[i]) {
        i++;
    }
    return a[i] == b[i];
}

static void df_copy(char *dst, const char *src, int n)
{
    int i;

    if (n < 1) {
        return;
    }
    for (i = 0; i < n - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

static float df_absf(float v)
{
    return (v < 0.0f) ? -v : v;
}

static float df_wrap_tau(float a)
{
    while (a >= DF_TAU) {
        a -= DF_TAU;
    }
    while (a < 0.0f) {
        a += DF_TAU;
    }
    return a;
}

/* x87, so the Pentium MMX build does not emit a libm call. */
static float df_sin(float x)
{
    float r;

    __asm__ __volatile__("fsin" : "=t"(r) : "0"(x));
    return r;
}

static float df_cos(float x)
{
    float r;

    __asm__ __volatile__("fcos" : "=t"(r) : "0"(x));
    return r;
}

#pragma GCC diagnostic pop

#endif
