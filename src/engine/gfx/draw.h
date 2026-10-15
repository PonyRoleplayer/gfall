#ifndef DF_DRAW_H
#define DF_DRAW_H

#include "engine/gfx/camera.h"

struct Level;

/* Transform, clip to the near plane, project, submit, swap. */

void draw_frame(const Camera *cam, const struct Level *level);

/* Called after the frame, before the buffer swaps. 0 clears it. */
void draw_set_extra(void (*fn)(void));
/* Called after the scene and before the HUD. 0 clears it. */
void draw_set_under(void (*fn)(void));

#endif
