#ifndef ENG_WALKER_H
#define ENG_WALKER_H

#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

/*
 * A body under gravity that follows a camera. Read docs/collision.md
 * before editing.
 *
 * With the walker on (`tcl 1`), each frame takes how far the viewer
 * moved its camera across the ground as where the body wants to go,
 * moves the body there through collide.c, lets gravity own the height,
 * and puts the camera back at the body's eye. So any viewer that hands
 * the console its camera walks without knowing it.
 *
 * The body is two parts, as the engines of Daggerfall's day stood on
 * stairs. The upper body, from a step above the feet to the top of the
 * head, is a swept ellipsoid (collide.c): walls and ceilings stop it
 * wherever it meets them. The feet stand on the highest floor under the
 * body's footprint, up to a step above them (a stair) or, walking, a
 * step below (down a stair); falling, the floor is looked for over the
 * whole of the frame's fall, so no fall goes through one. An ellipsoid
 * alone rolled back off every stair's edge on its round bottom.
 *
 * Daggerfall Unity's scale, 40 world units a metre: a body 1.8 m tall
 * (72 units) and 0.8 m across, the eye 9 cm under the top of the head,
 * gravity 9.8 m/s^2.
 */

#define WALK_UNITS_PER_M 40.0f
#define WALK_RADIUS 16.0f
#define WALK_HEIGHT 72.0f
/* The eye above the feet: 1.71 m. */
#define WALK_EYE 68.4f
#define WALK_GRAVITY (9.8f * WALK_UNITS_PER_M)
/* A fall gets no faster than 50 m/s: about a skydiver's. */
#define WALK_TERMINAL (50.0f * WALK_UNITS_PER_M)
/* The highest step walked up without a jump: 0.6 m. */
#define WALK_STEP 24.0f
/* The upper body's half height: from a step above the feet to the head. */
#define WALK_UPPER ((WALK_HEIGHT - WALK_STEP) * 0.5f)

/* Switch, default off: the camera flies free. */
void walker_set_on(int on);
int walker_on(void);

/*
 * One frame for the camera: dt is real seconds. Moves cam to the body's
 * eye. Does nothing while the walker is off.
 */
void walker_step(Camera *cam, const Scene *scene, float dt);

/*
 * Out of whatever the body is caught in: the nearest place round it
 * where it is free, or the last place it stood. Runs on the next step.
 */
void walker_unstick(void);

/* 1 while the body stands on a floor. */
int walker_grounded(void);

#endif
