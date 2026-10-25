#ifndef ENG_MIXER_H
#define ENG_MIXER_H

#include "engine/core/df.h"

/*
 * The software mixer: mono signed 8-bit clips added into one stereo
 * buffer of signed 16-bit frames (left, right). Read docs/sound.md
 * before editing.
 *
 * A sample s is widened to s << 8 and scaled by the voice's gain for
 * each side, a signed Q15 word: (s << 8) * g >> 16, the high half of
 * the product, which is MMX's pmulhw. At full gain a voice is half of
 * full scale, so two loud voices still fit; each voice is added with
 * saturation (paddsw). The C loop and the MMX loop compute exactly
 * that, bit for bit, and hostcheck holds them to it.
 *
 * A clip at the output's own rate (step MIX_ONE) takes the MMX loop,
 * four frames an iteration. Any other rate steps through the clip in
 * 16.16 fixed point, nearest sample, in C. Plain C and inline MMX with
 * no Windows, so hostcheck runs it.
 */

#define MIX_ONE 65536u

typedef struct MixVoice {
    const signed char *data;
    df_u32 frames;
    /* Next sample, and the fraction past it (16 bits) when stepping. */
    df_u32 pos;
    df_u32 frac;
    /* Clip samples per output frame, 16.16. */
    df_u32 step;
    /* Q15 gains, 0..32767. */
    df_i16 gl;
    df_i16 gr;
    /* 1 starts again at 0 at the end; 0 turns off. */
    int loop;
    int on;
} MixVoice;

/* 1 when the CPU has MMX. */
int mix_has_mmx(void);
/* Use MMX when the CPU has it (default 1). 0 forces the C loop. */
void mix_set_mmx(int on);
int mix_mmx(void);

/* frames stereo frames of silence. */
void mix_clear(df_i16 *dst, int frames);
/*
 * Add frames stereo frames of the voice into dst and move it on; a
 * voice that is not looping turns off at its end.
 */
void mix_voice(MixVoice *v, df_i16 *dst, int frames);

#endif
