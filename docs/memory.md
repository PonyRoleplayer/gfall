# Memory

Two different budgets. System RAM is the Win32 heap. Texture memory is the 2MB on the Voodoo. `out of memory` is the heap. `texture memory is full` is the card. A city used to die on the first, then on the second. Read this before changing `assemble`, `rmb_load`, `rdb_load`, `arch3d_load`, or `texcache`.

Copies and fills (`mem_copy`, `mem_fill`, `mem_zero`, and the CRT's `memcpy`, `memmove` forward, `memset`) go a 32-bit word at a time on the target (`rep movsd`, `rep stosd`), the odd bytes after.

There is no `mem_free` on the main or temp heap. Bytes live until `mem_temp_end` destroys the temp heap (a no-op on the host). Do not allocate a size you will not use. A streamed world is the exception, below: private heaps that free.

## Why a city died on the heap

Daggerfall is region 17, index 1231, location id 50026, an 8 by 8 grid. That is 64 RMB names and 45 distinct files. It places about 3500 instances once the ground is split into 512-unit patches, and about 202 different ARCH3D ids. The old loader did three wasteful things:

- The mesh cache lived inside one `rmb_load` and was thrown away with the block. Those 202 models were parsed 580 times. Each parse kept the raw ARCH3D record and a triangle buffer of `file_size / 8` slots. One copy of that buffer was about 10MB. The repeats were about 25MB.
- Each block reserved 2048 model slots and 2048 sprite slots, then `append_block` copied the few that were used and left the arrays behind. Across 64 blocks that was about 10MB of empty slots.
- The RMB bytes and the ARCH3D bytes stayed allocated after the mesh existed.

Privateer's Hold never showed this. It is 5 blocks. hostcheck still expects 365 models and 9075 triangles there.

## One mesh for the place

`assemble` in `world.c` allocates one `ArchEnt` table (`ARCH_CACHE`, 4096) and passes it to every `rmb_load` and `rdb_load`. `arch_cached` returns the mesh it already built for that id. The mesh is in model-local space. The instance holds the position and the rotation, so five copies of `WALLAA06.RMB` point at one `Mesh`.

`arch3d_load` walks the planes, counts the triangles `emit_tri` will accept, and allocates that many. It does not size the buffer from the file length. UV failures are counted on the emit pass only.

`block_reserve` allocates the number of instances, flats, and markers the record will place. The old 2048 / 2048 / 256 numbers remain as a ceiling for a corrupt file, not as the allocation. A missing mesh still counts in the tally, so a few slots can stay empty. Ground allocates two triangles per quad whose record exists, not 512 up front.

The world lists are still 8192 instances and 8192 flats. That is about 0.6MB and is the "dungeon is too large" cap. Do not grow the per-block arrays back to 2048 to "be safe".

## Scratch for file bytes

`mem_scratch` is two reusable buffers. Slot 0 (`BSA_SCRATCH_BLOCK`) holds the RMB or RDB being parsed. Slot 1 (`BSA_SCRATCH_MODEL`) holds the ARCH3D record. A model is read while the block file is still live, so they cannot share one slot. `bsa_read_scratch` fills the slot. The pointer is invalid after the next read of that slot.

The slot grows by doubling from 4KB. The previous buffer is abandoned until the temp heap dies. On Win32, `mem_temp_end` drops the pointers before `HeapDestroy`, because those bytes came from that heap. The host malloc heap is not rewound, so the host keeps the buffers and frees them in `mem_shutdown`.

`bsa_read` is still the call that keeps bytes. Map files and anything else that holds a pointer across another read must use it. Do not point a mesh at scratch memory. `arch3d_load` copies the points out before it returns.

Texture records are not in this cache. `place_image` already keeps one `Image` per archive plus record. The same brick on fifty walls is one picture. The whole `TEXTURE.###` file stays in `texture.c` until `texture_cache_clear`, which must run before `mem_temp_end`.

## Sound

About 1.5 MB on the main heap, kept for the process (`mem_keep`), since a voice may play a clip after the place that asked for it is gone: the clips loaded so far (at most 3 MB, a place's few hundred KB), the song buffers and the MIDI stream. A place's emitters are on its own heap. See `docs/sound.md`.

## The world map and the stream

`worldmap.c` keeps 2 MB for the process, on the main heap (`mem_keep`): `WOODS.WLD`'s height map (500 KB), the climate a pixel (500 KB), and the location a pixel (1 MB of 16-bit numbers, and the 15,251 locations' region and index). A stream's slots are allocated once on its temp heap (see "A streamed world" below); the wilderness's patch meshes are 8, shared, made again when the season turns. See `docs/streaming.md`.

## A streamed world

The stream can free (`docs/streaming.md`, Memory that can be freed). `mem_heap_new` makes a private heap (`HeapCreate`; malloc with a block list on the host); `mem_heap_use` points `mem_alloc` at it and gives back the one it replaces; `mem_heap_free` frees one block of it, `mem_heap_delete` all of it. `mem_track` lists every allocation made in the heap current when it was set, and no nested heap's, so a picture can be freed piece by piece. Heaps never move `mem_epoch`. `mem_scratch` always takes the main heap.

| What | Where | Cap |
|---|---|---|
| A chunk: its block, ground, far quads and baked picture (16 KB), doors' work lists | the chunk's heap, deleted when it drops | a chunk's |
| A chunk's lamps' decals | a heap per slot in `light.c`, deleted with the chunk | a chunk's |
| Pictures' pixels | `wres`'s pictures' heap, each under a track | 8 MB of unheld pictures (`wres_trim`) |
| Their mip chains | `texcache`'s chain heap, freed with the picture | follows the pictures |
| Meshes, per climate variant | `wres`'s meshes' heap, for the session | 10 MB, reported (`wres_full`) |
| `TEXTURE` files | `texture.c`'s own heap, least recently used freed | 4 MB (`texture_cache_limit`), the file just read kept |
| Ground tiles shrunk for far ground | static in `wild.c` | 32 KB |
| A block's walking cells | the chunk's heap | 4 KB a block |
| The townspeople's pictures | `wres`, held while a town wants them | 54 sheets in a city: 4 MB of pixels, 2.3 MB of mip levels |
| The land's far picture (16 KB) and its near and far meshes (a few KB) | the chunk's heap | a chunk's |
| WOODS.WLD's finer heights, 1024 pixels' | static in `worldmap.c` | 13 KB |
| MAPS.BSA kept open, the last region's location records | `dungeon.c`'s own heap (`exterior_keep`) | one region, a few hundred KB |
| The slots' arrays, light and sound tables | the stream's temp heap | about 4 MB at 121 slots |

A cutout picture (a sprite: a person, a tree, a flat) keeps its mip levels in RAM from half size down; its full level, three quarters of a 16-bit chain, is made again from its 8-bit picture when the card is sent it (`texcache.c`, `Chain.first`), into a buffer of `MIP_MAX_BYTES` kept for the process. A 256 sheet keeps 43 KB, not 171. That rebuild runs in the card's service after the frame, whenever a sprite near the camera goes up at full size: it was 20 ms of a 256 sheet on the 233, every clear texel of the padding gathering its 3 × 3, and is 2 to 5 ms now that only each row's opaque span and a texel either side are worked, straight to 16 bits (`cutout_top` in `mip.c`, the same bits). Walls keep every level: at 8 bits their full level is cheap.

On the card, a streamed world's sprites go up as one level, no chain: a 256 sheet takes a 128 KB block, not 256 (`docs/streaming.md`, The card).

On the card, a streamed world's pictures do not go up at load. The sky, falloff and font first; the rest of TMU0 is `texres.c`'s buddy allocator. Every picture has an 8 × 8 copy; sharper ones go up after the frame, at most 64 KB and 8 downloads a frame, chosen by how near each picture was drawn, the least recently drawn freed when short of room. This is not the drip warned against below: little is built during the frame (the chains are made when a picture's pixels are read and kept; only a sprite's full level is made again, above), the downloads come after the frame's last triangle under a byte budget, and the 8 × 8 copy means a face is never left on a wrong texture while it waits.

## Doors and actions

About 520 KB more per place, on its own heap: a `ModelAct` and a block byte for each of the 8192 model slots, 2048 usables and 1024 movers, and a work list while they are built. See `docs/interact.md`.

## The 2MB texture card

The image table is 384 (`TEX_SLOTS`). Daggerfall named 235 pictures before towns took their climate's pictures (`docs/daylight.md`), and 216 since. Wayrest names 273, and a table of 256 rejected that open with `texture count` after the world had loaded. The full squares still do not fit the card: Daggerfall's are about 2.8MB. `gfx_prepare` used to send every full square and fail with `texture memory is full` before the first frame.

`texcache_upload` sends each picture once, at load, as a mip chain, after a budget that takes the top level off the largest pictures until the level fits, the walls' first (two levels each) and the sprites' last. `docs/glide.md` has the budget and what it does to the castle and the cities. When a chain was first tried, the ground turned into bands and the font into the wrong glyphs, and the cause was taken to be even and odd levels sharing one address. That is doubtful: a download with `GR_MIPMAPLEVELMASK_BOTH` puts the whole chain at its start address. A size that left too little room for the chain would scramble the same way, and the upload size is now the larger of `grTexCalcMemRequired` and `grTexTextureMemRequired`. The history does not show which it was. `mipmap off` in the console is the comparison if the bands come back. A picture that still does not fit is left unbound. `texcache_bind` then returns without binding, so that picture's faces draw with the previous texture, not magenta. `glide_upload` hands back a handle, the index of its record, and `glide_bind` takes it directly and skips a repeat. `texcache_stats` reports live pictures, unfit ones, the level's count, and the bytes uploaded; LEVELFALL shows them in its frame panel. The font sheet goes on the card before the pictures, with the sky and the light falloff, so the budget's room is exact. It used to keep back 192 KB for a font uploaded after them. The sheet is 256 × 256 of `ALPHA_INTENSITY_44`, one byte a texel, 64 KB: its glyphs are ink or nothing, white on the card, drawn with the ink's colour as the tint (`glide_upload_ai44`, `ui_string_alpha`). As ARGB 1555 it was 128 KB. Together that is 128 KB more for the place's pictures: on a 2 MB card, Privateer's Hold now fits whole, and Daggerfall and its castle keep 8 more pictures at full size. Glide still maps s,t across 0..256, so the face UVs do not change.

A place loaded whole that has entities (a dungeon's creatures, `docs/entities.md`) keeps their pictures out of that budget. A picture only entities wear (no flat, no face uses it; `mark_entity_pictures` finds them from `Scene.entities`) is not sent at load: the budget runs on the room less a reserve, as much as those pictures take at full size, one level each, up to `ENTITY_ROOM` (512 KB, four creatures near at full size), and never less than their 8 × 8 copies. Then the rest of TMU0 opens a residency session as a streamed world's (`texres.c`): each of those pictures gets its 8 × 8 copy at once, the frame notes each creature's pictures by its distance (the view shown at its distance, the others four times farther), and after the frame sharper copies go up, 64 KB a frame (`texcache_resident`, the service call at the end of `draw_frame`). They are sprites, single levels, from the chain kept in RAM from half size (the stream's `Chain`). Privateer's Hold's 42 creatures wear 100 pictures, 8.7 MB at full size; sent at load they would have taken every wall down to a quarter. Its walls and flats need 1758 KB of chains, so with the reserve the largest of them give up a level: the sprites-first rule of the stream, kept here.

A place too large for the card at full size (its pictures' chains past the room, entities' reserve taken off) is not budgeted at load: every picture of it goes on the card by distance, as a streamed world's (`all_resident` in `texcache.c`, `texcache_resident_all`): walls with their chains at a texel for four world units, flats single level at their own texels over their world width, entities as above. The frame notes each drawn model's slot keys (`note_keys`, made by `paint`), sprite and entity, and the service sends sharper copies after it; `gfx_prime` sends the view's before the first frame. Daggerfall's castle needs 5 MB at full size, 3.8 of it its flats: budgeted at load, the walls gave two levels each (the sprites' grace, `MIP_WALL_GRACE`) and 145 of its pictures drew at a quarter size everywhere, blurry beside its sharp sprites. Now it is sharp near and small far, as the city in the stream round it. A place that fits is sent whole at load, as before. LEVELFALL's texture line is then the residency's (`tex sharp/added  usedK  ram NM`).

What entering a place costs is LEVELFALL's load line (`docs/viewers.md`). Daggerfall's castle took over 20 s on the 233. By the host's measure (×40) its creatures were 2.3 s and 6 MB of its 7.5 MB of files, its lights 0.9 s, its chains 0.7 s; its creatures are now read as they come near (`docs/entities.md`) and its lights' count casts no rays (`docs/lighting.md`): about 2.4 s of work instead of 4.4, and 5.8 MB read. Still to measure on the 233: what the rest of the 20 s is, the disk most likely.

`texcap` in the console holds TMU0 to a smaller card's size for testing, as from `grTexMinAddress`: `texcap 2048` is a 2 MB Voodoo 1, on a card with a 4 MB TMU (a Voodoo 2, or a 6 MB Voodoo 1). Everything the level sends has to fit under it, the font and the sky included, and a change sends the level again. 0, the default, is the card's own. It is a debugging tool: the budget already fits any card, and nothing else should set it. Tested on a Voodoo 2 from the card's own size down to `texcap 512`, the floor: the level still loads and draws, its pictures smaller, and the font draws as before. On a 2 MB card Daggerfall needs 3881 KB at full size and gets 1632 KB: 81 of its pictures go to half size, 3 to a quarter.

That font is one 256 square, uploaded once per `gfx_prepare`, and it is the only glyph sheet. With no `font_set`, the square is the stock OEM fixed font: 256 glyphs in 16-pixel cells, index 0 clear, index 1 white. Those pixels are a static buffer, so `mem_temp_end` does not free them. `GFALL.EXE` calls `font_set` from `FONT0000.FNT` before `gfx_prepare`, and that sheet is uploaded instead. Do not upload both. A stock glyph taller than 16 pixels is not stamped; the HUD then draws the built-in 8x8 rects. `RENDERFALL.EXE` and `LEVELFALL.EXE` leave the OEM sheet in place.

Nothing is uploaded during the frame. Do not put the drip back: walking the city and downloading a sharp copy when the camera reached a wall was a second renderer on the 233, and the close picture stayed the 16 by 16 until that download finished.

The triangle still stores one slot number. Do not write a second slot into the mesh. Two slots per record would blow the 384-image table.

`GPU_SLOTS` is 768. That array only tracks uploads. The hardware limit is the 2MB. The image table has to stay below that 768, with room left for the font.

A lit place keeps its light decals on the level's heap: castle 8285 decals and 32358 corners, about 1 MB at the default range (the hold 300 KB), and the falloff pictures take 64 KB of TMU0 before the texture budget runs (`docs/lighting.md`). Outdoors the lamps' decals are on the same heap, 774 KB in Daggerfall (5880 decals), and the sky takes 288 KB of TMU0, also before the budget. The sky's buffers are static, not on a heap: the circle in `sky.c` (220 KB), one tile (64 KB), the dome (32 KB), and a half picture in `skyfile.c` (112 KB) (`docs/daylight.md`).

An animated flat's frames share one picture, a sheet of them in the square its first frame was padded into, or the next one up (`docs/sprites.md`): 190 KB more for the hold's flames, raw, where a picture per frame was 440 KB.

Collision (`docs/collision.md`) keeps a grid of model indices on the place's heap, rebuilt by `mem_epoch`, tens of KB in a city, and a static working set of 1024 triangles, 53 KB.

`light.c` keeps its decal buffers to reuse when `lightrange` cuts the same place again. They come from `mem_alloc`, so in LEVELFALL and RENDERFALL they are on the place's temp heap. Leaving Daggerfall and opening a smaller place reused them after that heap was destroyed: an invalid page fault in `cut_face`. `mem_epoch` now changes at every `mem_temp_begin` and `mem_temp_end`, and a module that reuses a `mem_alloc` buffer records the epoch with it and allocates again when it moved. `draw.c`'s cache of picture scales was keyed by `Image` pointer, which a new temp heap can hand out again; `draw_prepare` empties it at every `gfx_prepare`. `arch3d.c`'s weld tables are static, about 260 KB: every face corner of the model being loaded, up to 3072. The engine log (`log.c`) is 256 lines of 96 characters, static, about 25KB. The console's `exec` buffers are four of 4KB, static. The command table and the 32-line history are static too. The first time it opens it allocates one 640 by 480 frame at 16 bits (about 600KB) on the main heap with `mem_keep` and keeps it. It used `mem_alloc`, which in LEVELFALL and RENDERFALL lands on the temp heap, because their views run inside one. Leaving the place destroyed that heap and left the pointer: opening the console in the next place made `grLfbReadRegion` copy the frame into freed pages, an invalid page fault in GLIDE2X.DLL. A buffer that is allocated on first use and kept for the process must use `mem_keep`. That grab is the picture shown while the world is not drawing. Do not grow the log with the session, and do not take another grab per frame. `shot.c` keeps a second buffer of the same size, allocated with `mem_keep` on the first screenshot and reused; F12 in a second place hit the same fault. The BMP is written a row at a time. Do not put that buffer in the console.

`paint` records each mesh's unique texture slots in `slot_keys`. Nothing in the frame reads that list anymore. Do not bring the distance bands back to decide which map is sharp.
