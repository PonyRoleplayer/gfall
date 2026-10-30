# Streaming the world (design)

LEVELFALL's "Stream World": walk Daggerfall's whole map as one place, loaded around the camera as it moves, its ground from the world's height map and its climate. Read this before editing `src/engine/scene/stream.c`, `src/engine/gfx/texres.c`, the stream modes of `texcache.c` and `light.c`, `src/game/dagger/formats/worldmap.c`, `wild.c`, `wres.c`, or LEVELFALL's stream view. Phases 0 to 4 are built (below): the land has its heights, its tiles and its flora; each phase ends in something testable on the 233.

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
2. **Locations.** Built (see "Phases 2 and 3, as built"): a location's blocks stream in as chunks: models, flats, lamps, emitters, doors (a house's door opens its inside as a place of its own, and leaving comes back to the stream). The texture residency manager (buddy allocator, mip streaming by distance). The per-chunk lights. Not yet: draws grouped by texture, contribution culling, the reading thread; each waits for the 233's profile to say it is needed.
3. **Climate across the map.** Built: each chunk's ground by its pixel's climate and the season; the sky and the weather odds follow the pixel under the camera; the season's turn loads the chunks again. Not yet: the soundscape stays a town's; cooked blocks, models and mip chains, if the measurements show parsing is the cost.
4. **Slopes.** Built (see "Phase 4, as built"): heights from `WOODS.WLD`, Daggerfall Unity's recipe in our own form, towns flat at their level with the land eased to them; near ground as merged tile quads, far ground as a two by two grid with skirts; DFU's tiles and transitions; flora. Horizon culling is left out on purpose (see Not yet).

## Phase 1, as built

