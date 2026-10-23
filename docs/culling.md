# Culling

`draw_frame` does not walk the scene. It draws the index list from `scene_visible_models` and `scene_visible_sprites`. Those two calls are the cull. Read this before editing `scene.c`, the far clip, or fog.

A null camera lists every stored entity, in storage order. `check_engine` uses that. With a camera, a model or sprite is dropped when its box is wholly outside the view, and then when it is wholly behind nearer walls. Survivors stay in storage order. `VIS_MAX` (2048) still truncates the list. A model with no triangles is omitted, so an empty slot does not consume the cap.

The rule for every step: the cull may keep something that turns out to be hidden, and the Z buffer sorts that out. It never drops something that has a pixel on the glass.

## What the camera is looking at

Once per call, the yaw and pitch become six inward planes:

- near, at `DF_NEAR` (4)
- far, at the distance below
- left, right, top, bottom, through the eye

The side slopes are the picture. `camera_project` is `sx = 320 + x * focal / z`. The focal length is `camera_focal()`, from the horizontal field of view. The default is 75 degrees, focal about 417, so a point is on the left or right edge when `|x| / z` is about 0.77, and on the top or bottom when `|y| / z` is about 0.58. `fov` in the console changes this on the next world frame, clamped to 45..120. The triangle clipper uses the same picture, plus 16 pixels, which stays far inside the SST-1 range of about +/-2048. `scene_set_cull(0)` lists every entity and skips these planes and the occlusion step.

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

## Occlusion

The frustum is a pyramid, so a wall in front of the eye still leaves every box behind it inside the view, out to the fog. The occlusion step removes those. It is a coarse depth picture built on the Pentium, after the frustum and before anything goes to the card.

1. The picture is cut into 40 x 30 cells of 16 x 16 pixels (`OCC_CELL`). Each cell holds one float: 1/z of a cover, or 0 for none.
2. The nearest models in the frustum list put their big front faces into the cells. A cell is written only when one face covers all of it. It keeps 1/z of the farthest point that face can have in the cell. When several faces cover a cell, the nearest of those bounds wins.
3. A model or sprite is dropped when its screen rectangle lands only on written cells, and every one of those cells is nearer than the nearest point of its box.

`scene_visible_sprites` reads the cells that `scene_visible_models` wrote in the same frame, so call the model list first, as `draw_frame` does. Sprites never hide anything: a flat has see-through texels.

### Why it never drops a visible model

Each rule errs toward drawing:

- A face is a cover only when `draw_frame` draws it. A plane of two or more triangles has to pass the same normal test (`instance_faces`). A single triangle is drawn from both sides, so it counts from both sides. Every triangle needs a texture slot; slot `0xffff` is never drawn, so that face covers nothing.
- The outline has to be convex. A notched face would cover its notch. The loader fans a plane from corner 0 (triangle k is corners 0, k+1, k+2), so the polygon is rebuilt exactly from the fan. A face whose triangles are not such a fan is not used. After clipping and projection, every corner must turn the same way (to within `OCC_BEND`), and x may reverse direction only twice on the way round, which refuses a polygon that crosses itself.
- A cell is written only when the outline holds the whole cell, grown by `OCC_EDGE` (0.25 pixel) on every side. That covers the card's 1/16-pixel vertex snap. Each row's run of cells is solved per edge, and the two end cells are then checked edge by edge, so rounding in that division cannot let in a cell the outline misses.
- The depth is a lower bound on 1/z. It is the larger of two bounds: the farthest clipped corner, and the face's plane at the cell corner where that plane is farthest. The plane uses Newell's normal and is pushed back to the face's farthest corner, so a slightly bent face is never nearer than its plane. Over a pixel, the plane gives `1/z = -(n . ray) / d` for the ray `((sx - 320) / focal, (sy - 240) / focal, 1)`, which is linear in the pixel.
- The box test uses the view-space box around the world box: the center plus half-sizes through the absolute camera rows, the same trick as `instance_world_box`. Its nearest depth and its screen rectangle hold every corner. A box that reaches nearer than the near plane is never hidden. The rectangle is clamped to the glass, because a part off the glass has no pixels.
- A box has to be `OCC_SLACK` (0.1% of 1/z) farther than a cell, so a sign or a door flush with its wall stays.
- A model that a nearer cover already hides adds no faces of its own. That is safe: anything it would hide is farther, behind the same nearer cover.

Joining walls is not a separate step. Two panels of one facade fill their own cells, and a box behind the pair is tested against all of them. A gap between buildings, a window opening, or a doorway is a run of empty cells, because the faces around it are separate planes.

### What counts as a candidate

