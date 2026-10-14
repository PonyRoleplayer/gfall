#ifndef DF_PLAYER_H
#define DF_PLAYER_H

#include "engine/platform/input.h"
#include "engine/gfx/camera.h"

/* Fly the camera from one input snapshot. Speed is per vsync. */

void player_tick(Camera *cam, const Input *in);

#endif
