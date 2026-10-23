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

## The 2MB texture card

The image table is 384 (`TEX_SLOTS`). Daggerfall names 235 pictures. Wayrest names 273, and a table of 256 rejected that open with `texture count` after the world had loaded. The full squares still do not fit the card: Daggerfall's are about 2.8MB. `gfx_prepare` used to send every full square and fail with `texture memory is full` before the first frame.

`texcache_upload` sends each picture once, at full size, with `smallLod` equal to `largeLod`. `grTexMipMapMode` stays `GR_MIPMAP_DISABLE`. A mip chain on a 2MB TMU writes even and odd levels to one address, and the odd levels replace the picture. That is what turned the ground into bands and the font into the wrong glyphs. A picture that does not fit is left unbound. 192KB stays free for the font sheet, uploaded after the pictures. Glide still maps s,t across 0..256, so the face UVs do not change.

That font is one 256 square, uploaded once per `gfx_prepare`, and it is the only glyph sheet. With no `font_set`, the square is the stock OEM fixed font: 256 glyphs in 16-pixel cells, index 0 clear, index 1 white. Those pixels are a static buffer, so `mem_temp_end` does not free them. `GFALL.EXE` calls `font_set` from `FONT0000.FNT` before `gfx_prepare`, and that sheet is uploaded instead. Do not upload both. A stock glyph taller than 16 pixels is not stamped; the HUD then draws the built-in 8x8 rects. `RENDERFALL.EXE` and `LEVELFALL.EXE` leave the OEM sheet in place.

Nothing is uploaded during the frame. Do not put the drip back: walking the city and downloading a sharp copy when the camera reached a wall was a second renderer on the 233, and the close picture stayed the 16 by 16 until that download finished.

The triangle still stores one slot number. Do not write a second slot into the mesh. Two slots per record would blow the 384-image table.

`GPU_SLOTS` is 768. That array only tracks uploads. The hardware limit is the 2MB. The image table has to stay below that 768, with room left for the font.

The console log is 48 lines of 96 characters, static. The first time it opens it allocates one 640 by 480 frame at 16 bits (about 600KB) on the main heap and keeps it. That grab is the picture shown while the world is not drawing. Do not grow the log with the session, and do not take another grab per frame. `shot.c` keeps a second buffer of the same size, allocated on the first screenshot and reused. The BMP is written a row at a time. Do not put that buffer in the console.

`paint` records each mesh's unique texture slots in `slot_keys`. Nothing in the frame reads that list anymore. Do not bring the distance bands back to decide which map is sharp.