- **The world map** (`worldmap.c`): read once and kept for the process (`mem_keep`), 2 MB: `WOODS.WLD`'s height map (read at its offset, the 26 MB file never whole), `CLIMATE.PAK` unpacked to a climate id a pixel, and a pixel's location from every region's `MAPTABLE` (15,251). A location's own pixel comes back by `worldmap_pixel_of`.
- **The stream** (`engine/scene/stream.c`): 121 slots (11 × 11: the radius up to 4, and one ring kept past it), each a fixed range of the scene's models with its own span. A slot's models are filled by the game in the chunk's own frame, then placed, boxed and spanned; a dropped slot's models lose their mesh and their boxes go negative, so the cull, the collision grid and the use ray all pass over it. Nearest missing chunk first, ring by ring; a chunk is dropped two rings out. The origin moves when the camera is more than 16 chunks (two map pixels) from it: everything placed and the camera move back by whole chunks. `Scene.version` changes with every load, drop and shift, and the collision grid is built again for it. The loader runs under `PROF_STREAM` with a budget from the cycle counter (4 ms in LEVELFALL, to be set from the 233's profile); uncalibrated, one step a frame.
- **The wilderness** (`wild.c`): a chunk is 4 × 4 patches of 1024 units, one quad each, its picture repeated over its 4 × 4 tiles: 32 triangles a chunk. A patch mesh per picture, shared by every chunk (nothing is allocated as the world streams). The picture is the chunk's map pixel's climate: each family's grass (its ground archive, record 2), the sea's water (record 0) off the coast; winter's archives in winter (the desert never snows). The sky and the weather odds are the start pixel's (phase 3 makes them follow); the soundscape is a town's.
- **LEVELFALL:** "Stream World" beside the list's Show box. Ticked, Open streams from the row's pixel (an indoor row's outside's), from the middle of the pixel at eye height, the radius from the far clip (8192 gives 2, a 5 × 5 ring), until Esc. The place panel shows the pixel, the location on it or "wilderness", the stream's radius and its chunks loaded and to go, and the pixel's climate.
- **Tests:** Daggerfall's pixel, climate and height; the sea at the map's corner; a stream opened at Daggerfall fills its 25 chunks; a walk of 40 chunks east keeps ground under the camera all the way, keeps its ring and the ring behind (30), moves the origin twice, and lands the camera on pixel 212 with its world position exact to the unit.
- **Since:** buildings, the far ground and the sky following the camera came with phases 2 and 3 (below). Not yet: the near tiles' variety in the wilderness, heights.

## Phases 2 and 3, as built

Measured over all 920 RMB blocks: at most 167 models (83 on average, ground patches included; a block's ground is 64 patches once its missing tiles are filled, below), 74 flats, 8 light flats; 725 distinct exterior meshes, about 105,000 triangles, 7 MB; 86 texture archives named by them before the climate swap.

### Memory that can be freed

`mem.c` has private heaps (`mem_heap_new`, `mem_heap_use`, `mem_heap_free`, `mem_heap_delete`; `HeapCreate` on Windows, malloc with a list on the host) that `mem_alloc` can be pointed at, and a track (`mem_track`): every allocation made while it is set, in the heap that was current when it was set, is listed, so a thing built (a picture) can be freed piece by piece later. A nested heap's allocations are not listed. Heaps do not move `mem_epoch`, so nothing that guards its caches by the epoch rebuilds when a chunk comes or goes. `mem_scratch` always takes the main heap: its buffers outlive any one chunk. See `docs/memory.md`.

- **A chunk's heap** (`wild.c`): everything a chunk alone owns (its block's arrays, its ground patches, its far quads and baked picture, its doors' work lists) is in a heap of its own, deleted when the chunk drops.
- **Pictures** (`wres.c`): a registry by archive, record and use (a wall tiles, a sprite is padded or a sheet of frames, so the same record used both ways is two pictures), its slot fixed for the session (meshes keep slot numbers in their triangles); the level's `images`, `cutout` and `glass` arrays are the registry's, 4096 long. A chunk acquires the pictures it draws and releases them when it goes; a picture's pixels are read into the pictures' heap under a track (`texture_load`, `texture_smooth` for a wall, `texture_sheet` or `texture_pad` for a sprite) and stay while any chunk holds it, and after, until `wres_trim` frees the least recently released past 8 MB (`WRES_PICTURE_RAM`). A game-made picture (a far chunk's ground) is `wres_dynamic`: a slot of its own, given back by `wres_forget` for the next one.
- **Meshes:** cached for the session per climate variant (four families, summer and winter: a mesh's triangles name its climate's pictures, so a desert house is another mesh from a woodland one), in the meshes' heap (`arch_cache_heap` while a block is read), their faces given picture slots once (`wres_paint`). `wres_full` says when they pass 10 MB; nothing acts on it yet (Daggerfall's 25 chunks and a walk east use 1.4 MB).
- **`TEXTURE` files:** kept whole (one file serves many records) in `texture.c`'s own heap, the least recently used freed past 4 MB (`texture_cache_limit`), the file just read always kept.

### The card

`texcache` in stream mode (`texcache_stream_begin`, from `gfx_prepare` for a level with `streamed`) over `texres.c`, plain C. After the sky, the falloff and the font, the rest of TMU0 is a buddy allocator (blocks of 256 bytes to 2 MB, split and joined, one arena per 2 MB half so no download crosses the edge). Every picture registered gets an 8 × 8 copy at once, kept while it is registered: nothing ever draws untextured. Its mip chain is built once (`mip_build`, every level), when its pixels are read, and kept in RAM: a sharper copy is a download (`glide_upload_chain`), never a rebuild. Each frame the draw notes, per picture, the nearest distance it was drawn at (a model by its mesh's slot keys, its box's distance less its extent; a sprite by its own); the level a picture needs follows from that distance and its density (texels a unit: a wall 0.25, a sprite its width over its world width, a far ground picture 1/32), doubling the distance halves the side. After the frame, `texcache_stream_service` uploads the sharper copies, sprites before walls (a sprite is the hand-drawn art: a blurry one breaks the picture, a blurry wall reads as distance), each class the picture most levels short of its need first, then the nearest, under 64 KB and 8 uploads a frame. Each class's first copy goes even past the byte budget (a 256 sprite is 128 KB alone), so the walls get a turn every frame however many people want sharpening; before that, a street of people kept the ground at its 8 × 8 copy. Short of room it frees the sharp copies not drawn this frame, a wall's before a sprite's, least recently drawn first of each; for a sprite still short, the sharp copies of walls drawn this frame farther than the sprite, the farthest first, two a frame at most (such a wall shows its 8 × 8 copy until a smaller sharp one fits round the sprites): the ground under the camera, nearer than any sprite on it, is never taken, and taking without a bound stripped the walls over and over waiting for a block to join; still short, it sends the next level down: a picture is downsized only as far as the card needs.

The blocks: small ones (under 16 KB: the 8 × 8 copies, small pictures) come from the top of the card, large ones from the bottom, each from the smallest free order that holds it. Taken from the bottom alike, the 8 × 8 copies split block after block beside the large ones, and a card with 650 KB free had no 128 KB block whole: a sprite in front of the camera settled for half size. A large block that does not fit is made by freeing one aligned region of its size whole (`free_region`): one holding no 8 × 8 copy and only sharp copies that may go (not drawn this frame, or for a sprite, walls drawn this frame farther than it, within the frame's two), the fewest drawn first, then the fewest copies; freed least recently drawn first one by one, copies came out all over the card and the block never joined. `check_texres` churns a card (pictures added, sharpened and removed at random, 4000 steps): half of it free still has a whole 128 KB block (64 KB with every block from the bottom), and a close sprite then gets its full size.

A sprite goes to the card as its one level, no chain (`card_chain`): it is small on the glass and the residency already sends the level its distance needs. A chain is a third more than its top level and the card's blocks are powers of two, so a 256 sprite's chain (171 KB) took a 256 KB block; its one level takes 128. Walls keep their chains: one wall spans near and far. `check_texres` checks that a sprite is served before a nearer wall, that a sharp wall keeps its room from a farther sprite and gives it to a nearer one, and that with sprites wanting and no budget left the wall still gets its copy. A picture unregistered (trimmed from RAM) frees both copies. A level loaded whole still goes through `texcache_upload`, which ends the stream mode first.

### The slots

121 slots, each of 192 models, 128 flats, 16 lights, 80 emitters, 128 usables and 8 movers (a chunk holds at most its land's near and far meshes, a block's 64 ground patches and 103 buildings, and its far quad: 170; 74 flats and 64 flora). Two spans a slot: its ground, near or far, and the rest. Sprites (`Span.sfirst`, `scount`) and emitters (`efirst`, `ecount`) are by span too when `Scene.sprite_spans` is 1: the sprite cull skips a span whose box is out of view, and the ambience walks the spans' emitters, not ten thousand records a frame. The rebase moves everything a slot holds, its boxes and spans, and the light decals (the source's `shift`).

### A chunk's contents

A chunk is filled in steps, each one call of its fill (`STEP_*` in `wild.c`). The stream checks its budget between calls, so a frame overruns it by at most one call: no call may be large. The repeated steps do one piece of real work a call (one mesh parsed, one tile shrunk, one picture read) and return until none is left.

- `STEP_START`: its heap; the towns whose ground reaches it (`gather_flats`: its pixel's, and on a pixel's edge ring its neighbours'); which block of its pixel's town, if any, has its corner in it (a location is centred in its pixel, so for an odd width a block's corner falls on a half chunk and the block reaches into the next), and that town's level.
- `STEP_LAND`: the land's heights and tiles (`terrain_chunk`, "Phase 4" below).
- `STEP_LIST`: the block's bytes (scratch, read again by each step that needs them), the ARCH3D ids its models name (`rmb_model_ids`), and its townspeople's walking cells (`make_nav`, `docs/entities.md`). A ground tile past the archive's records (63: outside a city's walls) becomes the climate's grass, so a block's ground covers its square whole; the land's cells are never needed under a town, nor drawn twice in one plane.
- `STEP_MESHES`, repeated: the next mesh not in the session's cache parsed into it and given its climate's pictures.
- `STEP_BLOCK`: `rmb_load` into the chunk's heap, every mesh already cached, so it only places.
- `STEP_TILES`, repeated: the next ground tile the far pictures need, shrunk: the land's, then the block's.
- `STEP_NEAR`: the land's near mesh, then the block's ground raised to the town's level.
- `STEP_FAR`: the land's far mesh under its baked picture, then the block's ground as one quad under its own, at the town's level.
- `STEP_REST`: the buildings and props, raised to the town's level, their pictures held (counted in, `wres_hold`, not read).
- `STEP_FLORA`: where the flora stands, its pictures held ("Flora" below).
- `STEP_HOLD_FLATS`: the flats' pictures held; in a settlement's block (a city, a hamlet, a village, while `population` is on), its townspeople's clothes too (`people_slots`: every outfit of the climate's people and the guard's, 54 pictures, `docs/entities.md`). The first of a town's chunks to come in range reads them, at the stream's edge, under the budget; the rest find them read.
- `STEP_PICTURES`, repeated: the next part of the next held picture not in RAM (`wres_load`): its TEXTURE file a 32 KB piece a call (`texture_file_step`, when the file is not in the cache), then its pixels (the decode and the stretch or the sheet), then its mip chain and its card copy. A picture is drawn only once its last part is done.
- `STEP_FLATS`: standing on their base at the town's level, padded or as a sheet of frames at their rate, a lamp of each `TEXTURE.210` flat (radius 160), an emitter of each that sounds (`sounds_flat`); then the flora.
- `STEP_DOORS`: the doors and people (`doors_build`, as a town's), their model and mover numbers counted from the slot.

A chunk with no block skips the block's steps. The `N` in the budget's log line (`stream: chunk step N took ...`) is the step's place in this list, from 0.

Measured on the host walking Daggerfall, the longest single call of each, before and after the steps were split (the 233 is roughly 40 times slower):

| Call | Before | After |
|---|---|---|
| A block read, every new mesh parsed in it | 6.5 ms | one mesh: 0.7 ms |
| A lamp's pools, with shadow rays | 2.0 ms | 0.07 ms (no rays, below) |
| Collision grid rebuilt per chunk change | 0.2 ms | none (spans, below) |
| A building step reading its pictures | 0.7 ms | one picture a call |
| A location record (MAPS.BSA opened, the region read) | 1.0 ms each miss | once a region (`exterior_keep`) |

A whole picture was a step past any frame's budget on the 233: a 256 sprite sheet's chain alone about 20 ms, a 92 × 90 wall's smooth stretch 18 ms, an 80 KB file one disk read. Measured again on the host walking Daggerfall (times 40 for the 233; the host's disk is its page cache, so a file read here says nothing of the 233's disk):

| Part | Before | After |
|---|---|---|
| A sprite sheet's chain (`mip_build`) | 15 to 20 ms | 5 to 9 ms: a cutout's clear texels gather their 3 × 3 only within each row's opaque span and a texel either side, the halving two channels a word |
| A cutout's full level, built in the frame when the card sharpens a sprite to full size | 20 ms | 2 to 5 ms: written straight from the picture, no RGBA pass |
| A wall's smooth stretch (`texture_smooth`) | 18 ms | 8 ms: the source positions a table a column and a row, not two divisions and three remainders a texel; three multiplies a channel, not four and the weights' four |
| A TEXTURE file | whole, in the step that decodes | 32 KB a call, its own steps |
| A record count (`texture_record_count`, asked by the climate swaps of every face a mesh paints) | the whole file read | its two-byte header, remembered |
| A picture | one step | three or more: the file's pieces, the pixels, the chain |

Every one bit for bit the same: the chains over 12,236 pictures and modes, the stretch over all 4,140 walls that stretch. Walking Daggerfall at a run, 6,000 frames: 3 frames past the budget by 8 ms before, 1 after; the worst picture step 13 ms before, 5 to 7 after.

The mesh parse itself was the weld: each face scanned every corner of its model, twice. The corners are now sorted by x once a model and a face looks only at those in its box, and the counting pass keeps its welded faces for the emitting pass (`docs/culling.md`, T-junctions): every one of ARCH3D.BSA's 10,237 models comes out bit for bit the same, in 158 ms instead of 442 on the host; the slowest, model 451, 0.8 ms instead of 6.8.

Measured on the host at Daggerfall's middle, radius 2 (25 chunks): 646 models past the ground, 301 flats, 505 doors, 114 lamps, 38 sounds; 244 picture slots, 4.2 MB of pictures in RAM, 183 meshes in 1.4 MB, 4.1 MB of files.

### Far ground

Within one ring of the camera's chunk the near ground draws (`STREAM_NEAR`); beyond, the far ground: the land as a two by two grid of 2048 quads under one picture of its tiles, with a skirt under each edge ("Phase 4" below), and a block's ground as one 4096 quad under a picture of its own 16 × 16 tiles. Both pictures are 128 × 128 (`FAR_SIDE`), 8 texels a tile, 32 units a texel, two or three pixels at the nearest a far chunk can be (`bake_tiles`). Each ground tile is shrunk to 8 × 8 once for the session (the mean of each 8 × 8, `mip_nearest`), and laid in turned and flipped as `rmb.c`'s `corner_uv` turns the tile's corners: with a and b the column and row in the tile, none reads (a, b), flip (7 − a, 7 − b), rotate (7 − b, a), both (b, 7 − a). The pictures are dynamic `wres` slots on the card like any other. Daggerfall's middle: 1600 near ground quads, 25 far ones, the same area.

### Lights

`light.c` in stream mode (`light_stream_begin` after `stream_open`, on the stream's heap). A light's decals are its own, in a heap of its slot's, freed with the chunk. When a chunk arrives (the source's `changed`), its lamps are queued, and so are the lamps of loaded neighbours whose reach meets its box (models and sprites, kept so the same test works after it goes); when one goes, its neighbours' lamps that reached into it are queued again, cut without it. `light_stream_service` cuts the queue in what the stream's budget left (a quarter of it at least), a light at a time, each against the still models of the loaded slots its reach box meets (the near ground and the rest, never the far quads; doors, the movers' models, take no decals, as indoors). A streamed lamp casts no shadow (`seen` passes everything): the rays were 97% of a lamp's cut, and outdoors they change next to nothing, since a house's inside is not drawn and a face turned away from the lamp takes no pool anyway. Daggerfall's middle has 4435 decals with them off, 3859 with them on. A sprite's lamplight is worked out per slot, from the lights of the slots near it. A chunk arrives at the stream's edge, where its lamps are too far to draw, so a pool cut a few frames late is never seen. `light_build` on the streamed scene (a switch: `lightrange`, `lightshadow`) queues every loaded light again. Cut chunk by chunk as they came, they are the same as cut all at once (tested).

### Collision

`collide_use` builds no grid for a streamed scene (`Scene.sprite_spans`): `collide_gather` walks the spans, two a slot with their boxes, and the models of those its box meets. A model is in one span at a time, so it needs no marks. The grid over every slot's 23,000 models was built again whenever a chunk came or went.

### The budget

With vsync the card shows a frame on a 60 Hz retrace; a frame that runs past 16.7 ms waits for the next and shows at 30 fps, which reads as a stutter. A fixed 4 ms of loading pushed a 13 ms frame over. `gfx_stream_budget` (`prof_spare_ms`, `docs/profile.md`) takes the last frame's time without its swap (`prof_last_ms(PROF_FRAME) - prof_last_ms(PROF_SWAP)`), finds the retrace that frame made anyway, and gives the stream what is left before it, less 1.5 ms, between 1 and 8 ms (LEVELFALL's fallback, 2 ms, before calibration or with vsync off). The lamps get what the chunks left. A single call that runs more than 8 ms past the budget is logged to `levelfall.log` (`stream: chunk step N took X ms, budget Y`, N a `STEP_*` number; or a lamp's), at most 64 lines a stream, so the 233's slow steps can be named. The worst calls are kept by `stream_worst` and `light_stream_worst`.

Before the first frame (opening, coming back out of a door, a season's turn) `wild_fill` loads everything within the radius under the hold, so the walk does not start with seconds of loading a millisecond at a time.

A run can outpace the budget: the 233 fills a town's chunk in a second or more, and a run crosses one in a few. The chunk under the camera, and any within 256 units of it (`STREAM_UNDER`: the walker moves in the frame, after the stream's update, and can step over an edge), is loaded at once when it is not there, whatever it costs (`fill_under`): a chunk being filled elsewhere is dropped and comes again in turn. A wait, logged (`stream: the camera outran the stream; its chunk x,z loaded at once (N ms)`, the first 16 a stream, counted by `stream_outran`), never the ground missing under the walker: before, a run fell through the unloaded chunk into nothing. `check_stream` runs 60,000 units at 1000 a frame with no budget and keeps ground under the camera all the way.

### The world follows the camera

Each frame `wild_update` reads the camera's pixel's climate. When it changes (and has a family: the sea keeps the last), the sky file and the weather's odds follow (`skyfile_open`, `forecast_set`), and `Level.sky_serial` goes up: the frame then calls `sky_change`, and the next `sky_update` reads the new sky's picture into the same tiles on the card. The season is checked each frame too (`wild_season_turned`): when it turns, LEVELFALL holds the frame under "Loading...", `wild_reseason` drops every chunk (`stream_reload`), makes the wilderness's patches in the new season's archives, and loads the camera's ring again before it returns (`wild_fill_near`); the rest streams in as on a walk. Meshes are cached per season already.

### Doors from the stream

LEVELFALL's handler lets a streamed door through when its pixel's town is in the list (`wild_location_at`: the town whose pixel holds the door, and its own frame's origin, the corner of its block 0, 0), into a house (`DOOR_ENTER`) or down into its dungeon (`DOOR_DUNGEON`). The step out of that door (`doors_landing`, three body radii) is kept in world units; the stream closes, and the house opens as a place of its own (`world_load_inside`, with the door moved into the town's frame, where the house is placed against it), or the dungeon. Its way out (`DOOR_LEAVE`, `DOOR_EXIT`) closes it and opens the stream again at the kept step (`wild_open_at`), facing out, the camera's ring loaded before the first frame. The card stays open throughout, each load under the hold and faded in. The door is moved into the town's frame in height too: a town loaded whole stands at 0, the stream's at its level. Tested: 20 of Daggerfall's doors into houses from the stream are where the town loaded whole has them.

### Bookmarks, a sharp start, the meshes' cap

- **Bookmarks.** F12 in the stream writes `stream=x,z,y,yaw,pitch` to `levelfall.log`: the camera's world position in whole units (`stream_world`, up to 32 million, past a float's exact range), its eye's height, its yaw and pitch in degrees, with the usual `fov=`, `time=`, `date=` and `weather=`. `stream=` in `levelfall.cfg` opens the streamed world there at start, without the list (`view_stream` with a pose: `wild_open_at`), and wins over `open=`.
- **A sharp start.** Under the hold, after a load fills the radius (opening, a door back out, a season's turn, a restart), `gfx_prime` notes every loaded picture by how near its models, sprites and entities stand to the camera, as a frame does, and runs the card's service with no byte budget until it sends nothing more. The first frame is drawn with the sharp copies already on the card, not the 8 × 8 ones. It also starts the place's song and sounds as the first frame would (`ambience_frame` with no time passed): the song is read and turned into MIDI buffers under the hold, not in the first frame (up to 20 ms on the 233). LEVELFALL calls it for every place it opens, whole or streamed, and GFALL for the hold.
- **Landing whole.** `wild_fill` ends with `people_fill`: in a town by day, every outfit of its people read (its chunks read them already) and its whole population out round the camera, ahead of it too, since nothing was seen yet. Before, a door back out into Daggerfall landed on empty streets that filled one person a tick over ten seconds, while their pictures were read one a frame, each 12 to 28 ms on the 233, for the first 54 frames.
- **The meshes' cap.** Every climate's variant of every model met stays read for the session; past 10 MB (`WRES_MESH_RAM`, `wild_crowded`) LEVELFALL holds the frame, and `wild_restart` drops every chunk and every mesh (`wres_meshes_reset`) and loads the radius again, its meshes with it, then primes. Daggerfall and a walk east use 1.4 MB, so it takes a long journey across climates.

### Tests

`check_stream` in `tests/check_dagger.c`: a stream at Daggerfall fills its 25 chunks, has the town's houses, flats, doors, lamps and sounds; its doors are the town's; a sound is heard beside it; its lamps' pools lie round their lamps and are the same cut chunk by chunk as all at once; its far ground covers the near ground's area in 25 quads, 25 of them baked (`build/far0.ppm` is one); a walk of 40 chunks east keeps ground under the camera, moves the origin twice, keeps the pools with their lamps and the pictures and files under their caps; a jump east into another climate brings its sky; the season's turn loads the ring again in its pictures. `check_wres`: held pictures stay, the least recently released go first, come back when held, and a forgotten dynamic slot is reused. `check_texres` (engine): the buddy allocator, the levels by distance, the budget, the eviction.

### Not yet

- Draws grouped by texture, contribution culling, the reading thread, cooked data: each when the 233's profile says it is the cost.
- The rebase (every 16 chunks) moves every slot's models and decals in one frame: 3.5 ms on the 233 by the host's measure, every two map pixels.
- The disk. Every read is still in the frame that asks for it: a TEXTURE file 32 KB a call, a block's bytes (`STEP_LIST`, BLOCKS.BSA, read again by each step that needs them, from Windows' cache after the first), a mesh's (`STEP_MESHES`), a song's at a change of weather or of day and night (its parse 12 to 15 ms on the 233), a sky picture in nine pieces. The host's page cache hides what the 233's disk costs; a seek is about 10 ms. The cure is a reading thread (`CreateThread` is KERNEL32's): the stream asks for the bytes a step needs a few steps ahead, and the step finds them read. When the 233's log names a step that is a read.
- A mesh's parse is one step whatever its size: model 451, the slowest, 0.8 ms on the host, about 30 on the 233. Each is parsed once a session.
- Horizon culling is not planned. The far clip is 16k at most, a few chunks of ground, and the fog, the clip and the frustum already cut most of what a hill would hide. The far ground costs 24 triangles a chunk, so there is little left to save.
- A swamp's own ambience (DFU's frogs). The music and the rest of the ambience are already DFU's: it plays the weather's playlists in a town or out of one.

## Phase 4, as built

### The land's height

`terrain.c`, plain C, host-tested. Daggerfall Unity's `DefaultTerrainSampler` in our own form: a smooth function of the world position, so two chunks agree on their shared edge to the bit and nothing has to be stitched.

- In DFU's "scaled" units: 8 × the height map's byte, plus 4 × WOODS.WLD's finer heights, plus a little noise, never under the sea floor, 27.2. A world y is 60 units a scaled unit above the sea floor (DFU's terrain scale of 1.5 m, at 40 units a metre), so the sea is y 0. Land bytes run to 109: the highest land is about 77,000 units (1.9 km), Daggerfall 13,374 (334 m).
- The height map: each byte on its pixel's south-east corner (where DFU's sampler has it), Catmull-Rom between.
- The finer heights: each pixel's record in WOODS.WLD holds a 5 × 5 block at +22, row 0 north, on the height map's scale. Its inner 3 × 3 are the pixel's thirds (the outer ring repeats the neighbours'), on average 6 above the pixel's own byte. Each is at the middle of its third, Catmull-Rom between. `worldmap_detail` reads a record on demand (two small reads, the 26 MB file never whole) into a cache of 1024 pixels. They put a low hummock on most pixels; DFU's land has them too.
- The noise: DFU's 10 × low × high, each half of a gradient noise, but at waves of 16 and 8 tiles where DFU's are about 3 and 1: our grid is 4 tiles, and finer bumps would only alias.

### Towns on the land

A town is flat at its level: the mean of the land's heights over its rectangle (9 × 9 samples, `terrain_level`), worked out once a location and kept with its record. Round it the land eases to that level over `TERRAIN_BLEND` (4096, one chunk), with strength (1 − dx/4096)(1 − dz/4096), smoothstepped, where dx and dz are the distances outside the rectangle; the strength is 1 at the rectangle's edge, so the land meets the town's ground there exactly. DFU blends over the whole pixel instead; with a reach of one chunk, a chunk on a pixel's edge ring also takes the neighbour pixel's town (`gather_flats`). Every block of the town, its ground, buildings, flats, lamps, sounds and doors, is raised to the level; the land's cells under the rectangle are not drawn (a town lies on 2048s, so the cells divide cleanly). `wild_ground` is the ground's y anywhere, for the camera's start and the step out of a door.

### The land's tiles

At each tile corner a kind, as DFU's `DefaultTerrainTexturing`: water at the sea floor; dirt up to the beach (scaled 40, ± 1.5 by a hash of the corner); else a three-octave noise (0.05 a tile, amplitudes 0.9, 0.36, 0.144) under 0.5 is dirt, over 0.95 stone, else grass. Each tile then takes DFU's marching table: the four corners' kinds make a shape (bit 0 of each: dirt and stone 1) and a ring (their mean), and those pick the archive's transition record, turned (0x40) and flipped (0x80). The tiles are kept as an RMB block keeps its own (row 0 north), so the near mesh, the far bake and `rmb.c` read them alike; a map of them shows the transitions join (checked by eye, edges continuous). The archive is the pixel's climate family's ground (the sea's swamp's), in winter its snowy one.

### The near ground

A grid of heights 1024 apart (5 × 5 a chunk). Each 1024 cell not under a town is drawn as rectangles of one tile each, found greedily: the first tile not taken, as wide as its row runs, as tall as whole rows of that width run. Its tile repeats across it, turned and flipped as `leaf_uv` says. Each corner's height is bilinear in its cell: along any axis-aligned line in a cell bilinear is straight, so a rectangle's corner on a neighbour's edge lies on that edge and nothing cracks. Each triangle is a face of its own (a quad is not flat, and `draw.c` sends a face as one polygon). Triangles are sorted by picture, one bind a run. Over 1246 land chunks: 122 triangles on average, 224 at most (a quadtree of squares gave 187 and 350). A plain meadow is 32.

### The far ground

The same grid's even points: 2 × 2 quads 2048 apart, under the land's baked picture. Each edge's two 2048 lengths get a skirt hung down by 64 plus twice how far the near grid's middle point stands off the far line, so a near neighbour's edge, bent at its middle, never shows sky under it. 24 triangles a chunk at most.

### Flora

Daggerfall Unity's `DefaultTerrainNature`: a chance on each plain tile of dirt (0.2), grass (0.9) or stone (0.05), times the pixel's height over 128 (between 0.4 and 1), a quarter in the desert; nothing on water, a transition, the beach, ground steeper than 50°, or within 1024 of a town. Records 1 to 31 of the climate's nature archive (500 to 511, the woodland and mountain sets snowy in winter). DFU draws thousands a map pixel as one batch on a modern card; here `FLORA_DENSITY` (0.3) of those chances stand, at most 64 a chunk, about 18 on average, chosen by a hash of each tile's place so a chunk is the same every time it loads. Each stands on the land a little sunk (more on a slope), and is drawn only within `FLORA_SIGHT` (3072) of the camera across the ground: `Flat.sight`, in 16s, which `scene_visible_sprites` checks before anything else.

The console's switches act on what stands, with no reload: `flora` (on, off; alone it flips), `floradensity` (the percent drawn, by each one's rank, `Flat.thin`, an even spread over 0..255 from a hash of its own; 40 by default, so 0.12 of DFU's chances draw), `florasight` (units, in place of each flat's own sight). They are the engine's (`scene_set_flora*`): a flat with a sight is flora to them.

