#ifndef DF_HUD_H
#define DF_HUD_H

#include "engine/gfx/camera.h"

/* Position and look, in the corner, before the buffer swaps.
   hud_set(0) skips the draw. GFALL leaves it on. LEVELFALL draws its own. */

void hud_set(int on);
void hud_draw(const Camera *cam);

#endif
