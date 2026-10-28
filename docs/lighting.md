# Lighting

Dungeons are lit by the place's own lights. Outdoors, the same decals are the street lamps, lit only at night, and the sun and the moon light the rest: `docs/daylight.md`. Read this before editing `src/engine/scene/light.c`, the light pass in `draw.c`, `glide_tint` or `glide_decal` in `glide.c`, or the light records in `rdb.c`.

## Where the lights come from

An RDB block lists its objects by type: 1 is a model, 3 a flat, and 2 a light. `rdb.c` used to skip type 2. Its resource is 10 bytes:

| Offset | Size | Use |
|---|---|---|
| 0 | 4 | Unknown. Small numbers in most records: 41 distinct values, 3 to 68, about 19 on average. Not kept. |
| 4 | 4 | Unknown. 0 or -2. Not kept. |
| 8 | 2 | Radius, in world units. Daggerfall Unity reads it the same way. |

The position is the object header's, like a model's or a flat's: y is negated into the engine's y-up.

What the data says, from all of `BLOCKS.BSA`:

- 187 dungeon blocks, every one with lights: 4268 in all. Privateer's Hold has 71; Daggerfall's castle 278.
- Radius: 1291 under 64, 2803 from 64 to 127, 174 above. 80 is typical.
- 93% of lights sit within 64 units of a flat from `TEXTURE.210`, the torch, candle, or brazier that shows them. The two are separate objects; the light usually sits 3 to 30 units from its flat.
- The rest, 312, light a room on their own, with no flat near them.
- The first field does not follow the flat's picture: it averages 17 to 27 for every torch and candle kind. It may be a brightness, but nothing here uses it yet.
- RMB blocks, the outdoors, have no light objects. `world.c` makes one for each `TEXTURE.210` flat there instead (`docs/daylight.md`).

Flats from `TEXTURE.210` are marked `Flat.glow` by `rdb.c` and `rmb.c`, and drawn full bright.

A model that moves (a door, a lever, a lift: `Scene.movers`, `docs/interact.md`) gets no decals, since they are cut where it stood and would stay there when it swings. It is drawn in one colour instead, the ambient raised by the lights round its box as the place built it, the sum a sprite gets (`light_model_rgb`). A house's inside has no light records: each `TEXTURE.210` flat there is a light, as outdoors, but always lit.

`lightview 512` in the console draws the lights within 512 units of the camera, the 64 nearest at most (`lightview` takes 0, which is off, to 2048): a square at each, and a ring as wide as its reach at its distance. Yellow when its decals passed this frame's cull, dim orange when not. It draws over walls, so a light behind one still shows. A light is drawn only when its centre is on the glass, and its ring only when at most 480 pixels wide. The first version drew every light up to the far clip: in LEVELFALL at 8192 the glass filled with rings, and a light beside the camera projected thousands of pixels off the glass, so its ring sent the card line coordinates past the SST-1's +/-2048. The card hung and took the machine with it. Anything drawn straight to the card has to stay inside that range; the scene's own polygons are clipped for the same reason (`docs/culling.md`).

## How it is drawn

Per-vertex (Gouraud) lighting does not fit. Daggerfall's walls are big quads, 256 to 512 units across, and a light reaches a few hundred, so most vertices sit outside every light. It would need the walls cut into many more triangles, and triangles are what the 233 pays most for.

So the light is laid on where it falls, as decals. It is the period's own trick: GLQuake drew its lightmaps on the Voodoo 1 the same way, a second pass that multiplies a light texture into the frame buffer. Here the light pictures are made at load from the place's light records, not baked by a level compiler.