### What the land costs

On the host walking out of Daggerfall: a chunk's heights and tiles 0.03 ms (about 1.3 ms on the 233), its far bakes 0.04 ms. Thirty chunks east of Daggerfall: 3802 near triangles (127 a chunk, but only the camera's ring draws them), 24 far a chunk, 549 flora standing, 40% of them drawn by default. Over 96,000 cells of the map, 88% rise under 0.1 (6°), and only a few cliffs pass the walker's 45°.

## Dungeons

A dungeon loaded whole takes about 10 s on the 233 for Daggerfall's castle, down from 20 after the load cuts. Streamed by block, it loads the blocks round the camera under the hold and the rest as it walks. The engine gives the stream's indoor parts (below), and the game a chunk source of RDB blocks (`places/`, like `wild.c`; `docs/game.md`). State that outlives a block's drop is `keep.c`'s (`docs/interact.md`).

### One stream, a chunk a block

A dungeon streams through `stream.c` as the world does: the same slots, budget, fill steps, `changed`, `drop` and the light queue. One thing changes: the chunk's size is the stream's, given at `stream_open`. The world's is `STREAM_CHUNK`, 4096. A dungeon's is 2048, one RDB block a chunk, named by its place in the dungeon's layout (`cx`, `cz` from the block's X and Z). `stream_chunk_size()` reads it back. Everything in `stream.c` that reads `STREAM_CHUNK` reads the stream's size instead.

