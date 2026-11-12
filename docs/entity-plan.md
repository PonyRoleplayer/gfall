# Entity management: a plan

A plan for the engine, written for the ENGINE implementer at the user's request (2026-11-11).

**Status:** steps 1..4 and 8 are built on branch `entity-registry` (commits 46531ad, 688e67f, c97facd, 6846e68, 252b08c), on top of GFALL; `make all` and `gfall-host` pass. Sections 3.1..3.3 and 3.5 describe what was built. Steps 5..7 and 9..11 are not built, and GFALL's save does not yet carry the new keep fields (3.5).

The user's model is this:

- Everything that stands or moves in a place is an **entity**.
- An entity is **static** or **non-static**.
- A non-static entity moves, and is **hostile** or **not hostile**.
- Talking, shops, quest ownership and the like are **properties** on an entity, not separate systems.

The picture stays what it is today: billboard sprite sets with five views and their mirrors.

Each claim about the current code cites the file it was read in. Claims I could not check are marked **[unverified]**. Costs are estimates, not measurements (`docs/profile.md`: measure on the target first).

## 1. What exists today

| Piece | What it is | Where |
| --- | --- | --- |
| `Entity` | Feet x, y, z, yaw; 7 poses × 5 views (`EntityPose`: slots, sizes, rate, mirror bits); `radius` (solid when > 0); `phase`, `moving`, `act`, `hold`, `frame`; `live` (drawn); `glow`; `dead` (the game's, kept); `key` (`KeepKey`); `home` (where the file put it). About 270 bytes. It has no hostility, no use, no body, no owner, no leash. | `src/engine/scene/entity.h` |
| The scene's entities | One array the game or the stream provides (`Scene.entities`, `nentities`). The engine only reads it. | `src/engine/scene/scene.h:290` |
| Drawing | `draw_entities` draws `Scene.entities`, then the spawn pool. Each live entity is culled by the far limit and `scene_box_visible` (frustum, then the occlusion cells), and drawn as a flat. | `src/engine/gfx/draw.c:1046..1091`, `src/engine/scene/scene.c:2507` |
| Collision | A body is pushed by `Scene.entities` and the spawn pool (`push_from_list`). A motor asks `entity_blocked` before a step. Both walk **every** entity. | `src/engine/scene/body.c:430`, `src/engine/scene/entity.c:162` |
| Spawn pool | `SPAWN_MAX` 32 entries at fixed addresses, ids with generations. The leash is **per spawn** (`spawn_add`'s `leash`; 0 = never). All are freed when the place closes, a stream opens or closes, or the heap changes. They are moved with the origin. **No keep row is written**. | `src/engine/scene/spawn.h` |
| Keep | One table, a row per changed thing by `KeepKey`. For an entity: position in the place's frame, yaw, dead, moving, act, hold, frame, and 16 game bytes. Saved before a place or chunk drops, put back after it builds. | `src/engine/scene/keep.h` |
| Stream | `StreamCaps.entities`: above 0, each slot owns a range of entities (a dungeon's, `CAP_ENTITIES` 64 a block, `src/game/dagger/places/dunstream.c:36`). At 0 (the world) the game supplies `Scene.entities` (the street people). | `src/engine/scene/stream.h:66..105`, `:209` |
| Street people | `PEOPLE_MAX` 96 in their own pool, on wild's walking cells. Hostility, health, hold, fall and corpse live in people.c's own per-person state (`g_per[].hostile`, `people.c:62`). Guards are spawned into it. | `src/game/dagger/places/people.h`, `people.c` |
| Dungeon creatures | Drawn from markers per block, keyed, in the slot's range. Treasure, 4 spares and 2 effects a block are entities too. `dunstream_kind` looks a creature's kind up by key. A dungeon loaded whole puts its creatures in one array (`MAX_FOES` 1024). | `src/game/dagger/places/dunstream.h`, `world.c:57`, `:1997` |
| Creature minds | dagger `foes.c`: `FOES_MINDS` 24, each with a `Body`, pointing at an `Entity`. GFALL has its own (`ai.c`) **[count unverified]**. | `src/game/dagger/play/foes.c:70..118` |
| Building people | An RMB people record becomes a `Flat` tagged `FLAT_PERSON` plus its faction (`rmb.c:174`). `doors_build` then makes a `USE_TALK` usable on that sprite (`what` = faction, `tag` = file and image for the face). These are not entities. | `src/game/dagger/formats/rmb.c:174`, `src/game/dagger/places/doors.c:604..618` |
| A game's people | `world_set_people_hook(fn)`: while a building's inside is built, the game adds `FLAT_PERSON` flats or changes the block's own. Inside buildings only. | `src/game/dagger/places/world.h:31`, `world.c:1706` |
| Dungeon people | **None.** RDB flats are made with `tag = 0` whatever their action (`rdb.c:460`), so an action-29 NPC (a castle resident) is a plain sprite: no usable, no hook. Verified. | `src/game/dagger/formats/rdb.c:451..466` |
| Rest encounters / quest foes (GFALL) | 8 actor slots (`ENCOUNTERS`). Their identity and start point are saved by GFALL; position and health are in keep rows by key. Live ones sit in the spawn pool with **leash 0**. | `src/game/gfall/play/encounter.h`, `encounter.c:56` |
| Other spawn users | dagger: shots (leash 0), conjured creatures (leash 64 m, `conjure.c:17`). GFALL: ground piles (4), dungeon splashes (8), spell visuals (8) and a light (1). | `src/game/dagger/play/shots.c:131`, `conjure.c:76`, `src/game/gfall/play/ground.c:14`, `dungeon.c:43`, `casting.c:18`, `:279` |

Corrections to the brief:

- **The leash is already per entity**, set on each spawn.
- "A quest's foe placed far from the player is freed by the leash" holds for **dagger's conjured creatures** (64 m). It does not hold for **GFALL's encounter actors**, which use leash 0 and are freed only when the place closes. **[Unverified]** whether GFALL's quest code makes them only near the player for that reason.
- **Spawn pool pressure.** GFALL's users can hold 29 of the 32 entries at once (8 + 4 + 8 + 8 + 1). One more burst of visuals and `spawn_add` returns 0 **[not observed; arithmetic only]**.

## 2. Requirements

**R1 One kind of thing.** Street people, building people, dungeon people (castle residents and quest persons), dungeon creatures, rest encounters, quest foes, treasure, piles and effects are all entities in one registry, with one id type.

**R2 Static / non-static.**
- A static entity never moves: no body, no motor, but it can still be solid, usable and drawn.
- A non-static entity may move and may have a body for movement and collision.
- Either may be switched by the game: a resident who starts to walk, a body that falls still.

**R3 Hostile / not hostile.** A bit on the entity, set and read by the game. The engine stores it, shows it in LEVELFALL's panel, and uses it for nothing else. Combat and AI stay the game's.

**R4 Use as a property.**
- An entity may be usable, with the same use kinds as `Usable` (talk, other), plus `what`, `tag`, label and reach.
- The use ray meets it like a usable and is blocked by walls.
- The game's handler gets the entity's id. The flats-as-people path goes.

**R5 The game owns who; the engine owns where.**
- Engine: position, facing, pose and frame, moving or not, body and collision, its home chunk, leash, keep row, drawing.
- Game: an **owner** word (its table index or a packed identity: faction, name seed, quest id), and everything it means.

**R6 Load hooks everywhere.** As a place or a stream chunk builds its entities, the game may:
- change one in place (a quest person takes over an NPC);
- drop one;
- add its own (quest persons on markers, givers in castles).

This covers inside buildings, towns, dungeons loaded whole and streamed dungeon blocks, with one hook signature.

**R7 Residency and leash per entity.** Each entity has a home:

| Home | Leaves when |
| --- | --- |
| a stream chunk | it goes with its chunk and comes back with it |
| the place | it stays until the place closes |
| roaming | it follows the camera's stream, saved under the chunk it stands in when that chunk drops |

Each also has a despawn rule: never, past a distance, with its chunk, with the place.

**R8 Persistence.**
- Any keyed entity's changed state survives its chunk dropping and a save.
- A keyed entity the game **made** (not the file) survives too. The engine keeps a row with its home, and the game rebuilds its look from the key when it comes back.
- Unkeyed transient entities (effects, projectiles) are never kept.

**R9 Cost bounds.** No per-frame allocation. Engine walks are O(live entities), not O(capacity). Collision and `entity_blocked` gather only the entities near the body (its chunk and neighbours, or a grid), not all of them.

## 3. The design

### 3.1 Data

`Entity` stays the picture and feet, so draw, keep and the stream's slot arrays keep working. Fields were added in the same struct, zeroed meaning today's behaviour (`src/engine/scene/entity.h` on `entity-registry`):

```c
enum {
    ENT_MOVES   = 0x01,  /* non-static: may move; may have a body */
    ENT_GHOST   = 0x02,  /* not solid even with a radius */
    ENT_HOSTILE = 0x04,  /* the game's bit, stored here (R3) */
    ENT_USABLE  = 0x08,  /* the use ray can take it (R4) */
    ENT_KEPT    = 0x10,  /* keyed and kept even when the game made it (R8) */
    ENT_HIDDEN  = 0x20   /* not drawn, not solid, not usable (a quest's hide) */
};
enum { ENT_HOME_CHUNK, ENT_HOME_PLACE, ENT_HOME_ROAM };
/* in struct Entity: */
    df_u8 residence;     /* ENT_HOME_*; 0 = as its array says */
    df_u32 flags;        /* ENT_* */
    df_u32 owner;        /* the game's: who this is (R5) */
    EntityUse use;       /* with ENT_USABLE: kind, reach, what, tag, label */
    float leash;         /* 0: none */
```

What differs from the first draft:

- **Solid by default.** The draft's `ENT_SOLID` became `ENT_GHOST`, so a zeroed entity with a radius stays solid as before. `entity_solid(e)` is the one test: a radius, and neither `ENT_GHOST` nor `ENT_HIDDEN`.
- **`residence`, not `home`.** The draft's `home` field is `Entity.residence`, since `home[3]` (where the file put it) already existed.
- **The leash frees pool entries only.** An array's entry is its owner's to free.

### 3.2 The registry

One engine registry (`src/engine/scene/ent.h`, `ent.c` on `entity-registry`) replaces "the scene's array plus the spawn pool" as what the engine walks. It names three kinds of storage:

| Storage | Size | Owner |
| --- | --- | --- |
| the place's own array, `Scene.entities` (a builder's, or the stream's slots) | up to `ENT_SCENE_MAX` 2048 | never the registry's |
| ranges a game registers beside it | `ENT_RANGES` 8, each up to `ENT_RANGE_MAX` 256 | the game's |
| the transient pool, at fixed addresses | `ENT_POOL` 64 | the registry's |

```c
typedef df_u32 EntId;    /* index + 1 in bits 0..15, generation in bits 16..23 */

void ent_place(const Scene *scene);                     /* scene is the place now */
int  ent_lists(const Scene *scene, EntList *out, int cap); /* what every walk reads */
int  ent_register_range(const Scene *scene, Entity *first, int n, int residence);
void ent_unregister_range(Entity *first);
EntId   ent_add(const Scene *scene, const Entity *e);   /* into the pool; 0 when full */
Entity *ent_get(EntId id);                              /* 0 once gone */
void    ent_remove(EntId id);                           /* pool entries only */
void    ent_clear(void);
int     ent_count(void);
int     ent_next(int i, EntId *id);
EntId   ent_id(const Entity *e);
EntId   ent_by_key(KeepKey key);                        /* a walk: for events */
void    ent_renew(const Scene *scene, int first, int n); /* slot filled or emptied */
void    ent_frame(const Scene *scene, const Camera *cam); /* ent_place + leash */
void    ent_shift(const Scene *scene, float dx, float dz);
int     ent_near_solid(const Scene *scene, float x, float z, float r,
                       const Entity **out, int cap);    /* -1: walk ent_lists */
void    ent_regather(void);
Entity *ent_pool(const Scene *scene, int *n);
```

How the registry behaves:

- **Ids are per place.** Ids work only in the registry's place, set by `ent_place`, which `ent_frame` calls each frame. A generation moves whenever an entry holds something else: a transient added, a slot renewed, a range registered or dropped, or another place. An old id then reads as gone.
- **Pool and ranges belong to their scene and heap epoch.** A call that finds the epoch moved drops them.
- **`spawn_*` is now a thin wrapper** over the pool. It goes in step 11.
- **The near-gather** (`ent_near_solid`) uses a grid of entities with a radius, built at the first query after `ent_frame` and again after any add, remove, renew or register. It is skipped, and the caller walks `ent_lists` instead, in three cases: before the first frame, with fewer than 32 entities with a radius, and for a reach across too many cells. A game that moves entities far without a frame between them (a long rest stepped in a loop) calls `ent_regather` first.

### 3.3 Use

- `interact_cast` also tests usable entities (`ENT_USABLE`, `use.reach > 0`, live, not `ENT_HIDDEN`) on their own box, nearer than the first wall; the nearer of an entity and a usable wins.
- `UseHit` gains `entity` (a `df_u32` id, 0 for none). `usable` stays, -1 when an entity was taken.
- The handler's twin is `typedef int (*EntityUseHandler)(const Scene *, df_u32 id)`, set by `interact_set_entity_handler`.
- `interact_use_entity(scene, id)` calls it, and `interact_frame` calls that for the use key.
- dagger's `people_in_view` (its own ray for street people) can then go: the same cast finds them.

### 3.4 Load hooks

One signature for every builder, run before keep's restore so a changed entity is then restored by key:

```c
typedef struct EntBuild {
    int place;          /* PLACE_INSIDE, PLACE_DUNGEON, PLACE_TOWN, a stream chunk */
    int cx, cz;         /* the chunk, or 0 */
    int block, record;  /* the builder's own numbers (RDB block, RMB block/record) */
    int source;         /* what the file object was: an RMB people record, an RDB
                           action-29 flat, a creature marker, a quest marker */
    df_u32 file_id;     /* its object id low word as the original numbers it,
                           so a game can match MAPS quest sites (docs: decomp
                           quest-places-and-start.md 8) */
} EntBuild;
/* For each entity the builder made: change e, or return 0 to drop it. */
typedef int (*EntityBuildHook)(const EntBuild *b, Entity *e);
/* After them: the game adds its own into out (cap) and says how many. */
typedef int (*EntityAddHook)(const EntBuild *b, Entity *out, int cap);
void ent_set_build_hooks(EntityBuildHook change, EntityAddHook add);
```

These hooks are dagger's, not the engine's, because the engine knows no file. They sit in `places/` (`world.c`, `dunstream.c`, `wild.c`). The engine part is only that entities from any builder land in the registry. `world_set_people_hook` becomes the inside-building case of this.

### 3.5 Persistence

*Built (252b08c).* `Keep` gains these fields (`src/engine/scene/keep.h` on `entity-registry`):

| Field | Meaning |
| --- | --- |
| `made` | 0 = a file's entity; 1 = a game-made entity filed under the stream chunk `cx`, `cz` it stood in when that chunk went; 2 = filed under its place (a place loaded whole) |
| `residence`, `flags`, `owner` | the entity's, for a made row only |
| `cx`, `cz` | the chunk, for `made` 1 |

A file's entity carries none of these. Its flags are **not** kept: its builder sets them again on each load, and `keep_restore` applies the rest of its row as before.

Only **pool** entities with `ENT_KEPT` and a key are kept this way. A game's registered ranges are kept by their owner.

The calls (`src/engine/scene/ent.h`):

```c
typedef int (*EntityReviveHook)(const Keep *row, Entity *out); /* 0 declines */
void ent_set_revive_hook(EntityReviveHook fn);
void ent_chunk_dropped(const Scene *scene, int cx, int cz, int chunk, float ox, float oz);
void ent_chunk_filled(const Scene *scene, int cx, int cz, int chunk, float ox, float oz);
void ent_keep_now(const Scene *scene, int chunk, float ox, float oy, float oz);
void ent_revive_place(const Scene *scene, float ox, float oy, float oz);
```

- **Dropping.** The stream's `drop_slot` calls `ent_chunk_dropped`, only while the keep is being saved. It writes each kept made entity standing in that chunk under the chunk, then frees it.
- **Filling.** `ent_chunk_filled` runs after `keep_restore`. For each made row filed under that chunk, it calls the revive hook, which builds the look from the row (`row->key`, `row->owner`). It then adds the entity to the pool with the row's position, flags, owner and residence.
- **No double revival.** A row already live in the registry is not revived twice.
- **A full pool** leaves the row to wait for the next fill.
- **Other places.** The stream does not know which place it is streaming, so the game's hook must decline rows of other places, by `row->key.place`.
- **Saving.** `ent_keep_now(scene, chunk, ox, oy, oz)` writes rows without freeing anything: under each entity's chunk for a stream (`stream_keep_now` calls it before a save), or under the place when `chunk` is 0. `ent_revive_place` revives a whole place's rows.
- **Rows stay** in the table while their entity lives.
- **Save files.** GFALL's KEEP record carries the new fields. **GFALL's `wire_keep` does not yet; that is the GAME Implementer's part.**
- **Test:** `check_kept_entities` (`tests/check_engine.c`).

### 3.6 Bodies

- Bodies stay the game's, inside its minds (`foes.c` `Mind.body`, GFALL `ai.c`).
- The engine only needs to know a body stands for an entity (`Body.self`, as today) so it does not collide with itself.
- An engine-owned body pool (awake ones only, about 24) is a later option and not required by R1..R8.
- What matters is that awake minds stay few. A non-static entity far away keeps its position in its entity and has no body.

## 4. How each system maps

| Today | Becomes |
| --- | --- |
| Street people (people.c's 96) | A range of roaming, non-static entities, `owner` = people.c's index, `ENT_USABLE` talk, `ENT_HOSTILE` mirrored from `g_per.hostile`. people.c keeps its motor, cells and health. Never kept. |
| Building people (`FLAT_PERSON` + `USE_TALK`) | Static entities with `ENT_USABLE` talk, built from RMB people records, `owner` = faction or the game's identity. A quest takeover is `EntityBuildHook`. |
| Quest people in a building (`world_set_people_hook` flats) | `EntityAddHook` entities on the 199.11 markers. |
| **Dungeon people** (none today) | RDB action-29 flats become static entities with talk, per block in the slot's range. The hooks give quest persons (op 30 on a dungeon marker) and castle givers. |
| Dungeon creatures (dunstream ranges) | The same ranges, flag `ENT_MOVES` (solid by their radius), `ENT_HOSTILE` per the game. |
| Treasure, spares, effects (dunstream) | The same, static. Effects unkeyed. |
| GFALL's 8 encounter slots | Transient `ENT_KEPT` entities with their own keys and home chunk. GFALL's saved actor table goes; keep rows plus revive replace it. |
| dagger's conjured creatures | `ent_add` with a 64 m leash, as now. |
| Shots, spell visuals, splashes, the light, ground piles | `ent_add`, unkeyed (piles keyed if GFALL wants them back). |

## 5. Migration, hostcheck green at each step

Each step is one commit; the stated test is added or kept passing.

1. **Fields only.** *Built (46531ad).* Add `flags`, `owner`, `use`, `leash`, `residence` to `Entity`, zero = today; `entity_solid` the one solid test. Nothing reads them yet. Test: `check_entities` unchanged; a zeroed entity behaves as before.
2. **Registry over the pool.** *Built (688e67f).*
   - `ent_*` with the transient pool and ranges; `spawn_*` wraps it.
   - draw, body and `entity_blocked` walk the registry instead of `Scene.entities` + spawn.
   - `Scene.entities` is registered as a range by `level`/stream setup.

   Test: `check_spawn` passes through the wrapper; a new check walks ranges and transients.
3. **The near-gather.** *Built (c97facd).* The grid of solid entities, used by `body.c` and `entity_blocked`. Test: the existing walker/entity collision checks give identical results; a check with 500 entities scattered gathers only the near ones.
4. **Use on entities.** *Built (6846e68).* `interact_cast` with usable entities, `UseHit.entity`, `EntityUseHandler`. Test: the ray takes an entity in reach, not through a wall, the nearer of an entity and a usable.
5. **Building people as entities** (dagger, `places/`).
   - RMB people records become entities instead of `FLAT_PERSON` flats; `doors.c`'s person usables go.
   - `world_set_people_hook` is kept for one step by an adapter that turns its flats into entities.

   Test: `check_dagger`'s Daggerfall armorer and its person, talk through the entity; counts updated (AGENTS: update the dagger test).
6. **Load hooks.** `EntityBuildHook` / `EntityAddHook` in `world.c` (inside, whole dungeon), `dunstream.c` (blocks), `wild.c` (towns). The adapter of step 5 is removed; both games move to the new hooks. Test: a hook that changes one person and adds one, in a building and in a streamed block.
7. **Dungeon people.** RDB action-29 flats become static usable entities per block (`rdb.c` keeps the action and faction it now drops). Test: Daggerfall castle (50027) has its residents as entities, talkable; the hold has none.
8. **Kept game-made entities.** *Built (252b08c).* Keep row fields (`made`, `residence`, `flags`, `owner`, `cx`, `cz`), pool entities with `ENT_KEPT` filed under their chunk or place, `EntityReviveHook(const Keep *row, Entity *out)` via `ent_set_revive_hook`, `ent_chunk_dropped` / `ent_chunk_filled` from the stream, `ent_keep_now` and `ent_revive_place`. GFALL's save carries the fields. Test: `check_kept_entities`. Left for the game: GFALL's `wire_keep` fields, and a revive hook in each game that declines other places' rows.
9. **GFALL encounters onto it.** The 8 slots become kept entities; its actor table goes. Test: GFALL's encounter host tests (rest, quest foe placed, save and load).
10. **Street people onto it.** people.c's pool registers as a roaming range with talk use; `people_in_view` goes. Test: `check_dagger`'s 96 people on cells, talk to one, a guard hostile.
11. **Remove** what is left (6).

Steps 1..4 and 8 are the engine's. Steps 5..7 and 10 are `places/`. Step 9 is GFALL's.

## 6. What can be removed afterwards

| Removed | Replaced by |
| --- | --- |
| The `FLAT_PERSON` → `USE_TALK` usable path (`doors.c:604..618`) and `FLAT_PERSON` on flats | building people as entities |
| `world_set_people_hook` and `world_building_marker_nth`'s role in it | the build and add hooks (the marker query stays useful) |
| `people_in_view` / `people_in_view_at` | the one use cast |
| GFALL's `ENCOUNTERS` actor table and its save fields | kept entities with revive |
| `spawn.h` | the registry; callers move from `spawn_add` to `ent_add`. `spawn_find` stays as a placement helper, renamed or not. |
| per-system `SpawnId` arrays (ground, splash, visual, light) | each caller keeps its `EntId`s; the arrays shrink to ids, no pool of their own |

## 7. Cost on the Pentium MMX 233

Counts in a frame, upper bounds from today's caps:

| Where | Live entities |
| --- | --- |
| a town street | up to 96 people + transients (64) |
| inside a building | its people (typically 2..30) + transients |
| a streamed dungeon | 9 blocks × up to 64 = 576 slots; typically 10..80 live **[typical numbers unverified]** |
| a dungeon loaded whole | up to `MAX_FOES` 1024 creatures in one range |

Per-frame engine work, estimated at the instruction level (not measured):

| Work | Today | After |
| --- | --- | --- |
| draw cull (distance + frustum + occlusion) | O(live), ~150 cycles each | same; 600 entities ≈ 90k cycles ≈ 0.4 ms |
| body push + `entity_blocked` | O(bodies × all entities): 24 minds × 600 × ~40 cycles ≈ 580k ≈ 2.5 ms **[estimate]** | O(bodies × near): grid rebuild 600 × ~30 cycles + 24 × ~10 near × 40 ≈ 30k ≈ 0.13 ms |
| leash / roam homes | spawn pool only | O(transient + roaming), ~10 cycles each, under 0.05 ms |
| use cast on entities | on the use key only | on the use key only |
| keep save/restore | on chunk drop/fill | same, plus kept game-made rows of that chunk |

The near-gather is the one place the plan saves time. Everything else is a wash. Measure `draw_frame`'s entity and body stages with `prof` in a dungeon block with many creatures before and after step 3.

Memory: about 300 bytes an entity.

| Pool | Size |
| --- | --- |
| 576 dungeon slots | ≈ 170 KB in the stream's heap (today ≈ 155 KB) |
| the transient pool of 64 | ≈ 19 KB |
| the grid | a few KB |

No per-frame allocation.

## 8. Not verified

- GFALL's mind count in `ai.c`, and whether GFALL places quest foes only near the player because of the leash (1).
- Typical live counts in a streamed dungeon (7).
- All cycle figures (7): estimates from the code's shape, not `prof` measurements on the 233.
- Whether `wild.c` towns need the hook for exterior people records. Today they are flats in town blocks, with talk only through `people_in_view` for street walkers **[unverified for RMB exterior people]**.
