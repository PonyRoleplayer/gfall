#ifndef ENG_CLIP_H
#define ENG_CLIP_H

#include "engine/core/df.h"

/*
 * A sound effect: mono, signed 8-bit samples at their own rate. The
 * game loads it (Daggerfall's are unsigned 8-bit at 11025 Hz and are
 * flipped to signed once, at load) and keeps it for the process: a
 * voice may still be reading it after the place that asked for it is
 * gone. Read docs/sound.md.
 *
 * Eight bits, not sixteen: half the memory, and the mixer widens a
 * sample to a word with one MMX unpack anyway.
 */
typedef struct SoundClip {
    const signed char *data;
    df_u32 frames;
    df_u32 rate;
} SoundClip;

#endif
