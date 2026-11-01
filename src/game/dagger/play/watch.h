#ifndef DF_WATCH_H
#define DF_WATCH_H

#include "engine/gfx/camera.h"
#include "engine/scene/entity.h"

/*
 * A street killing and the city watch it draws. Read docs/game.md (People)
 * before editing.
 *
 * A citizen dies in combat.c: 1 health, a to-hit roll at 100, the weapon's
 * damage. This module does not roll that blow. It marks the player wanted,
 * turns every living guard hostile, and queues three HALT lines, spoken one
 * at a time as they close, a second apart. Wanted is a keep row, so a
 * building does not clear it. The court, the fine and the bounty are not this.
 *
 * Plain C: hostcheck runs it. Nothing here is drawn.
 */

#define WATCH_HALTS 3

/* Shouts and the guard pointer. Does not clear wanted. */
void watch_reset(void);
/* 1 while the killing's chase is still on. */
int watch_wanted(void);
/* Wanted off, and every hostile guard back to the street. */
void watch_pardon(void);
/* The next unsaid HALT, once a second after the first. */
void watch_frame(float dt);
/*
 * While wanted, in a town: every living guard is hostile and chasing `at`.
 * Outside a town, wanted ends. A building still counts as the town.
 */
void watch_pursue(const Camera *cam, const float *at);
/* A citizen's health reached 0 at `at`. Every guard comes, or 2..5 are spawned. */
void watch_on_kill(const Camera *cam, const float *at);
/* How many HALT lines the last killing queued, and line i ("HALT"). */
int watch_halts(void);
const char *watch_halt(int i);
/* The nearest guard who came, or 0. */
Entity *watch_guard(void);

#endif
