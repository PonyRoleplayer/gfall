#ifndef GFALL_MOVE_H
#define GFALL_MOVE_H

#include "game/dagger/play/input/steer.h"

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
 * View combines independent movement axes with acceleration and air control.
 * Cursor uses the original regions and keeps takeoff velocity in the air.
 * Head bobbing uses view-flags bit 1 and affects the camera only. Motion
 * snapshots carry the Cursor sample phase. View bob eases in after a load.
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
    int view_mode; /* Selected scheme, including View with its HUD pointer up. */
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
    df_u32 mount;
} MotionState;
void move_capture(MotionState *out);
void move_restore(const MotionState *motion);

/*
 * The walk (func_00010585.c). Standing speed is WALK_ADD + (SPD - ATTRIBUTE_NEUTRAL), WALK_ADD being the initial value of FALL.EXE's D_00187CA9, so it is SPD + WALK_SPEED_ADD
 * (150): a creature's chase speed and the stealth momentum are that same sum. Crouched it is SPD + CROUCH_SPEED_ADD. Running scales it by (RUN_BASE + running skill * 256 / RUN_SKILL_DIVISOR)
 * over 256, and in water a quarter of it plus the swimming skill's share.
 */
/* Classic angles: ANGLE_FULL to a turn (the compass, the view's yaw, a creature's facing), with its half and quarter and the mask that wraps one. */
#define MS_PER_SECOND 1000.0f
#define ANGLE_FULL 2048
#define ANGLE_HALF 1024
#define ANGLE_QUARTER 512
#define ANGLE_MASK (ANGLE_FULL - 1)
/*
 * The classic steering grid is nine regions, three by three (intrface.c): the forward cell's speed is scaled by weight over STEER_FORWARD_DIV (a little faster than straight), the others over
 * STEER_DIV; the keys give every weight STEER_WEIGHT_KEYS. A slide turns the heading by a quarter turn, the back by a half. Climbing: a wall probe ours (CLIMB_PROBE_PAD past the body), the
 * player standing nearly still (moved under CLIMB_MOVED_MAX), facing the wall within CLIMB_COS_MIN/256, CLIMB_DELAY_TICKS of BIOS ticks (BIOS_TICKS_PER_SECOND) against it, then rising at
 * 1/CLIMB_SPEED_DIV of the speed; a Khajiit has KHAJIIT_CLIMB_BONUS more skill and a climber may slip once a minute when two rolls pass CLIMB_SLIP_ROLL and the skill. A jump costs JUMP_FATIGUE,
 * floating FLOAT_SPEED a second, and a swimmer over SWIM_SINK_WEIGHT carried sinks at FLOAT_SPEED unless levitating or water-walking. Ours: the slowfall cap SLOWFALL_SPEED, the crouch's
 * CROUCH_SCALE, the view's pitch limit and key speed.
 */
enum { STEER_FORWARD_LEFT, STEER_FORWARD, STEER_FORWARD_RIGHT, STEER_TURN_LEFT, STEER_CENTRE, STEER_TURN_RIGHT, STEER_SLIDE_LEFT, STEER_BACK, STEER_SLIDE_RIGHT };
#define STEER_WEIGHT_KEYS 255
#define STEER_FORWARD_DIV 220
#define STEER_DIV 256
#define CLIMB_PROBE_PAD 8
#define CLIMB_MOVED_MAX 5
#define CLIMB_COS_MIN 251
#define CLIMB_DELAY_TICKS 14
#define BIOS_TICKS_PER_SECOND 18.2065f
#define CLIMB_SPEED_DIV 3
#define KHAJIIT_CLIMB_BONUS 30
#define CLIMB_SLIP_ROLL 95
#define JUMP_FATIGUE 11
#define FLOAT_SPEED 80
#define SWIM_SINK_WEIGHT 250
#define SLOWFALL_SPEED 60
#define CROUCH_SCALE 0.5f
#define PITCH_LIMIT 256
#define LOOK_KEY_SPEED 160.0f
#define WALK_ADD 200
#define ATTRIBUTE_NEUTRAL 50
#define WALK_SPEED_ADD (WALK_ADD - ATTRIBUTE_NEUTRAL)
#define CROUCH_SPEED_ADD 50
#define RUN_BASE 320
#define RUN_SKILL_DIVISOR 200
#define WATER_SPEED_SHIFT 2

/* Standing speed in world units a second, before the steer weight. */
int ground_speed(int spd, int crouched, int running, int run_skill, int in_water, int swim_skill);
/* The steer region's scale of that speed. 0 for a turn in place. */
int region_speed(int region, int base);

void move_init(void);
Body *move_body(void);
/* Mount a carried horse or cart outdoors, or return to foot with item 0. */
int move_mount(const Scene *scene, df_u32 item);
enum { MOUNT_FOOT, MOUNT_HORSE, MOUNT_CART };
int move_mounted(void);
float move_mount_speed(void);
#define RIDE_EYE_ADD 50.0f
#define HORSE_SPEED_ADD 225
#define CART_SPEED_ADD 100
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

/* One frame. Returns true when the player moved under sprint power. */
int move_frame(const Scene *scene, const MoveIn *in);
/* The camera on the body's eye, plus the classic pitch. */
void move_camera(Camera *cam);

#endif
