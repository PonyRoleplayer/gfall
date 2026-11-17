#ifndef GFALL_AI_H
#define GFALL_AI_H

/* Original detection and wall-following on generic engine bodies.
 * ai_stealth_check, ai_move_toward_target and ai_center_in_corridor.
 * See docs/game.md. No renderer or platform dependencies. */
#include "engine/scene/body.h"
#include "engine/scene/entity.h"

/*
 * The creatures' clock and angles (ai_creature_think, equip.c). The original steps a creature once a frame in ticks; this port turns a per-tick count into seconds at TICKS_PER_SECOND.
 * A yaw is in 2048ths of a turn: ANGLE_FULL, ANGLE_MASK. A creature sees the player within FOE_SIGHT_ARC of its facing, turns FOE_TURN_PER_TICK a tick, keeps hunting for FOE_GIVE_UP
 * ticks after it saw or lost the player (FOE_GIVE_UP_HURT after it was hurt), and weighs a target at TARGET_SCORE_BASE less a step for each TARGET_SCORE_DISTANCE_DIV of distance, with
 * TARGET_SCORE_IDLE more when nothing else has it and TARGET_SCORE_SEEN more when it is in sight.
 */
#define TICKS_PER_SECOND 60
#define MS_PER_SECOND 1000
#define ANGLE_FULL 2048
#define ANGLE_MASK (ANGLE_FULL - 1)
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
