# Collision

With `tcl 1` the camera is a body: it walks on floors, stops at walls, climbs stairs, and falls under gravity. Read this before editing `src/engine/scene/collide.c`, `src/engine/scene/walker.c`, or the walker's call in `draw.c`.

## The body

Daggerfall Unity's scale is 40 world units a metre (`MeshReader.GlobalScale`, 0.025). Its player is 1.8 m tall with the eye 9 cm under the top (`PlayerHeightChanger`). So the body is 72 units tall, the eye 68.4 above the feet, 32 units across. Gravity is 9.8 m/s², 392 units/s², and a fall gets no faster than 50 m/s.

It is two parts, as the engines of Daggerfall's day stood on stairs:

- **The upper body**, from one step (24 units, 0.6 m) above the feet to the top of the head: an upright ellipsoid, 16 across and 24 tall (half), swept along its motion (`collide_slide`). Walls and ceilings stop it wherever it meets them.
- **The feet**, which stand on the highest floor under the body's footprint: the middle and eight points 70% of the radius out (`floor_under`). Standing, the floor may be up to a step above the feet (a stair: the body steps up) or a step below (down a stair: it steps down, rather than floating off the edge). Falling, the floor is looked for over the whole of that frame's fall.

A floor is a triangle whose plane rises at least 45° (`COLLIDE_FLOOR`), either way up: the triangles are two-sided, and ARCH3D winds a floor's normal down, away from the side it is seen from.

The first version was an ellipsoid alone, with a step-up: lift, move, put down. On every stair it rested on the edge on its round bottom and rolled back off, because a step only 6 units a frame never got the middle over the edge. Judging the step by the distance slid did not help either: riding up the edge looked like progress. The split body walks stairs in one frame, and leaves a low obstacle to the feet.

## The sweep

`collide.c` is Kasper Fauerby's method, "Improved Collision detection and Response" (2003). Scale the world by one over the ellipsoid's radii and the body is a unit sphere. A sphere swept along v against a triangle meets:

1. its plane, inside the triangle, at the time the sphere's front reaches the plane; or, if that point is outside,
2. one of its three corners: a quadratic in t; or
3. one of its three edges: a quadratic in t, then a check that the contact lies on the edge.

The first contact over all the gathered triangles, if any before t = 1, stops the body a hundredth of a radius short (`VERY_CLOSE`), and what is left of the motion is projected onto the plane through the contact, facing the sphere's centre: the body slides. Five slides settle any corner. Because the whole motion is tested, no speed and no frame time lets the body through a wall or a floor; a 700-unit move in one frame stops at the wall in hostcheck. Triangles count from both sides; one the body is moving away from is skipped, so a body already touching can always leave.

Point in triangle is barycentric (Ericson, *Real-Time Collision Detection*, 3.4); the static overlap test for `stuck` is the closest point on a triangle (Ericson 5.1.5).

## The gather

Nothing is copied at load but a grid of model boxes: 512-unit cells over the place, each listing the models whose box reaches into it (`collide_use`, rebuilt when the scene or its heap changes, `mem_epoch`). Daggerfall city is 3545 models in a 51 × 51 grid. A streamed scene has no grid: the gather walks its slots' spans and their models, since chunks come and go every few seconds and the grid over all 23,000 slots was a hitch each time (`docs/streaming.md`, Collision).

Each frame `collide_gather` takes one box round everything the frame's moves can reach. For each model in its cells whose box meets it (a stamp skips one met twice), the box goes into the model's own frame (the matrix is a rotation, so its transpose undoes it), and each triangle is tested there against its own corners' range: no transform for the many that miss. The few that are left are turned into the world, scaled into ellipsoid space, given their plane, and kept in a static set of up to 1024 (`COLLIDE_SET`). Every query reads that set.

Faces the loader could not texture (`slot` 0xffff) are not part of the place and do not collide. Flats, sprites, never do: Daggerfall let you walk through its scenery and its people; enemies are for later.

## A frame

