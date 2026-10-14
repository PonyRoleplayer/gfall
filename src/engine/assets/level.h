#ifndef ENG_LEVEL_H
#define ENG_LEVEL_H

#include "engine/assets/palette.h"
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
    int nimages;
    const Palette *palette;
    Scene scene;
} Level;

#endif
