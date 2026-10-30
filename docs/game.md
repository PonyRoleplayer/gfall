# The game (design)

What the game layer is, how it is built on the engine and Daggerfall's places, and the order it comes in. This is a design. Nothing in it is built yet except where it says so. Read `docs/architecture.md` first for the layers. Read this before adding anything to `src/game/dagger/play/` or rewriting `apps/gfall/`.

## What there is today

The engine draws, hears and walks any place. `src/game/dagger/` loads Daggerfall's places whole (`world_load_dungeon`, `world_load_inside`) or streams the whole map round the camera (`wild_open`, `wild_update`). Its doors swing, its levers run their chains, its creatures stand where their markers put them, and its townspeople walk.

LEVELFALL already goes through every passage: into a house and out, down into a dungeon and up, out of the stream and back. That code lives in the viewer (`lf_use`, `land_at`, `stream_inside`, `view_stream` in `apps/levelfall/main.c`) and leans on the viewer's own list rows (`pair_row`).

GFALL (`apps/gfall/main.c`) is the game: classic's title and start menu, then Privateer's Hold streamed by block, on foot, as an Argonian woman, with a weapon, armour, creatures that fight back and bodies to search (`src/game/dagger/play/`). There are no saves and no way out of the hold yet: the phases below say what is built.

## The pieces

Built, in `src/game/dagger/play/`:

```
game      one frame of play, Escape's pause, the use key
player    the player in the world: a body under rig_first, walk, run, jump, fall
character the sheet: attributes, skills, vitals, armour
rules     the sums: speeds, vitals, carrying (pure functions)
combat    the player's blows and a creature's, health kept in its keep row
foes      what a creature does: wake, chase, strike
inventory items, gold, what is worn and the armour it gives
loot      what a body or a heap of treasure holds
pile      what the player drops
weapon    the weapon in hand over the view
doll      the paper doll, drawn in memory
bag       the inventory screen, composed in memory, and its clicks
bagview   that screen on the card
overlay   the HUD: compass, vitals, crosshair
menu      the title, start, options and controls screens
keys      the bindings in DATA\KEYS.DAT
screen    classic's 320 x 200 on the card
sfx       DAGGER.SND's clips for combat
commands  the game's console commands
```

Still to come: `save` (P7), and `places/travel.c` (P8), which LEVELFALL's view code has today.

The player is Daggerfall's, so it lives here and not in the engine. So do items and the inventory: the engine is simulation and world. It gives a camera, a walking body (`body`), using things (`interact`) and entities (`entity`), and `play` builds Daggerfall's player out of them. Another game would build its own in its own folder. Each is a module of one `.c` and one `.h`, as in `docs/architecture.md`. What can be plain C is, and is host-tested in `tests/check_play.c`. What draws or reaches the keys (`PLAY_DRAW_SRC`) is checked by running GFALL.

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

