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
    /*
     * Where the eye is now (body_eye, so the camera): eye itself but just
     * after body_set_size, when it eases there at eye_speed units a second
     * in body_step, so a crouch or a stand does not pop the view.
     * BODY_EYE_SPEED from body_init; 0 snaps.
     */
    float eye_now;
    float eye_speed;
    /*
     * Its pull down, units a second squared: WALK_GRAVITY (9.8 m/s²) from
     * body_init; a game sets its own (Daggerfall Unity's player falls at
     * 20 m/s², 800). 0 floats.
     */
    float gravity;
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
/* Free flight: one full-body sweep, without walking steps or floor snapping. */
void body_fly_step(Body *b, const Scene *scene, float dx, float dy, float dz);
/* A jump from a floor at up_speed (units a second); 0 off the ground. */
int body_jump(Body *b, float up_speed);
/* 1 once at a landing: the speed down at the floor and the height fallen from the arc's top. */
int body_landed(Body *b, float *speed, float *height);
/* Its eye: the feet, eye above. */
void body_eye(const Body *b, float *x, float *y, float *z);
/* Out of whatever it is caught in, on its next step. */
void body_unstick(Body *b);

/*
 * A new size, its feet where they are: height tall, the eye eye above the
 * feet (a crouch, and the stand after it; the game owns the key and the
 * rules). The collision is the new size at once; the eye eases there
 * (eye_now). A height under a step and a unit is raised to that, an eye
 * over the height lowered to it. Taller than it is, ask body_can_stand
 * first: grown into a low ceiling, it would be caught in it, and its next
 * unstick would move it out of the room.
 */
#define BODY_EYE_SPEED 240.0f
void body_set_size(Body *b, float height, float eye);
/*
 * 1 when the body, height tall where its feet are, is caught in nothing
 * of scene: the headroom to stand. No taller than it is, always 1.
 * Uses collide's gather, as a step does; call it outside one.
 */
int body_can_stand(const Body *b, const Scene *scene, float height);

#endif
