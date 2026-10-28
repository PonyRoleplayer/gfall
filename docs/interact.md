# Interaction

Looking at a thing and pressing the use key does something: a door swings, a lever starts what it is chained to, a house's door takes you inside, a dungeon's exit takes you out, a person says who they are. Read this before editing `src/engine/scene/interact.c`, `src/game/dagger/places/doors.c`, `rmb_load_interior` in `rmb.c`, the action fields in `rdb.c`, or the passages in `apps/levelfall/main.c`.

## The parts

- **The engine** (`engine/scene/interact.c`, plain C, host-tested) knows two things: a `Usable`, something that can be used, and a `Mover`, a model that moves. Both are lists on the `Scene` (`usables`, `movers`). It casts the ray, picks the usable, starts movers and moves them on each frame. It knows nothing of Daggerfall.
- **The game** (`game/dagger/places/doors.c`) builds those lists from the blocks when the place loads: which models are doors, which records are actions, which faces lead somewhere, which flats are people.
- **The program** decides what a passage does, through a handler (`interact_set_handler`). LEVELFALL loads the place it leads to. GFALL sets no handler yet: its doors and levers work, and a passage only logs that nothing takes it.

## Using

The use key is the left mouse button or E, on its edge, polled in `draw_frame` (`use_edge`). The console being open stops it, as it stops the camera, and so does the game's pause. A game whose left button attacks turns the poll off (`gfx_set_use_key(0)`) and asks for a use on its own key's edge (`interact_request_use`, `docs/input.md`). In third person the use is cast from the body's eye along its facing, not from the camera behind it (`interact_request_use_from(x, y, z, dx, dy, dz)`; `interact_aim_from` for any origin), and the walk-into triggers are the game's body's (`interact_set_body`: its feet, height and radius; 0, the default, is the camera's eye with the walker's body under it).

The ray starts at the camera's eye and runs along its forward, pitch included: the middle of the glass, as Daggerfall Unity's `PlayerActivate` casts it. It goes as far as the longest reach in the place. `interact_cast` does two passes:

1. **The wall.** Every model whose box (`Scene.bounds`) the ray crosses is turned into the ray's frame, and its triangles are tested from both sides (Möller and Trumbore). The nearest hit is the wall: nothing past it is used. That is what stops a door being opened through the wall beside it.
2. **The usables.** Each usable's box is tested against the ray, up to the shorter of its reach and the wall plus half a unit. The nearest box entered wins.

A usable is one of three shapes:

- **A model** (`model >= 0`): its mesh box, as the model stands now. A door that has swung open is no longer in its doorway.
- **A sprite** (`sprite >= 0`): a box as wide as the flat both ways and as tall.
- **A box of its own**: a centre, a facing across the floor (`nx`, `nz`, the outward normal), and half sizes across the face (`hw`), up it (`hh`) and along the normal (`hd`). A passage is a face of a house or of a dungeon's exit model. Its box is 8 units either side of that face, so the ray enters it 8 units before it reaches the wall the face belongs to.

The reach is Daggerfall Unity's, 40 world units a metre: a door or a lever 128 (3.2 m), a person 256.

Using a `USE_MOVER` starts its mover, unless it is locked and closed: then the engine shows "This door is locked." for 2.5 s, the last half fading, at the middle of the glass a third of the way up (`interact_say`, drawn by `draw_saying`). Every other kind goes to the handler. With no handler, or a handler that returns 0, the use is logged and nothing more happens. Every use logs a line in the console: what it took and what that did (`use: door 3, to its end`, `locked (2)`, `still moving`), or that nothing was in reach and how far the wall was.

## Movers

A mover eases its model between two poses, linearly over `secs`:

- **start**: where the place put the model (`mover_init` copies it when the place is built);
- **end**: the start turned by `rx`, `ry`, `rz` about the model's own axes, Z, then X, then Y, as Unity's `Transform.Rotate` in self space turns Daggerfall Unity's actions, and moved by `tx`, `ty`, `tz` in the world.

