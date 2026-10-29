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

**Built (P1, `play/game.c`, `apps/gfall/main.c`).** GFALL's loop is `win_pump`, then `input_read`, then `game_frame(cam, in, dt)`, then `gfx_frame`. dt is the performance counter's, at most a tenth of a second. `game_frame`, bracketed as the profile's `play` stage:
1. Before any place, the title or the start menu takes the frame (Menus, below). In play, Escape opens the options over the stilled world.
2. Unless paused (the game's, or the console's: `gfx_paused`): the use key, the player's step, its landing taken, the camera on its eye (`rig_first`), and the creatures read near it (2 ms).
3. The compass is turned.

A passage says where it will lead, and a person that talk comes later, through the game's use handler (`interact_set_handler`). The pictures' room on the card is 256 KB (`GAME_PICTURE_KB`), set before the first place.


## Places and travel

A small state machine: **stream** (outside, the whole map), **inside** (one building, loaded whole, where it stands), **dungeon** (loaded whole). Every move between them goes through one passage, kept as a `Usable` (`doors.h`: `DOOR_ENTER`, `DOOR_LEAVE`, `DOOR_DUNGEON`, `DOOR_EXIT`). The landing comes from `doors_landing`, as LEVELFALL does it now.

- A place's data lives in its own heap (`docs/memory.md`), freed when the player leaves. What the player carries across (the sheet, the items, the quests later) is kept with `mem_keep` or in the game's own block, never in a place's heap.
- Going in from the stream keeps the stream's position, so going out lands at the same door.
- The first build starts in Privateer's Hold (`world_load`, marker 10), as Daggerfall does.

### What the player changed (built: the keys)
Every door, lever and creature of a place loaded whole is keyed for the engine's state table (`keep.c`, `docs/interact.md`, State that outlives a load). A key is two words:
- **place:** the location's id in the low 24 bits; a building's record and one in the next 6; the kind in the top 2 (1 a dungeon, 2 a town, 3 a building's inside). A place without an id keeps nothing.
- **thing:** the block's index << 24, and the record's offset in the block's file: an action record's for a mover, a marker's for a creature. An RMB model has no offset, so its thing is 0x800000 and its index.

`world.c` puts back what was kept at the end of every load (`world_keep_restore`); `world_keep_save` writes it before a place is left. A creature kept dead is never read or drawn (`FOE_DEAD`). The engine's `live` means drawn, so a creature not yet read is saved as alive.

Tested in the hold: all 105 things keyed, no key twice, a door and a lever left open come back open after a reload, a door shut again loses its row. Nothing leaves a place in GFALL yet (P6), and the streamed world's towns are not keyed yet (their chunks, with the streamed dungeons).

### A dungeon streamed by block (built: the source)
`places/dunstream.c` is the chunk source of a dungeon streamed by block (`docs/streaming.md`, Dungeons): each RDB block a chunk of 2048, by its place in the layout, the start block the origin.
- **Each block's chunk** holds what a whole load holds of it, by the same rules:
  - its models, painted by the dungeon's picture table (`swap_dungeon`, now shared with `world.c`);
  - its flats at their centres, its lights flickering by their flames, its sounds;
  - its doors, levers and teleports (`doors_build`, keyed);
  - its creatures, on their floors, keyed;
  - its water.
- **Pictures and meshes** go through the session's registry (`wres.c`), with a dungeon mesh variant (`wres_meshes_dungeon`, `wres_paint_by`).
- **Creatures are drawn at open, in the blocks' order.** A random marker's slot 0 and a class's gender take classic's rand, so the order decides who stands where. At open the source reads every block's markers alone (`rdb_markers`: the block's bytes, no models) and draws each creature as a whole load would; a block coming in places its creatures from that table.
- **Kept state** is the stream's to save and put back. A creature kept dead is hidden again in `changed` (live is "drawn").

**Tested against the hold loaded whole:** the ring round its start holds all of it (365 models, 130 flats, 71 lights, 63 movers, 42 creatures, as the whole load). Each creature is the whole load's, keyed and standing alike. A door opened, its blocks dropped 8 blocks off and come back, is open.

