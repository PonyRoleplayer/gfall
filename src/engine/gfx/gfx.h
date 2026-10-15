#ifndef ENG_GFX_H
#define ENG_GFX_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"

/*
 * The graphics call the game makes. Glide, the texture cache, and the
 * clipper sit behind this. Open the device, upload one level, draw frames.
 */

int gfx_startup(void *hwnd);
void gfx_shutdown(void);
const char *gfx_error(void);

/* Sends the level palette and every image. The first room fits in 2MB. */
int gfx_prepare(const Level *level);

/* Clear, draw what the scene says is visible, overlay, swap. */
void gfx_frame(const Camera *cam, const Level *level);

/* Drawn after the frame and before the swap. 0 clears it. */
void gfx_set_extra(void (*fn)(void));
/* Drawn after the scene and before the HUD. 0 clears it. */
void gfx_set_under(void (*fn)(void));

#endif
