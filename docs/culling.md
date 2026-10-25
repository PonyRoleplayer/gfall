# Culling

`draw_frame` does not walk the scene. It draws the index list from `scene_visible_models` and `scene_visible_sprites`. Those two calls are the cull. Read this before editing `scene.c`, the far clip, or fog.

A null camera lists every stored entity, in storage order. `check_engine` uses that. With a camera, a model or sprite is dropped when its box is wholly outside the view, and then when it is wholly behind nearer walls. Survivors stay in storage order. `VIS_MAX` (2048, exported as `DRAW_VIS_MAX` in `draw.h`) still truncates the list. A city holds more models than that, so `cull off` there drops the tail; LEVELFALL marks a count at the cap `CAP`. A model with no triangles is omitted, so an empty slot does not consume the cap.

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

The fog table is uploaded to go solid at `df_fog_far(clip_far)`: `clip_far * (DF_FOG_FAR / DF_FAR)`, 1100 out of 1400, never closer than `DF_FOG_NEAR + 1`, because `glide_fog_range` will not put solid fog in front of the fog start. The card does not reach solid there. Its 64 entries sit at w = 2^(3 + i/4) / (8 - i % 4), and it blends between two, so fog is 255 only from the first entry at or past that distance: `df_fog_solid`, 8192 / 7 = 1170 for 1100. At 1100 itself it is about 95%. The cull limit is `df_fog_solid(df_fog_far(clip_far))`, and so is `draw.c`'s far clip plane when fog is on. They used to differ: the cull dropped boxes from 1100, where a model was still about 5% visible and popped, and the clipper kept polygons out to 1400, filling pixels the fog had already turned to the clear colour. At LEVELFALL's 8192 the solid entry is 6553, a fifth of the range nearer than the clip. The clear colour is the fog colour, so a box entirely past the limit is invisible. With fog off, the limit is the far clip itself. A box that only straddles the limit is kept, and the clipper cuts it.

## Blocks, then models

`world` stores a `Span` per block: the first model index, the count, and the union of those models' boxes. The frame tests that box with the same six planes. A block that misses is not read. Survivors are still in storage order, block after block.

Inside a block, each instance has a `Bound` filled once, in `append_block`, after `shift_block` has written the final position. The cull reads that array and does not touch `m[]` or the mesh. `mesh_measure` runs at load (`arch3d_load`, ground patches). `bound_n` is the triangle count that produced the local box. A shared mesh is measured once.

The world box is the local box through the instance. The half-size is the local half-size times the absolute columns of `m[]`, so a rotated or scaled model stays inside. The box is larger than the triangles. It can keep a model that is actually just off screen. It does not drop a model that still has a vertex in view.

A sprite faces yaw and stays upright. Its box is half the width on X and Z and half the height on Y, centered on the flat. A flat with `w` or `h` below 1 is omitted. `draw_flat` already skips those. Sprites are not blocked by spans.

With no spans and no bounds (a single renderfall model, or a null camera), the old path remains: measure from the triangles if needed, and list everything when the camera is null.

## Occlusion

The frustum is a pyramid, so a wall in front of the eye still leaves every box behind it inside the view, out to the fog. The occlusion step removes those. It is a coarse depth picture built on the Pentium, after the frustum and before anything goes to the card.

1. The picture is cut into 40 x 30 cells of 16 x 16 pixels (`OCC_CELL`). Each cell holds one float: 1/z of a cover, or 0 for none.
2. The nearest models in the frustum list put their front faces into the cells. A face that covers all of a cell writes it, with 1/z of the farthest point that face can have in the cell. A face that covers only part of a cell lists itself on that cell instead.
3. A cell that no one face covers can still be covered by the faces listed on it together: a wall and its floor, two panels, a door leaf in its frame. The seam pass cuts those faces out of the cell. If nothing a pixel could land in is left, the cell is covered, at the farthest bound of the faces that took part.
4. A model or sprite is dropped when its screen rectangle lands only on covered cells, and every one of them is nearer than the nearest point of its box.

`scene_visible_sprites` reads the cells that `scene_visible_models` wrote in the same frame, so call the model list first, as `draw_frame` does. Sprites never hide anything: a flat has see-through texels.

`scene_box_visible` tests any world box against the planes, view, and cells that the last `scene_visible_models` call left, the same frustum and occlusion test a model gets. The light pass uses it for each light's decal box and then each decal (`docs/lighting.md`). Call it only after the model list of the same frame.

### Why it never drops a visible model

Each rule errs toward drawing:

- A face is a cover only when `draw_frame` draws it. A plane of two or more triangles has to pass the same normal test (`instance_faces`). A single triangle is drawn from both sides, so it counts from both sides. Every triangle needs a texture slot; slot `0xffff` is never drawn, so that face covers nothing.
- The outline has to be convex. A notched face would cover its notch. The loader fans a plane from corner 0 (triangle k is corners 0, k+1, k+2), so the polygon is rebuilt exactly from the fan. A face whose triangles are not such a fan is not used. After clipping and projection, every corner must turn the same way (to within `OCC_BEND`), and x may reverse direction only twice on the way round, which refuses a polygon that crosses itself.
- Outline corners are where the card puts them: `camera_project`'s sums, then `df_snap16`, the same 1/16-pixel snap `glide.c` applies (see the clipper below). The clip walks an edge from its inside end, as `draw.c` does.
- A cell is written whole only when the outline holds the cell grown by `OCC_EDGE` (1/16 pixel) on every side. A corner the clip made can land one snap step from the one `draw.c` sends, so an edge may sit 1/32 pixel off; the margin covers that and the fill rule. Each row's run of cells is solved per edge, and the two end cells are then checked edge by edge, so rounding in that division cannot let in a cell the outline misses.
- The depth is a lower bound on 1/z. It is the larger of two bounds: the farthest clipped corner, and the face's plane at the cell corner where that plane is farthest. The plane uses Newell's normal and is pushed back to the face's farthest corner, so a slightly bent face is never nearer than its plane. Over a pixel, the plane gives `1/z = -(n . ray) / d` for the ray `((sx - 320) / focal, (sy - 240) / focal, 1)`, which is linear in the pixel. A seam cell takes the farthest bound of the faces that covered it.
- The box test uses the view-space box around the world box: the center plus half-sizes through the absolute camera rows, the same trick as `instance_world_box`. Its nearest depth and its screen rectangle hold every corner. A box that reaches nearer than the near plane is never hidden. The rectangle is clamped to the glass, because a part off the glass has no pixels.
- A box has to be `OCC_SLACK` (0.1% of 1/z) farther than a cell, so a sign or a door flush with its wall stays.
- A model that a nearer cover already hides adds no faces of its own. That is safe: anything it would hide is farther, behind the same nearer cover. This check runs while faces are still going in, so it reads whole cells only, never a seam.

### The seam pass

Interiors are made of junctions. A room is one model whose walls, floor and ceiling meet along lines, and a door sits in a frame. With one face per cell, every junction was a staircase of empty cells, and any room box behind crossed one. In Daggerfall's castle at far 8192, facing a closed door, the frustum kept 473 models, 176 were drawn, and 4 had a pixel on the glass.

- While a face fills its cells, every cell it touches but does not hold is listed: up to `OCC_PART` (8) faces per cell, the nearest kept.
- A listed cell is resolved by cutting. It starts as the cell, grown by `OCC_EDGE`. Each face, nearest first, is cut out of it: what is left of a convex piece outside a convex face is one convex piece per edge, outside that edge and inside the ones before it. An edge with the whole piece on one side cuts nothing. At most `OCC_PIECES` (8) pieces are kept, or the cell stays open.
- A piece left over is closed only when the card cannot put a pixel in it. Either it is thinner than `OCC_NOISE` (1/4096 pixel), which is only rounding from the cuts, the trace of an edge two faces share: both were snapped to the same 1/16 pixel, and the card's fill rule puts every sample on a shared edge in one of them. Or no sample point lies within `OCC_SNAP` (1/16 pixel) of it. Every half pixel is tried, so this holds whether the SST-1 samples pixel centres or corners. The test walks each half-pixel row of the piece and solves its x run, so a long sliver costs one step per half pixel of its height.
- A T-junction stays open. Where a wall ends partway along the floor's longer edge, the wall's corner snaps a hair off the floor's line, and the wedge between them is a crack the card leaves, a pixel here and there. Anything behind can show through it. The ARCH3D loader now welds them inside a model (see T-junctions below); one between two placed models is still a T-junction.
- A loader point that lies along an edge snaps up to 1/32 pixel off it, so an outline can turn the wrong way by a hair. `poly_convex` lets a dent pass when it is no deeper than `OCC_SNAP` (1/16 pixel) from the chord. `occ_fill` and the seam pass treat an outline as the meet of its edges' half-planes, which for such a dent is the dented face itself, never more.
- The pass is lazy. A cell matters only when a box test lands on it, and a visible box stops at its first open cell, so `occ_hides` resolves a listed cell the first time it meets it, once every face is in, and `g_tried` keeps the answer for the frame. Running it on every listed cell up front cost three times as much.

