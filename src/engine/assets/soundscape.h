#ifndef ENG_SOUNDSCAPE_H
#define ENG_SOUNDSCAPE_H

#include "engine/assets/clip.h"

/*
 * What a place sounds like when nothing in it is making a sound of its
 * own: its music, the weather's loops, the thunder, and the odd noise
 * somewhere near. The game fills one per place (Level.sound) and owns
 * the clips; the engine decides when each plays (engine/sound/ambience.c).
 * Sounds from a point, a torch or a cat, are the scene's emitters
 * (Scene.emitters). Read docs/sound.md before editing.
 *
 * Any clip may be 0, any count 0: that sound is not played.
 */

#define SOUNDSCAPE_PICKS 16

typedef struct Soundscape {
    /*
     * The song for the moment: the weather's kind (WEATHER_*) and 1 at
     * night. A name the "song" loader knows, or 0 for none. Asked when
     * the place, the kind or night changes; the same name keeps the
     * song playing.
     */
    const char *(*song)(const struct Soundscape *s, int weather, int night);
    /*
     * Loops under everything, not from any point. rain follows the
     * weather's rain, wind its snow, night the dark of a fair night
     * (crickets). Outdoors only.
     */
    const SoundClip *rain;
    const SoundClip *wind;
    const SoundClip *night;
    /* After a lightning stroke, one of these, from the horizon, late by the distance. */
    const SoundClip *thunder[SOUNDSCAPE_PICKS];
    int nthunder;
    /* Now and then, somewhere round the listener: on a fair day outdoors (birds). */
    const SoundClip *day[SOUNDSCAPE_PICKS];
    int nday;
    /* Now and then, somewhere round the listener, at any hour (a dungeon's drips and moans). */
    const SoundClip *around[SOUNDSCAPE_PICKS];
    int naround;
    /* Seconds between those, at random between the two. */
    float wait_min;
    float wait_max;
    /* The game's own: which place, which playlist. */
    int index;
    int kind;
} Soundscape;

#endif