The model's origin is the pivot. For a door model that is its hinge.

`mover_start` sends the mover toward whichever end it is not at, and does the same for the mover named by `next`, and on down the chain (at most 64, in case one loops). A mover marked `once` only ever goes forward. While anything in a chain is moving, the chain cannot start again (`mover_busy`), as Daggerfall Unity's `IsPlaying` refuses. Each mover plays `go_sound` on its way to the end and `back_sound` on its way back, from its model's centre, full within 2 m and silent past 30 m (`sound_play_at`).

`mover_step` runs each frame from `interact_frame`, after the walker and before the cull, and writes the model's matrix and position. Nothing in the engine keeps a model's pose from one frame to the next: the cull, the occlusion cells and the collision gather all read the live instance. Three things are fixed at load, and are taken care of:

- **Its box.** `Scene.bounds` is read by the cull and by the collision grid. `world.c` writes the mover's whole sweep there (`mover_sweep`, nine poses along the way) and grows its block's span to hold it.
- **Its light.** Light decals are cut at load where the model stood. A moving model gets none (`light.c` marks it); instead the whole model is drawn in one colour, the ambient raised by the lights round its box, as a sprite is lit (`light_model_rgb`). See `docs/lighting.md`.
- **Its collision.** The walker meets a door's triangles where the door is now. A door that swings into the body does not push it; the body may be caught, and `stuck` frees it (`docs/collision.md`).

## Daggerfall's doors

`doors_build` runs after `paint`, once the flats are final, since a person is a flat by index. What it makes, from Daggerfall Unity's `RDBLayout`, `DaggerfallInterior`, `DaggerfallActionDoor` and `MeshReader`:

**Hinged doors.** In a dungeon, a model whose model list entry has the three letters `DOR`, `DDR`, `NEW` or `CAV` (`IsActionDoor`), except the red brick door 72100. In a house, the models of its door list. Each swings a quarter turn back about its own up (−90°) in 1.5 s. A dungeon door's starting lock is the top nibble of its trigger word, through the table 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 25, 30, 50, 128, 255. Sounds by `DAGGER.SND` position: a dungeon's doors 25 open and 24 shut, a house's 94 and 93. Privateer's Hold has 58 doors, one locked (lock 2).

**Actions.** An RDB model resource, after its turn and model index, has the trigger word (u32 at 14), a sound id (18) and the offset of an action record in the block file (i32 at 19). The action record is the axis (byte), the time in twentieths of a second (u16), the size (u16), the next object's offset in the block file (i32), and the flags (byte). `rdb.c` keeps them in `ModelAct`, beside the model.

| flags | what | motion |
| --- | --- | --- |
| 1 | slide | `size` units along the axis |
| 8 | turn | `size` 2048ths of a circle about the axis |
| 2 to 7 | slide | 8 units times the axis byte, along the flag's own axis, in 2.5 s |
| any other | relay | none: text, spells, traps, a teleport, and the rest pass the chain on, and do nothing more yet |

Axes are 1 −X, 2 +X, 3 −Y, 4 +Y, 5 −Z, 6 +Z. A turn goes the axis's way. A slide along X or Z goes against it and along Y with it, as `GetTranslationActionVector` has it. Daggerfall Unity mends a few models by their letters, and so does `action_motion`: `TRP` with axis 13 turns +X by 400, `C0E` with axis 12 turns −Y at once, `C0X` with axis 11 slides −Y and stays (`once`), a `LID` turns a quarter about −Z, a `WHE` a full turn about −Y.

The sound id names a `DAGGER.SND` record by id (`sndfile_by_id`), and plays both ways.

A chain is found by offset: the next object is the model in the same block whose object offset is the action's next. The trigger's low four bits say how it is set off. 2, 6, 8 and 9 are used by hand, and get a usable; 1, 3 and 9 by walking into the model (below); the rest start only from the one before them.

