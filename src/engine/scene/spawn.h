#ifndef ENG_SPAWN_H
#define ENG_SPAWN_H

#include "engine/gfx/camera.h"
#include "engine/scene/entity.h"
#include "engine/scene/ent.h"
#include "engine/scene/scene.h"

/*
 * Entities made while playing, after the place was built: a quest's
 * ambush on a road, foes that come out of the wild. Read docs/spawn.md
 * before editing.
 *
 * A place's own entities are its builder's array (Scene.entities). These
 * are a small pool of the engine's beside it, SPAWN_MAX at fixed
 * addresses (so a Body.self can point at one), in the same frame as the
 * scene: the engine draws them, bodies stop at them and solid entities
 * keep apart from them as from the scene's own.
 *
 * A spawn is named by a SpawnId, which reads as gone (spawn_get answers
 * 0) once it is freed, so the game needs no callback. It is freed by
 * spawn_remove; by its leash, when the camera is further off across the
 * ground (checked each frame, spawn_frame from draw_frame); and always
 * when the place closes (another scene, another heap by mem_epoch, a
 * stream opened or closed). A stream's origin move shifts them with the
 * chunks.
 *
 * Nothing is kept or saved by the engine: no keep row is written for a
 * spawn. A game that wants one back tracks it itself and spawns again.
 * Its pictures are the game's too, made resident as any entity's.
 *
 * spawn_find picks where one may stand: on a floor in a ring round a
 * point, caught in nothing, near no solid entity, and if asked out of
 * the camera's sight or out of water.
 *
 * Plain C, no Glide: hostcheck runs it.
 */

/* The registry's transient pool (ent.h): spawn_* is a wrapper over it, kept while the games move to ent_*. */
#define SPAWN_MAX ENT_POOL

/* An EntId of a pool entry: 0 for none; else its index plus one, and its generation in bits 16..23. */
typedef EntId SpawnId;

/*
 * A copy of e, live in scene from now on, freed when the camera is more
 * than leash away across the ground (0: never by distance). Its live
 * flag is as e has it: set it once its pictures are read. 0 when the
 * pool is full or scene is 0.
 */
SpawnId spawn_add(const Scene *scene, const Entity *e, float leash);
/* The spawn's entity, the game's to move and pose; 0 once it is gone. */
Entity *spawn_get(SpawnId id);
/* Gone now. */
void spawn_remove(SpawnId id);
/* Every spawn gone: a new game, a place left. */
void spawn_clear(void);
/*
 * The spawns in order, for the game's loops: from i = 0, the next live
 * one's id in *id and the i to pass next; 0 when there are no more.
 *     for (i = 0; (i = spawn_next(i, &id)) != 0;) { e = spawn_get(id); ... }
 */
int spawn_next(int i, SpawnId *id);
/* How many there are. */
int spawn_count(void);

/*
 * The engine's walkers' view of the pool: the entries (free ones not
 * live, so skipped as a free entry of Scene.entities is) and how many,
 * when scene is the pool's; 0 otherwise. Draw, the texture notes, a
 * body's push and entity_blocked read it beside Scene.entities.
 */
const Entity *spawn_entities(const Scene *scene, int *n);
/* Once a frame (draw_frame): spawns past their leash from cam are freed. */
void spawn_frame(const Scene *scene, const Camera *cam);
/* The stream's origin moved: what stands in scene moves by -dx, -dz with it. */
void spawn_shift(const Scene *scene, float dx, float dz);

enum {
    /* Out of the camera's sight: outside its view, or a wall between. */
    SPAWN_UNSEEN = 1,
    /* Not in water, not even its feet. */
    SPAWN_DRY = 2
};

typedef struct SpawnSpot {
    /* The ring's centre (the player's feet, say), and its radii across the ground (not near and far: windows.h makes those macros). */
    float x, y, z;
    float inner, outer;
    /* The body that must fit: across and tall. */
    float radius, height;
    /* How far above or below y its floor may be. */
    float climb;
    /* SPAWN_UNSEEN, SPAWN_DRY. */
    int flags;
    /* Where round the ring the tries start: the game's own roll. */
    df_u32 seed;
} SpawnSpot;

/*
 * Where something of want's size may stand: on a floor within climb of
 * want->y, the floor under its middle and round it, its body from a
 * third of its height up caught in nothing, no solid entity (nor the
 * player's body) where it stands, and want's flags met. 16 directions at
 * 3 radii, near first. 1 and the feet in *x, *y, *z; 0 when none fits.
 * cam is only read for SPAWN_UNSEEN. Uses collide's gather: call it
 * between body steps. For an event, not every frame: up to 48 gathers.
 */
int spawn_find(const Scene *scene, const Camera *cam, const SpawnSpot *want, float *x, float *y, float *z);

#endif
