# Data: MAIN.DAT and the .DAT files over it

The game's tables are records in small binary files, not C arrays. The first run cooks Daggerfall's own tables into `MAIN.DAT`. Other `.DAT` files, called mods, extend or override them in a load order. Models, pictures, blocks, sounds and songs stay in ARENA2 and are read there as before.

Read this before editing `src/engine/assets/recfile.c`, `src/game/dagger/tables/maindat.c`, `classic.c`, a table's schema (`enemies.c`, `forecast.c`), `apps/dfrec/`, or `apps/datfall/`.

## The rule

- **Daggerfall's files are read in place and never copied.** MAIN.DAT is made on the player's machine from their own ARENA2, plus the tables that classic keeps inside FALL.EXE. Nothing from ARENA2 goes into the repo or onto the disc.
- **Game logic reads records, never C tables.** Only the importer (`classic.c`) holds classic's numbers. The game reads `enemy_kind(id)` and the other record lookups.
- **The engine owns how things work; the data supplies the numbers.** The engine keeps defaults and takes a game's numbers through setters (`weather_set_look`, `weather_set_odds`). The game hands them over from its tables (`maindat_apply`). See `docs/architecture.md`.

## The file (`recfile.h`)

A file is `GDAT`, a u32 format version (1), then chunks until the end of the file. Each chunk is a 4-character tag, a u32 size, the data, and a pad byte to make the size even, as RIFF lays them out.

A record chunk describes itself:

```
u16 1, u16 fields, u16 row size, u16 0, u32 rows
per field (24 bytes): char name[16], u8 type, u8 size, u16 count, u16 offset, u16 0
rows: each row size bytes, little-endian
```

- **Field types:** `i` signed and `u` unsigned integers of 1, 2 or 4 bytes, `f` a 4-byte float, `s` text of a fixed size. A number field may be an array (`count` more than 1).
- **Field 0 is the id**, an integer. Rows are keyed by it.
- **The game describes each table once**, as a `RecSchema` over its own row struct (`REC_FIELD`, `REC_ARRAY`). Writing, merging and dfrec's text all use that schema, so they can't drift apart.

### Merging (`rec_merge`)

A chunk merges into the table that has its tag.
- **A new id adds a row:** the schema's defaults (`RecSchema.defaults`, or zeros), then the file's fields.
- **An existing id takes only the fields the chunk carries.** A mod's chunk with just `id` and `health_min` changes the rat's least health and nothing else.
- **Fields are matched by name.** A file's field the schema lacks is skipped and counted. Numbers convert among integer types and floats, and are clamped to the schema's range. Text is cut to fit. Text never goes into a number, or the other way round.
- **An array takes the elements the file carries**, from the start; the rest keep their values.
- **A chunk that is damaged is refused whole:** one that runs past the file, has a field outside its row, or uses an unknown type.

A table grows by doubling, and its old rows stay in `mem_keep`. That is fine because tables are small and built once at start.

**For load time only.** `rec_row` is a linear search by id, and a row's address can move while files are still merging. Look rows up while a place loads (`enemy_kind` for each creature placed), not in a frame's loop. Code that needs a row every frame copies it into its own arrays once the merge is done.

### HEAD

Every file starts with a `HEAD` chunk (`rec_head_schema`): rows of role, size and name.

| Role | Name | Size |
| --- | --- | --- |
| `SELF` | this file | the version of what made it (for MAIN.DAT, `MAINDAT_VERSION`) |
| `SOURCE` | an ARENA2 file it was made from | that file's size then |
| `NEEDS` | a file that must be merged before this one | 0 |

## The tables

