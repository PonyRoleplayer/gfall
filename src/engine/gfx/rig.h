#ifndef ENG_RIG_H
#define ENG_RIG_H

#include "engine/gfx/camera.h"
#include "engine/scene/body.h"

/*
 * A camera put on a body, each frame after the body's step: first person
 * at its eye, or third person behind it. Read docs/collision.md (Bodies
 * and cameras) before editing. Which way the camera turns (yaw, pitch)
 * is the game's.
 */

/* At the body's eye, the camera's own yaw and pitch kept: first person. */
void rig_first(Camera *cam, const Body *b);
/*
 * Behind the body and looking at its eye along yaw and pitch, distance
 * away: third person. A wall or a ceiling between them pulls it in
 * (collide_ray over scene's models), RIG_MARGIN short of what it met, so
 * it never sits inside a wall; never nearer the eye than RIG_NEAREST.
 */
#define RIG_MARGIN 12.0f
#define RIG_NEAREST 16.0f
void rig_third(Camera *cam, const Body *b, float yaw, float pitch, float distance, const Scene *scene);

#endif
