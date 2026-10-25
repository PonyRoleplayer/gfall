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

/*
 * Sends the level palette and every image, budgeted to fit the 2MB
 * card. A second call replaces what the first sent.
 */
int gfx_prepare(const Level *level);
/*
 * Mip chains, default on. A change sends the prepared level again on
 * the next gfx_frame, so the picture shows it. See docs/glide.md.
 */
void gfx_set_mipmap(int on);
int gfx_mipmap(void);
/*
 * Debug only: TMU0 holds at most kb KB, as a smaller card would. 2048 is
 * a Voodoo 1's 2 MB; 0 (default) is the card's own. A change sends the
 * level again on the next frame. See docs/memory.md.
 */
void gfx_set_texcap(int kb);
int gfx_texcap(void);
/* How far lights reach, 1..8 times their radius; cuts the decals again. */
void gfx_set_light_range(int scale);

/* Clear, draw what the scene says is visible, overlay, swap. */
void gfx_frame(const Camera *cam, const Level *level);

/* Drawn after the frame and before the swap. 0 clears it. */
void gfx_set_extra(void (*fn)(void));
/* Drawn after the scene and before the HUD. 0 clears it. */
void gfx_set_under(void (*fn)(void));
/*
 * Distance fog. 1 is on, 0 is off. Default is on. Safe before
 * gfx_startup; the device applies it when it opens, and a later call
 * switches the mode while the device is open.
 */
void gfx_set_fog(int on);
int gfx_fog(void);
/*
 * Far clip in world units. Default is DF_FAR (1400). A later call takes
 * effect on the next frame. Values above DF_FAR_MAX are clamped. Fog, when
 * on, goes solid at the same fraction of this distance as 1100/1400.
 */
void gfx_set_far(float far);
float gfx_far(void);

#endif
