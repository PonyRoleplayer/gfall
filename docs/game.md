# The game (design)

What the game layer is, how it is built on the engine and Daggerfall's places, and the order it comes in. This is a design. Nothing in it is built yet except where it says so. Read `docs/architecture.md` first for the layers. Read this before adding anything to `src/game/dagger/play/` or rewriting `apps/gfall/`.

## What there is today

The engine draws, hears and walks any place. `src/game/dagger/` loads Daggerfall's places whole (`world_load_dungeon`, `world_load_inside`) or streams the whole map round the camera (`wild_open`, `wild_update`). Its doors swing, its levers run their chains, its creatures stand where their markers put them, and its townspeople walk.

LEVELFALL already goes through every passage: into a house and out, down into a dungeon and up, out of the stream and back. That code lives in the viewer (`lf_use`, `land_at`, `stream_inside`, `view_stream` in `apps/levelfall/main.c`) and leans on the viewer's own list rows (`pair_row`).

GFALL (`apps/gfall/main.c`) flies a camera round Privateer's Hold, and `src/game/dagger/play/player.c` is that fly. There is no player, no rules, no screens and no saves yet.

## The pieces

```
apps/gfall/main.c           the window, the loop, the keys: calls play_frame
src/game/dagger/places/
  travel    where the player is, and going through a passage (moved here from LEVELFALL's view code)
src/game/dagger/play/
  game      the states (title, new character, play, menu, loading) and one frame of play
  body      the player in the world: the walker, run, jump, swim, fall damage
  sheet     the character record: attributes, skills, vitals, level
  rules     the sums: derived values, skill rolls, to-hit, damage (pure functions)
  foes      what a creature does: wake, chase, attack, hurt, die
  items     items, gold, the inventory, loot
  hud       the play screen: crosshair, compass, vitals, the line of text
  screens   the menus: character, inventory, rest, the map
  save      writing and reading a game
```

The player is Daggerfall's, so it lives here and not in the engine. The engine gives a camera, a walking body (`walker`), using things (`interact`) and entities (`entity`), and `play` builds Daggerfall's player out of them. Another game would build its own in its own folder. Each is a module of one `.c` and one `.h`, as in `docs/architecture.md`. `rules`, `sheet`, `items`, `save` and `travel`'s state machine are plain C and host-tested in `tests/check_play.c`. `hud` and `screens` draw through the engine and are checked by running GFALL.

`travel` is the one piece that is a place's business more than the game's: LEVELFALL needs it too. It goes in `src/game/dagger/places/travel.c`, not in `play/`, so that both GFALL and LEVELFALL use one copy. Its job is to load what a passage leads to and say where the player lands. It finds the other side from the location index (`dungeon.h`), not from a viewer's rows.

## A frame of play

GFALL's loop, once the game is running:

1. `win_pump`, then read the keys (`input_read`, plus the game keys; see the engine requests).
2. `game_frame(dt)` does nothing more while `gfx_paused()` (the console is open) or a screen is up. Otherwise:
   1. `body`: the walker moves the camera (`walker_step` runs inside `gfx_frame` today), with run, jump and swim on top.
   2. The use key and the attack. The use key goes through `interact_set_handler` and `travel` (passages) or `foes` (talk, later). The attack goes through `foes`.
   3. `foes` steps the creatures that are awake, under a time budget.
   4. The place's own steps: `world_creatures_step` (reading their pictures), or `wild_update` in the stream.
   5. The clock (the engine's `clock_advance`). Resting and waiting run it faster.
3. `gfx_frame(cam, level)`, with the HUD drawn by `gfx_set_extra(hud_draw)`.
4. `travel` changes place between frames, under `gfx_hold`, then calls `gfx_prepare` and `gfx_prime`, and fades in (`gfx_fade_in`).

## Places and travel

A small state machine: **stream** (outside, the whole map), **inside** (one building, loaded whole, where it stands), **dungeon** (loaded whole). Every move between them goes through one passage, kept as a `Usable` (`doors.h`: `DOOR_ENTER`, `DOOR_LEAVE`, `DOOR_DUNGEON`, `DOOR_EXIT`). The landing comes from `doors_landing`, as LEVELFALL does it now.

- A place's data lives in its own heap (`docs/memory.md`), freed when the player leaves. What the player carries across (the sheet, the items, the quests later) is kept with `mem_keep` or in the game's own block, never in a place's heap.
- Going in from the stream keeps the stream's position, so going out lands at the same door.
- The first build starts in Privateer's Hold (`world_load`, marker 10), as Daggerfall does.

## The player

### The body
Daggerfall is first person: the player is the camera. The physics is not the camera's, though. It is an engine **body** (requested, item 8 below): 72 units tall, a 24-unit step, gravity and stairs, which the game steps each frame with the motion it wants. A first-person rig then puts the camera at the body's eye. A third-person game would instead draw an entity at the body and put the camera behind it with a third-person rig; the engine allows both, and this game uses the first.

Entities have no physics of their own. A creature that only stands, or walks a flat town cell, needs none. A creature that chases the player down a stair gets a body of its own size, and the game copies the body's place to the creature's entity each frame.

The game adds, as rules on top of the player's body:
- **Run**: speed from Speed and the Running skill.
- **Jump**: height from the Jumping skill.
- **Swim**: when `scene_water_body` says the player is in over the head; Swimming slows the loss of breath.
- **Fall damage**.

The engine's G1 hooks (on its `engine-game-hooks` branch) give the jump and the landing; the run is the game's longer wanted motion.

### The sheet
Daggerfall's character, per UESP (Character Creation, Attributes, Skills, Leveling):
- the eight attributes, 0 to 100: Strength, Intelligence, Willpower, Agility, Endurance, Personality, Speed, Luck;
- the 35 skills, with the class's 3 primary, 3 major and 6 minor ones;
- race, class, level and experience;
- the three vitals (health, fatigue, spell points) and their maximums;
- reputation, then factions and legal standing later.

The derived values (the vitals' maximums, carrying weight, the bonuses) are functions in `rules`, each citing its source. The sheet's layout is ours.

Skill advance by use and the level-up rule come from UESP's Leveling page. A pre-made character comes first, and the creation screens come later.

## Controls and the HUD

Daggerfall's keys on a Win98 keyboard and mouse:
- WASD to walk;
- the mouse to look;
- the left button to attack, held to swing;
- the use key;
- shift to run, space to jump;
- keys for the screens (character, inventory, map, rest);
- Escape for the menu.

The engine's `Input` has only the movement and the look today, so the rest is an engine request.

The HUD is drawn at the card's 640 × 480 through the engine's text (`fnt_install` puts FONT0000.FNT in):
- a crosshair;
- a compass;
- the three vitals as bars;
- the engine's `interact_say` line.

Daggerfall's own screen pictures (IMG and CIF, 320 × 200) need a 2D picture draw from the engine; that is a request too.

## Creatures and combat

The creatures already stand in a dungeon as entities (`docs/entities.md`): each has a kind (`world_entity_kind`) and a standing and a walking pose. The game adds:
- **Friend or foe** from the kind (monsters and most class people hostile in a dungeon).
- **A motor**: asleep until seen or heard, then turn to the player, walk to them (walking cells in a town, straight with the collider in a dungeon), and attack within reach. One step a frame for the near ones, under a budget.
- **The attack and the hit**: to-hit and damage per UESP's Combat page, as Daggerfall Unity's formulas have it (read for what they do). The weapon's swing is a picture drawn over the view.
- **Hurt and death**: the hurt and death records (5 to 14, 20 to 29; `docs/entities.md`, Not yet), and a body left on the floor with its loot.
- **The player's death**: the menu, load the last save.

Moving an entity's position and pose from game code, and the walker colliding with entities, are engine requests.

## Items, gold, the inventory

- Item kinds, weights, values and pictures per UESP (Items) and Daggerfall Unity's item templates. Each item's picture comes from its TEXTURE or CIF record.
- One inventory for the player, a list for each body or chest. Gold is a count.
- Loot is drawn by the creature's kind, later.

## Time, rest, travel

- The engine's clock and calendar (`clock.c`) is the game's time. A save keeps it.
- Rest and wait run the clock faster, and give back health and fatigue by the rules. A creature that comes near stops the rest.
- The travel map comes later, with `worldmap.c`'s pixels and the location index: pick a place and land in the stream there, time passing as the trip.

## Saves

A small file beside the exe (`SAVE0.SAV`..). It holds:
- the version;
- the sheet and the items;
- the place (stream position, or which building or dungeon and where in it);
- the clock;
- the changes the player made to places: open doors, pulled levers, dead creatures and taken loot, as a list per place.

Plain C, written with the engine's file layer, and host-tested as a round trip.

## Later

Talk (the people's lists, `USE_TALK`), factions, quests (QBN/QRC per UESP), spells and spell effects, shops and banks, the law, horses and carts. Each gets its own design here when its phase comes.

## Budgets on the 233

- **CPU**: the game's own work (rules, foes, HUD) aims at 2 ms or less a frame, so it costs little out of the 60 Hz frame the engine and the stream need. The frame profile (`docs/profile.md`) gets a `play` stage, and the number is set from the 233's log, not guessed.
- **RAM**: the sheet, the items and the save's change lists stay under 64 KB, kept for the process.
- **The card**: the HUD's pictures and the weapon's swing come out of the same 2 MB. They are counted in the budget the way sprites are (`docs/memory.md`).

## What the engine must give

Requests for the engine engineer, each in its smallest form:

1. **Game keys in the input snapshot**: attack (left button, held), use, run, jump, and the screen keys. `Input` today has movement, look and quit.
2. **The walker: run, jump, and a fall's speed at landing**, so the game can scale speed, start a jump, and take fall damage.
3. **The walker against entities**: the body stops at a creature or a person, and they stop at it.
4. **Moving an entity from game code**: set its position, facing and pose (walk, stand, attack, hurt, die) for a creature's motor.
5. **A 2D picture draw** for the screens and the weapon: a picture on the card drawn at a screen rectangle, under or over the HUD's text.
6. **A pause that is not the console**, for the game's menus: the world stands still (`gfx_paused`) while a screen is up.
7. **A `play` stage in the frame profile.**
8. **A body apart from the camera**: the walker split into bodies the game steps (as many as wanted, each its own size) and camera rigs that place a camera from a body (first person at the eye; third person behind it, pulled in from walls). The viewers' walker stays as it is, built on one body. The use ray takes an origin and a direction, not only a camera.
9. **The calendar out of the engine**: the engine keeps minutes and days, and the game installs Tamriel's months and seasons. Built: `clock_set_calendar` (`docs/daylight.md`).

## Data
Daggerfall's tables are records in MAIN.DAT, cooked on the first run, with mods over it in a load order (`docs/data.md`). Built so far: enemy kinds, encounter tables, weather odds and looks. Each table below comes with the system that reads it:

| Tables | Holds | From |
| --- | --- | --- |
| `LOCS`, `WMAP`, `CLIM` | the index of 4232 dungeons and 15251 towns; the heights, climates and locations' pixels; each climate's numbers | MAPS.BSA, WOODS.WLD, CLIMATE.PAK (they cut the 233's start-up) |
| `ITEM`, `MAGI`, `WEAP`, `MATL`, `ARMR`, `LOOT` | items, artifacts, weapons, materials, armour, loot | the exe (via DFU), MAGIC.DEF; pictures stay in ARENA2 |
| `CALE` | the calendar | the exe; the engine takes it through a setter (request 9) |
| `RACE`, `CLAS`, `SKIL`, `SPEL`, `EFCT`, `ENCH`, `POTN`, `BIOG` | races, classes, skills, spells, effects, enchantments, potions, background | CLASS*.CFG, SPELLS.STD, BIOG*.TXT, the exe |
| `FACT`, `NAME`, `PEOP`, `POLI`, `GILD`, `LAW`, `PRIC`, `HOLI`, `FLAT`, `MUSI`, `SWAP`, `DISE`, `CURS` | factions, names, townspeople, politics, guilds, law, prices, holidays, flats, music, swaps, diseases, curses | FACTION.TXT, NAMEGEN.DAT, POLITIC.PAK, FLATS.CFG, the exe |
| `BOOK`, `TEXT`, `QUES` | indexes of the books, TEXT.RSC and the quests | BOOKS\, TEXT.RSC, QBN/QRC (their text and scripts stay in ARENA2) |

Blocks, models, pictures, sounds and songs are never cooked: they stay in ARENA2.

## Phases

| Phase | What | Tests (`tests/check_play.c`) |
| --- | --- | --- |
| G1 | GFALL on foot: start in the hold, walk, use doors, out to the stream and back, in and out of houses; `travel` lifted out of LEVELFALL; the HUD's crosshair, compass and line | the travel machine: each passage lands where LEVELFALL's did |
| G2 | the sheet and the rules: a pre-made character, derived values, the vitals on the HUD | each derived value against UESP's worked figures |
| G3 | combat in the hold: the motor, the attack, hurt and death, the player's death | to-hit and damage sums; a creature wakes and closes in |
| G4 | items, gold, loot, the inventory screen | weights and loot draws |
| G5 | saves | a round trip; a place's change list replayed |
| G6 | rest, the travel map | the clock and the vitals across a rest; a trip's landing |
| G7 | talk, factions, quests | each its own design first |

Each phase updates this page (what is built, what is not) and AGENTS.md's layout list.
