#ifndef ENG_ENT_H
#define ENG_ENT_H

#include "engine/gfx/camera.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/scene/keep.h"

/*
 * The registry: every entity in a place, under one id. Read
 * docs/entities.md, The registry, and docs/entity-plan.md before editing.
 *
 * Three kinds of storage read the same to every engine walk (draw, a
 * body's push, entity_blocked, the use ray, the texture notes):
 *
 * - the place's own array, Scene.entities: a builder's (a dungeon loaded
 *   whole) or the stream's slots (a block's creatures). The registry
 *   never owns it; it only names its entries.
 * - ranges a game registers beside it (ent_register_range): arrays of its
 *   own it wants drawn, solid and named, up to ENT_RANGES of ENT_RANGE_MAX.
 * - the transient pool, ENT_POOL entries at fixed addresses (so a
 *   Body.self can point at one): what is made while playing (a conjured
 *   creature, a missile, a pile). spawn.h is a wrapper over it.
 *
 * An EntId is an entry's index plus one in its low 16 bits and that
 * entry's generation in bits 16..23. A generation moves when what the
 * entry holds is something else: a transient added, a stream slot filled
 * or emptied (ent_renew), a range registered or dropped, another place.
 * An id kept after its entity went reads as gone (ent_get answers 0), so
 * a game needs no callback.
 *
 * The pool and the ranges belong to the scene and heap epoch they were
 * filled under (mem_epoch): any call that finds the epoch moved drops
 * them, and an add into another scene empties the pool first.
 *
 * Plain C, no Glide: hostcheck runs it.
 */

#define ENT_POOL 64
/* Entries of Scene.entities that get ids: a dungeon loaded whole holds up to 1024, a stream 9 slots of 64. */
#define ENT_SCENE_MAX 2048
#define ENT_RANGES 8
#define ENT_RANGE_MAX 256
/* Every index an id can name. */
#define ENT_IDS (ENT_POOL + ENT_SCENE_MAX + ENT_RANGES * ENT_RANGE_MAX)
/* The most arrays ent_lists hands out: the scene's, the pool, the ranges. */
#define ENT_LISTS (2 + ENT_RANGES)

typedef df_u32 EntId;

/* One array of entities for a walk: n entries from first, the free ones not live. */
typedef struct EntList {
    const Entity *first;
    int n;
} EntList;

/*
 * scene is the place now: its entries get ids, and the pool and ranges of
 * any other scene are gone. ent_frame does it each frame; a host test or
 * a builder that wants ids before the first frame calls it.
 */
void ent_place(const Scene *scene);

/*
 * The arrays every engine walk reads for scene, at most ENT_LISTS: the
 * scene's own, the pool when scene is its place and it holds any, the
 * ranges registered for scene. How many.
 */
int ent_lists(const Scene *scene, EntList *out, int cap);

/*
 * An array of the game's, n entries (at most ENT_RANGE_MAX), drawn, solid
 * and named from now on in scene. residence (ENT_HOME_*) is for the
 * entries that leave theirs at 0. 1, or 0 when all ENT_RANGES are taken.
 * The array stays the game's: it moves, poses and frees its entries.
 */
int ent_register_range(const Scene *scene, Entity *first, int n, int residence);
/* The range starting at first is no longer walked; its ids read as gone. */
void ent_unregister_range(Entity *first);

/*
 * A copy of e into the pool, in scene from now on, freed by its leash
 * (Entity.leash; 0 never by distance), ent_remove or the place's close.
 * Its live flag is as e has it. 0 when the pool is full or scene is 0.
 */
EntId ent_add(const Scene *scene, const Entity *e);
/* The entity, the game's to move and pose; 0 once it is gone. */
Entity *ent_get(EntId id);
/* A pool entity gone now; an array's entry is its owner's to free. */
void ent_remove(EntId id);
/* The pool emptied and the ranges dropped: a new game, a place left, a stream opened or closed. */
void ent_clear(void);
/* How many pool entries are in use. */
int ent_count(void);

