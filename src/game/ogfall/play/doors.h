#ifndef OGFALL_DOORS_H
#define OGFALL_DOORS_H

/* Dungeon picking and bashing; movers and collision remain generic engine
 * facilities. Failed attempts live in the mover's Keep.game bytes.
 * See docs/ogfall.md. */
#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

/* Pure original probabilities. Picking uses an inclusive 0..100 roll;
 * bashing uses 1..100. A zero chance means the lock cannot be attempted. */
int og_lock_chance(int skill, int level, int lock);
int og_bash_chance(int lock);
/* Actions preserve the underlying rating while temporarily unlocking a door. */
int og_door_lock(Mover *door, int rating);
int og_door_unlock(Mover *door);
int og_door_relock(Mover *door);
/* 1 when a door handled this use, including refusal. Other movers return 0. */
int og_door_use(const Scene *scene, int usable, int mode);
/* Called at the melee hit frame only when no creature was struck. */
int og_door_strike(const Scene *scene, const Camera *camera);

#endif