Tested in the hold: all 105 things keyed, no key twice, a door and a lever left open come back open after a reload, a door shut again loses its row. Nothing leaves a place in GFALL yet (P8), and the streamed world's towns are not keyed yet (their chunks, with the streamed dungeons).

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
  - Until creation (P6), the game takes the rolls' middles (`RULES_*`), and starts an **Argonian woman, a warrior** (`CHARACTER_START_*`): strength 65, speed 52, health 45, fatigue 120, 24 spell points. A class sets these numbers, so a Breton warrior has the same ones, and the tests' sums worked by hand stay a Breton's.
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
- **A fall** past 5 m costs 5 health a metre (PlayerHealth); "That hurt." A fall to 0 health says "You have died." and, until saves (P7), puts the player back at the start, whole.

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
- **The pack** is the player's (`Player.pack`): up to 64 items and the one in hand. The start gives the warrior an iron longsword in hand, a stand-in until character creation (P6) gives classic's starting kit.
- **Console** (`play/commands.c`), in play only: `additem [weapon] [material]` (bare, the weapons and materials there are; the material first or last, iron when none; any case, spaces and dashes ignored: `additem dai katana ebony`), `items` (the pack, `*` the one in hand, with weight, value and condition), `equip n|none`, `tai` (the creatures' AI on or off, below), and `tgm` (god mode: nothing takes the player's health, falls or blows, all through `player_hurt`; alone it flips, as Bethesda's consoles do; off with each new game).
- **Armour and gold** came with P5: Loot and the inventory, below.

### The weapon on the screen (built: P4's second slice)

- **The file** (`play/weapon.c`): the hand's WEAP `cif`, `WEAPON##.CIF`; bare hands `WEAPON10.CIF`, the fist (`WEAPON11.CIF` is the werecreature's claws). `weapon_hold` follows the pack every frame and reads the file again only when the weapon or its material changes. The file lives in the CIF set's own heap and goes with it (`cif_open`).
- **Where it stands:** every frame where classic draws it on its 320 × 200 screen, from the file (`CifFrame.x`, `y`: an IMG's own header, a swing's bytes 6 and 8). Every frame's box sits on the screen's bottom. Drawn under the compass and the bars.
- **Cropped at load:** a swing's frames are all stored at the whole swing's box, and one frame fills about 30% of it on average. Each frame goes on the card cut to the rectangle of its drawn pixels (`cif_bounds`: index 0 is clear), drawn at its place moved by the crop's corner, so the same pixels land on the same screen pixels. Worked out from the file each time it is read, for any `WEAPON##.CIF` a mod puts in ARENA2; nothing is kept per weapon. `check_weapon` checks every frame's crop loses no pixel and is tight.
- **A click swings:** one of the file's swings at random (groups 1..6: down, down-left, left, right, down-right, up), not yet the mouse's direction. Its 5 frames show 3 × (115 − Speed) / 980 s each (`rules_swing_frame_time`, DFU's GetMeleeWeaponAnimTime), about a second for Speed 50; then the armed picture, with no frame held (DFU's FPSWeapon). A click during a swing does nothing; the click that closes a menu does not swing. The swing stands still with the world in the console and the menus.
- **A bow** has no armed picture: nothing shows at rest, and a click plays its one 7-frame shot through. Classic holds the drawn bow until the button is let go: not yet.
- **The material** recolours the picture by classic's tables (`cif_metal`; Loot and the inventory, Colours).
- **The card:** the armed picture is kept; the swing's frame showing is sent when it shows and let go when the next one does (a frame every fifth of a second). A cutout goes as 8-bit P_8 with the chroma key on index 0 (the engine's `cc5ec07`, `docs/pictures.md`), or as 16-bit 1555 on a glide2x.dll without the key. Cropped and keyed, the largest frame is 39 KB, the bow drawn back (`WEAPON09.CIF`, 204 × 179); 65 KB whole, 78 KB cropped as 1555. The largest melee frame is the staff's, 271 × 125. With the HUD, the armed picture (under 13 KB) and the controls screen (66 KB) that is at most about 140 KB of `GAME_PICTURE_KB`'s 256 on a keyed card, about 195 KB without the key. `check_weapon` measures both cards and fails past 48 KB keyed or 96 KB at 1555.
- **Not the whole swing at once:** the largest swing's frames together are 196 KB even keyed (the bow's 7), so a swing stays one frame on the card at a time.

### Striking creatures (built: P4's third slice)

`play/combat.c`, by Daggerfall Unity's FormulaHelper (CalculateAttackDamage, CalculateSuccessfulHit, CalculateWeaponAttackDamage, CalculateHandToHandAttackDamage, RollEnemyClassMaxHealth, DamageModifier), EnemyEntity, WeaponManager and FPSWeapon, read for what they do.

