# cd-gfall

The construction set a person uses to look through gfall's data. It is the program that already lives in `apps/datfall/`. This page is the spec for growing it. The window, the readers, and the host check that exist today stay. The work is the lookup, the plain names, and the pictures that are still missing when a person arrives.

Read `docs/data.md` (the file, the tables, the viewer) and `docs/architecture.md` (DATFALL links no play layer and no Glide) before editing. `docs/game.md` is only where a table's numbers come from: weapons and armour, classes and skills, encounters.

## The name, and the one program

The title bar and every message box say **cd-gfall**.

The file on disk stays **DATFALL.EXE**. `make datfall-check` still builds `build/DATFALL.EXE` and `build/datfall-host`. The disc still carries `DATFALL.EXE`. The window class registered with the system can stay `DATFALL`; that string is not shown.

One program. New files go in `apps/datfall/`, one `.c` and one `.h` of the same name, beside `view`, `books`, `paper`, `beast`, `made`, `pics`, and `atlas`. Do not start a second app, a second makefile target, or a second tree of readers. Do not include anything from `src/game/dagger/play/`. Do not link Glide.

## Who it is for, and the first thing they do

A person who has the game beside the program (`ARENA2`, as the exe already requires) and wants to see a race, a creature, a weapon, a table row, or a place. They know the names. They do not know tags, archive numbers, or which tree to open.

The window opens maximized on **Commons**. The tree has selected **Characters**, the race list is showing, and the first race's paper doll is already in the well (Breton, when the table is classic's). **Find** has the keyboard. The status bar reads the merged files, as it does now.

They type. A name that is in the list in front of them filters that list, and the picture follows the row. A name that is not in that list fills the list with hits from the whole set, and the picture of the first hit is already in the well. Enter opens that hit for good.

## What already stands

Keep all of this. Later slices add to it.

The window is a normal overlapped window, opened maximized, system colours, MS Sans Serif. `comctl32.dll` and GDI load at run time. The import table is only `KERNEL32.dll` and `USER32.dll`. No Glide.

On startup the program cooks `DATA\MAIN.DAT` (`maindat_start`) and reads that file plus every name in `DATA\LOADORDR.TXT` (`books.c`). A missing mod is listed and skipped. There is one merged book (`books_merged`). Rows are keyed by field 0. A later file overlays the fields it carries. A new field or a new table is kept. The game's C structs are not the browser's copy.

**Commons** is a tree, then Find and a list, then a well over a field grid.

- **Data.** One node per chunk of the merged book, and the same under each file. The list is the records (id and the first text field). The grid is every field. An array is one line, elements joined by commas. Find filters the list.
- **Pictures.** Every `TEXTURE.###`, including `TEXTURE.000` and `TEXTURE.001` (`pics.c`, `tex_list`). The list is the records. The well is a grid of thumbnails and the selected record drawn large. Index 0 is a checker. A 160 ms timer advances a record with more than one frame.
- **Characters.** Each `RACE` row (`paper.c`). The list is the paper doll's files, from the row's own fields. An `.IMG` is drawn large (`img.c`). A `.CIF` is a strip of heads and the selected head large (`cif.c`).
- **Creatures.** Each `ENEM` row (`beast.c`). Walk, attack, hurt, stand when `has_idle` is set, and the corpse. Five view thumbnails, the selected one large, same timer. Archive 0 has no picture.
- **Items.** Each `WEAP` row, named from `ITEM` (`made.c`). The list is the `MATL` rows. The well is `WEAPON##.CIF`, recoloured by `cif_metal` for a metal. The grid ends with the made weight, value, and condition.

**World** is the other tab. The map is one picture built by `atlas_build` and blitted (`worldmap.c`: `WOODS.WLD`, `CLIMATE.PAK`). Sea is a byte of 1 or less. Climate tint and location dots are optional. Keys `1`, `2`, and `4` zoom. The middle button drags the map. The right side is Find, the places (`dungeon_list`, `exterior_list`), the selected place, and its block grid. A click on a place centres the map. A click on the map reports height, climate, and the place there.

