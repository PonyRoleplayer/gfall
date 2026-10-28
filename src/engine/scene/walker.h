#ifndef ENG_WALKER_H
#define ENG_WALKER_H

#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

/*
 * The viewers' walking: one Body (body.h) of a person's size, under
 * gravity, that follows a camera. Read docs/collision.md before
 * editing. A game steps bodies of its own (body_step) and puts its
 * camera on one (rig.h); the walker is for the console's camera.
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
/*
 * The camera was put somewhere, not walked there: the next step takes
 * the body to it. A new place (another scene, or another heap) does
 * this by itself.
 */
void walker_place(void);

/* 1 while the body stands on a floor. */
int walker_grounded(void);
/*
 * A jump: from a floor, the body goes up at up_speed (world units a
 * second, WALK_UNITS_PER_M a metre) and gravity brings it down; it rises
 * up_speed^2 / (2 WALK_GRAVITY). 0 when it is not on a floor (or the
 * walker is off). How high is the game's (Daggerfall's Jumping skill).
 * How fast it walks is the game's too: the walker goes where the game
 * moved the camera across the ground, so a run is a longer move.
 */
int walker_jump(float up_speed);
/*
 * 1 once, on the frame the body comes down on a floor after a fall or a
 * jump: the speed down at the floor (world units a second) and the
 * height fallen from the top of its arc. For the game's fall damage.
 * A body set somewhere (a new place, walker_place) owes no landing.
 */
int walker_landed(float *speed, float *height);
struct Body;
/*
 * A game's player (docs/collision.md): player is the Body the game steps
 * and puts its camera on. While it is set:
 * - draw_frame never steps the walker, and never takes a teleport for
 *   the console's camera: the game takes it (interact_teleport) and
 *   places its body;
 * - player is the player's body to the engine: creatures stop at it
 *   (entity_blocked), other bodies push off it, stuck frees it, each by
 *   its own size;
 * - tcl still sets walker_on(), which the game reads as collision on or
 *   off; off, its body is solid to nothing.
 * 0 gives the walking back to the viewers' walker. The game keeps player
 * alive while it is set.
 */
void walker_set_game(struct Body *player);
struct Body *walker_game(void);
/*
 * The player's body's feet, for the game's motors (entity_blocked): 1
 * while collision is on (walker_on) and there is one: a game's, or the
 * walker's once it has stepped.
 */
int walker_body(float *x, float *feet_y, float *z);
/* The player's Body (a game's, or the walker's), so a body does not push off itself through walker_body. */
const struct Body *walker_own_body(void);

#endif
