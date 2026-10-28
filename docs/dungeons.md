# Dungeons

A place is a list of blocks, not a loose file. Indoors (`MAPDITEM`) are RDB blocks on 2048-unit tiles. Outdoors (`MAPPITEM`) are RMB blocks on 4096-unit tiles. GFALL still loads only the hold.

GFALL calls `world_load()`, which still loads only Privateer's Hold and still requires editor marker 10. LEVELFALL calls `dungeon_list` / `dungeon_load_at` / `world_load_dungeon`. Do not fold the viewer fallback into `world_load`.

## Where the list comes from

`MAPS.BSA` is a named BSA. Region `N` is `MAPDITEM.NNN` (`MAPDITEM.017` is region 17). There are 62 region files, `000` through `061`. Seventeen of them are 4 bytes and contain no dungeons (002–004, 006–008, 010, 012–015, 024, 025, 027–030).

This `ARENA2` tree has **4232** dungeons. `tools/check_dagger.c` asserts that count. The largest block list is 22, under `DUN_MAX_BLOCKS` (64). Privateer's Hold is region 17, location id 50050, 5 blocks, climate woodlands.

`dungeon_list` copies the name, region, record index, block count, and location id, with `outdoor` 0. It does not resolve climate. Call it once. On the viewer, wrap the scan in `mem_temp_begin` so the map bytes die after the names are copied out. There is no `mem_free`.

## Record layout

`MAPDITEM` body:

- `u32` count
- count entries of `u32` offset + 4 unused bytes
- records begin at `4 + count * 8`. Offset is relative to that base.

One record, after the door prefix:

- `u32` door count, then `door count * 6` bytes
- the location header starts at `at`
- name: 32 bytes at `at + 71`
- location id: `u16` at `at + 33`
- has parent: `u16` at `at + 39`. Parent location id: `u16` at `at + 41`. `Dungeon.exterior_id` and `DunRef.exterior_id` are the parent when there is one, else the record's own id: the outside it lies under. An outside's is its own id. LEVELFALL pairs a dungeon with its outside by it (`docs/interact.md`)
- dungeon header starts at `at + 112` (`LOC_TAIL`)
- block count: `u16` at `head + 10`
- blocks start at `head + 17`, 4 bytes each

One block:

- `x`, `z`: signed bytes, in tiles
- `u16` bits: letter `(bits >> 11)` over `N W L S B M`, number `bits & 0x3ff`, entrance flag `bits & 0x400`
- filename is that letter, seven digits, `.RDB`, inside `BLOCKS.BSA` (`S0000999.RDB`)

The dungeon's kind (`Dungeon.dungeon_type`, `DUN_TYPE_*`: crypt 0 … scorpion nest 17, cemetery 18, 255 none) is the byte at `+12` of its `MAPTABLE` row, after the type bits and the latitude: the same row the climate is read from (the outside it lies under), so it costs no second read of `MAPPITEM`. Privateer's Hold is a human stronghold (2). It picks the encounter table its random creature markers draw from (`docs/entities.md`). An outside's is `DUN_TYPE_NONE`.

`dungeon_load_at` fills a stack `Dungeon` (about 2KB) including climate. Do that inside `mem_temp_begin`, because climate reads the whole `MAPPITEM` for the region (region 17 is about 2MB) plus `MAPTABLE` and `CLIMATE.PAK`.

## Climate and textures

Climate is the map pixel of the exterior location in the **same region**. `MAPPITEM.NNN`, `MAPNAMES.NNN`, and `MAPTABLE.NNN` share `NNN` with `MAPDITEM.NNN`. Privateer's Hold stays on 017. If the dungeon has a parent, look up the parent location id; otherwise look up its own. `MAPTABLE` stores longitude in bits 8..24 of the word at offset 4, latitude in bits 8..23 of the word at offset 8. A pixel is those values divided by 128. Latitude is flipped (`499 - latitude/128`). `CLIMATE.PAK` is RLE rows. `climate_family` maps the ten climate ids onto texture families 0 / 100 / 300 / 400. Same-region lookup resolves all 4232 dungeons in this tree.

`world.c` `build_tex_table` seeds Watcom's rand with the location id and remaps face archives 119, 120, 122, 123, 124. A draw of 2 becomes 4. Doors (set 74, records 0..2), sewers (168), and caves (47, only when the record exists) use the same family. Sprites are not remapped. The hold's result is documented in `AGENTS.md`. Do not re-derive it; the hostcheck asserts archives 319 and 347.

