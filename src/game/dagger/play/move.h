#ifndef GFALL_MOVE_H
#define GFALL_MOVE_H

#include "game/dagger/play/steer.h"

#include "engine/gfx/camera.h"
#include "engine/scene/body.h"

/*
 * player_movement_update on the engine body. +Y is up. Classic's
 * gravity matches the body's (392 world units a second squared, the
 * << 8 integrator's 100352/256). The jump's magnitude is converted
 * in jump_ups.
 *
 * Climbing uses the body's actual wall contact and normal, the original
 * approach angle, hold delay and skill/fall checks. Swimming, levitation
 * and climbing use full-body sweeps without the walker's floor snapping.
 *
 * Head bobbing uses view-flags bit 1 and the original int32 sample
 * curve, on the camera only. Motion snapshots carry its phase.
 *
 * Classic's step of 5 or 10 is how far one collide call slides, and
 * body_step covers the whole wish in one sweep, gravity included.
 * Calling it twice a frame would fall twice. Crouch still halves
 * the height and uses the slower speed (SPD + 50 against SPD + 150).
 */

typedef struct MoveIn {
    const unsigned char *held;
    const unsigned char *pressed;
    int region;
    int view_mode;
    int mouse_dx;
    int mouse_dy;
    float dt;
    int bar_blocks;
    /* Cursor-held steering fractions from intrface_steer; keys use 255. */
    int mouse_steer;
    int weight_left, weight_right, weight_up, weight_down;
} MoveIn;

/* Portable motion state; Body's scene/entity pointers never enter a save. */
typedef struct MotionState {
    float feet[3], safe[3];
    float eye, vy, top, vx, vz, bob_ms;
    int yaw, pitch, airborne, crouched, water, weight, grounded, have_safe;
} MotionState;
void move_capture(MotionState *out);
void move_restore(const MotionState *motion);

/* Standing speed in world units a second, before the steer weight. */
int ground_speed(int spd, int crouched, int running, int run_skill, int in_water, int swim_skill);
/* The steer region's scale of that speed. 0 for a turn in place. */
int region_speed(int region, int base);

void move_init(void);
Body *move_body(void);
/* A streamed place moved its local origin; motion and saved pose move once. */
void move_shift(float dx, float dz);
void move_place(float x, float eye_y, float z, int classic_yaw);
/* Consume an engine teleport arrival, keeping the original view angles and
 * crouch. The builder's target Y is a standing eye, not a foot position. */
int move_take_teleport(void);
int move_yaw(void);
int move_airborne(void);
int move_crouched(void);
int move_in_water(void);
/* 1 on the frame the body enters water. Sound 86. */
int move_splashed(void);
/* Carried weight. Over 250 sinks unless float-down is held. */
void move_set_weight(int weight);
int move_climbing(void);

/* One frame. scene may be 0 in a test that only checks the sums. */
void move_frame(const Scene *scene, const MoveIn *in);
/* The camera on the body's eye, plus the classic pitch. */
void move_camera(Camera *cam);

#endif
