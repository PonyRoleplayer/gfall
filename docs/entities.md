# Entities

Things that move about the world on their own, or stand where the game put them: townspeople, a dungeon's creatures; the player later. Read this before editing `src/engine/scene/entity.c`, `src/game/dagger/places/people.c`, `src/game/dagger/tables/enemies.c`, the creatures in `world.c`, the walking cells in `wild.c`, or the entity pass in `draw.c`.

The engine does engine things: here is an entity, where it stands, which way it faces, its pictures; it draws it turning as it faces. It does not know friend from foe, or what anything does. That is the game's: the townspeople's walking is `people.c`, a creature's kind is `enemies.c`, and what a creature does about the player is later game logic.

## What Daggerfall has

Two kinds of people stand in a town:

- **Static people.** An RMB block's "people" records: flats with a faction (`person_tag`), standing where the file puts them. They are already drawn, as flats that face the camera, and the use key says whose they are (`USE_TALK`, `docs/interact.md`).
- **Walking people.** None are in the files. The game makes them round the player, by day, and lets them go. This is what fills the streets, and what this note is about. The rules here are Daggerfall Unity's (`PopulationManager`, `MobilePersonMotor`, `MobilePersonBillboard`, `CityNavigation`), read for what they do, written our own way.

## An entity (engine)

`engine/scene/entity.h`: an entity is its feet (x, y, z in the scene's frame), its facing (`yaw`, the camera's sense: 0 looks along +z, a quarter turn along +x), two poses, and its place in the cycle of frames (`phase`). The game owns and moves its entities; the engine reads them once a frame to draw (`Scene.entities`, `Scene.nentities`; an entry with `live` 0 is skipped).

`radius` (0, as zeroed, passes through) makes it solid: the walker's body stops at it, and `entity_blocked(scene, self, x, z)` tells a motor whether a step would meet the body or another solid entity (`docs/collision.md`). Its height is its pose's front view (`entity_height`).

A pose (`EntityPose`) is five views: a picture slot each, each view's size in world units (a record is drawn at its own: a rat side on is twice as wide as from the front), a frame rate, and `right`, a bit a view whose picture faces the viewer's right. An entity has `ENTITY_POSES` of them, `pose[ENTITY_*]`:

| Pose | Shows | Daggerfall's records |
| --- | --- | --- |
| `ENTITY_WALK` | while `moving` is 1 | 0 to 4 |
| `ENTITY_IDLE` | while `moving` is 0 | 15 to 19, a townsperson's one |
| `ENTITY_ATTACK` | as the game's act | 5 to 9 |
| `ENTITY_HURT` | as the game's act | 10 to 14 |
| `ENTITY_CAST` | as the game's act | 20 to 24 (a spell, a bow's first shot) |
| `ENTITY_SHOOT` | as the game's act | 25 to 29 (a bow's second, 475, 489, 490) |
| `ENTITY_DEAD` | as the game's act | a corpse, one picture |

`act` (0 for none) is a pose the game shows instead of the walk or the stand, each frame it wants it; a pose not filled in (no width) is not shown, and the walk or the stand is. `hold` 1 stands the shown pose on `frame` of its sheet, the game stepping it (a swing played once, a body on its last frame); 0, as zeroed, runs its frames at its rate from `phase`. Which act, when, and for how long is the game's: the engine draws what it is told.

A pose of one picture (`slot[1]` 0xffff) faces the viewer whichever way the entity faces: a townsperson's idle picture, a body. An entity with no idle pose (`pose[ENTITY_IDLE].slot[0]` 0xffff) stands in its walking one: a bat, a ghost, a scorpion. `entity_init` zeroes an entity with no picture in any pose (every slot 0xffff); a zeroed one has picture 0 in all of them, which the card's marking of entity pictures would take from the place's walls (`docs/memory.md`). The townspeople and the creatures start with it. Creatures load their walk and stand today (`world.c`); their other records are the game's to reserve when it gives them motors.

### Its views

Daggerfall draws a walking body from five sides, each a record of its archive: toward the viewer (0), turned a little (1), side on (2), turned away (3), from behind (4), each turned to the viewer's left. The other three of the eight are those mirrored. Which one shows is the angle between where the entity faces and where the camera stands (`entity_view`):

- the yaw from the entity to the camera, and how far the entity's own yaw is turned from it, d, in −½..½ of a turn;
- record round(|d| / ⅛ turn), at most 4;
- mirrored when d is negative and the record is 1, 2 or 3: it faces the viewer's right;
- the other way round for a view whose `right` bit is set. Daggerfall drew a few that way: a rat's standing records 16 to 18, a ghost's and a wraith's side-on record 2 (DFU's `RatIdleAnims`, `GhostWraithMoveAnims`).