**Teleports.** Action 14 (`RdbActionFlags.Teleport`) moves nothing: it sends the player to its next object, as DFU's `Teleport` does (the player to `NextObject`'s position and turn), and ends its chain. The next object is found by offset in the same block, an editor marker as a rule (`Marker.pos`; `world.c` hands `doors_build` every block's markers of a dungeon), which has no turn, so the player faces +z, or a model, facing its own +z. The eye goes `WALK_EYE` above the floor under the target (the creatures' floor ray), and the walker starts again from there (`walker_place`). Every time it is reached, the player goes (`Mover.teleport`: it has no pose to toggle). A hinged door whose action is a teleport is a portal: used, it does not swing onto the wall behind it, it sends the player; locked, it says so. A teleport whose target is not found is none: a door swings, an action relays. In 73 dungeons, one in sixty of the list, 15 have teleports, 72 of them: 26 used by hand, 46 walked into, one a door (Daggerfall's castle has the door and one other, both into the castle).

The engine keeps the teleport a use or a chain reached (`interact_teleport`); `draw_frame` takes it right after `interact_frame` and moves the console's camera there, the camera the walker carries. With a game's player body set (`walker_set_game`, `docs/collision.md`), `draw_frame` leaves it: the game takes it after `gfx_frame` and places its body there, its eye at the point given.

**Walking into.** A usable with `touch` is used on the frame the body (the walker's, `WALK_RADIUS` round and `WALK_HEIGHT` tall under the eye) comes into its model's box as it stands now, not each frame it stays. With reach 0 it is never aimed at by hand. After a teleport, what the body lands in counts as come into already: a teleporter at the far end does not send it straight back; stepped off and on again, it does. In the hold, a lever (41020, an activate relay) starts a stone that turns 83° (41042, 1.7 s); another (61027) turns, and starts a slab that rises, which starts another.

**Passages.** A face is a door when its texture archive, less its climate hundreds unless it is 331 or 156, is a door set (`MeshReader.LoadVertices`):

| archive | where | leads |
| --- | --- | --- |
| 74 | a town's building | in (`DOOR_ENTER`, tag = block × 256 + building) |
| 74 | a house's inside | out to the street (`DOOR_LEAVE`) |
| 56, or 331 with a record above 0 | a town | down into its dungeon (`DOOR_DUNGEON`) |
| 95 | a dungeon | out (`DOOR_EXIT`) |

A town's own props, which are not a building, have doors painted on them too; those lead nowhere. A face that is not upright is not a door. The box is the face's span across and up, and its centre, in the world.

**People.** The people lists of a town's buildings and of a house's inside are flats marked `FLAT_PERSON` with their faction (`Flat.tag`). Each is a `USE_TALK` usable. Talking is the game's, later; LEVELFALL says the faction.

## Inside a building

`world_load_inside(town, block, record)` loads one building's inside from the same RMB block its outside came from. An RMB subrecord is the outside's lists and then the inside's: a 17-byte header (models, flats, section 3, people, doors, then 12 bytes) and the arrays in that order, doors 19 bytes each. `rmb_load_interior` skips the outside's lists to find it.

- The inside is placed exactly as the building's outside is (`place_child` with the building's position and yaw, then the block's tile), so the house's way out lies where the street's door is: 6 units from it in the Ashwing farmhouse, facing the other way.
- A prop (object type 3) stands with its lowest point at its file height, not negated, as `DaggerfallInterior.AddModels` places it.
- Flats stand on their file point, as outdoors (`World.base_flats`).
- Editor flats (archive 199) are kept as markers. Record 8 is where a visitor enters. The place starts there, eye height above it.
- Its hinged doors are models 9000 to 9004, the door record's model byte mod 5, at the record's position and yaw.
- It is indoors: no sky, no weather. Each `TEXTURE.210` flat is a light, always lit.
- The building's type and faction come from the block's building list (26 bytes each after the 640 bytes of positions: faction u16 at 18, type at 24). The music follows them, as `SongManager` chooses: shops `GSHOP`, taverns the square's and the folk tunes, the Mages Guild `GMAGE_3` and `MAGIC_2`, palaces `06`, temples `GNEUT` (the god's alignment is not read yet), the Fighters' Guild `17`, everything else `23`, with the FM arrangements on an FM card (`docs/sound.md`).

## A dungeon and its outside

A dungeon record names the location of the outside it lies under: its parent, when it has one, or its own number. `dungeon.c` keeps that as `exterior_id` on the `Dungeon` and the `DunRef`; an outside's is its own number. Privateer's Hold is location 50050, under 50049, the outside whose way down is a 40 × 78 door.

## LEVELFALL: going through

The handler (`lf_use`) records the passage and the view ends on that frame. `view_row` then opens where it leads, each place on its own temp heap. The card stays open from one place to the next: `gfx_hold` grabs the frame on the glass (the screenshot buffer, `shot_grab`), writes it back dimmed with "Loading..." in the middle and swaps it to the front, and it stays there while the old heap ends and the next place loads and goes onto the card (`gfx_prepare` again, which starts the card's memory over). The new place's first frames come up from black over 0.6 s (`gfx_fade_in`). Before this, every passage closed Glide and opened it again, and the Voodoo's pass-through handed the screen to Windows in between: it looked like the program had died and restarted. Nothing Glide keeps lives on a place's heap (the card's records, the sky, the font are static; the lights, the collision grid and the sound rebuild by `mem_epoch`), which is what lets the card outlive the place. A place that fails to load closes the card first, so Windows can show why. The landings:

- **In**: the inside of that building, loaded with the town's record still in hand. The camera lands a body radius and 0.4 m inside the way out nearest the street's door, facing into the room.
- **Out to the street**: the town again, landing three body radii out from the door it went in by, facing away from it.
- **Down**: the row whose `exterior_id` is the town's location. The camera starts on the dungeon's start and faces away from its nearest way out, as Daggerfall Unity turns the player on coming down.
- **Out of the dungeon**: the row whose location is the dungeon's `exterior_id`, landing three radii out from its way down.

A way down or out with no pair in the list says "It leads nowhere in the list." and the view stays. Esc goes back to the list from anywhere. F12 does not bookmark inside a building: `open=` names a list row, and an inside is not one. The landings are what Daggerfall Unity's `PlayerEnterExit` does, at 40 units a metre.

## Trigger vision

`trigview N` (the console; F3 in LEVELFALL steps 512, 1024, 2048, off) draws the triggers within N units, the nearest 32 (`draw_trigger_view`):

- **Boxes.** Violet: what the use key takes by hand, as `useview` boxes it. Orange: a walk-into trigger's volume, its model's box grown by the body's radius, the box `touched` tests; red while the body is in it (`interact_touching`). Gold: a passage. Dim blue-grey: a mover only a chain starts. A teleport draws a magenta line from its trigger to where it sends, and a cross there.
- **Words**, three lines at its middle:
  1. how it goes off and how far: `u12 walked into  3.2m`, `u4 by hand`, `u7 by hand, walked into`, `u30 passage`, `m9 by a chain`;
  2. what it does: `teleport to x y z`, `door lock 2`, `turn`, `slide`, `relay`, `once`, `at end` or `moving`, and `> m10` for the mover it starts next; a passage's word;
  3. the game's own numbers (`Usable.what`, `.tag`): for Daggerfall's movers the action and its trigger (`what 14 tag 1`: a teleport walked into; `act_numbers` in `doors.c`), for a passage its way and building.

`useview` stays the use key's picture: what can be used, the aimed one bright.

## Switches

| | |
| --- | --- |
| `useview N` | Boxes and words for what can be used within N units (0 off, up to 4096; 1 to 63 hold nothing, so they turn it on at 512): cyan what moves, gold a passage, green a person. The one the use key would take now is bright, the rest dim; the nearest 48 only. F11 in LEVELFALL steps it 256, 512, 1024, off. |
| `uselist [n]` | How many of each there are, and the n nearest (10): index, word, kind, locked, where, how far, reach. |
| `locks 0` | A locked door opens as if it were not. Default 1. |

## Memory and cost

Per place, on its own heap: one `ModelAct` and a block byte per model slot (8192, about 270 KB), 2048 usables (130 KB) and 1024 movers (120 KB), and a work list while the doors are built. The light module adds two bytes per model and three per mover when the place has movers.

A use is one pass over the model boxes and the triangles of the few the ray crosses, once, on the key's edge. A frame without `useview` does nothing but move what is moving: a door is three sines and cosines and a 3×3 product. With `useview` on, the aim is cast each frame.

## Tests

hostcheck: `check_teleport` walks into a pad and is sent once, lands on the far pad and stays, steps off and on and is sent back, and uses a pad by hand only when it has a reach; Daggerfall's castle has its portal door and its other teleport, both to a marker in the castle at a body's eye. Also: the cast takes a door in front of the eye at its distance, not one out of reach or beside the view; a passage painted on a wall wins over the wall; a wall in front hides a door; a passage reaches the handler and nothing without one; a locked door stays and says so, and opens with locks off; a door is half open half way, turns a quarter about its hinge, leaves its doorway, and its sweep holds the turn; a chain starts together and a once mover stays. The hold: 58 doors and 2 levers, one locked, one way out of a door's size used from in front, a door that swings and shuts, a lever whose chain moves and refuses to start again while it moves. The Ashwing Farmstead: a building's door leads in; its inside loads with 6 doors and a way out by the street's door, facing in, with its own song; turned round on landing either side, the eye meets the door it came through. Privateer's Hold's outside has a way down, and the dungeon lies under it.

## Not done

- State that outlives a load: a door opened, a lever pulled, a creature killed stay so when the place is left and come back to, or when a dungeon is streamed by block (a block dropped out of range and read again from its file comes back as the file has it). The design: every mover, usable and entity gets a stable key from the game, Daggerfall Unity's LoadID (the block's place in the location plus the object's offset in its block file, `ModelAct.pos`, `Marker.pos`); the engine keeps a table, key to state (a mover's place along its motion, which end, its lock; an entity's life, place and facing), saved when a block goes (a mover in motion at the end it was going to) and put back when it fills, before it shows. The game gives the keys at build (`doors_build`, `add_foes`) and decides what else to keep; written to disk, the table is a save game. Chains and teleports stay within their block, so none is half loaded. Wanted before dungeons are streamed, or for save games, whichever comes first.
- Triggers by attack (5), and doors setting off their own actions by opening (10) beyond the chain. Walking into (1, 3, 9) is a box: a pad's or a wall's, not its triangles.
- What text, spells, traps, damage and the quest's actions do; flats with actions, a teleport's target among them only as a destination.
- Picking and bashing locks; a house locked by the hour or by its type.
- Doors pushing the body; the body caught in a swing is freed by `stuck`.
- Talking.
- GFALL's passages (the game layer).
- The temple's god, and a building's name.
- Pointing at a thing to inspect it (talked over, not built): a key picks what the middle of the view rests on, or a free cursor picks what is under it (the ray through that pixel, the camera's projection run backwards). The pick keeps what the cast already finds: the model and the face the ray struck (its ARCH3D id, block, texture archive and record, its RDB offset or RMB building), a door's mover, a passage's way, a sprite (a box per flat, which the ray passes through today), or the light or sound source nearest the ray. It is drawn bright, as `useview` draws, lives on the place's heap, and console commands act on it: `sel` to print it, `sel open`, `sel lock n`, `sel unlock`, `sel run`, `sel go`, `sel tex`, `sel light`, `sel sound`. Whether the middle of the view, a free cursor, or both, is the open question.
