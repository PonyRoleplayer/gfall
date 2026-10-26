# Streaming the world (design)

The plan for LEVELFALL's "Stream World": walk Daggerfall's whole map as one place, loaded around the camera as it moves, its ground from the world's height map and its climate. Read this before editing `src/engine/scene/stream.c`, `src/game/dagger/worldmap.c`, `wild.c`, or LEVELFALL's stream view. Phases 0 and 1 are built (below); the rest is the design, and each phase ends in something testable on the 233.

## What it is

Today a place is loaded whole: every block of one location, on one temp heap, all its pictures sent to the card at once (`docs/memory.md`). Streaming loads a ring of ground around the camera instead, adds what comes into range and drops what falls out, so the camera can walk out of Daggerfall's gate, across the wilderness, into the next town.

## The world in numbers

From the files (`MAPS.BSA`, `WOODS.WLD`, `CLIMATE.PAK`, `POLITIC.PAK`) and Daggerfall Unity's `MapsFile`:

- The map is 1000 × 500 **map pixels**. A map pixel is 32,768 units (819 m at 40 a metre): 8 × 8 RMB blocks of 4096, or 128 × 128 ground tiles of 256. The world is 32,768,000 × 16,384,000 units.
- **Locations:** 15,251, every one on its own map pixel; no pixel has two. A location's pixel is its `MAPTABLE` longitude / 128 and 499 − latitude / 128 (as `dungeon.c` already reads it). Daggerfall is pixel (207, 213), Privateer's Hold (109, 158).
- A location's RMB blocks are centred in its pixel's 128 × 128 tiles: its origin tile is (128 − width × 16) / 2 on each axis (Daggerfall Unity's `GetLocationTerrainTileOrigin`, which notes a few 1 × 1 places at (72, 55)).
- **`WOODS.WLD`** (26 MB, so never read whole): a 144-byte header; a table of 500,000 record offsets at 144; at 2,001,168 a 1000 × 500 height map, one byte a pixel (oceans 1, up to 255). Each pixel's 47-byte record holds a texture file (302), a climate byte, a noise byte, and at +22 a 5 × 5 block of finer heights.
- **Climate** per pixel is `CLIMATE.PAK` (`climate_pixel` in `dungeon.c`); the region is `POLITIC.PAK`, the same run-length rows.

## What Daggerfall Unity does

`StreamingWorld` keeps a square of whole map pixels round the player, `TerrainDistance` (3 by default, so 7 × 7 of 819 m), and when the player crosses into a new pixel it marks those out of range and builds the new ones in coroutines over several frames. Each pixel is one Unity terrain of 128 × 128 tiles; a location in it is laid out as its RMB blocks at the centred origin, the terrain flattened under it. It keeps floats accurate by moving the world back toward the origin (`WorldCompensation`). Heights are its own (`DefaultTerrainSampler`): a bicubic blend of the 4 × 4 height-map bytes round the pixel, times 8, plus the 5 × 5 blocks times 4, plus Perlin noise, over an ocean floor. Ground tiles are chosen by height and noise (water, dirt, grass, stone), with marching-squares transitions.

## The techniques, whatever their year

Anything that makes this run on a Pentium MMX 233 with a Voodoo and 64 MB is fair game, from 1997 or from 2020. What decides is this machine's shape: the CPU transforms, clips and projects every vertex and sets up every triangle (the Voodoo has no geometry engine), so triangles and vertices cost CPU; the card fills fast but holds 2 MB of textures; the L1 data cache is 16 KB; the disk is slow and Windows 98 has no overlapped file reads; there is one core.

- **Measure first.** The Pentium's cycle counter (`rdtsc`) times each part of the frame (cull, occlusion, models, decals, sprites, the loader, uploads, sound, swap) into a LEVELFALL panel. Every budget below is checked against it on the 233, not guessed.
- **Far ground is two triangles.** A chunk's 16 × 16 ground tiles are composited once, on the CPU, into one small picture of the whole chunk (64 × 64, 8-bit, 4 KB with its mips), and a chunk past the near ring is drawn as one quad with it: two triangles and one bind for 4096 × 4096 units. Near the camera the real tiles draw, from a ground atlas: every tile of the ground archive packed into a few 256 × 256 pages, so a chunk's 256 tiles are a handful of binds, not 256. (Baked macro textures for distant terrain are how Starsiege: Tribes and most open worlds since kept far ground cheap.)
- **Slopes: fixed grids, chosen per chunk** (Ulrich's chunked LOD, 2002; de Boer's geomipmapping, 2000). Each chunk has three or four prebuilt grids (16, 8, 4, 1 quads a side) and draws one, chosen by distance with a little hysteresis so it does not flicker between two; skirts hang under each edge to hide the seams. Prebuilt means a frame only chooses, never builds. The continuous methods (ROAM 1997, Lindstrom 1996) and the GPU-era ones (geometry clipmaps, Losasso and Hoppe 2004; CDLOD, Strugar 2009) all trade CPU or vertex work for triangles, the wrong trade here.
- **Hills hide things** (horizon culling, Lloyd and Egbert 2002). Ground drawn front to back raises a one-dimensional horizon across the screen's columns; a chunk or a town wholly under it is not drawn. One compare a column, and the existing occlusion cells (`docs/culling.md`) take the near ground as occluders.
- **Small far things are skipped** (contribution culling). A model whose box covers fewer than a few pixels is not drawn. The fog hides the rest.
- **Draw by texture.** The visible list is grouped by picture so the card's texture state changes as few times as possible; the Voodoo pays for each change.
- **Textures arrive at the size their distance needs** (mip streaming, as in Unreal and Unity). Each picture's sharpest level is chosen from how near its closest use is, so a town in the distance is first sent at a quarter size (a sixteenth of the bytes) and sharpened as the camera approaches, the most visible first, a few tens of KB a frame. The card's room is a buddy allocator: every picture is a power-of-two square, so blocks split and join exactly, and the least recently drawn is freed first. This is what makes 2 MB enough.
- **Cooked data.** Reading the original files means parsing BSA records, ARCH3D meshes and pictures at load, the slowest part. Locations reuse a few hundred RMB blocks and a few thousand models; those, and the pictures' mip chains, can be prepared once into a cache beside the program (made on the host and copied with `ARENA2`, or on the 233 the first time a block is met), laid out so that a chunk is one read and a copy. Only once the measurements say parsing is the cost.
- **A reading thread.** Windows 98 cannot overlap file reads, but a second thread (`CreateThread`, KERNEL32) can block in `ReadFile` while the main thread draws; the disk's DMA then overlaps the frame. Parsing stays on the main thread, under its budget, so the frame time is the main thread's to control.
- **Never wait.** Every load is a resumable state machine: a few steps a frame under a cycle budget, nearest and most ahead first, starting a ring before the view needs it, and dropping a chunk only a ring after it is out (hysteresis, so a camera on a border does not thrash). Ground appears at once (it is cheap); buildings follow, far enough out that the fog hides them arriving.
- **Handles, not epochs.** Every loaded thing lives in a fixed slot with a generation number (a slot map), so a module that kept a reference can tell that the slot was reused without rebuilding everything when anything changes.
- **Data laid out for a 16 KB cache.** The cull reads boxes in their own packed array, apart from the models (as `Scene.bounds` already is); a loop over many reads few bytes of each.
- **Big coordinates:** a float has 24 bits of mantissa; at 32 million units it steps by 2 to 4, and everything shakes. Positions are a chunk (integers) and an offset in it; the scene draws in a frame whose origin is a chunk corner near the camera, and shifts everything by a whole number of chunks, a power of two, so exactly, when the camera strays (`WorldCompensation` in Daggerfall Unity; the "floating origin").

## What it costs here

Measured on the host: Daggerfall's 64 blocks are 3,545 models and 98,332 triangles, 216 pictures, 86 ms, 13.7 MB; about 1.3 ms and 170 KB a block. The 233 is very roughly 30 to 50 times slower, so a city block is 40 to 60 ms of work: a dropped frame if done at once, invisible if spread over ten. The Ashwing Farmstead's single block takes 3.9 MB, nearly all of it tables sized for the largest city (8192 models, 8192 flats, 2048 usables): a ring of 25 chunks cannot each carry that.

The game's view is 1400 units (fog solid at 1170): a ring of 3 × 3 blocks (12,288 units) covers it. LEVELFALL's 8192 needs 5 × 5 to 7 × 7.

## What in the engine assumes one place

- **Memory:** one temp heap a place (`mem_temp_begin`); ending it is how everything is freed.
- **Caches by heap epoch:** lights, collision, ambience, interaction and the walker throw away and rebuild their state when `mem_epoch` moves. A stream moves it all the time.
- **The scene:** arrays built once (`world.c`), indexed by model, sprite and light; spans per block; usables and movers by index.
- **The light decals:** cut once for the whole scene (`light_build`), one buffer.
- **The card:** `texcache_upload` budgets and sends every picture of the place, laying them end to end from `grTexMinAddress`; nothing is freed but all at once (`glide_tex_reset`).
- **The sky, the weather odds, the soundscape:** one per place, set at load by its climate.

## The design

**Chunks.** The unit is one RMB block's square, 4096 units: 8 × 8 to a map pixel. A chunk is wilderness ground or a location's block (its RMB, as now, with its own ground tiles). Its ground is drawn from the atlas within the near ring and as its baked picture beyond. The stream keeps the chunks within the view distance plus one ring, nearest first, and lets go a ring further out.

**Coordinates.** A position is a chunk (integers: map pixel × 8 + block) and an offset in it. The scene draws in a local frame whose origin is a chunk corner near the camera; when the camera is more than two map pixels from it, everything loaded and the camera shift by a whole number of chunks, a power of two, exact.

**The scene by slots.** A fixed number of chunk slots, each owning a fixed range of the scene's arrays (models, bounds, flats, lights, emitters, usables, movers), allocated once for the session, each with a generation number. A slot's span is its chunk; an empty slot has a count of 0, so the cull skips it for free. Indices never move, so usables and movers keep their model numbers. Sizes come from the data: a slot holds the largest RMB block's models (to be measured over all of them).

**Memory.** Slots are allocated once. Meshes (the ARCH3D cache) and pictures are shared by every chunk, so they live in a session arena; when it passes a cap, the stream rebuilds round the camera under the "Loading..." hold (`gfx_hold`) and starts the arena again. Parsing reuses the scratch buffers (`mem_scratch`).

**The loader.** A queue of chunks, ordered by distance and ahead of the camera's motion. Each chunk loads as a resumable state machine (read the block, place the models, the flats, cut its light decals, bake its ground picture, ask for its pictures), a few steps each frame, under a cycle budget, about 4 ms on the 233 to start with and set from the measurements. Reads go through the reading thread once there is one. A chunk a ring past the view is emptied at once.

**Per-chunk engine state.** The lights cut and free decals per slot; the collision grid is rebuilt for the slots that changed; emitters, usables and movers are per slot; the epoch-guarded caches learn that a slot changed, not the world.

**The card.** The sky, the font, the light falloff and the ground atlas stay. Everything else goes through the residency manager: a buddy allocator over the rest of TMU0, each picture at the level its nearest use needs, sharpened in order of how visible it is, the least recently drawn freed first, under a per-frame upload budget in bytes. Mip chains are built when a picture is first loaded (later, cooked) and kept in memory, so an upload is a copy to the card.

**The world's data** (game side, a `worldmap` module): the height map (500 KB) kept for the session; a pixel-to-location table built at start from every region's `MAPTABLE` (15,251 entries); climate and region by pixel; a record of `WOODS.WLD` read on demand.

**The ground's look.** Flat first: every wilderness tile the climate's grass, the ground archive by climate and season (302, 303 in winter). Then heights, ground tiles by height and noise, and nature (the climate's trees, as sprites, skipped far out by contribution).

## Phases

0. **The frame's costs.** Built: the cycle-counter profile (`docs/profile.md`), in the console (`prof`) and a LEVELFALL panel (P). The baseline on the 233 is still to be taken. Every later budget is set from it.
1. **Flat wilderness.** Built (see "Phase 1, as built"). The world map data. LEVELFALL's list gets a "Stream World" box: opening an outdoor row with it ticked starts the stream at that location's pixel, ground only, no buildings. Chunk slots with generations, the local origin, the time-sliced loader with hysteresis, the ground atlas near and baked chunk pictures far, the walker on the ground. Tests: the pixel and location tables; a walk of many map pixels on the host with the slot counts, the origin shifts and the loader's budget checked.
2. **Locations.** A location's blocks stream in as chunks: models, flats, lamps, emitters, doors (a house's door still opens its inside as a place of its own, and leaving comes back to the stream). The texture residency manager (buddy allocator, mip streaming by distance). The per-chunk lights. Draws grouped by texture; contribution culling. The reading thread.
3. **Climate across the map.** Each chunk's ground by its pixel's climate and the season; the sky, the weather odds and the soundscape follow the pixel under the camera. Cooked blocks, models and mip chains, if phase 2's measurements show parsing is the cost.
4. **Slopes.** Heights from `WOODS.WLD` (Daggerfall Unity's sampler, or one of our own over the same bytes), flattened under locations; prebuilt grids per chunk with skirts, chosen by distance; horizon culling; the walker on slopes; nature flats.

## Phase 1, as built

- **The world map** (`worldmap.c`): read once and kept for the process (`mem_keep`), 2 MB: `WOODS.WLD`'s height map (read at its offset, the 26 MB file never whole), `CLIMATE.PAK` unpacked to a climate id a pixel, and a pixel's location from every region's `MAPTABLE` (15,251). A location's own pixel comes back by `worldmap_pixel_of`.
- **The stream** (`engine/scene/stream.c`): 121 slots (11 × 11: the radius up to 4, and one ring kept past it), each a fixed range of the scene's models with its own span. A slot's models are filled by the game in the chunk's own frame, then placed, boxed and spanned; a dropped slot's models lose their mesh and their boxes go negative, so the cull, the collision grid and the use ray all pass over it. Nearest missing chunk first, ring by ring; a chunk is dropped two rings out. The origin moves when the camera is more than 16 chunks (two map pixels) from it: everything placed and the camera move back by whole chunks. `Scene.version` changes with every load, drop and shift, and the collision grid is built again for it. The loader runs under `PROF_STREAM` with a budget from the cycle counter (4 ms in LEVELFALL, to be set from the 233's profile); uncalibrated, one step a frame.
- **The wilderness** (`wild.c`): a chunk is 4 × 4 patches of 1024 units, one quad each, its picture repeated over its 4 × 4 tiles: 32 triangles a chunk. A patch mesh per picture, shared by every chunk (nothing is allocated as the world streams). The picture is the chunk's map pixel's climate: each family's grass (its ground archive, record 2), the sea's water (record 0) off the coast; winter's archives in winter (the desert never snows). The sky and the weather odds are the start pixel's (phase 3 makes them follow); the soundscape is a town's.
- **LEVELFALL:** "Stream World" beside the list's Show box. Ticked, Open streams from the row's pixel (an indoor row's outside's), from the middle of the pixel at eye height, the radius from the far clip (8192 gives 2, a 5 × 5 ring), until Esc. The place panel shows the pixel, the location on it or "wilderness", the stream's radius and its chunks loaded and to go, and the pixel's climate.
- **Tests:** Daggerfall's pixel, climate and height; the sea at the map's corner; a stream opened at Daggerfall fills its 25 chunks; a walk of 40 chunks east keeps ground under the camera all the way, keeps its ring and the ring behind (30), moves the origin twice, and lands the camera on pixel 212 with its world position exact to the unit.
- **Not yet:** buildings (phase 2), the far ground's baked pictures and the near tiles' variety, the sky following the camera, heights.

## Open questions

- How long a city takes to stream in on the 233, and whether a block's steps fit the budget, can only be measured there.
- How sharp the residency manager can keep a city on a 2 MB card at walking speed, and what upload budget a frame can spare.
- Indoors (dungeons, houses) stays as it is: a place of its own, entered from the stream and left back to it.
