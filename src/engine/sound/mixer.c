#include "engine/sound/mixer.h"

/*
 * See mixer.h and docs/sound.md. The MMX loop exists on x86 (the target,
 * and the host's x86-64, so hostcheck can hold it to the C loop); other
 * hosts take the C loop alone.
 */

#if defined(__i386__) || defined(__x86_64__)
#define MIX_HAVE_ASM 1
#else
#define MIX_HAVE_ASM 0
#endif

static int g_cpu = -1;
static int g_want = 1;

#if MIX_HAVE_ASM
/*
 * CPUID function 1, EDX bit 23. A 386 or an early 486 has no CPUID: the
 * EFLAGS ID bit (21) does not stay flipped.
 */
static int cpu_mmx(void)
{
    df_u32 eax;
    df_u32 ebx;
    df_u32 ecx;
    df_u32 edx;

#if defined(__i386__)
    df_u32 a;
    df_u32 b;

    __asm__ __volatile__(
        "pushfl\n\t"
        "popl %0\n\t"
        "movl %0, %1\n\t"
        "xorl $0x200000, %0\n\t"
        "pushl %0\n\t"
        "popfl\n\t"
        "pushfl\n\t"
        "popl %0\n\t"
        "pushl %1\n\t"
        "popfl"
        : "=&r"(a), "=&r"(b)
        :
        : "cc");
    if (((a ^ b) & 0x200000u) == 0) {
        return 0;
    }
#endif
    eax = 1;
    __asm__ __volatile__("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    (void)ebx;
    (void)ecx;
    return (int)((edx >> 23) & 1u);
}
#endif

int mix_has_mmx(void)
{
    if (g_cpu < 0) {
#if MIX_HAVE_ASM
        g_cpu = cpu_mmx();
#else
        g_cpu = 0;
#endif
    }
    return g_cpu;
}

void mix_set_mmx(int on)
{
    g_want = on ? 1 : 0;
}

int mix_mmx(void)
{
    return g_want && mix_has_mmx();
}

void mix_clear(df_i16 *dst, int frames)
{
    int i;

    for (i = 0; i < frames * 2; i++) {
        dst[i] = 0;
    }
}

static df_i16 sat16(df_i32 v)
{
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return (df_i16)v;
}

/* One frame: pmulhw then paddsw, each side. */
static void add_frame(df_i16 *dst, int s, int gl, int gr)
{
    df_i32 w;

    w = (df_i32)s * 256;
    dst[0] = sat16((df_i32)dst[0] + ((w * gl) >> 16));
    dst[1] = sat16((df_i32)dst[1] + ((w * gr) >> 16));
}

#if MIX_HAVE_ASM
/*
 * Four frames an iteration, groups > 0. mm7 holds the gains as words
 * (left, right, left, right). Four bytes are unpacked against zero into
 * the high byte of four words (s << 8), each word doubled into a left
 * and right pair, scaled by pmulhw and added into dst with paddsw.
 * MMX shares the x87 registers, so the loop ends in emms and names the
 * x87 stack as clobbered: no float may live across it.
 */
static void __attribute__((noinline)) add_mmx(df_i16 *dst, const signed char *src,
                                                int groups, df_u32 gains)
{
    __asm__ __volatile__(
        "movd %3, %%mm7\n\t"
        "punpckldq %%mm7, %%mm7\n\t"
        "1:\n\t"
        "movd (%1), %%mm0\n\t"
        "pxor %%mm1, %%mm1\n\t"
        "punpcklbw %%mm0, %%mm1\n\t"
        "movq %%mm1, %%mm3\n\t"
        "punpcklwd %%mm1, %%mm1\n\t"
        "punpckhwd %%mm3, %%mm3\n\t"
        "pmulhw %%mm7, %%mm1\n\t"
        "pmulhw %%mm7, %%mm3\n\t"
        "paddsw (%0), %%mm1\n\t"
        "paddsw 8(%0), %%mm3\n\t"
        "movq %%mm1, (%0)\n\t"
        "movq %%mm3, 8(%0)\n\t"
        "add $4, %1\n\t"
        "add $16, %0\n\t"
        "decl %2\n\t"
        "jnz 1b\n\t"
        "emms"
        : "+r"(dst), "+r"(src), "+r"(groups)
        : "r"(gains)
        : "memory", "cc",
          "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");
}
#endif

/* n frames, one sample each, at the output's rate. */
static void add_run(df_i16 *dst, const signed char *src, int n, int gl, int gr)
{
    int i;

#if MIX_HAVE_ASM
    if (n >= 4 && mix_mmx()) {
        int groups;

        groups = n >> 2;
        add_mmx(dst, src, groups, (df_u32)(df_u16)gl | ((df_u32)(df_u16)gr << 16));
        dst += groups * 8;
        src += groups * 4;
        n &= 3;
    }
#endif
    for (i = 0; i < n; i++) {
        add_frame(dst + i * 2, src[i], gl, gr);
    }
}

void mix_voice(MixVoice *v, df_i16 *dst, int frames)
{
    int silent;

    if (!v->on || !v->data || v->frames == 0) {
        v->on = 0;
        return;
    }
    silent = v->gl == 0 && v->gr == 0;
    if (v->step == MIX_ONE) {
        while (frames > 0 && v->on) {
            df_u32 left;
            int n;

            left = v->frames - v->pos;
            n = left < (df_u32)frames ? (int)left : frames;
            if (!silent) {
                add_run(dst, v->data + v->pos, n, v->gl, v->gr);
            }
            dst += n * 2;
            frames -= n;
            v->pos += (df_u32)n;
            if (v->pos >= v->frames) {
                if (v->loop) {
                    v->pos = 0;
                } else {
                    v->on = 0;
                }
            }
        }
        return;
    }
    while (frames > 0 && v->on) {
        if (!silent) {
            add_frame(dst, v->data[v->pos], v->gl, v->gr);
        }
        dst += 2;
        frames--;
        v->frac += v->step;
        v->pos += v->frac >> 16;
        v->frac &= 0xffffu;
        if (v->pos >= v->frames) {
            if (v->loop) {
                v->pos %= v->frames;
            } else {
                v->on = 0;
            }
        }
    }
}