Escape quits. F6 selects World. `WM_SIZE` reflows the tab that is showing.

## The window

Two tabs, **Commons** and **World**. No third tab. No floating tool window. No 3D view.

Commons keeps three panes:

- Left, the tree, about 220 pixels.
- Middle, the word Find, the box, and the list.
- Right, the well (about two fifths of the height) and the field grid under it.

World keeps the map large on the left and the places on the right. Under the map, the same three buttons: climate tint, location dots, centre. The middle button still pans, and nothing else uses that button. The map is still one device-independent bitmap, built when the tint or the dots change, and blitted for a pan or a zoom.

The place list's columns read **Name**, **Kind**, **Region**, in that order. Kind is the word Dungeon or Town. Region is the region number. (The headers on the list today do not match the cells. Fix that when the list becomes virtual.)

The field grid gains one column, **Means**, on the slices that define it. The value column stays the record's own text. Means is a name beside it, or blank when the field is already plain.

The status bar stays three parts: what is on screen, the selected id, and the names that merged.

Message boxes that today say DATFALL say cd-gfall. The failures stay the ones the program already reports: controls did not load, ARENA2 was not found, the tables did not load.

## Lookup

This is the feature the window is for. It lives in a new pair `apps/datfall/look.c` and `look.h`. It calls no Win32. It reads the merged `DatFile`, the place arrays the window already keeps, and the texture-archive list. It does not open the files again and it does not build a second copy of the rows.

### What the person does

Find filters the list of the tree node that is selected, as it does now, on every change of the box.

- While that list has any row, the first row is selected and its picture is in the well. Typing `br` on the races leaves the person on Breton. This is the path they already have.
- When the list has no row and the box holds at least two characters, the middle list becomes the hits from the whole set. Columns are **Where** and **Name**. The first hit is selected. Up and down move it, and the well follows.
- Enter or a double-click commits the selected hit: the tree selects that node, or World selects that place, and the list becomes that node's own list with the same Find text.
- Escape while the hit list is up clears Find and restores the tree selection. It does not quit. Escape with no hit list quits, as now.
- An empty box restores the current node's full list. It does not search.

A place hit does not switch tabs while the person is still typing. The well names the place, its kind, its region, and its climate (`atlas_climate_name`, `atlas_type_name`). Enter commits, World is selected, and the map centres on that place's pixel (`worldmap_pixel_of`). The bitmap is the one already built.

