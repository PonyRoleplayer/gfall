#ifndef ENG_SOUND_H
#define ENG_SOUND_H

#include "engine/assets/clip.h"

/*
 * Sound effects. Read docs/sound.md before editing.
 *
 * SOUND_VOICES voices, mixed in software (engine/sound/mixer.c) into the
 * one stream the card plays (engine/sound/audio.c). A voice plays one
 * clip, once or looping, either flat (a volume and a pan) or from a
 * point in the world, which the listener hears louder near and from its
 * side. Gains are worked out once a frame, in sound_update, which then
 * mixes what the card needs next. With every voice busy, a new sound
 * takes the voice that is quietest now, if it would be louder.
 *
 * Nothing here needs a card: without one (hostcheck, a machine with no
 * sound) every call works and nothing is heard.
 */

#define SOUND_VOICES 16

/* sound_play flags. */
#define SOUND_LOOP 1

/*
 * Open the card: rate frames a second (11025 is Daggerfall's own),
 * mode AUDIO_* (engine/sound/audio.h). 0 when nothing opened; the
 * calls below still work.
 */
int sound_startup(void *hwnd);
void sound_shutdown(void);
/* The card is open. */
int sound_ready(void);

/* Off stops every voice and mixes silence. Default on. */
void sound_set_on(int on);
int sound_on(void);
/* 0..100, every effect. Default 80. */
void sound_set_volume(int percent);
int sound_volume(void);
/* Output rate and way out; a change opens the card again. */
void sound_set_rate(int rate);
int sound_rate(void);
void sound_set_mode(int mode);
int sound_mode(void);

/* Where the listener stands and which way it faces (Camera.yaw). */
void sound_listen(float x, float y, float z, float yaw);

/* Gains for this frame, then mix what the card plays next. */
void sound_update(void);

/*
 * Flat: volume 0..1, pan -1 (left) .. 1 (right). A handle for the calls
 * below, never 0; 0 when nothing plays (no clip, sound off, every voice
 * louder).
 */
int sound_play(const SoundClip *clip, float volume, float pan, int flags);
/* From a point: full volume within inner, silent past reach, linear between. */
int sound_play_at(const SoundClip *clip, float x, float y, float z,
                  float inner, float reach, float volume, int flags);
/* A handle that ended or was taken is ignored. */
void sound_stop(int handle);
int sound_playing(int handle);
void sound_set_gain(int handle, float volume);
/* Start a playing voice from this frame of its clip: a loop's phase. */
void sound_seek(int handle, df_u32 frame);
void sound_stop_all(void);
/* Voices playing now. */
int sound_busy(void);

/*
 * The mix itself: frames stereo frames added into dst. audio.c calls it;
 * hostcheck calls it to hear the voices without a card.
 */
void sound_mix(df_i16 *dst, int frames);
/* The rate voices are stepped for; hostcheck sets it with no card. */
void sound_set_mix_rate(int rate);

#endif
