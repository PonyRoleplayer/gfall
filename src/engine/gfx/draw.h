#ifndef DF_DRAW_H
#define DF_DRAW_H

#include "engine/gfx/camera.h"

struct Level;

/*
 * Longest visible list one frame draws, for models and for sprites.
 * Past it the rest are not drawn. A count equal to this was cut.
 */
#define DRAW_VIS_MAX 2048

/* Transform, clip to the near plane, project, submit, swap. */

void draw_frame(const Camera *cam, const struct Level *level);

/* Called after the frame, before the buffer swaps. 0 clears it. */
void draw_set_extra(void (*fn)(void));
/* Called after the scene and before the HUD. 0 clears it. */
void draw_set_under(void (*fn)(void));
/* Far clip in world units. Default is DF_FAR. Clamped to DF_FAR_MAX. */
void draw_set_far(float far);
float draw_far(void);
/* 1 drops ARCH3D faces whose normal points away. 0 draws them. */
void draw_set_faces(int on);
int draw_faces(void);
/* Counts from the last world frame. A frozen console keeps these. */
void draw_stats(int *models, int *sprites, int *kept, int *dropped);

#endif
