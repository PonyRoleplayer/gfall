#ifndef ENG_LEVEL_H
#define ENG_LEVEL_H

#include "engine/assets/palette.h"
#include "engine/assets/sky.h"
#include "engine/assets/soundscape.h"
#include "engine/assets/texture.h"
#include "engine/scene/scene.h"

/*
 * One drawable place. The game fills it. The renderer does not care
 * which archive the pictures came from.
 * images[] and cutout[] are parallel. cutout[i] nonzero means index 0
 * of that image is transparent.
 */

typedef struct Level {
    Image *const *images;
    const df_u8 *cutout;
    /*
     * Beside images, or 0: 1 for a picture whose texels of MIP_GLASS are
     * a window's glass, lit after dark (docs/daylight.md).
     */
    const df_u8 *glass;
    int nimages;
    const Palette *palette;
    /* The sky, for an outdoor place; 0 indoors. See docs/daylight.md. */
    const Sky *sky;
    /*
     * The game adds one when it changed *sky in place (a streamed world
     * walked into another climate): the frame then reads the sky again,
     * into the same places on the card.
     */
    int sky_serial;
    /* Its music and ambience, or 0 for none. See docs/sound.md. */
    const Soundscape *sound;
    /*
     * 1 for a streamed world (docs/streaming.md): its pictures come and
     * go with its chunks, through texcache_stream_set and _drop, at the
     * size their distance needs. gfx_prepare puts none on the card.
     */
    int streamed;
    Scene scene;
} Level;

#endif