Facing the camera, record 0; a quarter turn to the camera's left, record 2; a quarter to its right, record 2 mirrored; away, record 4. A pose of one picture shows it facing the viewer, unmirrored, as a townsperson's idle record is drawn front on. `check_entities` (hostcheck) walks a camera round an entity facing north and checks every view and mirror, a standing pose that turns, each view's own size, a right-facing record, and an entity with no idle pose standing in its walk.

### Drawing

`entity_flat` turns an entity and a camera into a `Flat`: the view's picture slot, that view's size, mirrored or not (`Flat.flip`, which `draw_flat` honours by swapping the texels' left and right), standing on its feet (a flat's point is its middle), and its frames, a sheet of them (`texture_sheet`, `docs/sprites.md`) run at the pose's rate from the entity's own phase. A townsperson walks at 4 frames a second and stands at 1, as DFU runs them; a creature walks at 6, flies at 10, stands at 4 (EnemyBasics). `draw_entities` in `draw.c` draws them after the scene's sprites, each one within the visible far and in the frustum, in daylight's sprite colour outdoors. Where its pictures are on the card by distance (a streamed world, or a place loaded whole: `texcache_resident`) it notes each one's: the view shown at the entity's distance, the others four times farther.

## Townspeople (game)

`game/dagger/places/people.c`, in a streamed world (LEVELFALL's Stream World). A place loaded whole has none yet: its pictures go up to the card all at once at load (`texcache_upload`), and the people's 54 sheets would shrink every other picture of the town.

### Where they walk

Each block of a town is 64 × 64 walking cells of 64 units (its RMB's automap, 4096 bytes at +2251, row 0 north). A cell is walkable where the automap is 0 (no building, no prop), weighted by its ground tile (`tile_weight`, DFU's): a road 15, grass 12, dirt 6, stone 4, anything else 7, water and its edges 0. A chunk with a block makes its block's cells when it reads the block (`make_nav`, 4 KB in the chunk's heap). `wild_nav(wx, wz)` answers for any world point: the town whose pixel holds it, the block it is in, that block's chunk (`stream_chunk_data`), its cell. Out of town, or in a chunk not loaded, it is 0.

### Who comes and goes

DFU's population, ten times a second:

- Only a settlement has them: a city, a hamlet or a village (its MAPTABLE type, `worldmap_location_type`). A farm, a temple, a tavern, a wealthy or poor home, a graveyard, a coven or a dungeon's entrance has none; Daggerfall Unity also walks people round its farms, wealthy homes, temples and taverns. That is the game's rule; the engine has its own say over it, the console's `population` switch (`entity_population`): off, none come, and those about go once out of sight.
- A town is near when the camera is within 2500 of its rectangle. Its population is 24 for every 16 of its blocks, 1 to 4 sixteens: 24 to 96 (`PEOPLE_MAX`). Daggerfall has 96.
- By day (6:00 to 18:00), one at most a tick appears on a walking cell no one else stands on or walks to, within `PEOPLE_SPAWN` (6144) of the camera on each axis and `PEOPLE_RECYCLE` (6000) of it (the square's corners reach past that, and one there went the next frame), at the town's level (`wild_ground`).
- Landing (a stream opened, a door back out, `people_fill` at the end of `wild_fill`): the whole population at once, round the camera, ahead of it too, since nothing was seen yet. The town looks lived in from its first frame.
- One goes past `PEOPLE_RECYCLE` (6000), at dusk, out of town, or stuck (more than 4 seeks with nowhere to go).
- Never in sight: a person appears or goes only farther than `PEOPLE_POP` (4800) or behind the camera.

Who they are: a guard one time in 32 (archive 399: walking records 0 to 4, idle 15); else a man or a woman in one of four outfits of the climate's people. Redguards in the desert and the rainforest (381 to 384, 395 to 398), Nords in the mountains (387 to 390, 392, 393, 451, 452), Bretons elsewhere (385, 386, 391, 394, 453 to 456). Walking records 0 to 4, idle 5.

### How they walk

DFU's motor, cell to cell, north, south, east or west, at 52 units a second (1.3 m/s):

- At a cell it weighs the four next ones (0 for a cell another person stands on or walks to).
- Straight on when that is as good as here; else, four times in five, the best of the four; now and then (2.5%) a random turn instead. Nowhere to go counts a seek.
- A camera that stands still (under 8 units a second) within 100 (2.5 m): they stop, face it, and show their idle picture; when it moves, they walk on.

DFU also casts a ray to the next cell against walls; here the automap already marks every building and prop, so the cells are the walls.

### Their pictures

Each archive's six pictures are the stream's (`wres`, sprites, sheets of frames). A settlement's block chunks hold all of its climate's (`people_slots`: the four outfits of each gender and the guard's, 54 pictures) and read them with their own pictures, under the stream's budget, a part a call (`docs/streaming.md`, A chunk's contents): the first of a town's chunks to come in range, at the stream's edge, reads them, long before anyone is near. People hold the sets they wear too, and let them go once the camera has left the town and everyone has gone, so the RAM cap can free them when the chunks have gone as well. A set not read yet (population turned on in a town) is read a part a frame (`load_one`). A city of four outfits a gender and a guard is 54 sheets of 256 × 256: about 4 MB of pixels and, since cutout chains are kept from half size (`docs/memory.md`), 2.3 MB of mip levels.

On the card they are sprites, so they come first: the residency manager sharpens sprites before walls and takes room from farther walls for them (`docs/streaming.md`, The card). Every view of a person is its own picture. The draw notes the one shown at the person's distance and the others four times farther, two levels down, a sixteenth of the bytes: a view not noted is the first the card frees, and a turn landed on its 8 × 8 copy, a blob; all noted at full size, two people close took most of the card. A turn shows a quarter-size view for a frame or two, then the sharp one.

Sprites are point sampled (`spritepoint`, on by default), crisp texels as Daggerfall drew them: a townsperson is 110 texels tall and fills 300 pixels at arm's length, and bilinear filtering smeared each texel over three.

### With the stream

The people are local to the stream's frame: the origin's move moves them (`people_shift`, from the stream's shift). `people_update(cam, secs)` runs once a frame before the frame draws (LEVELFALL's stream hook), after the stream's update. Not while the world stands still (`gfx_paused`: the console is open, and the frame runs none of the world then): before, the people walked on behind the console and stood somewhere else when it shut. The stream's loading goes on under it. LEVELFALL's place panel shows how many walk now.

## A dungeon's creatures (game)

`game/dagger/tables/enemies.c` and the creatures in `world.c`, for a dungeon loaded whole (LEVELFALL's indoor rows, GFALL's hold). The rules are Daggerfall Unity's (`RDBLayout`, `EnemyBasics`, `RandomEncounters`, `DaggerfallMobileUnit`), classic's where DFU recreates classic.

### Where they stand

An RDB block's editor markers (archive 199, `docs/dungeons.md`) place them: record 16 a fixed creature, record 15 a random one. The marker's resource bytes say the rest:

- A fixed one's kind is the low byte of `id` (FactionOrMobileId; the high byte is garbage in Daggerfall's files). 99 is no creature. A woman of a class when `flags` has bit 1 and not bit 2, a man with bit 2, a coin with neither.
- A random one takes its kind from its slot (`flags`' low byte; 0 picks one of 1..6) in the dungeon's draws. Classic seeds its rand (Watcom's, as the face archives) with the location id, and draws 256 kinds from the dungeon's encounter table (by `Dungeon.dungeon_type`) and 256 from the underwater one. A draw is near the player's level four times in five, else from the table's start, so a level-1 player meets the first six of each table. There is no player yet: level 1 (`FOE_LEVEL`). A class's gender is a coin.
- A block's water level is its start marker's (record 10) sound byte, 8 units a step down, 0 for none. A random marker below it draws from the underwater table. A slaughterfish, a dreugh or a lamia is left out where there is no water, or its marker more than 20 above it.
- `action` 99 is passive: it attacks only when attacked. `world_entity_kind` reports it with the kind; what it means is the game's.

The kinds are Daggerfall's mobile ids: 0..42 monsters (archive 255 + id, but for the horse, 39, which has none, and the other dragonling, the dreugh and the lamia at 295..297), 128..146 people of a class (a man's archive and a woman's, 475..490; the city watch 399). The kinds and the encounter tables are records: the `ENEM` and `ENCT` tables of MAIN.DAT and of any mod over it (`docs/data.md`), with EnemyBasics' full stats. `enemies.c` holds the rules, and `classic.c` holds the numbers only for cooking MAIN.DAT. A mod's kind past 146 is placed like any other wherever a table or a fixed marker names it.

Privateer's Hold, a human stronghold, has 42: 14 rats, 6 giant bats, 3 imps, 2 bears, an orc, 3 skeletons, 4 rogues, 3 thieves, 6 archers. The draws are seeded by the location, so they are the same every load.

### Their pictures and poses

Walking records 0 to 4 at 6 frames a second (10 for a flyer); standing records 15 to 19 at 4, for a kind that has them. One that does not (an imp, a bat, a werewolf, a harpy, a wereboar, a ghost, a scorpion, a wraith, the dragonlings, the water dwellers) stands in its walking pose. DFU's exceptions: a woman of archive 483 (a thief, a bard, a burglar, an acrobat) stands turned a little in record 11, as her 16 is broken; the rat's and the ghost's mirrored records, above. Each record is a sprite picture in the place's table (`place_image`, `make_sheet`), sized as a flat is (`texture_extent`).

A record whose frames do not fit one 256 sheet stays on its first frame: 51 of the 725 creature records (the zombie's, the liches', the scorpion's, the dreugh's and the lamia's walks, a few views of the mages and spellswords). Half-size cells would fit them, at half the sharpness.

In a dungeon streamed by block (`dunstream.c`, which the game plays), each creature also has its acts: struck, records 10 to 14 at 10 frames a second (`ENTITY_HURT`, its five frames in half a second, mirrored as its walk), and its body, `corpse_archive`/`corpse_record` of its ENEM row, one picture facing every viewer (`ENTITY_DEAD`). Their pictures are held with the block's. A record that is missing leaves that pose empty, and the creature still stands. `dunstream_kind` names a streamed creature's kind and sex from its key. What the game does with them is `docs/game.md` (Combat).

On the card they are not sent at load: the hold's creatures wear 100 pictures, 8.7 MB at full size. They go up by distance from a reserve the load's budget leaves (`docs/memory.md`).

Nor are they read at load. A creature's archive is its whole TEXTURE file, and a class's is 300 to 440 KB, of which it wears ten records: the castle's creatures were 6 MB of its 7.5 MB of files and 2.3 s of its load on the 233 (the host's time ×40). At load each creature's picture slots are only reserved (`reserve_image`: its archive and record, no picture), and it is not drawn (`live` 0). `world_creatures_step(world, cam, radius, budget)` reads them, the nearest creature first: its file a 32 KB piece a call (`texture_file_step`), then each picture decoded, laid out as a sheet or padded, and put on the card by distance (`texcache_resident_add`); with all of them read, each view gets its size, it stands on its floor, and it is drawn. LEVELFALL and GFALL read those within a block (2048) of the landing before the first frame, then the rest a little a frame, in what the frame leaves (LEVELFALL's stream budget; GFALL 2 ms), while the world is not paused. The castle reads 38 of its 128 at the door (0.65 s on the 233, 3.4 MB); the world's own load went from 3.0 s to 0.7 s. The residency keeps its 512 KB for creatures still to come, whose sizes it cannot know yet.

### Standing

A flyer (an imp, a bat, a harpy, a dragonling) or a ghost or wraith hangs where its marker is, the marker its middle. The rest stand on the floor under the marker: a ray from 8 units above it, 120 down, against the place's models (DFU's `AlignControllerToGround`, 0.2 m and 3 m); with no floor there, the marker is its middle. Each faces one of eight ways, and starts its frames, by where it stands. They stand still (`moving` 0): what a creature does is later game logic.

`world_entity_count`, `world_entity`, `world_entity_kind`; `world_as_level` gives them to the scene. LEVELFALL's place panel shows how many.

## Tests

`check_stream` also lands at noon: 96 people out before any update, 54 pictures read by the chunks, some of them ahead of the camera in sight.

`check_entities` also: an attack shows its own pose and turns as the walk does, at its own rate; a hurt pose not filled in leaves the walk; a body faces the viewer and its held frame reaches the sprite; `entity_init` leaves no picture.

`check_entities` (engine): the eight views and their mirroring, a turned entity seen from the front, an idle one facing the viewer, a standing pose that turns, a right-facing record, no idle pose. `check_enemy_rules` (dagger): a fixed marker's mage, a woman, passive; kind 99 and the horse placed by none; a slaughterfish in a dry block, 30 above the water and at its surface; a level-1 crypt's random markers, dry and under water, within their tables' first six. `check_hold_creatures`: the hold a human stronghold, its 42 creatures and their draws, every one with its views and its kind's standing pictures, those that walk on a floor, every one drawn on its feet; the hold's picture test counts their 100 pictures apart from the load's budget. `check_stream` (dagger): Daggerfall is a city and the hold's outside no settlement; Daggerfall at noon fills to 96 people, every one on a walking cell at the town's level, drawn in its picture, walking; with population off they go and none come, and on again they come back; at night they go, out of sight, and none come.

## Not yet

- Static people as entities (they stand as flats: no turning).
- Using a walking person (talk) is the static people's only. Collision is done: an entity with a radius stops the walker (`docs/collision.md`), and the townspeople, 12 units round (`PERSON_RADIUS`), wait when the body or another person is in their next step (`entity_blocked`).
- People in a place loaded whole.
- What a creature does: friend or foe, its motor, its attack and spell records (5 to 9, 20 to 29). The game's. Struck and dead are built in a streamed dungeon; a place loaded whole (`world.c`) has neither yet.
- Creatures in a house (DFU's random encounters by building) or in the wilderness at night; a dungeon of a streamed world (an indoor row opened from Stream World is a place loaded whole, and has them).
- The player as an entity.