- **The origin.** A dungeon is at most 64 blocks: 16384 units a side, well inside a float's exact range. Its origin never moves in practice: the rebase is at 16 chunks from the origin, 32768 units, past any dungeon. It is the same code, not a second path. The origin chunk is the layout's (0, 0).
- **The ring.** Radius 1 (3 × 3 blocks) is enough. The far clip is 1400 and a block is 2048, so from anywhere in the camera's block, the edge of the ring is at least 2048 away. A block is dropped two rings out (`STREAM_KEEP`), so up to 5 × 5 are held.
- **The slots** are sized by the stream's radius at open, not always `STREAM_RADIUS_MAX`'s 121. That is 25 at radius 1, and `stream_set_radius` cannot go past the open's radius. A dungeon block's caps (models, flats, lights, usables, movers) are larger than a town chunk's, so 25 slots of them, not 121.
- **A missing block.** A dungeon's layout is not a full grid. A chunk with no block is filled at once with nothing. Its slot is empty and every pass skips it, as a world chunk with no land would be.
- **No ground and no sky.** A dungeon chunk writes no near or far ground (`near_n`, `far_n` 0), so its ground span is empty. The level has no sky, and the stream's far ground, climate and season-following are `wild.c`'s, not the engine's.

### What a slot holds, two more kinds