### What counts as a candidate

- Mesh id 0, the ground, is skipped. Nothing is under it.
- The model's box has to be nearer than `OCC_NEAR` (4096), and its screen rectangle at least 3 x 3 cells, unless the box reaches the eye.
- The `OCC_MODELS` (48) whose boxes cover the most of the glass are the candidates; a box around the eye counts as more than the whole glass. They are then written nearest first, so one already behind the cells is skipped. At most `OCC_FACES` (128) faces are written per frame.
- Candidates used to be the nearest 32 within 640 units, with 96 faces. A castle hall's walls are farther than 640, so they never covered anything, and in a hall the budget went to the chairs and torches by the camera. One room in the castle drew 603 of 1235 models with 49 occluders; it now draws 133. Ranking by screen area and raising the budget were measured separately on the host: area alone, at 32 and 96, took that room to 184; nearest-first with 2048, 64 and 192 took it to 114 but doubled the cull's time. Area with 48 and 128 is between them in both.
- A face needs `OCC_FACE_AREA`, an eighth of a cell, after clipping. Alone it covers no cell, but a door jamb or a beam end still closes a seam. Faces are clipped to the near plane and to the picture plus `OCC_MARGIN` (16 pixels), so a wall that fills the glass also fills the border cells. A face wholly behind the eye or off one side is dropped before its normal is worked out. There is no far plane: a wall that runs into the fog still hides what is farther.
- Floors and ceilings are covers like any other face. A model under a floor, seen from above, goes.

### Budget

These figures come from a host run of 3000 views beside random models in each place. Each view was drawn into a full 640 x 480 software Z buffer that models `draw.c` and the card: the same clip planes and inside-first cuts, corners snapped to 1/16 pixel, samples at pixel centres, and one side of a shared edge taking a tie. No pixel of any dropped model or sprite would have passed the depth test.

| Place | Frustum kept | Occlusion dropped | Sprites dropped | Lost pixels |
|---|---|---|---|---|
| Privateer's Hold | 60110 | 29971 (49.9%) | 8640 | 0 |
| Daggerfall castle, far 1400 | 71188 | 34620 (48.6%) | 8620 | 0 |
| Daggerfall castle, far 8192 | 636880 | 465045 (73.0%) | 154928 | 0 |
| Daggerfall city | 40220 | 5607 (13.9%) | 722 | 0 |

The frustum counts are higher than the first run's because the cull now reaches the fog's first solid table entry (the fog paragraph near the top), not 1100.

The second round of LEVELFALL screenshots, castle, far 8192, models drawn out of 1235. "Before" is the card's own count with the nearest-640 candidates; "now" is the host, with poses rounded to whole units.

| View | Before | Now | Occluders before, now |
|---|---|---|---|
| Door | 33 | 28 | 43, 128 |
| Stair hall | 251 | 79 | 27, 128 |
| Thrones | 38 | 16 | 70, 128 |
| Corridor | 8 | 9 | 64, 128 |
| Room | 21 | 23 | 68, 128 |
| Large storeroom | 603 | 133 | 49, 128 |
| Long corridor | 21 | 16 | 10, 128 |
| Corridor to a far room | 73 | 38 | 26, 128 |
| Outside the rooms, in the void | 106 | 108 | 96, 128 |

From outside the rooms every wall is seen from its back, which `draw_frame` does not draw and so is never a cover. A player is never there.

The three castle views from LEVELFALL's screenshots. Tris are front faces only.

| View | Far | Frustum | Before the seam pass | Now | Really visible |
|---|---|---|---|---|---|
| Door | 8192 | 473 models, 6808 tris | 176, 2945 | 46, 1047 | 4, 69 |
| Hall | 8192 | 378, 5356 | 275, 3975 | 148, 2373 | 24, 464 |
| Corridor | 8192 | 558, 6947 | 290, 3945 | 46, 678 | 6, 92 |
| Door | 1400 | 38, 627 | 24, 464 | 5, 107 | 4, 69 |
| Hall | 1400 | 30, 551 | 29, 535 | 23, 444 | 15, 352 |
| Corridor | 1400 | 38, 521 | 28, 455 | 6, 78 | 5, 72 |

What was left at far 8192 was mostly T-junctions: the floor line of a room and the split walls behind pillars. See T-junctions below for what welding them inside each model did.

