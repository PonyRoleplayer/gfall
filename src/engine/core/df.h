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
/* Fog is solid at DF_FOG_FAR. The default clip is a little farther. */
#define DF_FOG_NEAR 280.0f
#define DF_FOG_FAR 1100.0f
#define DF_FAR 1400.0f
/* gfx_set_far will not go past this. The Z-buffer's ooz, 65535 * DF_NEAR / z, is 16 here. */
#define DF_FAR_MAX 16384.0f
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

/*
 * Nearest integer, for |v| below 2^22. A C cast on the x87 switches the
 * rounding mode twice (fnstcw, fldcw, fistp, fldcw), about 30 cycles on
 * a Pentium. Adding 1.5 * 2^23 pushes the fraction out of a float's
 * 23-bit mantissa, so storing the sum rounds it to an integer in the
 * current mode, which is nearest, and the low bits are that integer.
 */
static int df_iround(float v)
{
    union {
        float f;
        df_i32 i;
    } u;

    u.f = v + 12582912.0f;
    return (int)(u.i - 0x4B400000);
}

/*
 * v rounded to the nearest 1/16, for |v| below 2^18. The SST-1 takes
 * screen x and y in 12.4 fixed point. Adding 1.5 * 2^19 leaves a float
 * four fraction bits, so the store rounds to 1/16 and the subtraction
 * is exact. volatile forces that store: on the x87 the sum would
 * otherwise stay in an 80-bit register with every bit. A union read of
 * the same member is not enough, GCC keeps it in the register.
 */
static float df_snap16(float v)
{
    volatile float t;

    t = v + 786432.0f;
    return t - 786432.0f;
}

/*
 * Where fog is meant to go solid for a far clip: the same fraction of
 * it as DF_FOG_FAR is of DF_FAR, never nearer than DF_FOG_NEAR + 1.
 * gfx uploads the fog table with this, and scene and draw read it.
 */
static float df_fog_far(float clip_far)
{
    float f;

    f = clip_far * (DF_FOG_FAR / DF_FAR);
    if (f < DF_FOG_NEAR + 1.0f) {
        f = DF_FOG_NEAR + 1.0f;
    }
    return f;
}

/*
 * Where the card's fog really is solid. The table's 64 entries sit at
 * w = 2^(3 + i/4) / (8 - i % 4), and the SST-1 blends between two
 * entries, so fog only reaches 255 at the first entry at or past
 * fog_far: 1170 for the default 1100, where 1100 itself is about 95%
 * fog. Culling or clipping nearer than this cuts off what is still a
 * few percent visible; drawing past it fills pixels the fog colours
 * over completely. Past the last entry, fog_far itself.
 */
static float df_fog_solid(float fog_far)
{
    int i;

    for (i = 0; i < 64; i++) {
        float w;

        w = (float)(8u << (i >> 2)) / (float)(8 - (i & 3));
        if (w >= fog_far) {
            return w;
        }
    }
    return fog_far;
}

/*
 * The fog table's farthest entry at or before limit: a fog that goes
 * solid there is solid by limit, so with the far clip at limit nothing
 * is cut off while still a few percent visible. The weather's simulated
 * view puts its fog end here (docs/weather.md). The first entry, 1,
 * when limit is nearer.
 */
static float df_fog_below(float limit)
{
    int i;
    float best;

    best = 1.0f;
    for (i = 0; i < 64; i++) {
        float w;

        w = (float)(8u << (i >> 2)) / (float)(8 - (i & 3));
        if (w > limit) {
            break;
        }
        best = w;
    }
    return best;
}

/* Largest integer not above v, for |v| below 2^22. No mode switch. */
static int df_ifloor(float v)
{
    int i;

    i = df_iround(v);
    if ((float)i > v) {
        i--;
    }
    return i;
}

#pragma GCC diagnostic pop

#endif