`draw_frame` calls `walker_step` with the console's camera, when it is the one being drawn, before anything reads it. So GFALL and LEVELFALL both walk, and neither knows: each moves its camera as it always did, and the walker takes how far it moved across the ground as where the body wants to go. Up and down are gravity's; a viewer's Q and E do nothing while walking. A camera moved more than 768 units in a frame was set, not walked, and the body is taken there. So is one in a new place: another `Scene`, or another heap (`mem_epoch`), however short the move, because a door to a house's inside lands the camera 80 units on, through the wall the door is in, and walked there the body stopped at that wall, outside the house's floor, and fell. `walker_place` does the same for a viewer that sets its camera. The doors and levers move after the walker, in the same frame (`interact_frame`, `docs/interact.md`); the gather reads a model where it is now, and a mover's box in the grid is its whole sweep. A door does not push the body: one that swings into it may catch it, and `stuck` frees it.

1. Gravity: `vy -= g dt`, `dy = vy dt`. Semi-implicit Euler, two multiply-adds.
2. One gather round the body, as far as the move and the fall reach.
3. Across: the upper body swept by the move.
4. Down: standing, the floor within a step up or down of the feet, and the upper body swept to it, so a low ceiling over a stair keeps the body off it. Falling, the floor within this frame's fall, landed on; or, with none, the upper body swept down by the fall.
5. Fallen more than 1024 units below the place's lowest point: back on the highest floor under where it fell, or the last place it stood on firm floor, or on top of the place.
6. The camera goes to the eye.

A camera that moved more than 768 units in a frame was set (`setpos`, a new place), not walked: the body is put where it is and freed if it is caught.

### Jumping and landing

`walker_jump(up_speed)` starts a jump from a floor: the body goes up at `up_speed` (world units a second, 40 a metre) and gravity brings it down, about `up_speed² / 2g` high (a little less, stepped at the frame's rate: 260 a second rises 86 in the test, 200 rises 47.7 at 30 frames a second). It returns 0 off the ground. How high is the game's to say (Daggerfall's Jumping skill), and so is how fast it walks: the walker goes where the game moved the camera across the ground, so a run is a longer move.

`walker_landed(&speed, &height)` is 1 once, on the frame the body comes down on a floor after a fall or a jump: the speed down at the floor and the height fallen from the top of its arc (walking off an edge, from the edge). For the game's fall damage. A body set somewhere (a new place, `walker_place`) owes no landing.

### Entities

An entity with a radius (`Entity.radius`, `docs/entities.md`) stands in the way. After the move across, the body is pushed out of every live solid entity its height meets, its radius and the entity's apart, the push swept through `collide` like any move, so it never goes into a wall (`push_off_entities`). For the game's motors, `entity_blocked(scene, self, x, z)` says whether an entity standing at x, z would meet the body or another solid entity, and `walker_body` gives the body's feet. Two that already overlap (born so) may walk apart.

## Stuck

`stuck` moves the body to the nearest free place: rings of 16 points 8, 16, 32 … 256 units out, each at the feet's height and 24, 48 and 96 above, nearest first. A place is free when the upper body there cuts into nothing and a floor holds the feet within a step; the feet then go onto that floor. With none, it goes back to the last place it stood on firm floor (a floor under its middle, not just the footprint's edge: a body put back on a ledge's very edge slid off again). The same search runs on its own when collision is turned on and when the camera is set.

## Cost

On the host, a step is about a microsecond: the sets near the body hold 2 to 42 triangles in the hold, the castle and Daggerfall's streets. On the 233 that is some tens of microseconds a frame. No allocation after the grid; no float-to-int cast in the frame (`df_ifloor`), so the x87 never changes its rounding mode.

Memory: the grid, on the place's heap: a start per cell, a model index per cell a model reaches, and a stamp per model: tens of KB in a city. The gathered set is static, 52 bytes a triangle, 53 KB.

Tried on the card (a Voodoo 2 on the 233 machine): it walks the hold, the castle and the streets, stops at walls and climbs stairs. A good first version, not a finished one: the list below is what it lacks.

## Switches

| Console | Effect |
|---|---|
| `tcl 1` / `tcl 0` | Collision on: the camera walks under gravity. Off (default): it flies free, as before. `tcl` alone flips it. |
| `stuck` | With collision on, move out of whatever the body is caught in. |

## Not done

- Doors are models: a closed door blocks, and nothing opens one yet.
- No crouching, swimming or climbing. Jumping and the landing's speed are here; the damage is the game's.
- The body does not push or ride anything; an entity stops it, and the game's motors stop at it.
- Ladders and the steepest stairs (a riser over 24 units) block.
