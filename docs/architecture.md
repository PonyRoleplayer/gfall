# Architecture

How the tree is laid out, the one rule between its parts, and where new code goes. Read this before adding a file or a folder.

## The layers

```
apps/                     the programs: a window, a loop, the keys
  gfall/  levelfall/  renderfall/  spintri/
src/game/dagger/          Daggerfall, the one game, all of it:
  play/                     its rules and state, its player, screens, saves
  places/                   a Daggerfall place built into the engine's Level
  tables/                   Daggerfall's rules for dressing a place
  formats/                  Daggerfall's files, read
src/engine/               the engine: knows Glide, Win98 and the 233, no game
tests/                    hostcheck: the engine's and the game's tests, run natively
tools/                    scripts: check-deps, check-layers
```

**A game is a folder under `src/game/`.** Everything that is Daggerfall's is in `src/game/dagger/`: its files, its tables, its places, and its game proper, the player included. The engine has no player: it gives a camera, a body that walks (`walker`), things to use (`interact`) and things that move (`entity`). A game makes its own player out of them, with its own sheet, rules and screens. A second game would be `src/game/<name>/` with the same four folders, using the engine and never another game.

**The rule:** a file includes its own layer and the ones below it, never one above. The order runs engine, formats, tables, places, play, apps. A game never includes another game. `tests/` may include anything. `make` runs `tools/check-layers` before anything builds, and it fails on an upward include or a cross-game include.

The rule keeps each layer testable without the ones above it and replaceable without the ones below. The engine draws any game. The formats read files for any program, a viewer as well as the game. Places do not know there is a player with a sword. The game does not know how a BSA is laid out.

The engine never calls up into the game by name. When it needs the game, the game installs a hook: `interact_set_handler`, `gfx_set_extra`, `gfx_set_under`, the loader registry (`dagger_bind_loaders`).

## What each part does

### `src/engine/`
Generic. Seven folders: `core`, `platform`, `assets`, `scene`, `gfx`, `sound` and `ui`. AGENTS.md describes each one. A new engine feature gets a doc in `docs/` and, when it can be switched, a console variable (`docs/console.md`).

### `src/game/dagger/formats/`
One module per Daggerfall file or family of files. Each turns the bytes into engine types (`Mesh`, `Image`, a `Scene` block, a clip, a song) or a small table of its own.