**GFALL plays the hold through it** (`start_hold` in `apps/gfall/main.c`): `dunstream_open` under "Loading...", `gfx_prepare` of the streamed level, the player at its start, `dunstream_fill` (the ring and its lights cut, shadows and all), `gfx_prime`, the fade-in. Each frame `game_frame` steps the stream with `GAME_STREAM_MS` (3 ms), and the blocks' creatures come with them. Tested on the host: the player stands on the streamed hold's floor and walks classic's distance. On the 233 it plays at a good frame rate. Its first run there found the body going through walls, ceilings and floors in places (a wall's end after a slide, a jump's rise, the legs in the air, a ramp, a step with no headroom); the engine's collision fixes (`c98b0c3`, `docs/collision.md`) cured them, confirmed on the 233. The `load:` time and the 3 ms budget are still to be measured there.

**Not yet:**
- LEVELFALL's indoor rows;
- a block's meshes parsed in steps (a block is read in one step today).

## The player

### The body
Daggerfall is first person: the player is the camera. The physics is not the camera's, though. It is an engine **body** (`body.h`): 72 units tall, a 24-unit step, gravity and stairs, which the game steps each frame with the motion it wants. A first-person rig then puts the camera at the body's eye. A third-person game would instead draw an entity at the body and put the camera behind it with a third-person rig; the engine allows both, and this game uses the first.

Entities have no physics of their own. A creature that only stands, or walks a flat town cell, needs none. A creature that chases the player down a stair gets a body of its own size, and the game copies the body's place to the creature's entity each frame.

The game adds, as rules on top of the player's body:
- **Run**: speed from Speed and the Running skill.
- **Jump**: height from the Jumping skill.
- **Swim**: when `scene_water_body` says the player is in over the head; Swimming slows the loss of breath.
- **Fall damage**.

**Built (P1, `play/player.c`, `play/rules.c`).**
- **The numbers are classic's, as Daggerfall Unity's PlayerSpeedChanger and AcrobatMotor have them:**
  - walking (Speed + 150 − drag) / 39.5 m/s;
  - running (Speed + 150) / 39.5 × (1.35 + Running / 200);
  - a jump's height, 4.5 m/s × (1 + Jumping / 200) rising under 20 m/s².
- **Until P2 the character is a level-1 one:** Speed 50, Running 20, Jumping 20 (`PLAYER_*`). So a walk is 4.43 m/s, 177 units a second.
- **Forward and sideways at once go no faster than one** (DFU's limitDiagonalSpeed).
- **The player falls at classic's 20 m/s²** (`Body.gravity`, DFU's AcrobatMotor), not the engine's default 9.8. So a jump is classic's in height and in time in the air.
- **The landing is taken but does nothing yet:** fall damage comes with the vitals (P2). Swimming comes when a place with water is played.

### The sheet
Daggerfall's character, per UESP (Character Creation, Attributes, Skills, Leveling):
- the eight attributes, 0 to 100: Strength, Intelligence, Willpower, Agility, Endurance, Personality, Speed, Luck;
- the 35 skills, with the class's 3 primary, 3 major and 6 minor ones;
- race, class, level and experience;
- the three vitals (health, fatigue, spell points) and their maximums;
- reputation, then factions and legal standing later.

