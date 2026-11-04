#ifndef DF_SFX_H
#define DF_SFX_H

/*
 * The game's sound effects: DAGGER.SND's clips by position (sndfile.h,
 * DFU's SoundClips), played on the engine's voices (sound.h). Read
 * docs/game.md (Combat, Sounds) before editing.
 *
 * A clip from a point is full within SFX_INNER and silent past
 * SFX_REACH; the player's own (a swing, its pain) play flat. With the
 * sound off, or no clip at that position, nothing plays.
 */

/* Units: full within 4 m, gone past 40 m. */
#define SFX_INNER 160.0f
#define SFX_REACH 1600.0f

/* The player's own: flat, centred. */
void sfx_play(int clip);
/* From a point in the scene's frame. */
void sfx_at(int clip, float x, float y, float z);
/* A clip looping flat at volume 0..1: a handle, 0 when nothing plays. */
int sfx_loop(int clip, float volume);
/* A loop's volume changed, or stopped. A handle that ended is ignored. */
void sfx_gain(int handle, float volume);
void sfx_stop(int handle);
/* One of first..first+n-1 at random (Hit1..Hit5, Parry1..Parry9). */
int sfx_pick(int first, int n);
/* A pain clip of a race (RACE_*) and sex, one of its three at random (DFU's race pain clips); -1 for none. */
int sfx_pain(int race, int female);

#endif