| Tag | Name in text | Row | Owner | What |
| --- | --- | --- | --- | --- |
| `ENEM` | `enemy` | `EnemyKind` | `enemies.c` | creature kinds by mobile id: EnemyBasics field for field, and `attributes`, the eight attributes cooked from each monster's `ENEMY###.CFG` in MONSTER.BSA and each class enemy's `CLASS##.CFG` (DFU's GetMonsterCareerTemplate; a mod's kind without them has 50 each) |
| `ENCT` | `encounter` | `Encounter` | `enemies.c` | encounter tables by dungeon type (`DUN_TYPE_*`), plus `ENCOUNTER_UNDERWATER` (19) |
| `WTHR` | `weather` | `WeatherOdds` | `forecast.c` | weather odds; id = climate × 4 + season |
| `WLOK` | `look` | `WeatherKindLook` | `forecast.c` | each weather kind's look (`WeatherLook`) |
| `CMON` | `month` | `CalMonth` | `calendar.c` | the calendar's months by index: name, days, season (`CLOCK_*`) |
| `CWEK` | `weekday` | `CalWeekday` | `calendar.c` | its weekdays by index: name |
| `CALE` | `calendar` | `CalInfo` | `calendar.c` | one record, id 0: era, whether each month restarts the week, the start date |
| `CLAS` | `class` | `CareerClass` | `careers.c` | the 18 classes, cooked from ARENA2's CLASS00..17.CFG: base attributes, skills, hit points a level, spell points an Intelligence point, resistances |
| `RACE` | `race` | `CareerRace` | `careers.c` | the 8 races by DFU's ids 1..8: names, the paper doll's pictures, resistances |
| `SKIL` | `skill` | `CareerSkill` | `careers.c` | the 35 skills: name, governing attribute, how hard to advance |
| `ITEM` | `item` | `ItemTemplate` | `items.c` | item templates by DFU's ItemTemplates index: name, weight, price, condition, the ground's and the paper doll's pictures, its draw order. The 11 pieces of armour 102..112, the 19 weapons 113..131 and gold 276, for now |
| `WEAP` | `weapon` | `WeaponTemplate` | `items.c` | what a swung weapon adds, by the same id: its damage range, its skill, its `WEAPON##.CIF`. 18 rows: arrows have none |
| `ARMR` | `armor` | `ArmorTemplate` | `items.c` | what an armour template adds, by the same id: the body part it covers, a shield's value and the parts it covers, which paper doll variants it shows in plate, leather and chain |
| `MATL` | `material` | `Material` | `items.c` | the 12 materials, iron 0 to daedric 9, then leather and chain, armour's alone: the damage modifier (ten times it to hit), the weight, value and condition multipliers, and the armour a piece in it gives |
| `LOOT` | `loot` | `LootChance` | `items.c` | classic's 21 loot tables, by letter 'A'..'U': gold's range and each kind of thing's chance. Only gold, weapons and armour are rolled yet |
| `LDUN` | `dungeon_loot` | `DungeonLoot` | `items.c` | which loot table each of the 19 dungeon types fills its treasure from |
| `BLDG` | `building` | `BuildingKind` | `commerce.c` | a building type: its name in talk, its opening hours, its shelves' chance numbers for weapons and armour |
| `CRIM` | `crime` | `CrimeKind` | `commerce.c` | a crime: what the court calls it, what it costs the legal reputation, its sentence's sums |
| `DISE` | `disease` | `Disease` | `diseases.c` | a disease by DFU's Diseases number: its name, each attribute's, health's, fatigue's and spell points' weight, the day's damage range, and its days of symptoms (255: until cured) |
| `GILD` | `guild` | `GuildKind` | `guilds.c` | a guild one can join, by its faction (41 Fighters, 40 Mages, a temple by its god, a knightly order by its own): its group, its ten rank titles (`rank0`..`rank9`), the skills its ranks ask for and those it trains (255 ends a list), what it gives members, the rank its healing comes at, and TEXT.RSC's words for joining and rising |
| `NAME` | `name` | `NamePiece` | `names.c` | a piece of a person's name, cooked from ARENA2's NAMEGEN.DAT: its bank (a race's, or monsters'), its set, its text; row id bank × 512 + set × 64 + place. A mod adds pieces by adding rows from place 0 on |
| `HOLI` | `holiday` | `Holiday` | `commerce.c` | a holiday every city keeps, by row id month × 30 + day (from 1): its name and flags (shops shut, prices halved, weapons halved, temple cure, temple half, free room, free drink) |

`maindat.c`'s `table_at` lists the tables a `.DAT` may carry. A new table means:
1. its row struct and `RecSchema` in the module that uses it;
2. a line in `table_at` and its count;
3. its rows in `maindat_import`;
4. a test.

MAIN.DAT is about 15 KB today (`MAINDAT_VERSION` 4).

**A field's name is under 16 characters**, or a chunk cuts it and it never matches; `check_data` checks every schema's names. The tables planned next, with their sources and order, are in `docs/game.md` (Data): the location index and world map, climates, items, weapons, spells, races, classes, factions, the calendar.

### What classic's tables are

`classic.c` holds what classic keeps inside FALL.EXE, as Daggerfall Unity carries it (MIT-licensed; the copyright line is in the file):
- **EnemyBasics:** 62 kinds. A field DFU leaves out is C#'s default, 0. Names are DFU's, except 39 "Horse" and 146 "City Watch".
- **RandomEncounters:** 20 tables of 20 kinds; the cemetery uses 11.
- **The Chronicles' weather odds** (p. 47): six climate groups, expanded at import into one record for each of the 10 climate ids and 4 seasons.

