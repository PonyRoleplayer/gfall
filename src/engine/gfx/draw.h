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
/*
 * Mean and worst world frame in ms over the last whole second of world
 * frames, swap to swap. Returns how many frames that second held, 0
 * before the first second. Console frames are not counted.
 */
int draw_frame_time(float *avg_ms, float *worst_ms);
/*
 * The view the last frame drew: its far clip, and where its fog started
 * and went solid. Under the weather's simulation they are the weather's.
 */
void draw_view(float *far_now, float *fog_near, float *fog_far);
/* Forget what the last level left in draw's caches. gfx_prepare calls it. */
void draw_prepare(void);
/*
 * Hold the frame on the glass, dimmed, with text in the middle
 * ("Loading..."), while a place loads with the card open. Swaps it to
 * the front; nothing else is drawn until the next frame.
 */
void draw_hold(const char *text);
/* The next secs of frames fade in from black. 0 cancels a fade. */
void draw_fade_in(float secs);
/*
 * Uploads the light falloff pictures when want is set (the place has
 * lights). Call after glide_tex_reset and before the level's pictures.
 * 0 when they did not fit: the place is then drawn unlit.
 */
int draw_light_upload(int want);
/* Lights whose decals passed the cull last frame, and decals drawn. */
void draw_light_stats(int *lights, int *decals);

#endif
