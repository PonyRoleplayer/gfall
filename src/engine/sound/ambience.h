#ifndef ENG_AMBIENCE_H
#define ENG_AMBIENCE_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"
#include "engine/scene/weather.h"

/*
 * A place's sound, once a frame: what its soundscape (Level.sound) and
 * emitters (Scene.emitters) ask for, by the weather and the hour. Read
 * docs/sound.md before editing.
 *
 * - The song the soundscape names for the weather and the night,
 *   loaded through the game's "song" loader when it changes.
 * - Loops under everything: rain by the weather's rain, wind by its
 *   snow, the night's crickets in the dark of a fair night, each fading
 *   in and out over two seconds.
 * - Thunder after each lightning stroke, late by the stroke's distance.
 * - Now and then (wait_min..wait_max seconds) a noise somewhere round
 *   the listener: birds on a fair day outdoors, drips and moans in a
 *   dungeon.
 * - Emitters: a loop plays while the listener is within its reach, the
 *   nearest AMBIENCE_LOOPS of them at once; the others play now and
 *   then within reach.
 *
 * Then the voices are aimed from the camera and the card is refilled
 * (sound_update), and the music's buffers go back to its stream.
 */

/* Emitter loops heard at once, the nearest. */
#define AMBIENCE_LOOPS 6

/*
 * night: 0 by day, 1 at night (the street lamps' share). stroke: 1 on
 * the frame a lightning stroke begins. secs: real time since the last.
 */
void ambience_frame(const Level *level, const Camera *cam, const Weather *w,
                    float night, int stroke, float secs);
/* The console is up: only aim and refill, so the sound runs on. */
void ambience_idle(void);
/* Emitters within reach last frame, and loops held. */
void ambience_stats(int *heard, int *loops);
/* The emitters of the place heard last frame, and how many; 0 for none. */
int ambience_emitters(const Emitter **list);
/* 1 while emitter index is playing a sound. */
int ambience_emitter_playing(int index);
/*
 * The debug picture (sndview): a square, a word and a ring where it is
 * no longer heard, for emitters within this many world units of the
 * camera, 0..AMBIENCE_VIEW_MAX. 0 (default) is off.
 */
#define AMBIENCE_VIEW_MAX 4096
/* 1 to AMBIENCE_VIEW_SMALL - 1 turns it on at AMBIENCE_VIEW_ON. */
#define AMBIENCE_VIEW_SMALL 64
#define AMBIENCE_VIEW_ON 1024
void ambience_set_view(int units);
int ambience_view(void);

#endif
