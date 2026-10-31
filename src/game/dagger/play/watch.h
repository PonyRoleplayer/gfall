#ifndef DF_WATCH_H
#define DF_WATCH_H

#include "engine/gfx/camera.h"
#include "engine/scene/entity.h"

/*
 * A street killing and the city watch it draws. Read docs/game.md (People)
 * before editing.
 *
 * A citizen dies in combat.c: 1 health, a to-hit roll at 100, the weapon's
 * damage. This module does not roll that blow. It brings a guard and queues
 * three HALT lines, spoken one at a time as they close, a second apart.
 * The court, the fine and the bounty are not this.
 *
 * Plain C: hostcheck runs it. Nothing here is drawn.
 */

#define WATCH_HALTS 3

void watch_reset(void);
/* The next unsaid HALT, once a second after the first. */
void watch_frame(float dt);
/* A citizen's health reached 0 at `at`. The nearest guard comes, or one is spawned. */
void watch_on_kill(const Camera *cam, const float *at);
/* How many HALT lines the last killing queued, and line i ("HALT"). */
int watch_halts(void);
const char *watch_halt(int i);
/* The guard who came, or 0. */
Entity *watch_guard(void);

#endif
