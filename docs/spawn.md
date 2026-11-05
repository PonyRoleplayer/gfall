# Spawned entities

A place's entities are made when it is built: a dungeon's creatures from its blocks, a town's people from its pool, each in the array its builder gives the scene (`Scene.entities`). Some come later, while the player is there: a quest's ambush on a road, foes out of the wild. Those are spawned, into a small pool of the engine's beside the scene's own (`src/engine/scene/spawn.c`, `spawn.h`).

A town's guards are not this: they come from the people pool (`people_spawn_guard`). A dungeon's quest foes are not either: they come through the dungeon stream's hook as its blocks open.

## The pool

`SPAWN_MAX` (16) entities, static, at fixed addresses, so a game's `Body.self` can point at one. They stand in the scene's frame, as its own do. Nothing is allocated.

- `spawn_add(scene, &e, leash)` copies `e` into a free entry and returns its `SpawnId`; 0 when the pool is full. Its `live` flag is as `e` has it: set it once its pictures are read, as for any entity.
- `spawn_get(id)` is the entity, the game's to move and pose each frame; 0 once it is gone.
- `spawn_remove(id)`, `spawn_clear()`.
- `spawn_next(i, &id)` walks them for the game's own loops (its minds, its combat, its loot): `for (i = 0; (i = spawn_next(i, &id)) != 0;)`.

A `SpawnId` is the entry plus one in its low byte and the entry's generation, bumped at each add, in the next. An id kept after its spawn went reads as gone even when the entry is used again, so the game needs no callback: it asks `spawn_get` and drops its own record on 0.

## When one goes

- `spawn_remove`.
- Its leash: `draw_frame` calls `spawn_frame` each frame, and a spawn further from the camera than its leash across the ground is freed. 0 is never by distance. A leash a little under the stream's reach frees it before its floor goes.
- Its place: the pool notes the scene and the heap's epoch (`mem_epoch`) it was filled under. Any call that finds the epoch moved empties it, and an add into another scene empties it first. A stream's open and close empty it (`stream_close`). A stream's reload (a season) keeps it: the floors come back where they were.

## The stream

An origin move shifts the spawns by the chunk amount with everything else (`spawn_shift` from `stream.c`'s rebase). A game's bodies for them are the game's to shift, in the stream source's `shift` hook, as it does its other bodies.

## What sees them

The engine walks the pool beside `Scene.entities` wherever it walks those (`spawn_entities(scene, &n)`, the whole pool, free entries not live):

- `draw.c`'s entity pass: drawn, and their pictures noted for the card's residency in a stream.
- `body.c`'s push: a body stops at a solid spawn (`Entity.radius`), and a spawn's own body at the others.
- `entity_blocked`: solid entities keep apart from them.

Not read: `texcache.c`'s marking of a whole place's entity pictures, at its load, when the pool is empty. A spawn's pictures are the game's to make resident (`wres_acquire` in a stream, `texcache_resident_add` in a whole place, whose `Level` must then have the slot). Nor `draw_prime`.

## Keep and saves

The engine keeps nothing of a spawn: `keep.c` writes no row for one, and a save holds none. A game that wants one back (a quest's foes) tracks it itself and spawns it again.

## Where one may stand

`spawn_find(scene, cam, &want, &x, &y, &z)` with a `SpawnSpot`: the ring's centre and its radii across the ground (`inner`, `outer`), the body's `radius` and `height`, how far above or below the centre its floor may be (`climb`), `flags`, and a `seed` for where round the ring the tries start.

It tries 16 directions on 3 rings (inner, between, outer), inner first, the directions seven at a time round the ring so the tries spread. A spot is taken when:

1. A floor is under its middle within `climb` of the centre's height, and under four points round it at 0.7 of its radius within a third of its height of that floor: it does not hang over an edge.
2. Its body from a third of its height up is caught in nothing (`collide_overlap`). A third is a person's step (24 of 72): a slope or a kerb under it is walked onto, not a wall.
3. No solid entity of the scene's or the pool's, nor the player's body (`walker_body`), stands where it would.
4. `SPAWN_DRY`: no water over its feet (`scene_water_body`).
5. `SPAWN_UNSEEN`: the camera cannot see it. It is outside the view across the ground (more than half `camera_fov` and 10 degrees off the camera's yaw), or the place stands between the camera's eye and both its head and its middle. The ray is gathered and cast 256 units at a time, so no gather holds a whole road (`COLLIDE_SET`). The view's height is not tested.

In a stream a spot can only be in a loaded chunk: nowhere else has floor.

It uses collide's gather, as a body's step does: call it between steps. It is for an event, not for every frame: up to 48 tries, each a small gather, and two rays for `SPAWN_UNSEEN`.

## Tests

hostcheck, `check_spawn`, on a floor 1200 across with a wall at x 200:

- **The pool:** it holds 16 and refuses a 17th; a freed spawn's old id reads as gone; `spawn_next` walks every spawn.
- **When one goes:** past its leash a spawn goes and one with no leash stays; an origin move moves it; another place or a new heap empties the pool.
- **Solid:** another entity cannot walk into a spawn.
- **`spawn_find`:**
  - stands one on the near ring's floor;
  - finds nothing past the floor's edge;
  - never puts one in the wall;
  - never puts one on another solid entity;
  - with `SPAWN_DRY`, never puts one in a pond;
  - with `SPAWN_UNSEEN`, never puts one in the camera's view, and takes a spot behind the wall when the camera looks at it.
