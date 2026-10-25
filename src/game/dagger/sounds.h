#ifndef DF_SOUNDS_H
#define DF_SOUNDS_H

#include "engine/assets/soundscape.h"
#include "engine/scene/scene.h"

/*
 * What Daggerfall's places sound like, as Daggerfall Unity has it
 * (SongManager, AmbientEffectsPlayer, RDBLayout and GameObjectHelper).
 * Read docs/sound.md.
 */

enum {
    SCAPE_DUNGEON,
    SCAPE_TOWN
};

/*
 * A place's soundscape: kind SCAPE_*, index the location (which of a
 * playlist's songs it gets). The clips are loaded now, the songs when
 * the engine asks.
 */
void sounds_scape(Soundscape *sc, int kind, int index);
/*
 * The sound a flat of TEXTURE.file, record image makes from (x, y, z):
 * a flaming torch's fire, an animal's call. 1 and out filled, or 0.
 */
int sounds_flat(int file, int image, float x, float y, float z, Emitter *out);

#endif
