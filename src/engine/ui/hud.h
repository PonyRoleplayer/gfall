#ifndef DF_HUD_H
#define DF_HUD_H

#include "engine/gfx/camera.h"

/* Position and look, in the corner, before the buffer swaps.
   hud_set(0) skips the draw. GFALL leaves it on. LEVELFALL draws its own. */

void hud_set(int on);
int hud_shown(void);
/* The fps corner. F3 flips it too. Default on. */
void hud_set_fps(int on);
int hud_fps(void);
/*
 * The fps counter a game turns on by itself (gfx_set_fps_counter): the
 * same box as the HUD's corner, drawn with the HUD off, over a menu
 * too. Default off. hud_counter_draw is draw.c's, once a frame.
 */
void hud_set_counter(int on);
int hud_counter(void);
void hud_counter_draw(void);
void hud_draw(const Camera *cam);

#endif
