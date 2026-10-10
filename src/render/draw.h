#ifndef DF_DRAW_H
#define DF_DRAW_H

#include "render/camera.h"

struct World;

/* Transform, clip to the near plane, project, submit, swap. */

void draw_frame(const Camera *cam, const struct World *world);

#endif