The derived values (the vitals' maximums, carrying weight, the bonuses) are functions in `rules`, each citing its source. The sheet's layout is ours.

Skill advance by use and the level-up rule come from UESP's Leveling page. A pre-made character comes first, and the creation screens come later.

**Built (P2, `play/character.c`, `play/rules.c`, `tables/careers.c`).**
- **Where it comes from:** classic builds a character from its **class**, not its race.
  - MAIN.DAT's `CLAS` table is cooked from ARENA2's own CLASS00 to CLASS17.CFG (`formats/classcfg.c`): base attributes, the 3 primary, 3 major and 6 minor skills, hit points a level, spell points an Intelligence point (bits 10 to 12 of its abilities: 3, 2, 1.75, 1.5, 1 or ½), resistances.
  - `RACE` holds DFU's RaceTemplate: names, the paper doll's pictures, what each race resists. Bretons resist magic, Nords frost, and High Elves are immune to paralysis; the other racial gifts come with magic.
  - `SKIL` holds the 35 skills, each with its governing attribute and how hard it is to advance.
- **The rolls (DFU's StatsRollout and SkillsRollout):**
  - an attribute is its class's base and a roll of 0 to 10;
  - a primary skill is 28, a major 18, a minor 13, each and a roll of 0 to 3; any other skill is 3 to 6.
  - Until creation (P8), the game takes the rolls' middles (`RULES_*`), and starts a **Breton warrior**: strength 65, speed 52, health 45, fatigue 120, 24 spell points.
- **The vitals (FormulaHelper, DaggerfallEntity):**
  - health is 25, plus the class's hit points a level, plus a roll each later level;
  - fatigue is strength and endurance, kept in 64ths;
  - spell points are intelligence times the class's multiplier, rounded down.
- **Movement:** the walk, run and jump now come from the sheet's Speed, Running and Jumping.
- **Collision and `tcl`:**
  - The game hands the engine its player's body (`walker_set_game`): the engine's own walker is never stepped in a game, and creatures, other bodies and `stuck` take the player's body.
  - The console's `tcl` is the game's collision switch (`walker_on`), on by default. `tcl 0` flies through everything, Oblivion-style: along the view, pitch and all, Q up and E down, at the walk's and run's speeds, with no gravity. Back to `tcl 1`, the body comes out of whatever it is in and falls from there.
  - Before this, `tcl 1` stood the engine's walker body on the player's, and the two shoved each other: sideways, through walls.
- **Teleports** are the game's to take: each frame, before the step, `interact_teleport` puts the body where the last frame's teleport sends it, at its eye, facing its way.
- **A fall** past 5 m costs 5 health a metre (PlayerHealth); "That hurt." A fall to 0 health says "You have died." and, until combat (P4) and the menu's reload (P3), puts the player back at the start, whole.

## Controls and the HUD

Daggerfall's keys on a Win98 keyboard and mouse:
- WASD to walk;
- the mouse to look;
- the left button to attack, held to swing;
- the use key;
- shift to run, space to jump;
- keys for the screens (character, inventory, map, rest);
- Escape for the menu.

The engine's `Input` gives these as actions (`INPUT_*`: held, pressed, released). The game sets Escape as its menu, not a quit (`input_set_escape_quits(0)`), and takes the use key itself (`gfx_set_use_key(0)`, then `interact_request_use` on E), so the left button is free to attack.

The HUD is drawn at the card's 640 × 480. Classic's 320 × 200 pieces are drawn twice as wide and 2.4 times as tall, where classic put them, with the engine's pictures (`pic.h`, `docs/pictures.md`).

**Built (P1, `play/overlay.c`):**
- **The compass:** COMPASS.IMG's 322 × 14 strip scrolled under COMPBOX.IMG in the bottom right, as DFU's HUDCompass draws it. It scrolls 258 pixels a turn, with yaw 0 north and growing east. The pictures are read by `formats/img.c` under ART_PAL.COL.
- **A crosshair:** a "+" in the middle. It is DFU's, not classic's: classic had none. `overlay_set_crosshair` turns it off.
- **The message line:** the engine's `interact_say`.
- **The pause line:** shown while paused.

- **The vitals (P2):** MAIN03I0, MAIN04I0 and MAIN05I0.IMG (health, fatigue, spell points, 4 × 32 each), filled from the bottom to their share as DFU's HUDVitals does, where their files put them (x 49, 57 and 65, y 161).

Still to come: classic's bottom panel (MAIN00I0.IMG, 320 × 46) with the menu work, and the weapon (P4).

## Menus

**Built (P3, `play/menu.c`, `play/keys.c`, `play/screen.c`).** GFALL opens on classic's menus, then starts the hold.
- **Each menu is classic's own picture, where classic puts it.** Its buttons are where DFU's windows put them. Only the open menu's picture is on the card (`pic_load` on open, `pic_free` on close).
- **The pointer is classic's ARROW.RAW** (10 × 10, a cutout under ART_PAL.COL), drawn at the engine's pointer (`input_set_pointer`, `input_pointer`). A click is the left button (`INPUT_ATTACK`'s press).
- **The screens before any place** are the engine's screen frame: `gfx_prepare(0)`, then `gfx_frame(cam, 0)` each frame, with the game drawing through `gfx_set_extra`.

| Menu | Picture | Buttons |
| --- | --- | --- |
| Title | TITL00I0.IMG (its own palette) | any key or click goes on |
| Start | PICK03I0.IMG (its own palette) | Load (72, 45): "There are no saved games yet." (P7); Start a new game (72, 99): loads the hold under "Loading...", which fades in; Exit (125, 145) quits |
| Options | OPTN00I0.IMG at 85, 40, over the stilled world (`gfx_set_pause`) | Save, Load: later (P7); Exit, clicked twice, quits; Controls; Continue, or Escape, goes back to play. Classic's full screen, head bobbing, sound, music and detail are settings, left out |
| Controls | CNFG00I0.IMG | each action's key in classic's slot (47 × 7); a click on one waits for a key (`input_key_pressed`), and Escape leaves it be; a key another action has is swapped between them; Escape, the menu's, and the console's are refused (`input_bind`). Default puts classic's keys back; Continue, or Escape, goes back to the options |

**The controls' slots** are classic's for the actions the game has:
- forward, back, left and right (strafe), in the walk's columns;
- jump and run;
- rest;
- the swing (attack);
- the character sheet and the inventory;
- use (activate the centre);
- the travel map.

Classic's other slots (turning, crouching, spells, the log...) stay empty until their actions come.

**Bindings are saved** in `DATA\KEYS.DAT`, a record file (`docs/data.md`) of one `KEYS` record per action (id `INPUT_*`, its virtual key), when the controls screen closes with a change. They are read at start (`keys_load`) over classic's defaults (`keys_defaults`): W, S, A, D, the left button, E, Shift, Space, C, I, M, R.

On the 233: the title, the start menu, the pointer, the start into the hold and Escape's options run. The options' Exit did not quit at first: its question ("Click Exit again") was taken back every frame with no click, so the second click only asked again. It is now taken back only by a click on another button (`d403b0f`); not yet tried there. The controls screen is not tried there yet.

**Escape is the menu's only since the engine's 127d3d3.** Before that, the window's own Escape quit the program whatever the game had set, so the options never opened. Now both places that see Escape obey `input_set_escape_quits` (`docs/input.md`).

## Creatures and combat

The creatures already stand in a dungeon as entities (`docs/entities.md`): each has a kind (`world_entity_kind`) and a standing and a walking pose. The game adds:
- **Friend or foe** from the kind (monsters and most class people hostile in a dungeon).
- **A motor**: asleep until seen or heard, then turn to the player, walk to them (walking cells in a town, straight with the collider in a dungeon), and attack within reach. One step a frame for the near ones, under a budget.
- **The attack and the hit**: to-hit and damage per UESP's Combat page, as Daggerfall Unity's formulas have it (read for what they do). The weapon's swing is a picture drawn over the view.
- **Hurt and death**: the hurt and death records (5 to 14, 20 to 29; `docs/entities.md`, Not yet), and a body left on the floor with its loot.
- **The player's death**: the menu, load the last save.

Moving an entity's position and pose from game code, and the walker colliding with entities, are engine requests.

### Weapons and the pack (built: P4's first slice)

- **The records.** `ITEM`, `WEAP` and `MATL` (`tables/items.c`, `docs/data.md`) carry classic's 19 weapon templates, what the 18 swung ones add (damage, skill, CIF), and the 10 materials, as Daggerfall Unity's ItemTemplates, FormulaHelper and ItemBuilder have them.
- **An item** (`play/inventory.c`) is a template in a material, by ItemBuilder's SetItemPropertiesByMaterial: value × 3 × `value_q`; weight in whole quarter kilograms × `weight_q` / 4, rounded half to even (Unity's `Mathf.Round`: a steel longsword's 22.5 quarters are 22, 5.5 kg), back to kilograms; condition × `condition_q` / 4.
- **The pack** is the player's (`Player.pack`): up to 64 items and the one in hand. The start gives the warrior an iron longsword in hand, a stand-in until character creation (P8) gives classic's starting kit.
- **Console** (`play/commands.c`), in play only: `additem [weapon] [material]` (bare, the weapons and materials there are; the material first or last, iron when none; any case, spaces and dashes ignored: `additem dai katana ebony`), `items` (the pack, `*` the one in hand, with weight, value and condition), `equip n|none`.
- **Not yet:** to-hit and damage (the next slice), armour and the rest of the items (P5).

### The weapon on the screen (built: P4's second slice)

- **The file** (`play/weapon.c`): the hand's WEAP `cif`, `WEAPON##.CIF`; bare hands `WEAPON10.CIF`, the fist (`WEAPON11.CIF` is the werecreature's claws). `weapon_hold` follows the pack every frame and reads the file again only when the weapon or its material changes. The file lives in the CIF set's own heap and goes with it (`cif_open`).
- **Where it stands:** every frame where classic draws it on its 320 × 200 screen, from the file (`CifFrame.x`, `y`: an IMG's own header, a swing's bytes 6 and 8). Every frame's box sits on the screen's bottom. Drawn under the compass and the bars.
- **Cropped at load:** a swing's frames are all stored at the whole swing's box, and one frame fills about 30% of it on average. Each frame goes on the card cut to the rectangle of its drawn pixels (`cif_bounds`: index 0 is clear), drawn at its place moved by the crop's corner, so the same pixels land on the same screen pixels. Worked out from the file each time it is read, for any `WEAPON##.CIF` a mod puts in ARENA2; nothing is kept per weapon. `check_weapon` checks every frame's crop loses no pixel and is tight.
- **A click swings:** one of the file's swings at random (groups 1..6: down, down-left, left, right, down-right, up), not yet the mouse's direction. Its 5 frames show 3 × (115 − Speed) / 980 s each (`rules_swing_frame_time`, DFU's GetMeleeWeaponAnimTime), about a second for Speed 50; then the armed picture, with no frame held (DFU's FPSWeapon). A click during a swing does nothing; the click that closes a menu does not swing. The swing stands still with the world in the console and the menus.
- **A bow** has no armed picture: nothing shows at rest, and a click plays its one 7-frame shot through. Classic holds the drawn bow until the button is let go: not yet.
- **The material** recolours ART_PAL's grey ramp, 112..127 (`cif_metal`, DATFALL's colours, which follow UESP's names, not yet classic's dye tables).
- **The card:** the armed picture is kept; the swing's frame showing is sent when it shows and let go when the next one does (a frame every fifth of a second). A cutout goes as 8-bit P_8 with the chroma key on index 0 (the engine's `cc5ec07`, `docs/pictures.md`), or as 16-bit 1555 on a glide2x.dll without the key. Cropped and keyed, the largest frame is 39 KB, the bow drawn back (`WEAPON09.CIF`, 204 × 179); 65 KB whole, 78 KB cropped as 1555. The largest melee frame is the staff's, 271 × 125. With the HUD, the armed picture (under 13 KB) and the controls screen (66 KB) that is at most about 140 KB of `GAME_PICTURE_KB`'s 256 on a keyed card, about 195 KB without the key. `check_weapon` measures both cards and fails past 48 KB keyed or 96 KB at 1555.
- **Not the whole swing at once:** the largest swing's frames together are 196 KB even keyed (the bow's 7), so a swing stays one frame on the card at a time.

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
Daggerfall's tables are records in MAIN.DAT, cooked on the first run, with mods over it in a load order (`docs/data.md`). Built so far: enemy kinds, encounter tables, weather odds and looks, the calendar. Each table below comes with the system that reads it:

| Tables | Holds | From |
| --- | --- | --- |
| `LOCS`, `WMAP`, `CLIM` | the index of 4232 dungeons and 15251 towns; the heights, climates and locations' pixels; each climate's numbers | MAPS.BSA, WOODS.WLD, CLIMATE.PAK (they cut the 233's start-up) |
| `ITEM`, `MAGI`, `WEAP`, `MATL`, `ARMR`, `LOOT` | items, artifacts, weapons, materials, armour, loot | the exe (via DFU), MAGIC.DEF; pictures stay in ARENA2 |
| `CMON`, `CWEK`, `CALE` | the calendar (built) | the exe; the engine takes it whole (`clock_set_calendar`, request 9) |
| `RACE`, `CLAS`, `SKIL`, `SPEL`, `EFCT`, `ENCH`, `POTN`, `BIOG` | races, classes, skills, spells, effects, enchantments, potions, background | CLASS*.CFG, SPELLS.STD, BIOG*.TXT, the exe |
| `FACT`, `NAME`, `PEOP`, `POLI`, `GILD`, `LAW`, `PRIC`, `HOLI`, `FLAT`, `MUSI`, `SWAP`, `DISE`, `CURS` | factions, names, townspeople, politics, guilds, law, prices, holidays, flats, music, swaps, diseases, curses | FACTION.TXT, NAMEGEN.DAT, POLITIC.PAK, FLATS.CFG, the exe |
| `BOOK`, `TEXT`, `QUES` | indexes of the books, TEXT.RSC and the quests | BOOKS\, TEXT.RSC, QBN/QRC (their text and scripts stay in ARENA2) |

Blocks, models, pictures, sounds and songs are never cooked: they stay in ARENA2.

## Phases
The first goal is a **prototype**: from classic's start menu, Privateer's Hold played from start to finish, out to the first town and back after a save (P1 to P7). Then the rest of classic (P8 to P12). Then a mode of our own (P13). Each phase:
- is measured on the 233 with the profile's `play` stage;
- adds its rules' tests to `tests/check_play.c`;
- adds its numbers to MAIN.DAT;
- gets a view in LEVELFALL.

| Phase | What | Who | Done when |
| --- | --- | --- | --- |
| P0 | Foundation: the layers, MAIN.DAT and mods, dfrec; the engine's keys, bodies and rigs, pause, poses, calendar, `DATA\` | all | built |
| **P1** | **On foot** (built, runs on the target). GFALL rewritten as the game's single play state: the player a body under `rig_first`, walk, run, jump and fall, mouse look; the use key on doors and levers; HUD crosshair, compass and message line; Escape pauses | GAME | the hold walked from marker 10 to its exit on the 233 |
| P2 | A character (built; its bars run on the target): a fixed level-1 one, classic's attributes, skills and vitals; vitals bars on the HUD; `RACE`, `CLAS`, `SKIL` | GAME, ENGINE's pictures | derived values match UESP's worked figures |
| P3 | The start menu (built; the title, the start menu and the start into the hold run on the target, the options and controls not tried there yet): classic's main menu (PICK03I0.IMG: load, start, exit) before the hold, the title before it, Escape's menu in play (go on, exit) | GAME, ENGINE's pictures | GFALL opens on the menu, starts the hold, and exits from both |
| P4 | Combat: the weapon's swing from WEAPON##.CIF (the shown swing's frames only on the card), classic's to-hit and damage; creatures wake, chase on bodies of their size, attack, flinch, die; the player's death; `WEAP`, `MATL` | GAME; LEVELFALL shows each creature's state | the hold can be cleared, or the player killed, from records |
| P5 | Loot and inventory: corpses, treasure, gold, pick up, equip weapon and armour; `ITEM`, `ARMR`, `LOOT` | GAME, ENGINE's pictures | equipment changes combat |
| P6 | Out of the hold: `places/travel.c` lifted from LEVELFALL and shared; the hold's outside in the stream, the nearest town, its houses | GAME and LEVELFALL | the trip is one session |
| P7 | Prototype polish: loading holds, one save slot in `DATA\`, the menu's load, rest and wait, a 233 profile pass | GAME | from the menu to town and back after a save |
| P8 | Character creation and levelling: classic's screens, background questions, skill gains, level-ups | GAME | |
| P9 | Magic: spells, effects, spellbook, casting, potions, enchantments | GAME | |
| P10 | People: talk from TEXT.RSC, shops, banks, factions, reputation, law | GAME | |
| P11 | Quests: the QBN interpreter, guild quests | GAME | |
| P12 | The wider world: travel map, horse and cart, holidays, diseases, lycanthropy and vampirism | GAME | |
| P13 | Our own mode: a ruleset switch over classic's rules, a mode `.DAT` over MAIN.DAT; classic stays the default and the reference | GAME | |

Each phase updates this page (what is built, what is not) and AGENTS.md's layout list.
