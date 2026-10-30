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

`DATFALL.EXE` (`apps/datfall/main.c`) is a Windows 98 browser of races, enemies and weapons. It is not dfrec, and it is not a chunk editor. It reads. The window is a normal overlapped window, opened maximized, with the system button face, MS Sans Serif, radio buttons, a drop-down, a `SysListView32` and a picture well. The list and the picture are moved in `WM_SIZE`, so a resize reflows them. `comctl32.dll` is loaded at run time for the list (`InitCommonControls`), and GDI the same way for the font and `StretchDIBits`. Neither is a DLL import. The executable imports only `KERNEL32.dll` and `USER32.dll`. It does not use Glide.

`make datfall-check` is the same PE check as `make gfall-check`, then a host program with no window. It still cooks a MAIN.DAT and checks the chunks. It also checks three pictures from `ARENA2`: Breton's body, the rat's front frame, and a dwarven recolour of `WEAPON00.CIF`.

The three views, one at a time:

- **Races.** The eight races from `classic_races` (RaceTemplate, the `RACE` records). The drop-down picks the paper-doll file that race names: `SCBG##I0.IMG` (background), `BODY##I0.IMG` and `BODY##I1.IMG` (body and clothed, male then female), `FACE##I0.CIF` and `FACE##I0`'s female twin. IMG files go through `img_load`. A head is a CIF of several pictures, read by `formats/cif.c`.
- **Enemies.** Every kind in `classic_enemies` that has pictures (the horse has none). The archive is that kind's `male` field (`TEXTURE.255` for the rat). The drop-down is the view: walking records 0..4, and standing records 15..19 when `has_idle` is set. A frame is `texture_load_frame`. The rat's front record is 8 frames, 65×67.
- **Weapons.** Every `WEAPON##.CIF` and `WEAPO1##.CIF` in ARENA2. `WEAPON09.CIF` is the bow (it has no armed picture). The others start with the armed picture, an IMG, then the swings. `cif.c` is the reader: there was not one. Each frame keeps where classic draws it on its 320 × 200 screen (`CifFrame.x`, `y`: an IMG's header, a swing's bytes 6 and 8; byte 4 is the width again), and the game draws the weapon there (`docs/game.md`, The weapon on the screen). The drop-down is the material. The armed picture is drawn in the sixteen ART_PAL indices from 0x70, which are steel as they stand. `cif_metal` swaps them by classic's own table for each metal, as Daggerfall Unity's ImageProcessing.GetMetalColorTable carries them.

Index 0 is drawn as a checker so a cutout reads as empty. A timer (160 ms) advances a picture that has more than one frame. Left and right, while the list has focus, step one frame and leave the timer running. Up and down move the list. Escape quits.

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
