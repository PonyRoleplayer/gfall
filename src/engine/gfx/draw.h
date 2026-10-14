#ifndef DF_DRAW_H
#define DF_DRAW_H

#include "engine/gfx/camera.h"

struct Level;

/* Transform, clip to the near plane, project, submit, swap. */

void draw_frame(const Camera *cam, const struct Level *level);

#endif