On the host, per frame in the hold, the whole cull took about 0.021 ms (0.017 with the old candidates), against 0.011 ms of vertex work for the frustum's list, of which it removed 0.005 ms. In the castle at far 8192 it takes 0.044 ms (0.028 before), worst 0.125. Each removed triangle also skips Glide's triangle setup on the Pentium and the fill on the Voodoo, which a hidden room still pays for when the Z test rejects it. It removes about 0.060 ms of vertex work there. Outdoors it is about even. These are host numbers, not a measurement on a Pentium MMX 233.

What keeps it cheap:

- 16-pixel cells. 8-pixel cells hid more, at about twice the cost. The 40 x 30 grid is 4.8 KB and fits in the Pentium's data cache.
- A row of cells is one multiply per edge, not a test per cell. The same solve gives the run a face only touches.
- A plane is transformed once per corner and clipped once, not per triangle. A plane it does not cross does not clip it.
- The seam pass is lazy, skips a cell none of whose faces is nearer than its cover, cuts into its output slots instead of copying pieces, and runs the sample test only on what is left at the end.
- Only the rectangle of cells written or listed last frame is cleared.
- Pixel to cell is a multiply and `df_ifloor`, never a C cast. On the x87, GCC turns a cast into `fnstcw`, `fldcw`, `fistp`, `fldcw`, about 30 cycles. `df_iround` adds 1.5 * 2^23 so the store rounds, and reads the low bits. `scene.o` has no `fldcw`.

### Debugging

- `occlude off` keeps the frustum and skips this step. Compare a frame with it on.
- `covers on` washes the covered cells green, seam cells included; with it on, every listed cell is resolved so the picture is complete. One quad is drawn per run of cells in a row. A model or sprite whose box lands only on green, and is behind it, was not drawn. Default off.
- `stats` prints `occluders`, the faces written into the cells, and `occluded`, the models and sprites they dropped, on the last world frame.

### T-junctions

`arch3d.c` builds each face from the file's points:

- A face lists its points as byte offsets into the model's point table, which holds `points` (header offset 4) entries of 12 bytes from the offset at 48. In v2.6 and v2.7 the reference is that byte offset. In v2.5 it counts 4 bytes a point, so the point is `reference / 4`. The loader used to read v2.5 references as indexes and multiply by 12. The 8 v2.5 records (41500-41505, 41509, 41510) then read points from three times past their tables, out in the rest of the file: coordinates in the millions, faces collapsed to one point. Each now reaches exactly the radius in its own header (246 for 41500, 1803 for 41510). A reference outside the point table now skips that face and is counted (`arch3d_bad_refs`); none of the 10,237 records has one. `check_dagger` checks 41500.

- A point that lies on the straight line between its neighbours, within `STRAIGHT_NEAR` (4 file units, 1/64 world unit), is dropped (`straighten`), and so on until only the real corners are left. The old rule measured the turn seen from two points back and dropped anything under 0.6 degrees. A door jamb or frame is 2 units wide, and its real corners turn less than that: 477 faces in `ARCH3D.BSA` lost a triangle-sized piece, up to 16% of their area. That is the door with a chunk missing. With the distance test none loses area. `check_dagger` checks model 457's jambs.
- Then `weld` looks at every other face of the same model. Where one runs along this face's edge and ends inside it, its corner is inserted into this edge, on the line, with texel coordinates interpolated along the edge. The two faces then share that seam exactly, the card leaves no crack, and the occlusion cells close over it. A corner where a face only touches the edge from another direction is not inserted: it closes no seam.
- Keeping every file point instead of straightening was tried. It removed three quarters of the in-model T-junctions on its own, but it left corners on a model's outer edges that the next model's face, along the same line, does not have: a new T-junction between models, which opened a row of cells along the stair hall's floor and drew 124 models instead of 79.
- Faces are fanned from the corner whose first triangle is largest (`fan_apex`), so the face normal comes from a real triangle. An inserted corner next to the apex makes a zero-area triangle that the card skips and that keeps the fan exact. With all points, 2 faces in the archive are not convex, and their fans from that corner still cover them exactly.
- A face can now have up to 32 corners. `OCC_POLY` is 24, `GLIDE_POLY_MAX` 32, and `draw.c`'s `FACE_MAX` 26.

In the castle, T-junctions inside models went from 196 edges in 87 models to 4 edges in 1. The castle's world load went from 12.0 to 17.7 ms on the host. The weld's tables are static, about 260 KB, sized for model 451's 2720 corners.

