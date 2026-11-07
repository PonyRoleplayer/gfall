#ifndef DF_CONJURE_H
#define DF_CONJURE_H

#include "engine/gfx/camera.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"

/*
 * Creatures that come while playing, not from a block's markers: what an
 * interrupted rest brings, as classic and DFU do (DaggerfallRestWindow,
 * RandomEncounters.ChooseRandomEnemy, a FoeSpawner). One kind from the
 * dungeon's encounter table by the player's level, made in the engine's
 * spawn pool (spawn.h) out of the player's sight on a floor near them,
 * with a key of its own (dunstream_conjured_key): combat, its body and
 * the foes treat it as one of the dungeon's. Read docs/game.md (Rest).
 *
 * The play loops over creatures walk the stream's slots and then the
 * spawn pool, as one more group: creature_groups, creature_group.
 */

/* Where it happens, set by the game each frame (its scene and camera). */
void conjure_use(const Scene *scene, const Camera *cam);
/*
 * A creature for an interrupted rest beside feet, for a player of level: drawn, made, awake and
 * hunting. Its entity, or 0 when there is no dungeon (outdoors none yet), no table, no picture or no
 * room out of sight.
 */
Entity *conjure_rest_foe(const float *feet, int level, unsigned seed);

/* The groups of creatures: the stream's slots, then the spawn pool. */
int creature_groups(void);
/* Group g's entities and how many (free pool entries are not live); 0 for none. */
Entity *creature_group(int g, int *n);

#endif
