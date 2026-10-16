# Culling

`draw_frame` does not walk the scene. It draws the index list from `scene_visible_models` and `scene_visible_sprites`. Those two calls are the cull. Read this before editing `scene.c`, the far clip, or fog.

A null camera lists every stored entity, in storage order. `check_engine` uses that. With a camera, a model or sprite is dropped when its box is wholly outside the view. Survivors stay in storage order. `VIS_MAX` (2048) still truncates the list. A model with no triangles is omitted, so an empty slot does not consume the cap.

The per-triangle clipper in `draw.c` is unchanged. It still cuts a triangle that crosses the near plane, the far clip, or the wide Glide guard (`CLIP_X` / `CLIP_Y`, about +/-2048 screen pixels). Culling only decides whether that triangle is transformed at all.

## What the camera is looking at

Once per call, the yaw and pitch become six inward planes:

- near, at `DF_NEAR` (4)
- far, at the distance below
- left, right, top, bottom, through the eye

The side slopes are the picture, not the Glide guard. `camera_project` is `sx = 320 + x * focal / z`. The focal length is `camera_focal()`, from the horizontal field of view. The default is 75 degrees, focal about 417, so a point is on the left or right edge when `|x| / z` is about 0.77, and on the top or bottom when `|y| / z` is about 0.58. `fov` in the console changes this on the next world frame, clamped to 45..120. The guard is `1600 / focal`, wide enough that a triangle which merely crosses the picture does not wrap on the SST-1. A box outside the picture has no pixels, so the test uses the picture slopes, not the guard. `scene_set_cull(0)` lists every entity and skips these planes.

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

There is no portal step. A neighbor block whose box still intersects the view is drawn, including the part hidden by a wall. The Z buffer hides it. A mesh that is one rigid object larger than the picture, other than the ground, is still one box.
