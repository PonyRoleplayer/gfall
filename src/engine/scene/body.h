#ifndef ENG_BODY_H
#define ENG_BODY_H

#include "engine/scene/scene.h"

/*
 * A body that walks a place under gravity: the player's, a creature's,
 * anything the game moves on its feet. Read docs/collision.md before
 * editing.
 *
 * As many as the game wants, each its own size. A frame takes the motion
 * the game wants across the ground and does the rest: the upper body,
 * from a step above the feet to the top of the head, a swept ellipsoid
 * (collide.c) that walls and ceilings stop; the feet on the highest
 * floor under the footprint, up a step or down one walking, anywhere in
 * the frame's fall falling; gravity; a jump; the landing's speed and
 * height; out of anything it is caught in; back on the ground when it
 * falls out of the place. Solid entities stop it (Entity.radius, all but
 * its own), and so does the walker's body when it is not that one.
 *
 * The engine steps none of them but the walker's (walker.h, the viewers'
 * camera). A game steps its own in its frame, then puts its camera on
 * one (rig.h) and its entity where its body is.
 *
 * Plain C, no Glide: hostcheck runs it.
 */

struct Entity;

typedef struct Body {
    /* Its size: across (radius), tall, the highest step walked up, the eye above the feet. */
    float radius;
    float height;
    float step;
    float eye;
    /* The entity it moves, skipped when solid entities push it; 0 for none. */
    const struct Entity *self;
    /* The point between its feet, and its fall speed (up positive). */
    float feet[3];
    float vy;
    /* The top of its arc while off the ground; the last place it stood on firm floor. */
    float top;
    float safe[3];
    /* A landing not yet taken (body_landed): the speed down at the floor, the height fallen. */
    float land_speed;
    float land_height;
    /* The place it is in, and its heap: another of either takes it out of what it is caught in. */
    const Scene *scene;
    df_u32 epoch;
    df_u8 grounded;
    df_u8 landed;
    df_u8 have_safe;
    df_u8 want_unstick;
} Body;

/*
 * A body of radius across, height tall, walking up steps of step, its
 * eye eye above its feet; standing nowhere until body_place.
 */
void body_init(Body *b, float radius, float height, float step, float eye);
/* Its feet at x, feet_y, z: set, not walked; out of anything it is caught in on its next step. */
void body_place(Body *b, float x, float feet_y, float z);
/*
 * One frame in scene: the motion across the ground the game wants
 * (world units this frame, not a speed), then gravity, the floor and
 * what stops it. dt is real seconds.
 */
void body_step(Body *b, const Scene *scene, float want_dx, float want_dz, float dt);
/* A jump from a floor at up_speed (units a second); 0 off the ground. */
int body_jump(Body *b, float up_speed);
/* 1 once at a landing: the speed down at the floor and the height fallen from the arc's top. */
int body_landed(Body *b, float *speed, float *height);
/* Its eye: the feet, eye above. */
void body_eye(const Body *b, float *x, float *y, float *z);
/* Out of whatever it is caught in, on its next step. */
void body_unstick(Body *b);

#endif