- **When:** once a swing, on its hit frame: frame 2 of a melee swing, 5 of the bow's (`weapon_hit`). The swing costs 11 sixty-fourths of fatigue then, struck or not.
- **What:** classic's (FALL.EXE): every live creature of the stream whose feet are under 91 units from the player's (2.25 m), within 35.156 degrees (200 of 2048) of the player's facing on the ground (any way nearer than 11 units), that no wall hides: a ray from the eye to its middle on the scene's collision (`combat_reaches`, `collide_use`, `collide_ray`, as `rig_third`'s). Each is struck by its own blow. The view's pitch does not matter. The swing's sound plays only when none was struck.
- **The creature as the sums see it** (`combat_foe`): a monster's level and armour value × 5 from ENEM; a class enemy at the player's level and armour 60 (DFU's cap for worn armour, as it wears nothing yet); Dodging 5 a level and 30, at most 100; its attributes from ENEM (`docs/data.md`).
- **To hit:** the weapon's skill (Hand-to-Hand bare), the swing's (down −10, down-left −5, left and right 0, down-right +5, up +10), the material's modifier × 10, a Dark Elf's level / 4, a Redguard's level / 3 or a Wood Elf's with a bow, the Backstabbing skill when it faces away, its armour, (Luck − its Luck) / 10, (Agility − its Agility) / 10, less its Dodging / 4, Critical Strike / 10 on a roll under that skill, +40 against a creature, −50; held to 3..97, rolled on 100. A Breton warrior at a rat with an iron longsword swung left: 31%.
- **Damage:** the weapon's range, the swing's (down +4, down-left +2, down-right −2, up −4) and the race's, a skeletal warrior halving a blunt weapon's and doubling silver's, floor((Strength − 50) / 5), the material's modifier; below 1 is 0. Bare: Hand-to-Hand / 10 + 1 to / 5 + 1, the swing's, the strength's. A backstab triples it on a roll under the skill.
- **A metal too poor** for the creature (ENEM's `min_metal`: a ghost needs silver) does nothing: "The material of the weapon you are using is ineffective." (classic's line). A backstab says "Successful backstab!"
- **Health** is rolled the first time a creature is struck (a monster between ENEM's least and most; a class enemy 10 and a roll of 1..its class's hit points for each level) and kept in its keep row's game bytes (health in two bytes, then 1 once rolled), so it stays wounded or dead as its block goes and comes back.
- **Struck and living:** it flinches, its hurt pose for half a second. **Killed:** it lies as its body, on the floor under it (a flyer falls), passed through (radius 0); with no body's picture it goes. A body come back with its block is laid again (`combat_frame`).
- **Sounds** (`play/sfx.c`, DAGGER.SND by DFU's SoundClips): a hit, Hit1..5 (108..112) at the creature; a blow that takes nothing, a parry (Parry1..9, 428..436) from a creature that parries (ENEM's `parries`) struck by a weapon, else the swing; a swing that reaches no creature, the swing: SwingMediumPitch (347) with a weapon, SwingHighPitch (106) bare (DFU's FPSWeapon default and its bare-hands fallback). A creature killed: BodyFall (15) where it lies, our choice; DFU's death sound was not found.
- **Not yet:** blood and knock-back; skills advancing; the class's proficiencies and group bonuses; creatures of a place loaded whole.

### Creatures awake (built: P4's fourth slice)

`play/foes.c`, by Daggerfall Unity's EnemySenses, EnemyMotor and EnemyAttack, read for what they do.

- **Asleep** where its marker stood until it sees the player: within 4096 units (classic's sight), anywhere not behind it (180 degrees), no wall between its eye (two thirds up) and the player's (`combat_sight`). Two sleepers look a classic tick (about 18 a second, `FOES_TICK`), round the stream's creatures. A passive one (its marker's action byte, `dunstream_passive`) wakes only when struck. Struck, any creature wakes (`foes_alert`).
- **Awake**, it has a mind (24 at most): a body a quarter of its picture's width across (8 to 24 units), its height tall, under classic's gravity (a flyer, none), solid to the player and the others (`Entity.radius`). Waking, it barks (ENEM's `bark_sound`).
- **Each classic tick** it looks again every sixth (DFU's reacquiring), turns at most 20 degrees toward where it last saw the player, counts down 200 ticks unseen and then gives up (asleep where it stands), and may start a swing: the player seen, within 2.25 m, within 22.5 degrees of its facing, its melee timer run out, and a roll on its Speed (at least 8) under Speed / 8 + 6, as classic's AI has it (about one tick in four at Speed 50; see Checked against classic). It plays its attack sound.
- **Each frame** it walks along its facing at (Speed + 150) / 39.5 m/s (`rules_walk_speed`) toward where it last saw the player, stopping short of reach (0.85 of 2.25 m) while it sees them; a flyer rises or sinks to the player's feet at the same speed. A move sound every 2 to 5 s while it walks (our interval). A flinch stands it still, and cancels a swing begun.
- **Its swing** is its attack pose (records 5 to 9) at 10 frames a second, held by the game frame by frame; the blow lands on its third frame (index 2, our choice: DFU takes the frame from each kind's animation) if the player is still within 2.25 m and 0.25 m, and within 35.156 degrees of its facing (or within 0.25 m). Rolled by `combat_foe_blow`: a monster's three attacks each rolled to hit and summed, a class enemy's bare hands. The melee timer: 1500 to 3000, less 50 a player level past 10, reflexes average, over 980: seconds.
- **The player hit:** Hit1..5 flat and the race's and sex's pain clip (DFU's, three each); missed, the high swing at the creature. Health through `player_hurt`, so `tgm` keeps the player whole. A blow that kills: "You have died.", back at the start whole, every creature asleep again where it stands (`die` in `game.c`, the fall's too), until saves (P7).
- **The console's `tai`** switches them off: no one wakes, moves or strikes, and a swing begun stops. Kept across games.
- **Far creatures move less often** (`foes_due`). A creature within `FOES_NEAR` (12.8 m) of the player moves every frame; past it every second frame, and past `FOES_FAR` (25.6 m) every fourth, staggered by its mind's number. The time it did not move is kept and spent by its next move, so it covers the same ground. Senses and blows are unaffected: a blow only lands within 2.25 m, and senses keep classic's ticks.
- **On the 233:** a near mind steps a body (a collision gather and sweep) every frame and looks along a ray every sixth tick; not measured there yet.
- **Not yet:** hearing, stealth and the give-up search past the last point seen; doors opened or bashed by creatures; ranged attacks and spells; the creature's hurt voice; wandering; creatures of a place loaded whole.

### Checked against classic (FALL.EXE, decompiled)

`daggerfall/decompiled/` is not authoritative (AGENTS.md), and nothing of it is copied: it was read, by its constants, to check DFU's numbers where they were a guess or a paraphrase. What it showed:

- **The swing roll:** the AI's decision routine starts a melee swing when the melee timer is 0 and a roll modulo Speed is *under* Speed / 8 + 6 (about one tick in four at Speed 50). `foes.c` had it the other way round (three in four); fixed.
- **Confirmed as built:** the melee timer (1500 to 3000, less 50 a player level past 10, 450 a step of reflexes past 2); reach 90 units against the player (60 against another creature); the give-up count, 200; a blow lands within 91 units, at any angle within 11, else within 200 of 2048 (35.156 degrees) of the attacker's facing; a creature's death plays sound id 17 (DAGGER.SND's position 15, BodyFall) at it, never at the player; a blow that takes health plays id 378 + 0..4 (positions 108..112, Hit1..5). Classic names clips by record id; `sndfile_by_id` maps them.
- **Classic's messages,** from FALL.EXE's strings: "The material of the weapon you are using is ineffective." and "Successful backstab!", now shown.
- **The player's strike** goes through the same routine: classic strikes every creature within 90 units and 35 degrees of the player's facing (on the ground, not the view's pitch), each counted, and plays the swing only when it counted none. `combat.c` took only the nearest near the line of the view, as DFU's box does; it now strikes as classic does.
- **Not found:** the frame of a creature's attack its blow lands on (still the third, our choice), and how often a walking creature plays its move sound (still 2 to 5 s, ours).

## Loot and the inventory (built: P5)

**Inventory is the game's, not the engine's.** The engine is simulation and world: it holds, moves and draws what stands in a place. What an item is, what is carried and what a body holds are the game's, in `play/`. The engine's generic `scene/pack.c` is not used: there weight belongs to a kind of item and wearing is 16 slots summed into 4 stats, where here weight and value belong to one item, by its material, and armour counts per body part.

### Items and armour

- **The records.** `ITEM` holds 31 templates: the armour (102..112), the weapons (113..131) and gold (276). `ARMR` says what an armour template adds. `MATL` gains leather and chain, which only armour comes in, what each material's armour gives, and a second value multiplier.
- **A piece's numbers** (ItemBuilder.ApplyArmorMaterial): plate as a weapon's material; leather half the weight, the template's price; chain the template's weight, twice the price.
- **Armour** (DaggerfallEntity.UpdateEquippedArmorValues). Each of the seven body parts starts at 100, lower being better. A piece takes 5 × its material's armour (iron 7, steel 9, daedric 21, leather 3, chain 6) off its part. A shield takes 5 × its own (1..4, whatever it is made of) off each part it covers.
- **Armour changes combat.** A creature's blow comes at one part by classic's odds (of 20: head 2, each arm 3, chest 4, hands 4, legs 3, feet 1), and that part's value goes into its chance to hit. A rat's 83% at a bare warrior is 48% through iron.
- **Colours** (`cif_metal`, DFU's ImageProcessing.GetMetalColorTable, classic's own tables). Weapons and armour are drawn in the sixteen palette indices from 0x70, which are steel as they stand; each other metal swaps those sixteen by its table, on the doll, in the lists and in the swing alike. Leather and chain keep their picture's colours (ItemHelper.GetArmorDyeColor gives them none). A picture's pixels outside those sixteen stay as drawn: a sword's hilt is always gold, and two of the six helms (variants 0 and 3) are gold all over, whatever they are made of, so `additem helm daedric`, which takes the first variant, shows a gold helm. Clothing's dyes, the sixteen from 0x60, come with clothing. `make hostcheck` writes every metal side by side to `build/dye0.ppm`.
- **Wearing.** A weapon goes in the right hand. The left hand takes only a shield until a second weapon can swing. A two-handed weapon takes the shield off, and a shield takes a two-handed weapon out of the hand.
- **Carrying:** Strength × 1.5 kg (UESP); past it a thing cannot be taken. A pack's gold weighs 400 coins a kilogram. Classic's weight does not slow the walk (DFU's PlayerSpeedChanger takes no weight).

**Not yet:** condition wearing down, a class's forbidden materials and armour, clothing, jewellery and everything else in the other tabs, class enemies wearing what they carry.

### What can be searched

`play/loot.c`, by LootTables.GenerateRandomLoot and FormulaHelper's RandomMaterial and RandomArmorMaterial.

- **The use key** searches what the view rests on within 3.2 m (ours), no wall between (`loot_find`): a dead creature, a heap of treasure, a dropped pile. Otherwise it does what it always did. With nothing to find it says "You find nothing."
- **Treasure** (`dunstream.c`, DFU's RDBLayout). A block's random treasure markers (archive 199, record 19) and its fixed flats of TEXTURE.216 stand as entities, keyed like its creatures, not as flats, so they can be searched and can go. A random marker's heap lies on the floor under it, its picture one of DFU's 20 (`randomTreasureIconIndices`), chosen by the marker's offset so it is the same each time it is seen. It fills from the table the dungeon's type names (`LDUN`, `dungeon_loot`): a human stronghold's is N. Emptied, it goes for good (`Entity.dead`, kept); a body stays where it fell. The hold has 8. A place loaded whole still draws its treasure flats as flats, and nothing searches them: GFALL only plays the stream.
- **The rolls,** under the table the creature's kind names (`ENEM.loot`, `LOOT`) or the dungeon's type does: gold between the table's two figures, times the player's level; then weapons, then armour, each rolling again at half the chance after a success. A weapon or piece is any of its kind alike; armour is leather 69%, chain 20%, else plate; plate and weapons take their material from a roll of 0..255 moved by the level.
- **A body or a heap holds no list.** Its things are worked out when it is opened, from the game's seed (`loot_new_game`) and its own key, on its own dice, so nothing else rolled changes them. A dungeon of them costs no memory until one is searched. Its keep row holds the rest in its game bytes (`COMBAT_KEEP_*`): the player's level when it died or was first opened, and a bit for each thing taken. So what was taken stays gone as its block goes and comes back, and a save only has to keep the seed and the rows. The price: they can only be emptied, not given things.
- **Not yet:** ingredients, books, potions, maps; the other columns of `LOOT` are read in but not rolled.

### Dropped piles

`play/pile.c`. Dropping is classic's: Remove moves a thing to the right list, and when the screen closes those things land as a pile at the player's feet (DFU's CreateDroppedLootContainer). Near a pile already there (48 units, ours) they join it. Searched again, a pile takes things and gives them back, and with nothing in it, it goes.

- **Where it stands.** A pile is one of its block's `DUNSTREAM_SPARES` (4) spare things: entities the stream builds with every block, not drawn until used. Being the block's entities, the stream moves them with the origin and keeps their place when the block goes and comes back, so the game needs no list of positions and the engine no new call. A spare's keep row says it is used (`DUNSTREAM_SPARE_USED`), which `changed` in `dunstream.c` reads to draw it again. A block's fifth pile joins its nearest.
- **What is in it** is one pool of 256 items in `pile.c`, each marked with its pile's key, so a block gone from memory loses nothing and a save only writes the pool. Past 256 the screen says there is no room. With no block loaded round the player the things go back in the pack: nothing dropped is ever lost.
- **The card:** each block holds its 4 spare pictures whether used or not, a few KB of its picture residency.

### The inventory screen

`play/bag.c` is what the screen shows and what a click does, plain C and host-tested. `play/doll.c` draws the paper doll. `play/bagview.c` puts the result on the card. The inventory key (I) opens it, the use key on a body opens it with the body's things in the right list, and Escape, the inventory key or Exit closes it.

- **Everything is classic's picture at classic's place,** by DFU's DaggerfallInventoryWindow rectangles: INVE00I0.IMG the screen, INVE01I0.IMG the chosen tab and button in gold, INVE06I0 and INVE07I0 a list's green and red arrows, ITEM00I0 the stone behind the small panel, FONT0003.FNT the labels (`formats/smallfont.c`) in classic's ink, index 146.
- **The doll** (`doll_compose`, DFU's PaperDollRenderer): the race's SCBG background cropped at 8, 7, the body, its underclothes, the head, then what is worn by the template's draw order. Each picture's own header places it, less classic's doll corner 200, 8. Armour's archive is 245 for a woman and 249 for a man, plus the body shape (Argonian 0, elf 1, human 2, Khajiit 3). A worn picture's index 255 is a mask: it shows the background again, which is how a helm takes off the horns under it. In a list it is only clear.
- **The lists** show four rows of 50 × 38. A thing worn is on the doll, not in the list. Clicks go by the mode: Equip wears, Remove drops, Info shows a box. A click on the right list takes the thing, a click on the doll takes it off, found by a byte per doll pixel saying which worn place drew it.
- **Ours, not classic's:** the "Carrying", "Dropped", "Body", "Treasure" and "Pile" words over the lists, and the info box, a dark box where classic has a scroll of TEXT.RSC lines. Wagon, Use and Gold say why they do nothing yet.

**What it costs the 233.** The whole screen is composed in memory into one 320 × 200 picture, only when something on it changes: a click, or the pointer coming to rest on another thing. An open frame sends nothing to the card and draws that one picture's 29 quads, however full the lists are. It takes the 66560 bytes of `GAME_PICTURE_KB` the controls screen already counts on, not one more. A fitted item picture is kept in one of 8 places, so a list scrolled back reads nothing. While it is open `gfx_frame` gets no level (`game_world_shows`, the controls screen too): no world is drawn only to be covered. All this is by the design's sums and host tests: **none of it has run on the 233 yet.**

**Memory:** two heaps of its own while open, one for its 64 KB pictures and buffers, one for what it only reads to copy, let go past 512 KB. Both go at close, and any TEXTURE file read into them leaves the cache with them (`texture_forget`).

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
| `ITEM`, `MAGI`, `WEAP`, `MATL`, `ARMR`, `LOOT`, `LDUN` | items, artifacts, weapons, materials, armour, loot, each dungeon type's loot table | the exe (via DFU), MAGIC.DEF; pictures stay in ARENA2 |
| `CMON`, `CWEK`, `CALE` | the calendar (built) | the exe; the engine takes it whole (`clock_set_calendar`, request 9) |
| `RACE`, `CLAS`, `SKIL`, `SPEL`, `EFCT`, `ENCH`, `POTN`, `BIOG` | races, classes, skills, spells, effects, enchantments, potions, background | CLASS*.CFG, SPELLS.STD, BIOG*.TXT, the exe |
| `FACT`, `NAME`, `PEOP`, `POLI`, `GILD`, `LAW`, `PRIC`, `HOLI`, `FLAT`, `MUSI`, `SWAP`, `DISE`, `CURS` | factions, names, townspeople, politics, guilds, law, prices, holidays, flats, music, swaps, diseases, curses | FACTION.TXT, NAMEGEN.DAT, POLITIC.PAK, FLATS.CFG, the exe |
| `BOOK`, `TEXT`, `QUES` | indexes of the books, TEXT.RSC and the quests | BOOKS\, TEXT.RSC, QBN/QRC (their text and scripts stay in ARENA2) |

Blocks, models, pictures, sounds and songs are never cooked: they stay in ARENA2.

## Phases
The first goal is a **prototype**: from classic's start menu, Privateer's Hold played from start to finish, out to the first town and back after a save (P1 to P8). Then the rest of classic (P9 to P12). Then a mode of our own (P13). Each phase:
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
| P4 | Combat (built; the swing runs on the target, striking and creatures awake not tried there yet; the player's death is back to the start until saves; LEVELFALL's view of a creature's state not yet): the weapon's swing from WEAPON##.CIF (the shown swing's frames only on the card), classic's to-hit and damage; creatures wake, chase on bodies of their size, attack, flinch, die; the player's death; `WEAP`, `MATL` | GAME; LEVELFALL shows each creature's state | the hold can be cleared, or the player killed, from records |
| P5 | Loot and inventory (built, host-tested, not run on the target; the other tabs' items not yet): bodies, treasure, dropped piles, gold, pick up, equip weapon and armour, classic's inventory screen and paper doll, classic's metal colours; `ITEM`, `ARMR`, `LOOT`, `LDUN` | GAME, ENGINE's pictures | equipment changes combat |
| P6 | Character creation and levelling: classic's screens, background questions, skill gains, level-ups; the character's attributes and the damage rules polished | GAME | |
| P7 | Prototype polish: loading holds, one save slot in `DATA\`, the menu's load, rest and wait, a 233 profile pass | GAME | from the menu through the hold and back after a save |
| P8 | Out of the hold: `places/travel.c` lifted from LEVELFALL and shared; the hold's outside in the stream, the nearest town, its houses | GAME and LEVELFALL | from the menu to town and back after a save, in one session |
| P9 | Magic: spells, effects, spellbook, casting, potions, enchantments | GAME | |
| P10 | People: talk from TEXT.RSC, shops, banks, factions, reputation, law | GAME | |
| P11 | Quests: the QBN interpreter, guild quests | GAME | |
| P12 | The wider world: travel map, horse and cart, holidays, diseases, lycanthropy and vampirism | GAME | |
| P13 | Our own mode: a ruleset switch over classic's rules, a mode `.DAT` over MAIN.DAT; classic stays the default and the reference | GAME | |

Each phase updates this page (what is built, what is not) and AGENTS.md's layout list.
