#ifndef OGFALL_ACTIONS_H
#define OGFALL_ACTIONS_H

/* Original dungeon links, separate from engine movers. The adapter supplies
 * raw action records and resolved model/mover/next indices. See docs/ogfall.md. */
#include "engine/scene/scene.h"
#include "engine/gfx/camera.h"

#define OG_ACTIONS 1024
#define OG_USE_ACTION 0x4f4800
typedef struct OgAction {
    KeepKey key;
    int model, mover, next;
    int trigger, kind, axis, duration, magnitude, sound;
} OgAction;

int og_actions_open(const Scene *scene, const OgAction *actions, int count);
void og_actions_close(void);
int og_actions_trigger(const Scene *scene, int model, int event);
int og_actions_accepts(int trigger, int event);
void og_actions_step(float seconds);
void og_actions_capture(void);
void og_actions_strike(const Scene *scene, const Camera *camera, int event);

#endif