- **RaceTemplate's eight races and the 35 skills** (DFCareer, DaggerfallSkills). The classes are not classic's tables from the exe: they are cooked from ARENA2's CLASS*.CFG.
- **Tamriel's calendar**, from DaggerfallDateTime: twelve months of thirty days with their seasons, the seven weekdays that restart each month, and a new game on the 1st of Midyear, 3E 405.

The weather looks are the engine's own defaults, read with `weather_look` when cooking, so the picture is the same until a mod changes them.

**The calendar** goes to the engine as one `Calendar` (`calendar_apply`, `clock_set_calendar`). A record table holds no list of names inside a record, so the months and weekdays are tables of their own, read from id 0 up to the first id missing. The calendar is installed only when it differs from the one in use, so without a calendar mod the clock is never moved; LEVELFALL's `date=` and `time=` from its cfg stay. A different calendar moves the clock to its start date, keeping the time of day.

## The first run, and when it runs again (`maindat_start`)

1. GFALL and LEVELFALL call `maindat_start` after `win_startup` and before any place loads.
2. It reads `DATA\MAIN.DAT` beside the exe (`vfs_read_beside`). `DATA\` is made when missing (`fs_make_dir`).
3. If the file is missing or stale, it cooks it again (`fs_create`) and reads it back. *Stale* means one of:
   - another `MAINDAT_VERSION`;
   - a `SOURCE` whose size has changed (MONSTER.BSA and CLASS00..17.CFG today);
   - not a record file.
4. It merges MAIN.DAT alone and checks it is whole (`maindat_whole`): every table has records, the calendar has its record, and no chunk was refused as damaged. A file that is current but damaged inside (a bad disk, a copy cut short) is cooked again once, quietly.
5. It merges each name in `DATA\LOADORDR.TXT`, read from `DATA\`:
   - one name per line; blank lines and `#` lines are skipped;
   - MAIN.DAT is skipped if listed, since it always goes first;
   - a missing file is a warning in the log.

   A file whose `NEEDS` hasn't been merged yet is skipped whole, with a log line saying why.
6. `maindat_apply` hands the engine the values it takes from data: the weather looks and the calendar.

**When it can't go on**, `maindat_start` returns 0 and `maindat_error` says what a person should do. GFALL shows it in a message box and stops; LEVELFALL logs it and goes on without the tables.
- **MAIN.DAT can't be written**, for example when the game runs from a CD or a read-only folder: run it from a folder it may write to, with room on the disk.
- **It is damaged even as cooked:** delete `DATA\MAIN.DAT` and start again, and it is made anew from ARENA2. If it comes back, check ARENA2 and the disk.

A damaged mod never stops the game: it is skipped, and the log says why.

Mods never cause a re-cook; they are merged at every start. Cooking reads no large file today and takes milliseconds. When the location index and world map come, it will take seconds on the 233, and it gets a held screen (`gfx_hold`).

**Where the files go:** `DATA\` beside the exe holds MAIN.DAT, `LOADORDR.TXT` and every mod.

**hostcheck** cooks MAIN.DAT into memory (`check_data`) and merges it before any dungeon loads. It never writes into the game data's folder.

## The tool: dfrec

`apps/dfrec/main.c` is a native program, built by `make hostcheck` as `build/dfrec` from the same code as hostcheck.

| Command | Does |
| --- | --- |
| `dfrec import ARENA2 OUT.DAT` | cooks MAIN.DAT |
| `dfrec dump FILE.DAT [TAG]` | prints every record as text |
| `dfrec build IN.TXT OUT.DAT` | turns text into a `.DAT` that carries only the fields the text sets |
| `dfrec same A.DAT B.DAT` | prints `same`, and exits 0, when both merge to the same records |

The text a dump prints is the text `build` reads:

```
needs = MAIN.DAT          # before any section: what must load first
[enemy 200]               # a table's name or tag, and an id
name = Goblin             # text runs to the end of the line
male = 262
damage_max = 6, 0, 0      # a list fills an array from its start
[enemy 0]
health_min = 30           # the rat: this field alone
```

- `#` starts a comment line.
- A section repeated for the same id adds to it.
- `build` writes one chunk for each table and set of fields, so each record carries only what its text named.
- An error names the file and line (`goblins.txt:4: no such field: helth_max`), and nothing is written.
- Out-of-range numbers are errors in `build`; they are clamped only when merging.

