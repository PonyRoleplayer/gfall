#ifndef DF_WILD_H
#define DF_WILD_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"

/*
 * Daggerfall's world, streamed round the camera (docs/streaming.md),
 * phase 1: the wilderness, flat. Every chunk (an RMB block's square,
 * 4096 units) is its map pixel's ground: the climate's grass, the sea's
 * water off the coast, in the season of the day the stream opened. No
 * locations yet. One stream at a time, on the current heap.
 */

/*
 * Open the stream on map pixel (px, py), radius chunks round the
 * camera's (engine/scene/stream.h), and fill level for gfx_prepare and
 * gfx_frame. The camera's start: the pixel's middle, at eye height. 0
 * when the world map or the pictures do not read.
 */
int wild_open(int px, int py, int radius, Level *level, float *x, float *y, float *z);
void wild_close(void);
/* Once a frame, before gfx_frame: load and drop chunks round cam, within budget_ms. */
void wild_update(Camera *cam, float budget_ms);
/* The map pixel under the camera. */
void wild_here(const Camera *cam, int *px, int *py);

#endif
