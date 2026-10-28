# Glide

`src/engine/gfx/glide.c` is the only file that calls Glide. This note is the state it sets, what each choice costs the Pentium and the SST-1, and what a later change has to keep. Read it before editing `glide.c`, `draw.c`'s submission, or anything that changes a combine, depth, or texture mode. Texture memory is `docs/memory.md`. The clipper and the 1/16-pixel snap are in `docs/culling.md`.

## Where a triangle's time goes

On a Voodoo 1 the card only rasterizes. Glide 2 does the rest on the CPU: it sorts the three vertices, computes a start value and an x and y gradient for every parameter the current state needs, and writes them to the card over PCI. Each parameter is three more register writes and more x87 work, per triangle. So the cheapest triangle is one that needs the fewest parameters, and the cheapest polygon is one whose gradients are computed once.

The scene's triangles carry x and y, and s/w, t/w and 1/w for TMU0. Nothing else. That is why:

- **Models take a constant alpha.** The alpha combine for walls is `GR_COMBINE_FUNCTION_LOCAL` on `GR_COMBINE_LOCAL_CONSTANT`, with the constant colour's alpha at 255. Iterated alpha (`GR_COMBINE_OTHER_ITERATED`) made Glide send alpha gradients with every wall triangle, for an alpha that was always 255 and never blended. Blend opacity for the console wash, the HUD plates, and fading text lives in the constant colour too (`glide_opacity`, `glide_dim`, `glide_plate`). `fill_vertex` writes 255 into each vertex's alpha, and nothing reads it. `alpha_apply` is the one place that sets the alpha combine, test, constant, and blend. Do not set them anywhere else, and do not put `GR_COMBINE_OTHER_ITERATED` or `GR_COMBINE_LOCAL_ITERATED` on the alpha path.
- **Colour is the texture alone.** The scene's colour combine is `SCALE_OTHER` by `ONE` of the texture, so no RGB gradients are sent. Overlays (`glide_overlay`, `glide_flat`) use iterated colour and no texture. They are a few quads a frame.
- **Depth is the W-buffer** (below). It reads the 1/w texturing already sends, so ooz is not sent either.
- **Fog is the table.** `GR_FOG_WITH_TABLE` also reads 1/w. No fog parameter is sent.

## Depth

`grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER)` is the default, with `GR_CMP_LESS` and a clear to `GR_WDEPTHVALUE_FARTHEST`. The card stores 1/w as a 16-bit float, so two surfaces stay apart when they are about distance / 4096 apart: 0.25 units at 1000, 2 at 8192.

The Z-buffer is the fallback, `wbuffer off` in the console: `GR_CMP_GREATER` on `camera_project`'s ooz, cleared to `GR_ZDEPTHVALUE_FARTHEST`. ooz is linear in 1/z, so one depth step is z² / 65535 per unit of range. ooz used to be `65535 / z`. With the near plane at 4 its largest value was 16383, so three quarters of the buffer was never used. A step was 15 units at 1000 and about 1000 at 8192, and door frames, floor edges, and the castle at `far 8192` flickered through each other. It is now `65535 * DF_NEAR / z`, which uses all 16 bits: 4 units at 1000, 256 at 8192. The fallback is there to compare on hardware, not to ship.

Every overlay that turns the depth test off (`glide_ui`, `glide_overlay`, and the modes built on them) restores `g_depth_cmp`, not a fixed compare. A new mode that touches `grDepthBufferFunction` must do the same, or the scene draws with the other buffer's compare after the first HUD string.

## Lighting

A lit place draws texture times the constant colour's RGB (`glide_tint`), not iterated colour: still no colour gradients per triangle. White keeps `FACTOR_ONE`. Outdoors the tint changes per face, by its angle to the sun; `glide_tint` sends nothing when the colour is the one already set. Light decals blend `DST_COLOR, ONE` with fog off and clamped addressing, and their colour is texture times the constant colour (`glide_decal`): set the tint before turning it on, white indoors. A tint change during the decal pass keeps the decal combine. `alpha_apply` writes the constant colour, so it carries the tint too. See `docs/lighting.md` and `docs/daylight.md`.

## Sky