A slot already holds models, sprites, lights, emitters, usables and movers. Two kinds join them:
- **Entities.** `StreamCaps.entities`, `StreamOut.entities`. The stream owns one array of every slot's entities, and the scene's `entities` points at it. An empty slot's entities are not live. The game reaches a slot's entities, writable, for its creatures' motors (`stream_slot_entities(slot, &first, &n)`) and through the chunk's data. The creatures' pictures come in as `world_creatures_step` reads them today, nearest first.
- **Water.** `StreamCaps.water`, `StreamOut.water`: a block's rectangles (`docs/water.md`). An empty slot's rectangles are 0 wide. `scene_water_at` and the water pass skip them.

Both move with the slot on a rebase, as everything else does.

### Kept state, by the stream

`stream.c` calls `keep_restore` for a slot's movers and entities when its fill is done, before it is in the scene. It calls `keep_save` for them before its `drop`, with the stream's origin in the place's frame as `o` (the origin chunk × the chunk's size). A loaded chunk's changes are only on its own arrays until then, so a save game calls `stream_keep_now` before it writes the keep table: the same `keep_save` for every loaded chunk, nothing dropped, and nothing with no stream open. A chunk still being filled is skipped, since nothing in it has been shown. The source's `changed` for a chunk loaded runs after `keep_restore`, and the game relies on that: `dunstream.c` reads a keep row there to draw a dropped pile again, because `Entity.live` is never kept. The game only gives the keys at build, as it does for a whole place. A chunk's movers are `mover_init`'s before its fill ends (`doors_build` does that). The world's chunks get the same calls; while they have no keys, the calls do nothing.

