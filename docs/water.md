# Water

A place flooded to a level: a dungeon's lower rooms, later the sea and the rivers outdoors. Read this before editing `Water` in `scene.h`, the water queries in `scene.c`, `draw_water_pass` or the underwater fog in `draw.c`, `glide_water`, or `add_water` in `world.c`.

The engine draws water and says where it is and how deep a body stands in it. What water does to the player (swimming, holding breath, water breathing, the walker's motion in it) is the game's, later.

## In the engine

`Scene.water` is a list of `Water` rectangles: a floor-plan rectangle `x0..x1` by `z0..z1`, flooded to `y`, in a colour (`0xRRGGBB`). Rectangles may meet and overlap; where they do, the highest level counts.

- `scene_water_at(scene, x, z, &y, &color)`: the highest surface over a point, 0 for none.
- `scene_water_body(scene, x, eye_y, z)`: how deep a body stands, by its eye, with the walker's heights (feet `WALK_EYE` under the eye, the waist half `WALK_HEIGHT` over the feet): `WATER_DRY`, `WATER_FEET`, `WATER_WAIST`, `WATER_HEAD` (the eye under). The game asks it of the player for its own rules: the waist is where Daggerfall Unity's player swims, the head where it starts to drown.

### Drawing

`draw_water_pass`, after every opaque face, sprite and entity, inside the sprites' profile stage: each rectangle within the visible far whose box is in the frustum is one flat polygon at its level, clipped as any face. `glide_water` draws it in the constant colour at alpha 150 (`WATER_ALPHA`): no texel, no vertex colour, blended over the frame, depth tested but not written, and fogged as the scene is. Under it, everything below shows through its colour; over it, what stands above draws in front. It is drawn from above and from below alike.

The eye under water (`WATER_HEAD` at the camera): the clear and the fog take the water's colour (`glide_sky_color`), and the fog closes in, clear to 32 units and solid at 640 (16 m, `UNDER_NEAR`, `UNDER_FAR`), or nearer if the frame's own fog is. `fog_apply` sets the cull's far by it too, so under water the frame draws only what is within 16 m: cheaper, not dearer. Outdoors the sky is not drawn under water.

`water` in the console (on by default; alone it flips) turns the drawing off: the place is drawn as if dry. The game still knows the water is there.

### What it costs

A rectangle is a polygon whose pixels are all filled, blended, wherever it is in view, even under a floor where the depth test throws them away: the Voodoo's fill pays for each. A plane over a whole dungeon block would cover the glass under every floor of it. So the game lays water only where there are rooms under the level (below), and each rectangle is culled by the frustum and the far.

## Daggerfall's dungeons

`world.c`, for a dungeon loaded whole. Each RDB block has its own level, from its start marker (editor record 10): its sound byte, 8 units a step, classic's height (down positive), `enemy_water`, the same the creatures' water rule reads; 0 is no water. The world's level is its negative: `8 × the byte` above the block's zero.

Daggerfall Unity lays one water plane over the whole block. Here (`add_water`) the block's square is cut in cells of 256 (`WATER_CELL`, 8 × 8 a block); a cell is wet when a model of the block meets it and reaches below the level, so solid rock and rooms above the water have none. Each row's runs of wet cells are one rectangle. The colour is a murky green-blue (`DUNGEON_WATER`, 0x1c4652).

In a survey of 71 dungeons, one in sixty of the list, 44 are flooded somewhere, always low: levels 144 to 496 in places 0 to about 2000 high, 519 rectangles in all, about half a block of area each. Anthor's Web has 8 rectangles at 144. Privateer's Hold is dry.

`world_water_count`, `world_water`; `world_as_level` gives them to the scene. LEVELFALL's frame panel adds `water dry|feet|waist|under` on its fog line in a place with water.

## Tests

`check_water` (engine): a body's depth by its eye, at each height and off the rectangle; the highest of two rectangles over a point. The dagger test: the hold is dry; Anthor's Web's water lies within its height, each rectangle over a model that reaches under it, and a body under it is under.

## Not yet

- Outdoors. The sea is the land at height 0 under water tiles, walked on as ground; a sea with depth needs the land under it lower. Rivers and lakes the same.
- The walker in water: it walks the bottom as on land. Swimming, floating and how fast are the game's.
- A water texture, its waves and its sound. Daggerfall Unity's surface is a plain plane too.
- Water in a house (none in the data) or in a streamed world's chunks.