| View, castle far 8192 | Old loader | Straightened only | Straightened and welded | Really visible |
|---|---|---|---|---|
| Door | 28 | 28 | 4 | 4 |
| Corridor to a far room | 38 | 38 | 32 | 6 |
| Stair hall | 79 | 73 | 73 | 22 |
| Bank vault | 105 | 105 | 104 | 41 |

Over the 3000 random views the weld costs a little: occlusion drops 49.6% of the hold's frustum models (50.0 straightened only), 72.9% of the castle's at far 8192 (73.1), 13.2% of the city's (13.9). A seam welded inside one model can leave a corner on an edge that another model's face also borders. The views above, the ones that were reported slow, gain far more. Lost pixels stay 0 in all four runs.

### What it does not do

- A T-junction between two placed models stays open. The loader welds only inside one model; a mesh is shared by every placement, so a weld against a neighbour would need a copy per placement.
- A window or a grille is a real opening. In the bank vault the cull draws 104 of 125 frustum models against 41 with a pixel on screen: the cells around each barred window are part wall, part bars and cell interior, small separate models no set of large faces covers.
- There is no portal step and no frame-to-frame reuse. The cells are built again each frame.
- A mesh that is one rigid object larger than the picture is still one box. Its faces can hide others, but the box itself is hidden only as a whole.

### What was tried before

Rectangles cut from each wall lost polygons. Do not bring these back:

- A single depth for the whole picture hid the street beside a wall that did not fill the glass.
- The nearest vertex of a wall as its depth hid a post standing in front of the far end of a wall seen at an angle.
- Joining the rectangles of nearby faces treated the gap between them as solid. Testing only the center of a box against the joined rectangle hid a house whose roof rose above the wall.
- The outer square of a slope reaches into the sky beside the slanted edge. The inner square fixed that one shape, but not a notched face, whose hull covers the notch.

In the seam pass:

- Calling any sliver thinner than 1/32 pixel closed, on the grounds that the card snaps it shut, lost about one pixel in a hundred views. Without the snap in `glide.c` a seam is not watertight on the card, and a T-junction wedge is not a shared edge at all.
- Testing for samples with a 1/16-pixel margin on every piece, shared edges included, was sound but kept almost every seam open: the floor line of a room was a column of open cells again.
- A float reference that rasterized unsnapped vertices reported cracks the card does not have. Check a change against a reference that models `draw.c` and the snap.

## The clipper

The clipper in `draw.c` cuts a polygon that crosses the near plane, the far clip, or the picture: a whole ARCH3D face, a sprite quad, or a lone triangle (see `docs/glide.md`). The picture margin is 16 pixels, scaled by `camera_focal()`, and it is computed once per frame. A polygon already inside those six planes is projected directly. A close wall used to be clipped only to a 1600-pixel guard, so a corner was submitted at screen x = -3242. The SST-1 walker then started thousands of pixels off the glass. Distance is the far clip and the fog, not a second test in software.

`glide.c` snaps every vertex's screen x and y to 1/16 pixel (`df_snap16`) before it reaches the card, as the Glide 2 guide asks. The SST-1 takes 12.4 fixed point. Unsnapped, two triangles that share an edge reach the card a fraction apart, and its edge walker drops pixels along the seam. Snapped, a shared edge is shared exactly and the fill rule leaves no gap. The occlusion seam pass counts on that. `df_snap16` adds 1.5 * 2^19 and subtracts it again through a `volatile` float: on the x87 the sum would otherwise stay in an 80-bit register and keep every bit.

A cut edge is always walked from its inside vertex to its outside one. Two triangles that share an edge see it in opposite orders, and a lerp from either end rounds differently. The two cut points then snap to different 1/16 pixels on the card and leave a crack along a wall that crosses the near plane or the glass. The occlusion check found one such crack, a pixel wide, on the diagonal of a wall in the hold. From the inside end, both triangles do the same arithmetic on the same numbers and meet exactly.

## Planes

Each ARCH3D plane keeps a normal and the triangle range it fanned into. The stored normal is the opposite of the first triangle's cross product: that winding points away from the visible side, the same way a ground quad's cross product points down while the patch normal points up. `draw_frame` tests that normal before transforming the range. Two or more triangles are worth the test. A single triangle is drawn directly. A zero normal is drawn too. The test keeps the face when the camera is on the front side (`instance_faces`).

Ground is not one 4096-wide mesh. `add_ground` emits 512-unit patches, each with an upward normal, so a patch behind the camera is dropped with the block or the box.

`grCullMode` stays `GR_CULL_DISABLE`. The Voodoo cull would run after the Pentium had already built the triangle.