The sky's four tiles are P_8 (`glide_upload_p8`), drawn under the sky's own palette, not the level's. The dome above them is RGB 565 (`glide_upload_565`), so the palette does not touch it. `glide_sky(1, rgb)` downloads that palette, turns off the depth test and write and the fog, and clamps; `glide_sky(0, 0)` downloads the level's palette again, kept from the last `glide_palette`, and restores the depth, fog, and wrap. The palette is one table on TMU0, so every P_8 picture drawn between those two calls is read through the sky's colours. A screen picture (`docs/pictures.md`) swaps the table too: `glide_pic_palette(id, table)` sends a picture's palette only when another is on TMU0, and `glide_level_palette`, called by `draw_frame` after the game's overlays, sends the level's back only when it was replaced. `glide_palette`, `glide_sky` and `glide_glow` keep that record of which table is on, so nothing is sent twice and nothing is left behind. `glide_replace` sends new texels of the same shape to a picture's place on the card. `glide_sky_color` puts the horizon colour in the clear and the fog in place of the background while an outdoor place draws its sky. See `docs/daylight.md`.

## Polygons

`draw.c` sends each ARCH3D face as one polygon through `glide_polygon`:

- The loader fans a plane from its first corner. `face_fan` checks once per face that the triangles are an exact fan of one polygon, with the same positions, texels, and picture on the shared corners, and keeps the answer in `Face.fan`. In the hold and the castle every face of two or more triangles passes, about nine in ten faces. The rest are single triangles.
- Each corner is transformed and projected once. A quad is 4 corner transforms, not 6. Corner transforms in the hold and the castle are 68% and 69% of what triangle-by-triangle submission took.
- The polygon is clipped once, by the same planes and the same inside-first cuts as before, so a face's outline on the glass is what its triangles' union was. The occlusion pass in `scene.c` clips its outlines as whole polygons too.
- A sprite is one quad, clipped and sent the same way. So is each glyph of text and each bar of the `covers` wash. LEVELFALL's panels are about 700 glyphs, more triangles than the whole scene in a corridor, so with them up the frame rate there measures the panels as much as the cull; F1 hides them.
- `glide_polygon` converts each corner to a `GrVertex` once, then calls `grDrawPlanarPolygonVertexList`. That computes the gradients from the first three vertices and sends only start values for the other triangles. On a plane, s/w, t/w and 1/w are exactly linear on the glass, so the gradients hold across the polygon. The polygon is rotated to start at the corner whose triangle with its two successors is largest, so the 1/16-pixel snap moves the gradients least.
- The entry is optional. A DLL without it, or `planar off` in the console, gets a fan of `grDrawTriangle` from the same converted vertices. A face of more than `FACE_MAX` (26) corners, or one that is not an exact fan, goes triangle by triangle as before. `GLIDE_POLY_MAX` is 32: the loader's faces reach 32 corners once T-junctions are welded (`docs/culling.md`).

Shared edges between faces are unchanged: both faces cut a shared edge from the same endpoints with the same arithmetic, and the snap makes them meet. Inside a polygon the diagonals are Glide's, from vertices already snapped, so they are shared exactly too.

## Textures