| Module | Reads |
| --- | --- |
| `bsa` | the BSA archive directory, named and numbered |
| `arch3d` | ARCH3D.BSA models into engine meshes |
| `texture` | TEXTURE.### pictures and their frames |
| `palette` | a .COL palette |
| `fnt` | FONT0000.FNT into the engine's glyph sheet |
| `img` | IMG pictures, the HUD's and the screens' (uncompressed for now); RAW pictures; uncompressed BSS animations, including the classic HUD compass |
| `cif` | CIF pictures: a face's frames, a weapon's armed picture and its swings |
| `classcfg` | CLASS*.CFG, the classes |
| `rdb` | an RDB block (a dungeon's): models, flats, lights, actions, markers |
| `rmb` | an RMB block (a town's or a house's inside) |
| `dungeon` | the location index (MAPS.BSA): places, their blocks and climates |
| `worldmap` | WOODS.WLD's heights, CLIMATE.PAK, every location's map pixel |
| `skyfile` | SKY##.DAT and the night skies |
| `sndfile` | DAGGER.SND's clips |
| `hmi` | MIDI.BSA's HMI songs |
| `climate.h` | the climate ids and texture families the files use |
| `bind` | registers the readers with the engine's loader registry |

### `src/game/dagger/tables/`
Daggerfall's rules for what a place has, as Daggerfall Unity carries them, read for what they do.

| Module | Decides |
| --- | --- |
| `maindat` | MAIN.DAT: cooking it, whether it is stale, the load order, merging each `.DAT` into the tables (`docs/data.md`) |
| `classic` | classic's tables inside FALL.EXE, for cooking MAIN.DAT only |
| `forecast` | the weather's odds by climate and season, and each kind's look (records) |
| `swaps` | a face's picture by climate and season, and which ones are windows |
| `sounds` | a place's songs, ambience and sounding flats |
| `enemies` | the creature kinds and the encounter tables (records), and the rules that place them |
| `calendar` | Tamriel's calendar (records), installed in the clock |
| `careers` | the classes, races and skills (records) a character is built from |

### `src/game/dagger/places/`
A place, built: what the engine draws, hears, walks on and uses.

| Module | Builds |
| --- | --- |
| `world` | one place loaded whole: a dungeon, a town, a house's inside |
| `wild` | the world streamed round the camera, chunk by chunk |
| `terrain` | the land's heights, tiles and meshes |
| `wres` | the streamed world's pictures and meshes, by session |
| `people` | townspeople walking a streamed town |
| `doors` | the doors, levers, passages and people a block names, as usables and movers |

### `src/game/dagger/play/`
The root coordinates the frame (`play.c`), shared session state (`state.c`),
global configuration and console cheats. Each folder owns a gameplay responsibility.

- `character/` owns creation, careers in play, progression, vitals, survival, rest and ailments.
- `combat/` owns melee and weapons, creature AI, dungeon actors and encounters.
- `input/` owns controls, activation, steering and movement.
- `items/` owns inventory, descriptions, ground containers and the possessions controller.
- `magic/` owns effects, casting, equipment enchantments, potions and spell services.
- `people/` owns dialogue, standing, entry rules, law, guilds and commerce.
- `quests/` owns quest interpretation, events and persistent quest state.
- `save/` owns the save codec and slot selection.
- `travel/` owns passage actions, travel, journeys, map controllers and world state kept in saves.
- `ui/` owns shared presentation primitives, notifications, HUD, options and text macros.

Headers live beside their implementation. Only expose what a caller uses.
Folders organize the play layer and follow the same include rules as its root.

### `apps/`

Each program has an entry point. The GFALL app wires the gameplay layer into
the Win32/Glide frame loop and renders game-specific assets. `main.c` owns the
loop, `boot.c` initializes the app, and `seam.h` is their interface to the
rest of the app and to the host game tests.

- `world/` owns the active level, place heap, arrivals, travel callbacks and encounter art.
- `buildings/` resolves building IDs and names, announces entry and adapts the door gate.
- `people/` connects NPC selection, conversations, services and street guards to gameplay.
- `quests/` adapts quest text, world lookups and placement of quest entities.
- `screen/` composes classic screens and owns pictures, palettes and fonts. Map views,
  possessions screens and first-person overlays have separate presenters.

A subsystem's `hooks.c` adapts or registers callbacks. Use direct calls
between subsystems for operations such as asking a building for its name.

LEVELFALL and RENDERFALL are diagnostic viewers (`docs/viewers.md`). DATFALL is
the construction set and dfrec is its native data tool (`docs/data.md`).
SPINTRI is the standalone Glide demo.

### Data
Daggerfall's tables are records, not C arrays (`docs/data.md`):
- A table's row struct and `RecSchema` live in the module that uses it.
- `tables/classic.c` reads the original values used to cook MAIN.DAT.
- The record file itself is the engine's (`engine/assets/recfile.c`), and it knows no game.
- `apps/dfrec` reads and makes `.DAT` files. `apps/datfall` is cd-gfall: the tables, the pictures, the characters, the creatures, the items and the world map.

**The engine takes values, the data supplies them.** The engine owns how things work: how weather rolls and blends, how fog draws, how a body walks. Its numbers are defaults, so it runs with no data, and it takes a game's numbers through setters, each with a getter for its default (`weather_set_look` and `weather_look`). The game reads its numbers from MAIN.DAT and the mods, and hands them over (`maindat_apply`). An engine table a game may want to change gets a setter and a getter. A table only the game uses stays in the game.

## Where new code goes

1. Is it about Glide, the window, the card, sound output, or something any game would need? Put it in **engine**.
2. Does it read a Daggerfall file? Put it in **formats**. From here down, each folder is Daggerfall's own, under `src/game/dagger/`.
3. Is it a Daggerfall rule that picks what a place contains or looks like (a table, odds, a swap)? Put it in **tables**.
4. Does it turn files and tables into a Level, or step a loaded world (the stream, townspeople)? Put it in **places**.
5. Is it about the player, the rules of play, a screen, a save? Put it in **play**. The engine never gets a player; it gets the generic part the player needs (a key, a hook, a body's rule) as an engine request.
6. Is it a key, a panel, or a diagnostic for one program? Put it in that program's **apps** folder.

Gameplay data such as creature health, spell costs and weather odds belongs in
the existing records. Fixed format IDs, masks and dimensions belong with the
code that interprets them. Share them through the owning domain's header when
more than one module needs them. For example, `tables/buildingtypes.h` defines
building record IDs for the table and its gameplay consumers, and
`play/ui/canvas.h` defines the classic screen dimensions for presenters.

When a lower layer needs something from a higher one, the lower layer gains a hook, and the higher one installs it.

## Conventions

- **A header defines an interface.** A module may span several C files. Keep local helpers and state private. Document format quirks and dependencies at the interface that uses them.
- **Names** are snake_case, prefixed by the module: `world_load`, `wild_update`, `forecast_set`. Types are `CamelCase`.
- **Lifetimes.** Something that lives for the process is `x_startup` / `x_shutdown` with `mem_keep`. A place's data lives in that place's heap (`docs/memory.md`). Per-frame work is `x_step` or `x_frame`, takes `dt` in seconds, and skips itself while `gfx_paused()`.
- **No Win32 below `apps/` and `src/engine/platform`**, except where the engine guards it with `DF_HOST`. `src/game/` is plain C, so hostcheck builds it. `fnt.c` is the one exception, and `tools/check-layers` lists it.
- **Docs.** Behavior a later edit must keep goes in `docs/` before the commit (AGENTS.md, "Document the change").

## What each program links

The Makefile groups sources by layer: `ENGINE_HOST_SRC` (plain C), `ENGINE_SRC` (plus Win32 and Glide), `FORMATS_SRC`, `TABLES_SRC`, `PLACES_SRC` (together `DAGGER_SRC`), and `PLAY_SRC`.

| Program | Links |
| --- | --- |
| GFALL | engine, all of dagger (play included) |
| LEVELFALL | engine, dagger's formats, tables and places but `fnt` |
| RENDERFALL | engine, `bsa` `arch3d` `texture` `palette` |
| hostcheck | the plain-C engine, all of dagger but `fnt` and play's drawing (`overlay`, `game`), and `tests/` |
| SPINTRI | its own `main.c` alone |
| DATFALL | cd-gfall. The tables, pictures, characters, creatures, items and the map. No Glide. GDI and comctl32 are loaded at run time |

Every executable passes the same PE check (`pe_check` in the Makefile): PE32, 80386, GUI, Windows 4.0, KERNEL32 and USER32 only, glide2x.dll bound at runtime, no P6 or SSE instruction.