/*
 * Every entity in order, for a game's loops: from i = 0, the next one's
 * id in *id and the i to pass next; 0 when there are no more. An array's
 * entry is walked while it is live, a pool entry while it is in use.
 *     for (i = 0; (i = ent_next(i, &id)) != 0;) { e = ent_get(id); ... }
 */
int ent_next(int i, EntId *id);
/* The id of an entity in the registry's storage; 0 for any other address. */
EntId ent_id(const Entity *e);
/* The entity with key (not 0, 0) in the registry's storage; 0 when none. For events, not each frame: a walk. */
EntId ent_by_key(KeepKey key);

/*
 * Entries first .. first + n - 1 of scene's own array hold other things
 * now: their old ids read as gone. The stream calls it as a slot fills
 * and empties; a builder that refills Scene.entities in place calls it too.
 */
void ent_renew(const Scene *scene, int first, int n);

/* Once a frame (draw_frame): scene made the place (ent_place), and pool entries past their leash from cam freed. */
void ent_frame(const Scene *scene, const Camera *cam);
/* The stream's origin moved: the pool moves by -dx, -dz with it. An array is its owner's to move. */
void ent_shift(const Scene *scene, float dx, float dz);
/*
 * The near-gather, for a body's push and entity_blocked: every entity
 * with a radius whose feet stood within r of x, z (plus the largest
 * radius and what it may have moved since the grid was built) into out,
 * at most cap; how many. Candidates only: the caller tests each by its
 * own rule (entity_solid, its height, the circles). -1 when there is no
 * grid to ask (before the first ent_frame, too many entities, a reach
 * across too many cells, more than cap): walk ent_lists instead.
 * The grid is built at the first query after ent_frame, and again after
 * anything was added, removed, renewed or registered. A game that moves
 * entities far without a frame between (a long rest stepped in a loop)
 * calls ent_regather first.
 */
int ent_near_solid(const Scene *scene, float x, float z, float r, const Entity **out, int cap);
void ent_regather(void);
/*
 * Kept entities the game made (docs/entities.md, Kept): a pool entity
 * with ENT_KEPT and a key is written to the keep table (Keep.made) when
 * the chunk it stands in drops, or when the place is kept now, and freed
 * with its chunk. When a chunk fills, every row filed under it is
 * offered to the game's hook, which fills in out's look (its poses, its
 * size, its use, its leash) from row->key and row->owner and answers 1;
 * the engine then puts it back where it stood, as it was (its feet,
 * facing, dead, act, frame, flags, owner, residence), into the pool. 0
 * declines: a row of another place (the stream does not know which place
 * it is; the game does, from row->key.place), or one not wanted now.
 * Rows stay in the table while their entity lives: a save holds them.
 * An entity of ENT_HOME_PLACE is never dropped with a chunk.
 */
typedef int (*EntityReviveHook)(const Keep *row, Entity *out);
void ent_set_revive_hook(EntityReviveHook fn);
/*
 * The stream's: chunk cx, cz (chunk units a side) dropped or filled, the
 * scene's origin at ox, oz in the place's frame. Dropped writes and
 * frees the kept made entities standing in it; filled revives its rows.
 */
void ent_chunk_dropped(const Scene *scene, int cx, int cz, int chunk, float ox, float oz);
void ent_chunk_filled(const Scene *scene, int cx, int cz, int chunk, float ox, float oz);
/*
 * Every live kept made entity of the pool written now, before a save or
 * before leaving a place: under the chunk it stands in (chunk > 0, a
 * stream), or under the place (chunk 0, a place loaded whole). The scene's
 * origin is ox, oy, oz in the place's frame. Nothing is freed.
 */
void ent_keep_now(const Scene *scene, int chunk, float ox, float oy, float oz);
/* A place loaded whole is built: its rows filed under the place (made 2) offered to the hook, as a chunk's are. */
void ent_revive_place(const Scene *scene, float ox, float oy, float oz);

/* The pool's array (ENT_POOL entries, free ones not live) when scene is its place; 0 otherwise. */
Entity *ent_pool(const Scene *scene, int *n);

#endif