A race, creature, weapon, armour, class, or picture hit draws in the well at once, on Commons, before Enter. A table-row hit shows that row's grid, and the well says the table's name (`dat_kind`'s table name) and the row's label.

### What is searched

Case is folded. An exact name comes before a name that merely contains the text. Inside one rank the order is: Race, Creature, Weapon, Armour, Class, Skill, Place, Table, Picture.

| Where | Matched on | Opens |
| --- | --- | --- |
| Race | `RACE` row label (`dat_row_label`) | Characters, that race, the paper doll |
| Creature | `ENEM` row label | Creatures, that kind, the walk pose, or no picture when the archive is 0 |
| Weapon | `ITEM` name of a `WEAP` row (`made_weapon_at`) | Items, that weapon, iron's CIF if a material is not already chosen |
| Armour | `ITEM` name of an `ARMR` row | Items, that piece, once the armour slice exists. Until then a weapon-only search is correct |
| Class | `CLAS` row label | Characters, that class, once the class slice exists |
| Skill | `SKIL` row label | Characters, that skill, once the skill slice exists |
| Place | `DunRef.name` from `dungeon_list` and `exterior_list` | World, that place, map centred |
| Table | a merged row's label, or a chunk's tag or table name | Data, that chunk, that row when a row matched |
| Picture | `TEXTURE.###`, or a paper-doll file name (`paper_file`) | Pictures, that archive, or the race part whose file it is |

`Warrior` is both a creature (mobile id 144) and a class (id 16). Both rows appear. The creature is first because it has a picture. The class is the next row. Neither is dropped.

Collect every exact hit. Stop substring hits at 200. The status says when more were left out. Exact hits are never the ones left out, so `daggerfall` is the first place row.

A one-character box only filters the current list. It does not walk the places.

### Readers

`dat_row_label`, `dat_find_tag`, `dat_kind`, `dat_filter` on the one merged book. `made_weapon_at` and, later, the armour names from the same book. `dungeon_list` and `exterior_list` for places, already in memory for World. `worldmap_pixel_of` to centre. `paper_file` for a CIF or IMG name. `tex_list` for an archive name. No new format reader.

## Plain names in the grid

The value column is still the record. **Means** is filled only for fields the browser knows. Anything else stays blank. A mod's new field stays blank. Do not guess.

| Field | Means |
| --- | --- |
| `ENCT` `kind` | the `ENEM` row's name for that id. A missing id stays blank |
| `CLAS` `primary`, `major`, `minor` | the `SKIL` row's name |
| `CLAS` `attribute`, `SKIL` `attribute`, `ENEM` `attributes` | Strength, Intelligence, Willpower, Agility, Endurance, Personality, Speed, Luck, in `ATTR_*` order |
| `WEAP` `skill` | the `SKIL` name |
| `CMON` `season` | `clock_season_name`, with the first letter capital (`clock.c` is already linked) |
| `ENEM` `male`, `female`, `corpse_archive` | `TEXTURE.###` via `texture_filename` |
| `RACE` picture fields | already a file name; Means stays blank |
| `WEAP` `cif` | `WEAPON##.CIF` |

Enter or a double-click on a Means cell that names a creature, a skill, a texture archive, or a paper-doll file commits the same way a lookup hit does. A blank Means cell does nothing.

When the selected row is an `ENCT` row, the list of that row's kinds is also the well: the selected kind's front frame only (walk, record 0 of the male archive, through `beast_poses` and the texture reader the creature view already uses). One archive at a time. Do not decode the other nineteen to paint the row. The names are in the grid. Clicking the kind's Means opens the creature.

Dungeon-type words, used by the place slice, are fixed strings next to `DUN_TYPE_*`: Crypt, Orc stronghold, Human stronghold, Prison, Desecrated temple, Mine, Natural cave, Coven, Vampire haunt, Laboratory, Harpy nest, Ruined castle, Spider nest, Giant stronghold, Dragon's den, Barbarian stronghold, Volcanic caves, Scorpion nest, Cemetery. Id 19 is the underwater encounter table, not a place type. 255 is no type. These strings live in the app. `atlas_type_name` stays the map's own type (city, hamlet, farm) and is a different word.

No new format reader. The names come out of the merged book, `clock_season_name`, and `texture_filename`.

## Classes and skills

Under **Characters**, after the races, a **Classes** node and a **Skills** node. The races stay where they are.

Classes are the `CLAS` rows, labelled by name. Eighteen when the book is classic's. The list for one class is three groups, Primary, Major, and Minor, each skill by its `SKIL` name. The well has no portrait. No class picture exists in the tables, and none is invented. The well draws the class name, the hit points a level, and the spell points an Intelligence point gives (`magicka`), in the same font as the rest of the window. The grid is the raw fields plus Means. Enter on a skill opens that skill.

Skills are the `SKIL` rows. The list is the name and the governing attribute in words. The grid is the four fields plus Means. There is no picture.

A custom class (`CLASS_CUSTOM`) is not in MAIN.DAT. It is not shown.

Readers: the merged book only. No new reader.

## Armour

Under **Items**, after the weapons, one node per `ARMR` row, named from the `ITEM` row with the same id (Cuirass, Gauntlets, and the rest). Weapons stay as they are.

The list is the materials that piece comes in: the ten metals, then leather and chain. A weapon's list stays the metals only. Leather and chain are armour's (`MATERIAL_LEATHER`, `MATERIAL_CHAIN`).

The well is that piece's paper-doll record, drawn with the texture reader and `cif_metal` for a metal. Leather and chain keep the picture's own colours. The default wearer is a Breton man: archive `249 + 2` (a man's archives start at 249, a woman's at 245, and Breton's body shape is 2). The record is the template's `player_record` plus the variant. A metal uses `plate_low` through `plate_high`, leather uses `leather_low` through `leather_high`, chain uses `chain_low` through `chain_high`, both ends included. Left and right step the variant, the same keys that step a weapon frame. Two buttons under the well, **Man** and **Woman**, flip 249 and 245. The body shape stays human for this slice, so there is no race picker. The status says `Breton man` or `Breton woman`.

The grid is the item, the armour template, and the material, then the made line. The sums are the ones `item_make` uses, copied here so the app still does not include `play/inventory.c`:

- value = price × `value_mul` × `value_q`
- weight = whole quarter-kilograms × `weight_q` / 4, half rounded to even, back to kilograms
- condition = hit points × `condition_q` / 4

An iron cuirass (template 102, material 0) is 12.50 kg, value 300, condition 4096. Its archive for a Breton man is 251. Its record is `player_record` (3) plus `plate_low`.

This is not a new format reader. `texture.c` and `cif_metal` already draw it. The archive formula is the game's (`ARCHIVE_MALE` 249, `ARCHIVE_FEMALE` 245, body shape Argonian 0, the three elves 1, Khajiit 3, Breton and the other humans 2). Put it in the app next to `made.c`, with a comment that names `item_picture`, and lock it with the host check. Do not compose a whole worn doll. One piece is the picture.

## A place and the other half

Selecting a place still centres the map and fills the block grid from the `DunRef` the lists already hold. Add three lines to the place text, filled when the selection changes, not when the list is built:

- The map type (`atlas_type_name`) and the climate (`atlas_climate_name`), which a map click already reports in the status.
- For a dungeon, the dungeon type in the words above. Read it with one `dungeon_load_at` for the selected region and index. Do not load every dungeon. An outside has no dungeon type.
- The other half, when `dungeon_outside` or `dungeon_under` returns an index. The line is `Above: <name>` or `Below: <name>`. Enter on that line selects the other place and centres the map. A miss shows nothing. One call per selection.

Under that, for a dungeon whose type has an `ENCT` row, the kind names of that table, the same Means rule as the encounter grid. The well on World is the map, so the names are lines in the place text, not thumbnails on the map. Enter on a kind commits to Commons and that creature, with its walk frame up.

Do not load an RDB or an RMB. The block names already in the grid are enough of the place's files.

Readers: `dungeon_load_at`, `dungeon_outside`, `dungeon_under`, `worldmap_*`, `atlas_climate_name`, `atlas_type_name`, and the merged `ENCT` / `ENEM` rows. No new reader.

## Copy

Ctrl+C copies the selected grid value to the clipboard, as text, through the clipboard calls on `USER32.dll` (already imported). When the grid has no selection, copy the selected list row's name. This writes no file.

## Keyboard and mouse

| Input | Does |
| --- | --- |
| Type in Find | Filters the current list. Two or more characters and no row: the hit list, and the well follows |
| Enter in Find, or on a hit | Commits the hit. On a place, opens World and centres the map |
| Enter on a Means cell, a skill, an encounter kind, or a place's other half | Opens that thing |
| Escape | Clears the hit list, or quits when there is none |
| Ctrl+F | Puts the keyboard in Find |
| Ctrl+C | Copies the selected value or the row's name |
| F5 | Commons |
| F6 | World |
| Up, Down | The list, and the well follows |
| Left, Right | On Commons, the frame or the head or the armour variant, as today. On World, scroll the map when Find does not have the keyboard |
| 1, 2, 4 | Zoom, on World, when Find does not have the keyboard |
| Ctrl+Tab | The tab control's own switch. Do not replace it |
| Click a tree node | That node's list, as today |
| Click a list row | Selects it. A thumbnail click still selects that record, head, or view |
| Double-click a hit or a Means cell | Same as Enter |
| Click the map | Height, climate, and the place, as today |
| Middle button, drag, release | Pans the map under the cursor. Unchanged. No other button pans |
| Climate, dots, centre | Unchanged. Centre recentres on the selected place |

Arrow keys on World still do nothing while Find has the keyboard, so a query can contain letters without scrolling the map.

## Staying fast on a 233 with 64 MB

- **One parsed copy.** Lookup, Means, classes, skills, and armour all read `books_merged` and the place arrays. Do not call `dat_open` again, and do not merge again, for a search.
- **The map is one bitmap.** Pan and zoom change the source rectangle of the blit. Rebuild only when climate tint or location dots toggle (`atlas_build`). Never walk pixels on a paint, a drag, or a zoom.
- **Paint visible rows.** The place list is a `SysListView32` with `LVS_OWNERDATA`. The program keeps the hit index it already builds (`g_hit_i`, `g_hit_out`) and answers `LVN_GETDISPINFO` for the rows the control asks for. Set the count with `LVM_SETITEMCOUNT`. Do not `LVM_INSERTITEM` once per dungeon and town. The same owner-data path serves every Commons list, so a long filtered list asks for text only for the rows on screen. `dat_value` runs for those rows, not for the rest of the table.
- **Win98's comctl32** (4.70 or later) has owner-data. The program already loads `comctl32.dll` at run time. Do not fall back to inserting every place.
- **A keystroke walks names in memory.** Places are the long walk, about twenty thousand names, once per change of Find, and only when the current list missed and the box has two characters. It writes integers into the hit index. It does not allocate a string per hit.
- **One picture decoded for the well.** An encounter shows the selected kind's front frame, then drops that archive when the kind changes (`texture_forget`, as the creature view already drops an archive). Armour shows one record. The thumbnail grid decodes a cell when it is first shown and keeps it until another archive is selected. A paint walks the cells that fit in the well, not every record in the archive.
- **One dungeon record** when a place is selected (`dungeon_load_at`). The lists stay the `DunRef` arrays from startup.
- The map bitmap is 1000×500, three bytes a pixel, about 1.5 MB, kept with `mem_keep`. The place arrays are the ones World already holds. Do not keep a second zoomed copy.

## Deliberately not in this build

- **No Glide, no 3D, no walk through a dungeon or a house.** Blocks are names on the grid. Models stay RENDERFALL's and LEVELFALL's.
- **No writing of `.DAT` files.** `rec_put_begin` and `rec_put_rows` in `recfile.h` are the writers, and `dfrec build` is the program that uses them: it turns text into a file that carries only the fields the text sets. cd-gfall does not call them. A person who wants to change a row dumps it with dfrec, edits the text, builds a mod into `DATA\`, and adds the name to `LOADORDR.TXT`. Restarting cd-gfall shows the merge. A grid that writes would be a second editor with none of dfrec's line errors.
- **No deleting records.** A mod cannot delete a row yet (`docs/data.md`).
- **No per-field "which file won".** Each file is already its own node under Data. Open that node to see what it carried.
- **No pictures from a mod over ARENA2.** The VFS has one root. `docs/data.md` already says so.
- **No spells, factions, books, quests, or TEXT.RSC.** Those tables are not in MAIN.DAT.
- **No sound.**
- **No full paper doll of a worn body**, no race picker on armour beyond man and woman, no class portrait, no biography questions.
- **No second executable and no console port of dfrec.**

## Host checks, and the PE rule

`make datfall-check` must pass after every slice. It is the same `pe_check` as `make gfall-check`: PE32, 80386, GUI subsystem 4.0, and an import table of only `KERNEL32.dll` and `USER32.dll`. Then `build/datfall-host`, which has no window. GDI and comctl32 stay `LoadLibrary`. `glide2x.dll` is not imported and not loaded. The SSE and P6 check is unchanged.

The checks already in `apps/datfall/check.c` stay: the merged chunks and their field names, a mod's new field and new table, 472 texture archives, `TEXTURE.255`'s 20 records, a Breton body pixel, Breton's 10 male heads, the rat's poses, a dwarven weapon colour, a steel longsword at 5.50 kg, value 90, condition 1200, the 1000×500 map, Daggerfall at pixel 207,213, 4232 dungeons and 15251 outdoor places.

Add these, in the slice that makes them true. Each is a function in `apps/datfall/` called from `check.c`, with no window.

- **Lookup.** `breton` is the Breton race. `rat` is creature id 0. `longsword` is the weapon whose template is 120. `daggerfall` is a place in region 17 at pixel 207,213. `midyear` is a `CMON` row found by its name, not by a hardcoded index. `warrior` returns two hits, creature 144 first and class 16 second. A one-character query does not return place hits.
- **Place filter.** The hit index for the place list, asked for `Daggerfall`, includes that town, and building the index does not require a list-view control.
- **Means.** Encounter id 2 (human stronghold) has 20 kinds, and the first is 144, whose name is Warrior. Class 16 is named Warrior. Skill 21 is named Running and its attribute is Speed (`ATTR_SPEED`). `clock_season_name` supplies the season word for a month.
- **Classes and skills.** 18 `CLAS` rows and 35 `SKIL` rows. Each of the Warrior's primary, major, and minor ids resolves to a `SKIL` name.
- **Armour.** Template 102 in material 0 is 12.50 kg, value 300, condition 4096. A Breton man reads archive 251 and record `3 + plate_low` from the `ARMR` row. Leather is reported as not a `cif_metal` recolour.
- **The other half.** For Privateer's Hold in region 17, `dungeon_outside` agrees with the line the place text would show (a name when the index is found, nothing when it is −1). Dungeon type 2's word is Human stronghold.

## Build order

Each slice leaves `make datfall-check` green and the previous window usable. Do them in order. Do not start a slice by rewriting `main.c` into a new program.

1. **The name and the first screen.** Title bar and message boxes say cd-gfall. The exe stays `DATFALL.EXE`. On open, Characters is selected, the first race's doll is up, Find has the keyboard. Fold that into the viewer section of `docs/data.md` and the DATFALL line in `docs/architecture.md`.
2. **Visible rows.** The lists, and the place list in particular, answer only the rows on screen. Place columns read Name, Kind, Region. Pan, zoom, and the middle button are unchanged. Extract the place hit index so the host check can ask it for Daggerfall.
3. **Lookup.** `look.c`, the hit list, Enter, Escape clearing the hits, a place committing to World. The host checks for breton, rat, longsword, daggerfall, midyear, and the two warriors.
4. **Means.** The extra grid column, the name tables above, Enter following a creature, a skill, or a texture. The encounter well shows the selected kind's front frame. Host checks for the human stronghold, the Warrior class, and Running.
5. **Classes and skills.** The two nodes under Characters. Host checks for 18 and 35, and the Warrior's skill names.
6. **Armour.** Nodes under Items, the Breton man and woman pictures, the made numbers. Host check for the iron cuirass.
7. **The other half of a place.** Type, above or below, and the encounter's names, one dungeon loaded on selection. Host check for the hold's outside and the human-stronghold word.
8. **Copy and the two keys.** Ctrl+C, Ctrl+F, and F5 for Commons. No new host numbers. The PE check is the proof the clipboard did not add an import.

When a slice changes what the window does, update `docs/data.md`'s viewer section in that same slice so it does not still describe only the first screen. This file stays the spec.
