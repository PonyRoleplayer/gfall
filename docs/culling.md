# Culling

`draw_frame` does not walk the scene. It draws the index list from `scene_visible_models` and `scene_visible_sprites`. Those two calls are the cull. Read this before editing `scene.c`, the far clip, or fog.

A null camera lists every stored entity, in storage order. `check_engine` uses that. With a camera, a model or sprite is dropped when its box is wholly outside the view. Survivors stay in storage order. `VIS_MAX` (2048) still truncates the list. A model with no triangles is omitted, so an empty slot does not consume the cap.

The per-triangle clipper in `draw.c` is unchanged. It still cuts a triangle that crosses the near plane, the far clip, or the wide Glide guard (`CLIP_X` / `CLIP_Y`, about +/-2048 screen pixels). Culling only decides whether that triangle is transformed at all.

## What the camera is looking at

Once per call, the yaw and pitch become six inward planes:

- near, at `DF_NEAR` (4)
- far, at the distance below
- left, right, top, bottom, through the eye

The side slopes are the picture, not the Glide guard. `camera_project` is `sx = 320 + x * 400 / z`, so a point is on the left or right edge when `|x| / z = 320/400 = 0.8`, and on the top or bottom when `|y| / z = 240/400 = 0.6`. The guard is `1600/400 = 4`, wide enough that a triangle which merely crosses the picture does not wrap on the SST-1. A box outside the picture has no pixels, so the test uses 0.8 and 0.6.

A plane rejects the box when the nearest corner is outside: `dot(normal, center) + d + radius < 0`. The radius is `ex*|nx| + ey*|ny| + ez*|nz|`. Far is tested first.

## How far

`scene_set_limits(clip_far, fog)` is the limit. Defaults are fog on and `DF_FAR` (1400), the same as a fresh device. `gfx_set_far` and `gfx_set_fog` are the callers. Do not set the clip in `draw_set_far` alone; the cull would keep the old distance.

Fog is solid at `clip_far * (DF_FOG_FAR / DF_FAR)`, the same fraction `gfx_set_far` uploads (1100 out of 1400). The clear colour is the fog colour, so a box entirely past that distance is invisible. The distance is never closer than `DF_FOG_NEAR + 1`, because `glide_fog_range` will not put solid fog in front of the fog start. With fog off, the limit is the far clip itself. A box that only straddles the limit is kept, and the clipper cuts it.

## The box

Mesh bounds live on the `Mesh`: center `cx,cy,cz`, half-size `ex,ey,ez`. `bound_n` is the triangle count that produced them. `0` means not measured. The first cull scans the vertices once. A shared mesh (one ARCH3D id, many instances) is scanned once. Rebuilding the triangle list with a new count measures it again.

The world box is that local box through the instance. The center goes through `instance_point`. The half-size is the local half-size times the absolute columns of `m[]`, so a rotated or scaled model stays inside the box. The box is larger than the triangles. It can keep a model that is actually just off screen. It does not drop a model that still has a vertex in view.

A sprite faces yaw and stays upright. Its box is half the width on X and Z and half the height on Y, centered on the flat. A flat with `w` or `h` below 1 is omitted. `draw_flat` already skips those.

## What this does not do

One outdoor ground mesh is 512 triangles over a 4096 square. Standing on it, the box intersects the view, and every ground triangle is still transformed. The same is true of any mesh larger than the picture. Splitting that mesh is a loader change, not a change to the plane test.

`grCullMode` stays `GR_CULL_DISABLE`. Winding is still unproven, and a Glide cull runs after the Pentium has transformed the triangle. This pass does not reject backfacing planes.

There is no portal step. A neighbor block behind a wall and inside the fog is still drawn. The Z buffer hides it.
