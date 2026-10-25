#ifndef ENG_AUDIO_H
#define ENG_AUDIO_H

#include "engine/core/df.h"

/*
 * The sound card's output: one stream of signed 16-bit stereo frames
 * that the mixer writes, refilled from the frame loop (no thread). Read
 * docs/sound.md before editing.
 *
 * Three ways out, cheapest first; AUDIO_AUTO takes the first that opens:
 *
 * - AUDIO_PRIMARY: DirectSound's primary buffer, written by us
 *   (DSSCL_WRITEPRIMARY). The mixer's frames go straight into the
 *   buffer the card plays; DirectSound mixes nothing.
 * - AUDIO_SECONDARY: one DirectSound buffer, at the primary's own
 *   format, so DirectSound's mixer only copies it.
 * - AUDIO_WAVEOUT: WINMM's wave blocks, for a card with no DirectSound
 *   driver. DirectSound on such a card is emulated over the same calls
 *   with more latency, so AUTO skips an emulated DirectSound.
 *
 * Each refill mixes only up to a lead ahead of what the card plays (a
 * frame and a half and 40 ms) and writes silence past it, so a stall
 * (a place loading) runs out into quiet, not a loop of the last moment.
 */

enum {
    AUDIO_AUTO,
    AUDIO_PRIMARY,
    AUDIO_SECONDARY,
    AUDIO_WAVEOUT,
    AUDIO_NONE
};

/* Adds frames of the next stereo frames into dst, which is silence. */
typedef void (*AudioFill)(df_i16 *dst, int frames);

/*
 * Open at rate frames a second the way want says. Returns the way it
 * opened (AUDIO_PRIMARY..AUDIO_WAVEOUT), or AUDIO_NONE. The card may
 * run at a rate of its own: audio_rate says which.
 */
int audio_open(void *hwnd, int rate, int want);
void audio_close(void);
int audio_mode(void);
int audio_rate(void);
const char *audio_mode_name(int mode);
/*
 * Mix what the card needs next. It times itself (the performance
 * counter): a caller that stalled for seconds, or was not called while
 * a console was open, is caught up rather than trusted.
 */
void audio_update(AudioFill fill);
/* The ring and the lead now, in milliseconds. */
int audio_buffer_ms(void);
int audio_lead_ms(void);

#endif