### The rest of the engine indoors

- **Lights.** These are already per slot (`light_stream_*`). A streamed lamp casts no shadow: the rays were 97% of a lamp's cut, and outdoors they change little. Indoors they matter: without them a torch lights the next corridor through its wall. So a stream can ask for shadows (`light_stream_shadow(1)`), and then a light's cut is split into calls of a few faces each, picked up again where it left off, because one lamp's rays are tens of ms on the 233. Cut chunk by chunk, the decals must be the same as a whole load's (tested, as the world's are).
- **Collision.** Already by span for a streamed scene (`collide_gather`). Nothing new.
- **Occlusion.** The cells are drawn per frame in screen space from the models in view (`docs/culling.md`), not built per block. Nothing new.
- **Use, chains, teleports.** Movers and usables are per slot already ("Doors from the stream"). A chain and a teleport stay within their block, so neither is ever half loaded. A teleport's destination is in its block's frame, moved into the scene's like its models.
- **Ambience.** The emitters are per slot already. The song is the place's, set once.
- **Treasure and spare things.** A block's random treasure markers and TEXTURE.216 flats are entities in its slot, not flats, and each block adds `DUNSTREAM_SPARES` entities not drawn until the game puts one down (a dropped pile). So a slot's entities are more than its creatures: `dunstream_kind` is -1 for these. See `docs/game.md`, Loot and the inventory.
- **The card.** Pictures by residency, as the world's (`texres`, `wres`): a block's walls and creatures held when it fills, read in steps.

### The first frame

Under the hold, the source fills the camera's ring (3 × 3 blocks, the ones present) before the first frame, then `gfx_prime`, as `wild_fill` does for a town. The rest streams in as the camera walks. The castle then loads 9 of its blocks up front, not all of them. Its `load:` line (`docs/viewers.md`) names what the remaining seconds are.

### Built so far

Step 1 is built:
- `stream_open_sized(scene, src, radius, cx, cz, chunk)`, `stream_chunk_size()`. `stream_open` stays the world's, with `STREAM_CHUNK` and 121 slots.
- Slots by the open's radius, which `stream_set_radius` cannot pass.
- `StreamCaps.entities` and `.water`, `StreamOut.entities`, `.water`, `entities_n` and `water_n`. With caps of 0 (the world's) the stream leaves `Scene.entities` and `Scene.water` to the game.
- `stream_slot_entities(slot, &first, &n)`.
- `keep_restore` in `finish` and `keep_save` in `drop_slot`, with the origin chunk × chunk size as `o`. A kept creature's killing is `Entity.dead`, the game's byte. `live` is only "drawn", and keep never touches it, so a creature saved before its pictures were read is not taken for dead.
- `check_dungeon_stream`: a fake source of 2048 chunks on a sparse layout. It checks:
  - the 3 × 3 ring loads 3 blocks and 6 empty chunks;
  - the water is where its block is;
  - a slot's entities are in the scene's frame;
  - after walking 6 blocks away and back, a door opened and a creature moved and killed come back so, and an unopened door comes back shut.

Step 2 is built too:
- A streamed scene that is not outdoor keeps its shadow rays (`seen`), and outdoors stays without them. `light.c` already knows which a scene is, so the source asks for nothing.
- A light's cut goes a face at a time (`cut_start`, `cut_run`, `cut_finish`), so the service stops within its budget mid-light and goes on at the next call. The light reports no decals until `cut_finish` has stored them whole, since a frame may draw between those calls (`docs/lighting.md`).
- The light being cut is given up and queued again when a slot drops or the origin moves (`cut_abandon`), since its list of models is stale. A slot coming in needs nothing, because its neighbours' lights are queued again anyway.
- `light_stream_pending` counts it.
- `light.c` takes the stream's slot count (`stream_slots`), not `STREAM_SLOTS`. Before this, a dungeon's 25 slots were read as 121, and every slot's lights were the wrong range.
- `check_dungeon_lights`: Privateer's Hold by block cuts 1667 decals, the same as the hold loaded whole with shadows. Cut again with no budget, a face a call, it takes 8636 calls for 76 lights and slots queued, and comes out the same 1667; after every call, no light reports decals it does not have. Daggerfall's outdoor stream keeps its 4435.

The game's step 3 (`dunstream.c`) is in. Step 4 (GFALL's hold, LEVELFALL) is the game's next.

### The order of the work

1. **Engine:**
   - the chunk size per stream, slots by radius;
   - entities and water in the slots;
   - `keep_save` and `keep_restore` by the stream;
   - host tests with a fake source of 2048 chunks on a sparse layout. These check that blocks fill and drop, that a door opened in a dropped block comes back open, that a creature moved stays moved, and that the water and entities follow their slots.
2. **Engine:** lights with shadows in steps. Tested against a whole load's decals (built).
3. **Game:** the RDB chunk source, reusing `world.c`'s per-block building, with its keys.
4. **Game:** GFALL's hold through it, then LEVELFALL's indoor rows.

## Open questions

- How long a city takes to stream in on the 233, and whether a block's steps fit the budget, can only be measured there.
- How sharp the residency manager can keep a city on a 2 MB card at walking speed, and what upload budget a frame can spare.
- Indoors (dungeons, houses) stays as it is: a place of its own, entered from the stream and left back to it.
- Whether 128 texels a far chunk reads well enough at 4096 to 8192 units, or a 64 one would do and save the RAM (16 KB and its chain a town chunk now).
