# Memory

Two different budgets. System RAM is the Win32 heap. Texture memory is the 2MB on the Voodoo. `out of memory` is the heap. `texture memory is full` is the card. A city used to die on the first, then on the second. Read this before changing `assemble`, `rmb_load`, `rdb_load`, `arch3d_load`, or `texcache`.

There is no `mem_free`. Bytes live until `mem_temp_end` destroys the temp heap (a no-op on the host). Do not allocate a size you will not use.

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

`worldmap.c` keeps 2 MB for the process, on the main heap (`mem_keep`): `WOODS.WLD`'s height map (500 KB), the climate a pixel (500 KB), and the location a pixel (1 MB of 16-bit numbers, and the 15,251 locations' region and index). A stream's slots are allocated once on its temp heap: 121 slots of 16 models, about 100 KB of models and boxes; the wilderness's patch meshes are 8, shared. See `docs/streaming.md`.

## Doors and actions

About 520 KB more per place, on its own heap: a `ModelAct` and a block byte for each of the 8192 model slots, 2048 usables and 1024 movers, and a work list while they are built. See `docs/interact.md`.

## The 2MB texture card

The image table is 384 (`TEX_SLOTS`). Daggerfall named 235 pictures before towns took their climate's pictures (`docs/daylight.md`), and 216 since. Wayrest names 273, and a table of 256 rejected that open with `texture count` after the world had loaded. The full squares still do not fit the card: Daggerfall's are about 2.8MB. `gfx_prepare` used to send every full square and fail with `texture memory is full` before the first frame.

`texcache_upload` sends each picture once, at load, as a mip chain, after a budget that takes the top level off the largest pictures until the level fits. `docs/glide.md` has the budget and what it does to the castle and the cities. When a chain was first tried, the ground turned into bands and the font into the wrong glyphs, and the cause was taken to be even and odd levels sharing one address. That is doubtful: a download with `GR_MIPMAPLEVELMASK_BOTH` puts the whole chain at its start address. A size that left too little room for the chain would scramble the same way, and the upload size is now the larger of `grTexCalcMemRequired` and `grTexTextureMemRequired`. The history does not show which it was. `mipmap off` in the console is the comparison if the bands come back. A picture that still does not fit is left unbound. `texcache_bind` then returns without binding, so that picture's faces draw with the previous texture, not magenta. `glide_upload` hands back a handle, the index of its record, and `glide_bind` takes it directly and skips a repeat. `texcache_stats` reports live pictures, unfit ones, the level's count, and the bytes uploaded; LEVELFALL shows them in its frame panel. The font sheet goes on the card before the pictures, with the sky and the light falloff, so the budget's room is exact. It used to keep back 192 KB for a font uploaded after them. The sheet is 256 × 256 of `ALPHA_INTENSITY_44`, one byte a texel, 64 KB: its glyphs are ink or nothing, white on the card, drawn with the ink's colour as the tint (`glide_upload_ai44`, `ui_string_alpha`). As ARGB 1555 it was 128 KB. Together that is 128 KB more for the place's pictures: on a 2 MB card, Privateer's Hold now fits whole, and Daggerfall and its castle keep 8 more pictures at full size. Glide still maps s,t across 0..256, so the face UVs do not change.

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
