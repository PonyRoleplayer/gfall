#ifndef GFALL_ACTIONS_H
#define GFALL_ACTIONS_H

/* Original dungeon links, separate from engine movers. The adapter supplies
 * raw action records and resolved model/mover/next indices. See docs/game.md. */
#include "engine/scene/scene.h"
#include "engine/gfx/camera.h"

#define ACTIONS 1024
#define USE_ACTION 0x4f4800
typedef struct ActionInfo {
    KeepKey key;
    int model, mover, next;
    int trigger, kind, axis, duration, magnitude, sound;
} ActionInfo;

int actions_open(const Scene *scene, const ActionInfo *actions, int count);
void actions_close(void);
int actions_trigger(const Scene *scene, int model, int event);
int actions_accepts(int trigger, int event);
void actions_step(float seconds);
void actions_capture(void);
void actions_strike(const Scene *scene, const Camera *camera, int event);

#endif
