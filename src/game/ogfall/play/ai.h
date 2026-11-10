#ifndef OGFALL_AI_H
#define OGFALL_AI_H

/* Original detection and wall-following on generic engine bodies.
 * ai_stealth_check, ai_move_toward_target and ai_center_in_corridor.
 * See docs/ogfall.md. No renderer or platform dependencies. */
#include "engine/scene/body.h"
#include "engine/scene/entity.h"

typedef struct OgMind {
    int detected, encountered;
    float give_up;
    int blocked, direction, turns, stuck;
    float clock;
} OgMind;

int og_ai_stealth(int detected, int distance, int encountered);
int og_ai_octant(float dx, float dz);
int og_ai_angle(int from, int to);
int og_ai_turn(int from, int to, int step);
void og_ai_walk(OgMind *mind, Body *body, Entity *entity, const Scene *scene,
                 const float *target, int visible, int flying, float speed, float dt);

#endif
