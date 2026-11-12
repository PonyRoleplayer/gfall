#ifndef GFALL_AI_H
#define GFALL_AI_H

/* Original detection and wall-following on generic engine bodies.
 * ai_stealth_check, ai_move_toward_target and ai_center_in_corridor.
 * See docs/gfall.md. No renderer or platform dependencies. */
#include "engine/scene/body.h"
#include "engine/scene/entity.h"

typedef struct Mind {
    int detected, encountered;
    float give_up;
    int blocked, direction, turns, stuck;
    float clock;
} Mind;

int ai_stealth(int detected, int distance, int encountered);
int ai_octant(float dx, float dz);
int ai_angle(int from, int to);
int ai_turn(int from, int to, int step);
void ai_walk(Mind *mind, Body *body, Entity *entity, const Scene *scene,
                 const float *target, int visible, int flying, float speed, float dt);

#endif
