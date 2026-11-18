#ifndef GFALL_AI_H
#define GFALL_AI_H

/* Original detection and wall-following on generic engine bodies.
 * ai_stealth_check, ai_move_toward_target and ai_center_in_corridor.
 * See docs/game.md. No renderer or platform dependencies. */
#include "game/dagger/play/input/move.h"
#include "engine/scene/body.h"
#include "engine/scene/entity.h"

/*
 * The creatures' clock and angles (ai_creature_think, equip.c). The original steps a creature once a frame in ticks; this port turns a per-tick count into seconds at TICKS_PER_SECOND.
 * A yaw is in 2048ths of a turn: ANGLE_FULL, ANGLE_MASK. A creature sees the player within FOE_SIGHT_ARC of its facing, turns FOE_TURN_PER_TICK a tick, keeps hunting for FOE_GIVE_UP
 * ticks after it saw or lost the player (FOE_GIVE_UP_HURT after it was hurt), and weighs a target at TARGET_SCORE_BASE less a step for each TARGET_SCORE_DISTANCE_DIV of distance, with
 * TARGET_SCORE_IDLE more when nothing else has it and TARGET_SCORE_SEEN more when it is in sight.
 */
#define TICKS_PER_SECOND 60
/* The eight octants a heading falls in. */
#define OCTANTS 8
/* ai_stealth_check: a creature further than STEALTH_RANGE never notices; the player is unnoticed when a roll of 1..100 is over (stealth skill * distance / STEALTH_RANGE) * STEALTH_MULTIPLIER;
 * a player moving faster than 1/STEALTH_MOMENTUM_DIV of the walk speed is always noticed by one that has met them. */
#define STEALTH_RANGE 1024
#define STEALTH_MULTIPLIER 2
#define STEALTH_MOMENTUM_DIV 2
/* monster_pacify_check: the chance is PACIFY_BASE (PACIFY_BASE_ARMED with a weapon drawn) plus Personality / PACIFY_PER_DIV, plus Etiquette / PACIFY_ETIQUETTE_DIV for a class enemy, and the
 * creature is calmed when a roll of 1..PACIFY_ROLL is not over it. The reseed's hash is ours. */
#define PACIFY_BASE_ARMED -25
#define PACIFY_BASE 10
#define PACIFY_PER_DIV 5
#define PACIFY_ETIQUETTE_DIV 10
#define PACIFY_ROLL 200
/* ai_move_toward_target (monster.c): a creature that has seen the player turns in place while more than NAV_TURN_FIRST_ARC off, keeps a detour for NAV_TURNS ticks, is stuck after
 * NAV_STUCK_STEPS failed steps, and tries the directions NAV_DIR_AHEAD, NAV_DIR_BACK (turned NAV_BACK_OFFSET) and NAV_DIR_SIDE (NAV_SIDE_OFFSET) in turn, wrapping at NAV_DIR_WRAP. Within
 * NAV_CENTRE_ARC of its target it centres on the NAV_GRID-unit corridor: under NAV_SNAP it snaps back, under NAV_GRID_HALF it steps back NAV_SNAP, over NAV_SNAP_FAR it snaps forward. A step
 * moved more than STEP_MOVED_MIN a tick. WALK_STOP_SHORT is ours: a creature stops that short of its target. */
#define NAV_TURN_FIRST_ARC 32
#define NAV_TURNS 12
#define NAV_STUCK_STEPS 6
#define NAV_DIR_AHEAD 2
#define NAV_DIR_BACK 4
#define NAV_DIR_SIDE 8
#define NAV_DIR_WRAP 16
#define NAV_BACK_OFFSET 1024
#define NAV_SIDE_OFFSET 1536
#define NAV_CENTRE_ARC 256
#define NAV_GRID 64
#define NAV_SNAP 8
#define NAV_GRID_HALF 32
#define NAV_SNAP_FAR 56
#define STEP_MOVED_MIN 2
#define WALK_STOP_SHORT 60
#define FOE_SIGHT_ARC 512
#define FOE_TURN_PER_TICK 64
#define FOE_GIVE_UP 200
#define FOE_GIVE_UP_HURT 60
#define TARGET_SCORE_BASE 30
#define TARGET_SCORE_DISTANCE_DIV 128
#define TARGET_SCORE_IDLE 5
#define TARGET_SCORE_SEEN 20

typedef struct Mind {
    int detected, encountered;
    float give_up;
    int blocked, direction, turns, stuck;
    float clock;
} Mind;

int ai_marker_pacify(int kind, KeepKey key);
int ai_stealth(int detected, int distance, int encountered);
int ai_octant(float dx, float dz);
int ai_angle(int from, int to);
int ai_turn(int from, int to, int step);
void ai_walk(Mind *mind, Body *body, Entity *entity, const Scene *scene,
                 const float *target, int visible, int flying, float speed, float dt);

#endif
