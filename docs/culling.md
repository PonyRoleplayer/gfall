# Culling

`draw_frame` does not walk the scene. It draws the index list from `scene_visible_models` and `scene_visible_sprites`. Those two calls are the cull. Read this before editing `scene.c`, the far clip, or fog.

A null camera lists every stored entity, in storage order. `check_engine` uses that. With a camera, a model or sprite is dropped when its box is wholly outside the view. Survivors stay in storage order. `VIS_MAX` (2048) still truncates the list. A model with no triangles is omitted, so an empty slot does not consume the cap.

## Occlusion, in progress

The wall test is not finished. It draws rectangles so a screenshot can show what it used (`covers on`, the default). Green is one face. Red is faces that met and were joined. A model whose center sits in a red rectangle, and is behind that face, is dropped.

What has been tried, and what is still wrong:

- A single depth for the whole picture hid the street beside a wall that did not fill the glass.
- Joining every tall face at one distance packed a sign post and a building into one rectangle, and the gap between them was treated as solid.
- The outer square of a roof slope includes the sky beside the shingles. Pieces of a slope are now cut at edge midpoints, and a piece is kept only when its square sits inside the triangle. A wall that already fills its square stays one piece. Standing on a roof can still hide the town if a square reaches past the shingles. Do not treat this as done.

The frustum and the far clip are the part that is settled. Occlusion is the part still being tuned.

A tall mesh hides a box that is more than 32 units past the nearest point of its front faces. The frustum is a pyramid, so without this a wall in front of the eye still lists every model behind it out to the fog. Each face keeps its own screen rectangle, clipped to the picture, and the four largest are kept. A floor or a ceiling is not a cover. A face counts when it has a vertex closer than 512, a building across the street. A rectangle is kept only when the triangles fill at least 55 percent of it. The edges of a slope's pieces are the inner square: grown from the centroid until the corners hit the triangle, so the box does not include the sky beside the shingles. A wall that fills its square is kept whole. Vertices farther than 320 are cut off the rectangle. Tall faces share a rectangle only when those rectangles meet, within 16 pixels, and their depths are within 48. A brick course and the column beside it become one wall. A sign post across the street stays separate, so the gap between them is not solid. The shared rectangle tests the center of the box, because the box of a house behind the brick sticks out of the wall even when the mesh does not. A shallow band, such as a roof edge, does not join that rectangle. When one face reaches the edges of the picture, every box behind that face goes. A smaller rectangle hides only a box that sits inside it, so the street beside a partial wall stays. Ground patches are mesh id 0. The mesh is at least 32 units tall. `covers on` draws the test. Green is one face. Red is the joined facade, and a model whose center sits inside the red rectangle and is behind that wall is dropped. `covers off` hides the drawing. The test still runs.

The per-triangle clipper in `draw.c` cuts a triangle that crosses the near plane, the far clip, or the picture. The picture margin is 16 pixels, scaled by `camera_focal()`, and it is computed once per frame. A triangle already inside those six planes is projected directly. A close wall used to be clipped only to a 1600-pixel guard, so a corner was submitted at screen x = -3242. The SST-1 walker then started thousands of pixels off the glass. Distance is the far clip and the fog, not a second test in software.

## What the camera is looking at

Once per call, the yaw and pitch become six inward planes:

- near, at `DF_NEAR` (4)
- far, at the distance below
- left, right, top, bottom, through the eye

The side slopes are the picture. `camera_project` is `sx = 320 + x * focal / z`. The focal length is `camera_focal()`, from the horizontal field of view. The default is 75 degrees, focal about 417, so a point is on the left or right edge when `|x| / z` is about 0.77, and on the top or bottom when `|y| / z` is about 0.58. `fov` in the console changes this on the next world frame, clamped to 45..120. The triangle clipper uses the same picture, plus 16 pixels, which stays far inside the SST-1 range of about +/-2048. `scene_set_cull(0)` lists every entity and skips these planes.

A plane rejects the box when the nearest corner is outside: `dot(normal, center) + d + radius < 0`. The radius is `ex*|nx| + ey*|ny| + ez*|nz|`. Far is tested first.

## How far

`scene_set_limits(clip_far, fog)` is the limit. Defaults are fog on and `DF_FAR` (1400), the same as a fresh device. `gfx_set_far` and `gfx_set_fog` are the callers. Do not set the clip in `draw_set_far` alone; the cull would keep the old distance.

Fog is solid at `clip_far * (DF_FOG_FAR / DF_FAR)`, the same fraction `gfx_set_far` uploads (1100 out of 1400). The clear colour is the fog colour, so a box entirely past that distance is invisible. The distance is never closer than `DF_FOG_NEAR + 1`, because `glide_fog_range` will not put solid fog in front of the fog start. With fog off, the limit is the far clip itself. A box that only straddles the limit is kept, and the clipper cuts it.

## Blocks, then models

`world` stores a `Span` per block: the first model index, the count, and the union of those models' boxes. The frame tests that box with the same six planes. A block that misses is not read. Survivors are still in storage order, block after block.

Inside a block, each instance has a `Bound` filled once, in `append_block`, after `shift_block` has written the final position. The cull reads that array and does not touch `m[]` or the mesh. `mesh_measure` runs at load (`arch3d_load`, ground patches). `bound_n` is the triangle count that produced the local box. A shared mesh is measured once.

The world box is the local box through the instance. The half-size is the local half-size times the absolute columns of `m[]`, so a rotated or scaled model stays inside. The box is larger than the triangles. It can keep a model that is actually just off screen. It does not drop a model that still has a vertex in view.

A sprite faces yaw and stays upright. Its box is half the width on X and Z and half the height on Y, centered on the flat. A flat with `w` or `h` below 1 is omitted. `draw_flat` already skips those. Sprites are not blocked by spans.

With no spans and no bounds (a single renderfall model, or a null camera), the old path remains: measure from the triangles if needed, and list everything when the camera is null.

## Planes

Each ARCH3D plane keeps a normal and the triangle range it fanned into. The stored normal is the opposite of the first triangle's cross product: that winding points away from the visible side, the same way a ground quad's cross product points down while the patch normal points up. `draw_frame` tests that normal before transforming the range. Two or more triangles are worth the test. A single triangle is drawn directly. A zero normal is drawn too. The test keeps the face when the camera is on the front side (`instance_faces`).

Ground is not one 4096-wide mesh. `add_ground` emits 512-unit patches, each with an upward normal, so a patch behind the camera is dropped with the block or the box.

`grCullMode` stays `GR_CULL_DISABLE`. The Voodoo cull would run after the Pentium had already built the triangle.

## What this does not do

There is no portal step and no software raster of the wall. A box is hidden only when its projected corners sit inside a close wall's screen rectangle. A window or a gap inside that rectangle is still covered, so a prop seen through a hole in the mesh can be dropped. The Z buffer hides a wall that did not qualify. A mesh that is one rigid object larger than the picture is still one box.
