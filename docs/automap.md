# The indoor automap

Classic's dungeon map shows the rooms the player has seen, from above or from a camera turned about the player, and a floor at a time. The engine keeps which models were seen and draws them; the game owns the window: its chrome (AMAP00I0.IMG), its keys, the player's marker, and the camera it looks from.

## The parts

- `src/engine/scene/automap.c` (`automap.h`): the seen table, a frame's marking, and the seen geometry turned into screen polygons. Plain C: hostcheck runs it.
- `gfx_automap` in `src/engine/gfx/gfx.c`: those polygons on the card.
- `draw_frame` calls `automap_frame` after the cull, while the game names a place and the world is not paused.
- The console's `mapreveal`.

## Seen

The game names the place it is in with `automap_set_place(key)`; 0 is none (outdoors, a menu), and nothing is marked. For Daggerfall the key to use is the place's `KeepKey.place`: the MAPDITEM id, or a building's inside as its town's id `| (building + 1) << 24` (`docs/interact.md`).

Each world frame, a model is seen when:

- the frame drew it (it passed the cull and the occlusion cells) and its box is within `AUTOMAP_SEEN_RANGE` (1024) of the eye; or
- its box is within `AUTOMAP_NEAR_RANGE` (320) of the eye, drawn or not: the room round the player, behind them, round a corner. Height counts twice over for this, so the floor above or below is not taken for the room the player stands in.

The near test walks the scene's spans first and reads only the models of a span whose box is near; a scene with no spans reads every model.

## Keys

A seen model is a row, `AutomapRow`: the place, its mesh's id, and where it stands in the place's frame, rounded to a unit. A mover's model is keyed by where it starts (`Mover.x0, y0, z0`), so a door half open or a raised platform is still the same model.

The place's frame is the one `keep.c` uses. A stream draws round an origin that moves by whole chunks; `stream_scene_origin` gives that origin in whole units, and it is added to the model's rounded local position. So a key is the same:

- when a streamed block is dropped and filled again, in whichever slot;
- when the stream's origin moves;
- when the place is left and opened again on a new heap.

Two models with the same mesh on the same spot share a row. That is the same piece twice; the map loses nothing.

## The table and a save

The rows are one table for the whole game, sorted by key, binary-searched, in a heap of their own that grows by doubling, never a place's heap. 20 bytes a row. A dungeon of 400 models seen is 8 KB.

A save writes them with `automap_count` and `automap_at` (in key order) and reads them back with `automap_put`; `automap_clear` empties the table for a new game. `AutomapRow` is a fixed-size record, so the game's record files can carry it.

Asked about a scene, the table answers from a byte a model slot, made again only when the scene, its version, its heap (`mem_epoch`), the place, the stream's origin or the rows change. A frame with nothing new seen reads that byte for the drawn models and the near spans' models, and searches nothing.

## Drawn

`automap_polygons(scene, view, emit, ctx)` hands each polygon to `emit`: up to `AUTOMAP_CORNERS` corners in screen pixels from the lower left, a `1/w` for the depth test (larger is nearer), and a colour. For each seen model whose box reaches the slice and the view:

1. Each face of the model the world draws (its slot is not `0xffff`). From above, only a face turned up (its world normal's y above 0.5): the floors. From a camera, a face turned to it, by the world's own `instance_faces`.
2. Its corners, read from the fan ARCH3D's loader makes; a face that is not a clean fan goes triangle by triangle.
3. Cut to the slice, `bottom <= y <= top` in world y, the corners made where its edges cross.
4. Projected. From above (`AUTOMAP_TOP`): straight down on `cx, cz`, turned so `yaw` (Camera's sense) is up the screen, `scale` pixels a unit, and `1/w` from a point `TOP_EYE_ABOVE` (64) over the slice's top, so a higher floor wins. From a camera (`AUTOMAP_3D`): the game's `Camera` and `fov` across the rectangle, cut at `DF_NEAR` before the divide.
5. Cut to the rectangle `x0, y0 .. x1, y1`, edge by edge.

The colour is the face centre's height in the slice: dark blue-grey at its bottom, light at its top. From a camera it is also shaded by the face's turn to a light from above, so walls read apart from floors.

`automap_to_screen(view, x, y, z, &sx, &sy)` is the same projection for one point: where the game draws the player's marker. It returns 1 when the point is inside the rectangle.

### The slice

The game chooses `bottom` and `top`. DFU's upstairs and downstairs are a slice moved by a storey; the whole place is a slice of everything. A floor cut by the slice's top is drawn up to the cut, not dropped.

## On the card

`gfx_automap(level, view, back)` is called from the game's `gfx_set_under` hook:

1. Fog off, blending off.
2. The rectangle filled with `back` (0xRRGGBB), writing a far depth whatever the depth there was (`glide_flat_reset`). The map therefore never tests against anything drawn behind it.
3. The polygons, flat iterated colour, depth tested as the world's, each fanned into pieces of at most `GLIDE_POLY_MAX` corners.
4. The state put back.

Its order with the game's own pictures is the game's. Drawing the chrome first (it does not write depth) and the map after leaves the map's ground over the chrome's inside, which is what classic's black map area wants.

The world is stilled while the map is open. The cheapest frame for it is a screen frame, `gfx_frame(cam, 0)`: no world drawn under the map at all, only the clear, the hook and the swap (`docs/pictures.md`). A world frame with the map over it also works, at the world's cost.

## Cost

The whole of Privateer's Hold revealed, from above, is 1207 polygons, 4840 corners: about what a world frame sends, without textures, without fog and without a texture bind. Each corner is a few multiplies and one divide in the 3D view; the clips are one comparison per corner per plane. Nothing is allocated per frame.

In a world frame, marking costs a byte read for each drawn model and for each model of a near span, and nothing else until something new is seen. Making the per-model bytes again, when a block loads, is a binary search per model slot that holds a model.

## The game's part

- `automap_set_place` when a place opens; 0 outdoors.
- The rows in its save file; `automap_clear` on a new game.
- The window: AMAP00I0's chrome, the keys (turn, zoom, the slice, the view), `AutomapView` from classic's rectangle (1, 1)..(318, 169) of 320 × 200 turned to screen pixels from the lower left, and the marker where `automap_to_screen` says.

## Switches

| | |
| --- | --- |
| `mapreveal` | Every model of the place on its automap, as if walked. Prints how many are seen and how many were new. Needs a place named. |

## Tests

hostcheck, `check_automap`: with no place named nothing is marked; the floor stood on and the room round it are seen, a floor beside it is seen by passing, a far floor drawn is not; seen follows each model's key through the scene rebuilt in another order; a door swung open stays seen; another place shares nothing and the first comes back; the rows go out, are cleared and come back in key order; from above two floors draw, and not the ceiling, the wall or the door; a floor half out of the rectangle is cut, not dropped; a slice under the floors draws nothing; the player's marker lands at the centre and turns with the map; from a camera, the four faces turned to it draw. `check_dungeon_automap`: a look round from the hold's start sees its cave, which draws inside classic's rectangle at 640 × 480 from above and from behind the player; the hold opened again on a new heap knows the same models seen; revealed, the whole hold draws inside the map.

## Not done

- Classic's doors and the exit marked on the map; notes the player writes on it.
- Telling apart floors of one model: a model is seen whole, so a tall piece that spans two storeys shows on both slices, cut.
- The outdoor automap (a town's), which the game draws from its blocks' own maps.