Changing a creature: dump it, edit the text, build a mod into `DATA\`, and add its name to `DATA\LOADORDR.TXT`. The example is `tests/data/goblins.txt`.

`make hostcheck` runs `datacheck`, which:
- cooks, dumps, builds again, and checks `same`;
- builds the example;
- checks that `tests/data/misspelled.txt` fails on its line 4.

## The viewer: DATFALL

`DATFALL.EXE` (`apps/datfall/main.c`) is a read-only construction set. The title bar and the message boxes say cd-gfall. It is not dfrec, and it does not write `.DAT` files. The window is a normal overlapped window, opened maximized on Commons, with the system button face and MS Sans Serif. Characters is selected, the race list is showing, the first race's paper doll is in the well, and Find has the keyboard. `comctl32.dll` is loaded at run time (`InitCommonControls`) for a `SysTabControl32`, a `SysTreeView32`, two `SysListView32`s and a status bar. GDI is loaded the same way for the font. Neither is a DLL import. The executable imports only `KERNEL32.dll` and `USER32.dll`. It does not use Glide. Two tabs, **Commons** and **World**. `WM_SIZE` reflows whichever tab is showing, maximized or not. Escape quits. F5 selects Commons and F6 selects World. Ctrl+F focuses Find. Ctrl+C copies the selected grid value, or the selected list row's name when the grid has no selection.

The first run cooks `DATA\MAIN.DAT` (`maindat_start`), then the browser reads that file and every name in `DATA\LOADORDR.TXT`. A missing mod is shown and skipped. The status bar is the file on screen, the selected record's id, and the names that merged.

- **Commons.** Data, Pictures, Characters, Creatures and Items. Left, the tree. Middle, Find and the record list. Right, the preview and the field grid.

- **Data.** One tree node per chunk of the merged book, and the same again under each file. The merge (`books.c`) does not use the game's C structs: rows are keyed by field 0, a later file overlays the fields it carries, and a new field or a new table is kept. The middle list is the records (id and the first text field). The grid is every field, by name and value. An array is one line, elements joined by commas. The Find box filters the list. Up and down move it.

`make datfall-check` is the same PE check as `make gfall-check`, then a host program with no window. It cooks a MAIN.DAT, prints every merged chunk and its field names, and checks that a mod setting the rat's health and adding a `note` field, plus a new `NOTE` table, shows up in the merge while the rat's name stays. It also checks three pictures from `ARENA2`, the readers the later views use: Breton's body, the rat's front frame, and a dwarven recolour of `WEAPON00.CIF`.

- **Pictures.** Every `TEXTURE.###` in ARENA2, including `TEXTURE.000` and `TEXTURE.001` (solid colours; the engine's record count skips those two). The tree node is the archive. The list is its records, with the size and the frame count. The well is a grid of thumbnails and, under it, the selected record drawn large. Index 0 is a checker. A 160 ms timer advances a record that has more than one frame. Left and right step one frame. Clicking a thumbnail selects that record. `make datfall-check` expects 472 archives and 20 records in `TEXTURE.255`, whose front frame is 65×67.

- **Characters.** The eight `RACE` rows, then Classes and Skills. A class lists Primary, Major and Minor, each skill by its `SKIL` name. The well names the class, the hit points a level and the spell points an Intelligence point gives. There is no class picture. Skills list the name and the governing attribute. Enter on a class's skill opens that skill. The race list is the paper doll: background, body, clothed body, the female pair, and the two head files. Those names are the record's own fields (`background`, `body_male`, `clothed_male`, `body_female`, `clothed_female`, `heads_male`, `heads_female`), so a mod that points a race at another picture shows that picture. An `.IMG` is drawn large. A `.CIF` is a strip of every frame along the top and the selected head large under it. Click a head, or press left and right, to pick one. The grid is the race's fields. `make datfall-check` expects Breton's `heads_male` to be `FACE00I0.CIF` and that file to hold 10 heads.

- **Creatures.** Every `ENEM` row. The list is its poses: Walk (records 0..4), Attack (5..9), Hurt (10..14), Stand (15..19, only when `has_idle` is set) and Corpse (`corpse_archive` / `corpse_record`, one picture). Each of the five views is a thumbnail; the selected one is drawn large and advanced by the same 160 ms timer as a texture. Click a thumbnail to change view. Left and right step a frame. Archive 0 (the horse) has no picture. The grid is the kind's fields, attributes included. `make datfall-check` expects the rat's walk on `TEXTURE.255` records 0..4, its stand on 15..19, and its corpse on `TEXTURE.401` record 1.