## Start position

An RDB flat's point is its centre, an RMB flat's its base: `paint` lifts flats by half their height outdoors only (`docs/sprites.md`).

`rdb_load` keeps an RDB block's lights (object type 2) as `Light` records, and `world.c` shifts and appends them with the block's models and flats, up to 8192 for a place; the hold has 71. RMB blocks have none; for an outdoor place `world.c` makes one per `TEXTURE.210` flat after `paint`, and gives the place its sky (`docs/daylight.md`). The record layout is in `docs/lighting.md`.

`rdb_load` keeps each editor marker (archive 199) as a `Marker`: where it stands, its record, and the bytes of its flat resource after the picture, as Daggerfall Unity's `RdbFlatResource` reads them: `flags` (`u16` at `res + 2`), `id` (FactionOrMobileId, the `u16` at `res + 4`: the magnitude byte, then the sound byte), `action` (`res + 10`). Record 10's sound byte is its block's water level, 8 units a step down (0 none; the world's y is 8 × the byte, `docs/water.md`); records 15 and 16 are a random and a fixed creature (`docs/entities.md`).

`world_load` fails when the entrance block has no marker record 10. That marker is the new-game start in `S0000999.RDB` at about (1135, 1559, 496), yaw 0, pitch 0.

`world_load_dungeon` uses marker 10 when it is there. Otherwise the first marker on the entrance block. Otherwise the center of the mesh bounds, after `measure`. An outdoor place ignores markers and starts above the bounds, pitched down. Y is up. A floor is constant Y.

## Outdoors

`MAPNAMES.NNN` is a `u32` count and then that many 32-byte names. Two names in the game are exactly 32 bytes and have no NUL. `MAPPITEM` is not a `MAPDITEM`: the directory is `count * 4` byte offsets and the count comes from `MAPNAMES`, not from a header in `MAPPITEM`.

After the shared location header (`at + 112`) an exterior has a `u16` building count, 5 unknown bytes, `count * 26` building records, then the exterior data. Width and height are at `+40` and `+41` of that data. Letter 1 is at `+46`. The three 64-byte arrays (index, number, character) start at `+49`, `+113`, and `+177`. Grid index is `z * width + x`. This tree has **15251** such places. Daggerfall is region 17, 8 by 8, and block 0 is `WALLAA02.RMB`.

The RMB name is prefix + letter1 + letter2 + number + `.RMB`. Prefixes run `TVRN` through `WITC` (two `TEMP`s). Letter 1 is the location letter when bit `0x10` is set, otherwise `A`. Letter 2 is `A L M S` from `((character * 2) & 0xff) >> 6`. Temples (index 13 and 14) use a letter plus the raw number. Other blocks use at least two digits.

`rmb_load` reads the 6776-byte fixed header, each exterior subrecord, then the misc models and flats. A building's inside loads on its own, when its door is used (`rmb_load_interior`, `world_load_inside`, `docs/interact.md`). Each model keeps the building it belongs to (`ModelAct.record`), and each people-list flat is marked a person. A building is placed at `(X, 0, 4096 - Z)` with its model at `(X, -Y, Z)` in that frame. Yaw is the RDB 2048-unit circle, negated the same way as `place_model`. Props sit at `(X, -Y - 4, Z + 4096)`. Ground is 16 by 16 quads of 256, textured from climate family + 2 (`TEXTURE.002`, `102`, `302`, `402`). Tile bits `0x40` and `0x80` pick one of four orientations (none, 90 degrees, 180 degrees, the other 90). They are not a horizontal mirror. The camera starts above the bounds, pitched down.

`assemble` owns one `ArchEnt` table (`ARCH_CACHE`, 4096) and passes it into every `rmb_load` and `rdb_load`. A mesh is in model-local space, so two blocks that name the same ARCH3D id share one `Mesh`. Do not put that cache back inside a single block load. How the bytes are sized, and how the 2MB texture card is filled, is `docs/memory.md`.

## What will not open

A missing block name, a texture the climate file does not contain (except a cave record, which stays on the archive the face named), or more than 384 distinct images fails the open. The image table is 384 (`TEX_SLOTS`). Wayrest is 273 and used to die on the old table of 256. The viewer shows `world_error()` or `gfx_error()` and returns to the list. House interiors are not part of the outdoor load. A city whose full-size pictures are larger than the 2MB texture card still opens: far faces stay at 16 by 16. See `docs/memory.md`.