1. The scene is drawn as texture times the ambient colour. The ambient is the RGB of Glide's constant colour (`glide_tint`), with the colour combine `SCALE_OTHER` by `FACTOR_LOCAL` of `LOCAL_CONSTANT`. A register, not a vertex colour: a triangle still carries no colour gradients. White keeps the old `FACTOR_ONE` path, so an unlit place draws exactly as before.
2. At load (`light_build`, from `gfx_prepare`), each light cuts its decals. For each face of each model whose box the light reaches, on the face's lit side: the light's height h over the face's plane, the circle it reaches on the plane, radius sqrt(R² - h²), the square around that circle, and the face cut to that square. The cut is kept as a polygon in the world with texture coordinates across the square. A decal whose patch misses the circle entirely is dropped. A face of more than 32 corners, or not an exact fan, is cut triangle by triangle, unless it is flat and its triangles cover the whole of the square inside its outline: then that rectangle is one decal (`merge_face`). That is open ground outdoors (`docs/daylight.md`).
3. Each frame, after the models and before the sprites (`draw_lights`), every light whose decal box passes `scene_box_visible` (this frame's frustum and occlusion cells) draws its decals. Each decal is tested on its own: the camera on its lit side, then its box against the frustum and cells. It is drawn 0.2% nearer the eye, so the same pixels pass the depth test on their own face.
4. The decal blend is `GR_BLEND_DST_COLOR, GR_BLEND_ONE`: the pixel becomes dst + dst × src. src is the falloff picture times the constant colour: white in a dungeon; outdoors the lamps' level times a flame colour, and three passes (`docs/daylight.md`). The decal's colour is the light it adds: black at its edge, so the edge leaves the wall exactly as it was, and at most white at its centre, which doubles the wall. Overlapping lights compound. A neutral of 0.5 with a 2x modulate was the first idea, but RGB 565 cannot hold 0.5: 16/31 left a 3% step at every decal's edge.
5. No fog in the decal pass. Under a multiply, black fog already on the wall stays right. A coloured fog (LEVELFALL's grey background) is brightened a little under a light.
6. Sprites take one colour each: ambient × (1 + the lights around them), the same sum a wall gets, worked out at load (`light_sprite_rgb`). A glowing flat is white.

The falloff pictures are 8 RGB 565 squares of 64, 64 KB on TMU0, one per band of h over the reach. They are uploaded before the level's pictures, so the texture budget sees the room left, and only for a place with lights. Their colour is a torch's: red 1.0, green 0.8, blue 0.55.

The falloff at distance d of reach R, with angle θ to the face: L = 1.5 (1 - d/R)² (0.35 + 0.65 cos θ), clipped at 1. The softened Lambert term keeps a torch on its own wall lighting the wall around it. The 1.5 gives a brighter core with the same reach; at 1 a host preview showed the pools too faint against the ambient.

## Flicker

A flame's light wavers. The records do not say which lights are flames; the pictures do. In `TEXTURE.210` the torches, candles, braziers and fires are records of several frames, and the lanterns, chandeliers and lamp posts a single one (`docs/sprites.md`). So `world.c` (`flame_lights`) marks a dungeon light as flickering (`Light.flicker`) when the light flat nearest it, within 64 units, is a moving flame; a light with none near lights its room steadily. A town's lamp flickers when its own flat moves: braziers and fires do, the lantern posts do not. 36 of the hold's 71 lights flicker.

`light_flicker` gives a flame's brightness each frame, 179 to 255 of 255: value noise, a random level about ten times a second and a smooth step between, each light hashing its own index into the ticks, so no two flames move together. It is the tint of the light's decal pass, one register a light, so a flickering pool dims and brightens on its wall and floor. Two integer hashes and a blend a lit flame a frame, no float-to-int cast. Sprites keep their one colour from load and do not flicker. Daggerfall Unity flickers its dungeon and town lights by varying their range; this varies their strength, which is what a decal can change for nothing.

## Switches

| Console | Default | Effect |
|---|---|---|
| `lights` | on | Off draws every place full bright, as before. |
| `ambient` | 112 | Brightness away from lights, 0..255. A lit wall goes to at most twice this. |
| `lightrange` | 3 | A light reaches this many times its radius, 1..8, as Daggerfall Unity's range of 3. A change cuts the decals again (`gfx_set_light_range`). |
| `lightshadow` | on | A light lays no decal on a face it cannot see (below). Off lights through floors and walls, as before. A change cuts the decals again (`gfx_set_light_shadow`). |
| `lightview` | 0 | The squares and rings above, for lights within this many units, up to 2048. 0 is off; 1 to 63, which hold nothing, turn it on at 512. |

`stats` adds how many lights lit something on screen and how many decals were drawn.

## Line of sight

A decal is cut only where the light can see it: the line from the light to the decal's middle, or to one of four of its corners drawn a fifth of the way in, must not pass through a model on the way (`seen`, `light_reaches`). Without it a light reached through anything inside its reach. A house's ceiling lamps, three storeys of them 129 units apart, each lit all three floors: the alchemist in Daggerfall's `CUSTAA01.RMB` (building 10) cut 2771 decals from 9 lamps and drew 17 fps in LEVELFALL, against 52 with `lights 0`. Seeing only its own storey, it cuts 509. The hold's 71 lights went from 2282 to 1667, the ones that lit the next room through a wall; the Ashwing farmhouse from 1053 to 268.

`light_build` counts the decals, allocates them, then cuts them. The count skips the rays (`seen` runs only when filling), so it is an upper bound (the castle counts 7710 for its 5238) and the rays, three quarters of the cut, are cast once: before, the count cast them too, and the castle's lights were 0.9 s of its load on the 233 (the host's time ×40), 0.57 s now, the same decals.

The test runs once, at load, against the models the light reaches (the ones whose box is within its reach, `g_near`), with their triangles from both sides (`instance_ray`, the use key's test). A model that moves does not shade: it is not in that list. A face partly behind something keeps its whole decal when any one of the five lines is clear. Sprites and moving models still take their colour from every light in reach, through walls (`light_point`).


## Cost

At load, in the castle at the default range: 278 lights, 8285 decals of 32358 corners, about 1 MB, cut in about 5 ms on the host. The hold: 71 lights, 2379 decals, 300 KB. At range 2: 4595 decals. The decals live on the level's heap: a `Decal` is 52 bytes and a corner 20. A second build in the same place reuses them; a build after a temp heap ended allocates again (`mem_epoch`, `docs/memory.md`).

Per frame, in the castle at far 1400 (host count, default range):

| View | Lights through the cull | Their decals | Drawn |
|---|---|---|---|
| Corridor | 6 | 196 | 26 |
| Door | 2 | 98 | 14 |
| Stair hall | 13 | 367 | 80 |
| Vault hall | 15 | 342 | 20 |
| Thrones | 2 | 94 | 24 |
| Storeroom | 10 | 699 | 35 |

Testing each decal on its own is what keeps the drawn count low: a visible light's decals are mostly behind the camera, off the glass, or behind a wall. The pass costs its tests, those polygons, and fill only where light lands. None of this is measured on the 233 yet.

## A streamed world

`light_stream_begin` puts `light.c` in stream mode for a stream's scene (`docs/streaming.md`, Lights): each light's decals in a heap of its slot's, freed with the chunk; `light_corners_of(i)` is the corner array light i's decals index (`light_corners()` is 0 then), and `draw_lights` takes it per light. A light with no decals is skipped before anything else in the pass, as most of a stream's 1936 are. The cutting is queued when a chunk comes or goes and done under a budget by `light_stream_service`; a lamp's neighbours' pools are cut again when a chunk arrives or leaves beside them, and the result is the same as cutting all at once (tested). The rebase moves every decal, its box and its plane (`light_stream_shift`). An outdoor stream's lamp casts no shadow (`seen` passes everything): the rays were 97% of the cut. Daggerfall's middle, 25 chunks: 114 lamps, 4435 decals. A dungeon's stream (not outdoor) keeps the rays, so a torch does not light the next corridor through its wall. Its cut goes a face at a time across the service's calls, given up and queued again when a slot drops or the origin moves. Privateer's Hold by block cuts the same 1667 decals as loaded whole (`docs/streaming.md`, Dungeons).

## Not done

- No player light, no flicker, no light from windows outdoors at night.
- Models are not lit by distance from the camera; only the place's lights and the ambient.
- The first two fields of a light record are not understood. Neither says which lights flicker.