- **Items.** Every `WEAP` row, named from its `ITEM`, then every `ARMR` row. A weapon's list is the ten metals. The well is that weapon's `WEAPON##.CIF`, recoloured by `cif_metal` (steel is the picture as drawn). The frames animate, and left, right or a click picks one. The grid is the template, the weapon and the material, then the made item (a steel longsword is 5.50 kg, value 90, condition 1200). Armour's list adds leather and chain, which are not recoloured. The well is that one piece on a Breton man (archive 251, `player_record` plus `plate_low` for iron) or a Breton woman. Man and Woman sit under the well. Left and right step the variant. An iron cuirass is 12.50 kg, value 300, condition 4096.

- **World.** The other tab. The map is the large view on the left, one picture built once and blitted: height-shaded, sea where `WOODS.WLD`'s byte is 1 or less, optional climate tint, optional location dots coloured by type. Keys `1`, `2` and `4` zoom. Arrows scroll when the map has the keyboard. Holding the middle mouse button on the map and dragging moves the map under the cursor, and releasing it stops. The side panel on the right is the locations: Find, every dungeon and town, and the selected place's name, region, climate, type, pixel and block grid. A dungeon adds its type in words, the line Above when it has an outside, and that type's encounter kinds. A town adds Below when a dungeon sits under it. Enter on that line selects the other place. Enter on a kind opens the creature. The place list's columns are Name, Kind and Region. Every list paints only the rows on screen. The grid's third column is Means: a creature, skill, attribute, season, or texture the browser knows how to name. The value stays the record. An encounter row's well is that table's first kind, one front frame. Enter on a Means cell that names a creature, a skill, or a texture opens it. Find filters the selected node's list. When that list has no row and the box holds two or more characters, the list becomes hits from the whole set, columns Where and Name. Enter commits the hit. Escape clears the hits and restores the node. A one-character box only filters the current list. Clicking a location centres the map. Clicking the map reports height, climate and the location there, and selects that place in the panel. `make datfall-check` expects a 1000×500 map, Daggerfall's pixel 207,213 in the woodlands at height 20 as a city, 4232 dungeons and 15251 outdoor places.

`WEAPON09.CIF` is the bow (no armed picture). The others start with the armed picture, then the swings. `cif.c` is the reader. Each frame keeps where classic draws it on its 320 × 200 screen (`CifFrame.x`, `y`: an IMG's header, a swing's bytes 6 and 8; byte 4 is the width again), and the game draws the weapon there (`docs/game.md`, The weapon on the screen). The armed picture is drawn in the sixteen ART_PAL indices from 0x70, which are steel as they stand. `cif_metal` swaps them by classic's own table for each metal, as Daggerfall Unity's ImageProcessing.GetMetalColorTable carries them.

Index 0 is clear. A cutout is drawn as a checker.

## Tests

- **`check_engine.c`, `check_recfile`:**
  - rows written and merged back the same;
  - a chunk of some fields changes only those;
  - another program's wider types clamped, converted to float, cut;
  - an unknown field skipped;
  - damaged chunks refused.
- **`check_dagger.c`, `check_data`:**
  - MAIN.DAT cooked and not stale;
  - 62 kinds, 20 tables, 40 odds and 7 looks;
  - 12 months and 7 weekdays, equal byte for byte to the engine's own Tamriel (`clock_calendar_default`);
  - the rat is EnemyBasics' rat;
  - the human stronghold's table and the cemetery's 11 are classic's;
  - the fog look is the engine's own;
  - another version or another MONSTER.BSA size is stale;
  - a mod adds kind 200 over the defaults, changes only the rat's least health, only the fog, and only Midyear's name, which reaches the clock;
  - a later file wins the field both set;
  - a file whose need is missing is skipped.
- hostcheck's lines for the hold's 42 creatures and their draws are unchanged.

## Not yet

- **The other chunks**, in the order `docs/game.md` gives:
  1. the location index, world map and climates;
  2. items and weapons;
  3. the character tables;
  4. people and society;
  5. text and quests.
- **MONSTER.BSA's `ENEMY###.CFG` names and stats**, read once their layout is confirmed against UESP.
- **A mod's pictures over ARENA2** (`TEXTURE.600` beside a mod). This needs the VFS to search several roots, an engine request.
- A Win98 console build of dfrec.
- **Removing records:** a mod can add and change records, but not delete them yet.