- Mesh id 0, the ground, is skipped. Nothing is under it.
- The model's box has to be nearer than `OCC_NEAR` (640), and its screen rectangle at least 3 x 3 cells, unless the box reaches the eye.
- The nearest `OCC_MODELS` (32) go first. At most `OCC_FACES` (96) faces are written per frame.
- A face needs at least two cells of area after clipping. Faces are clipped to the near plane and to the picture plus `OCC_MARGIN` (16 pixels), so a wall that fills the glass also fills the border cells. There is no far plane: a wall that runs into the fog still hides what is farther.
- Floors and ceilings are covers like any other face. A model under a floor, seen from above, goes.

### Budget

Cost is measured against what it saves. These figures come from a host run of 3000 views beside random models in Privateer's Hold and in Daggerfall city. Each view was drawn into a full 640 x 480 software Z buffer, and no pixel of any dropped model or sprite would have passed the depth test.

| Place | Frustum kept | Occlusion dropped | Sprites dropped | Lost pixels |
|---|---|---|---|---|
| Privateer's Hold | 55294 | 15205 (27.5%) | 5813 | 0 |
| Daggerfall | 37413 | 2786 (7.4%) | 412 | 0 |

On the host, per frame in the hold, the occlusion step took about 0.0065 ms. The vertex work it removed was 0.0024 ms out of 0.0104 ms. Each removed triangle also skips Glide's triangle setup on the Pentium and the fill on the Voodoo, which a hidden room still pays for when the Z test rejects it. Outdoors it is about even. These are host numbers, not a measurement on a Pentium MMX 233.

What keeps it cheap:

- 16-pixel cells. 8-pixel cells hid 29% in the hold against 27.5%, at about twice the cost. The 40 x 30 grid is 4.8 KB and fits in the Pentium's data cache.
- A row of cells is one multiply per edge, not a test per cell.
- A plane is transformed once per corner and clipped once, not per triangle. A plane it does not cross does not clip it.
- Only the rectangle of cells written last frame is cleared.
- Pixel to cell is a multiply and `df_ifloor`, never a C cast. On the x87, GCC turns a cast into `fnstcw`, `fldcw`, `fistp`, `fldcw`, about 30 cycles. `df_iround` adds 1.5 * 2^23 so the store rounds, and reads the low bits. `scene.o` has no `fldcw`.

### Debugging

- `occlude off` keeps the frustum and skips this step. Compare a frame with it on.
- `covers on` washes the covered cells green. One quad is drawn per run of cells in a row. A model or sprite whose box lands only on green, and is behind it, was not drawn. Default off.
- `stats` prints `occluders`, the faces written into the cells, and `occluded`, the models and sprites they dropped, on the last world frame.

### What it does not do

- A cell that two faces share between them is left empty. Only one face has to cover a whole cell, so the seam between two panels is a column of empty cells. Anything whose box crosses that column is kept.
- There is no portal step and no frame-to-frame reuse. The cells are built again each frame.
- A mesh that is one rigid object larger than the picture is still one box. Its faces can hide others, but the box itself is hidden only as a whole.

### What was tried before

Rectangles cut from each wall, which this replaced, lost polygons. Do not bring these back:

- A single depth for the whole picture hid the street beside a wall that did not fill the glass.
- The nearest vertex of a wall as its depth hid a post standing in front of the far end of a wall seen at an angle.
- Joining the rectangles of nearby faces treated the gap between them as solid. Testing only the center of a box against the joined rectangle hid a house whose roof rose above the wall.
- The outer square of a slope reaches into the sky beside the slanted edge. The inner square fixed that one shape, but not a notched face, whose hull covers the notch.

## The clipper

The per-triangle clipper in `draw.c` cuts a triangle that crosses the near plane, the far clip, or the picture. The picture margin is 16 pixels, scaled by `camera_focal()`, and it is computed once per frame. A triangle already inside those six planes is projected directly. A close wall used to be clipped only to a 1600-pixel guard, so a corner was submitted at screen x = -3242. The SST-1 walker then started thousands of pixels off the glass. Distance is the far clip and the fog, not a second test in software.

A cut edge is always walked from its inside vertex to its outside one. Two triangles that share an edge see it in opposite orders, and a lerp from either end rounds differently. The two cut points then snap to different 1/16 pixels on the card and leave a crack along a wall that crosses the near plane or the glass. The occlusion check found one such crack, a pixel wide, on the diagonal of a wall in the hold. From the inside end, both triangles do the same arithmetic on the same numbers and meet exactly.

## Planes

Each ARCH3D plane keeps a normal and the triangle range it fanned into. The stored normal is the opposite of the first triangle's cross product: that winding points away from the visible side, the same way a ground quad's cross product points down while the patch normal points up. `draw_frame` tests that normal before transforming the range. Two or more triangles are worth the test. A single triangle is drawn directly. A zero normal is drawn too. The test keeps the face when the camera is on the front side (`instance_faces`).

Ground is not one 4096-wide mesh. `add_ground` emits 512-unit patches, each with an upward normal, so a patch behind the camera is dropped with the block or the box.

`grCullMode` stays `GR_CULL_DISABLE`. The Voodoo cull would run after the Pentium had already built the triangle.