- **Binding is a handle.** `glide_upload` returns the index of its upload record, and `glide_bind` takes it. It used to return the card address, and `glide_bind` searched up to 768 records for it on every texture change. `glide_bind` also skips a bind of the texture already on TMU0, so `texcache` keeps no cache of its own, and text binding its sheet no longer needs a `texcache_forget` afterwards: the next model bind sees a different handle and sends it. `glide_rewind` clears the remembered handle when it drops that record.
- **Cutouts bleed their colour.** A sprite is `GR_TEXFMT_ARGB_1555` with index 0 transparent. The card filters colour and alpha together. A transparent texel used to be black, so at every edge the bilinear filter pulled the sprite's colour halfway to black, and the alpha test let that pixel through: a dark outline around every sprite. Each transparent texel now takes the mean colour of its opaque neighbours (8 around it, wrapping like the texture), and keeps alpha 0. A texel with no opaque neighbour stays black; no filter tap from the sprite reaches it.
- **The cutout test is half.** Sprites and text keep a pixel whose filtered alpha is above 127 (`CUT_REF`), the outline a point-sampled sprite would have, not the half-texel-fatter one that `> 0` gave. A fade multiplies the alpha down, so while `glide_opacity` is fading text the reference is 0.
- **Sprites are padded, not stretched.** The loader used to stretch every picture to powers of two by repeating texels, so a 36 x 77 sprite became 64 x 128 with uneven doubled columns and rows, then repeated it until square. Every sprite in the hold, the castle, and Daggerfall was stretched like that. A picture only sprites use is now its own size in the top-left corner of the square, with index 0, transparent, around it (`texture_pad`, called from `world.c` once each picture's use is known). Its texels are exact, and at every mip level the texels next to its edges are clear, not the next copy's opposite edge. The square's side, and so its memory, is unchanged. A picture both a wall and a sprite use stays a repeating square, because the wall needs it to tile; none of the three places has one.
- **Walls are stretched smoothly.** A wall has to tile, so one whose sides are not powers of two is still stretched to them. `texture_smooth` redoes that stretch bilinearly, wrapping at the edges as the wall does, and maps each blended colour to the nearest palette entry through `mip_nearest`. A texel that lands on a source texel keeps its index. The hold has 16 such walls, the castle 26, Daggerfall 5.
- **Sprites are inset half a card texel.** `draw_flat` maps a sprite from half a card texel inside each edge. The picture was rescaled to a power of two, so that is `width / span_w / 2` source texels. The texture wraps, so bilinear filtering at the very edge of a picture at the corner of the square read the far side of the square. Padding makes that clear texels, and the inset keeps the full level's edge texels whole under the half alpha test. Walls keep their UVs: they are meant to repeat.
- **Mipmaps, budgeted.** `grTexMipMapMode` is `GR_MIPMAP_NEAREST`: the card picks the level per pixel and does not blend two, so it costs no fill rate. Without levels a far wall sampled a 256 picture several texels apart per pixel and sparkled as the camera moved. The font is uploaded as one level, and the card clamps a texture to the levels it has, so text is untouched. See Mipmaps below.

## Mipmaps

`src/engine/gfx/mip.c` builds every upload, with no Glide in it, so hostcheck tests it:

- Levels are laid out largest first with no gap between them, the layout `grTexDownloadMipMap` reads. A chain runs down to 1 x 1.
- An 8-bit picture keeps its full level exactly as loaded. Each smaller level is the 2 x 2 mean colour of the one above, mapped back to the nearest palette entry. The search is remembered per 5-bit colour cell and per palette, so a level asks a few thousand searches, not 32768. Squares come from a table: `imul` is 10 cycles on a Pentium and does not pair.
- A cutout carries alpha as a mean down the chain and is opaque where at least half of a texel's area was. The colour bleed (below) happens on the full level, so every level is bled.
- The card's sizes come from `glide_texture_bytes`: the larger of `grTexCalcMemRequired` and `grTexTextureMemRequired`, plus the 8-byte pad.

`texcache_upload` budgets the level before it sends anything (`mip_budget`). The room is what TMU0 has free once the sky, the light falloff and the font are on it (`texcap` can hold it to a smaller card's, `docs/memory.md`). Starting from full size, the walls give first: the wall that takes the most bytes of those that have lost fewer than two levels (`MIP_WALL_GRACE`) loses its top level, again and again; once every wall has given two, the picture that takes the most bytes, wall or sprite, until the total fits, never below 8 x 8. Each loss saves three quarters of that picture. A blurry wall reads as distance; a blurry hand-drawn sprite reads as a fault, so the sprites keep their detail longest, but no wall is ruined for them: past the grace it is the largest again. Then each picture is uploaded with its chosen `drop`. If a 4 MB TMU's 2 MB edge wastes a gap and one does not fit, that picture alone loses another level. A dropped picture keeps its UVs: Glide maps s, t across 0..256 whatever the largest level is.

What it does to real places (host estimate, 1856 KB of room; the cities measured before towns took their climate's pictures, which takes Daggerfall from 235 to 216):

| Place | Pictures | All full size, one level | All full size, chains | Chains after the budget |
|---|---|---|---|---|
| Privateer's Hold | 114 | 1148 KB | 1531 KB | all full size |
| Daggerfall's castle | 216 | 3334 KB | 4446 KB | 43 at half, 8 at a quarter |
| Daggerfall | 235 | 2910 KB | 3881 KB | 67 at half, 3 at a quarter |
| Wayrest | 273 | 3875 KB | 5167 KB | 110 at half, 6 at a quarter |

Building every chain is load time, never frame time. On the development host it takes 4 ms for the hold and 12 ms for Wayrest; a 233 is perhaps 30 to 60 times slower, so roughly half a second for a city. That has not been measured on the target.

Before the budget the castle and the cities sent full single levels in table order until the card was full, and every picture after that drew with whatever texture was bound last. Now every picture is on the card; the largest ones are softer close up. A cutout costs twice an 8-bit picture of the same side, so the largest-first rule alone took the sprites' detail first; the walls' grace (above) turned that round.

`mipmap off` in the console uploads single levels, still budgeted, and `mipmap on` chains again. Either change sends the whole level again on the next frame (`gfx_set_mipmap`, through `gfx_prepare`, which starts the card from empty with `glide_tex_reset`). A chain was tried once before, early on, and the ground came out in bands and the font in the wrong glyphs. The cause was never found; `docs/memory.md` has what is known. If that comes back on hardware, `mipmap off` is the comparison.

## The FPU

The Pentium's x87 divides in 19 cycles at 24-bit precision, 33 at 53, 39 at 64; Windows starts a program at 53. Every vertex takes one divide (1/z), and a clipped edge more. `draw_frame` sets the precision control to 24 bits for the render passes (sky, cull, models, decals, windows, sprites, precipitation: `df_fpu_single`) and gives the old word back before the overlay (`df_fpu_restore`), as Quake did around its renderer. Their arithmetic is float throughout, so nothing they compute changes but the time; rounding is untouched (`df_iround`'s `fistp`, `df_snap16`'s store to a float). Kept at the full precision: the walker and collision (the sim stage, tuned at it), the sound, and the overlay, where a viewer's panel prints doubles (a stream's world position: 32 million units needs 25 bits). Never put a double inside the passes. `fpu24 0` in the console runs them at the full precision, to compare in the profile panel.

A vertex's projection is done in place in `project_vert` (`camera_project`'s arithmetic on draw.c's own copy of the focal length): a call of seven arguments a vertex was some twenty cycles of pushes. The model's view transform was already inline (`view_point`).

MMX does integers only on this chip: the frame's geometry is x87 float, so MMX has nothing to do there. It mixes the sound (`mixer.c`). The CRT replacements copy and fill a 32-bit word at a time (`rep movsd`, `rep stosd` in `mem_copy`, `mem_fill`, `memcpy`, `memset`), where they were byte loops; the frame itself makes no such calls, so that is load time.

Not done, the largest saving left on the CPU's side: a corner of an ARCH3D model is transformed and projected once for each face that has it, 2.9 times on average over every model in ARCH3D.BSA (837,101 face corners, 288,304 distinct). A cache of each drawn model's transformed corners would cut that work by up to about half once back faces are dropped; it needs each mesh's corners indexed, and its clipping kept per face.

## Kept as they are

- Bilinear filtering costs the SST-1 nothing over point sampling. Text is point sampled so glyph edges stay hard, and so are sprites by default (`spritepoint`, `glide_point_sample` around the sprite pass): up close a sprite is magnified three times, and bilinear smeared its hand-drawn texels.
- The SST-1 has no early depth rejection, so drawing front to back should save little on the card. The occlusion cells are what remove hidden models.
- `grBufferClear` clears colour and depth every frame. Its cost has not been measured. Skipping it needs a frame where every pixel is drawn, which dungeons with open void do not have.
- `grSstQueryHardware` comes after `grGlideInit`, as the Glide 2 samples do. LEVELFALL shuts Glide down and starts it again for each place; that works on the boards tried so far, but not every Glide 2 driver promises it.

## Console

`wbuffer`, `planar`, and `mipmap` switch the choices above at run time. `texmem` reports the chains and how many pictures the budget shrank. `vsync`, `stats`, and `glinfo` show what they cost. `glinfo` names the depth buffer and whether planar polygons are in use. See `docs/console.md`.

## Water

`glide_water` is the water's surface (`docs/water.md`): the constant colour's RGB and alpha alone (no texel fetched, no vertex colour), blended source alpha over the frame, depth tested but not written so what is under it still shows, fog left as the scene's. Off restores the depth write, the alpha state (`alpha_apply`) and the textured colour path.
