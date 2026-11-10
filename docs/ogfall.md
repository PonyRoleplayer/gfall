# OGFALL

The current [missing-work list and target playtest checklist](ogfall-roadmap.md#what-is-still-missing)
are recorded at the top of the full continuation plan. They distinguish the
implemented Hold slice, including combat and Rest, from the systems still to build.

A second program, `OGFALL.EXE`, beside GFALL. It starts a new character through the menus FALL.EXE uses, then walks Privateer's Hold on foot with that executable's HUD, cursor, and movement rules. The byte-matched decompilation at `/home/shade/git/daggerfall-decomp` (FALL.EXE 1.07.213) is the authority for those rules, including the bugs named below. The gfall engine draws, collides, loads Glide, and reads the files. Nothing from XnGine is ported: not `src/xngine/`, `src/xngine_c/`, or `src/engine/` of the decomp.

The original slice-1 plan is retained below as history; current implementation
and verification are described in the dated status sections, through Pile clicks,
creature variants and Rest. GFALL's design stays in `docs/game.md`.

## Current build and verification

Build the Windows program with `make ogfall-check`, run its focused native suite
with `make ogfall-host`, or build and validate all programs with `make all`.
`build/gfall.iso` contains OGFALL.EXE; game data and the system Glide driver stay
on the target machine. OGFALL's runtime files are `ogfall/settings.cfg` and
`ogfall/saves/S0001.SAV` onward beside the executable. God mode is the process-local
console command `tgm`.

The 2026-11-08 implementation passed focused OGFALL tests, full hostcheck,
fresh-process save restores, layer checks, Win98/Pentium executable validation
and OGFALL's stack-probe check. The user has exercised saving/loading and reported
improved audio on the target. Remaining target acceptance and full-game work are
listed in the roadmap; host results do not establish target appearance or speed.

## 1. What done means

A player can do all of this on the host tests and, for the picture and the doors, on the Win98 build:

1. Boot to the classic title. The keys are `L`, `S`, and `E` (`title_menu` in `src/hand/func_0003A3CE.c`, the key list noted there as "LSE"). `S` or a click in (74,100)–(217,112) starts a new game. `E`, Escape, or a click in (125,145)–(165,158) quits. `L` or a click in (72,46)–(217,58) opens the real six-slot OGFALL load screen (see the current Options/save section below).
2. Character creation follows the path `tools/scenarios/newgame.txt` drives, and accepts the same clicks: province picked by the colour under the cursor on the map image, the race's confirm text (TEXT.RSC record `2000 + index`), gender, a class chosen from the list (the acceptance character is a male Breton Battlemage), the generated background, the reputation summary, a typed name, face OK, ten bonus points into one attribute, six bonus points in each skill group, reflexes left at average, summary OK. Escape on the province map returns to the title. Gold is set to 100 before the background, as `title_menu` does. The class questions, the class maker, and the biography questions are on those screens so the other button is not dead; the acceptance test is the list and the generated background.
3. The intro VID is not played. `newgame.txt` skips it with Escape. From the summary's OK the game grants starting equipment (`chargen_give_starting_equipment` in `src/lifted/intro.c`) and lands in Privateer's Hold.
4. The landing is the decomp's, not a cleaned one. `dungeon_load` (`src/lifted/maplogic.c`) stands on editor marker 8. `newgame_place_player` (`src/lifted/init.c`) then moves to the nearest editor marker 6 when any exist. `world_start` already returns marker 8, else 10, and is not changed. The app applies the marker-6 rule on top.
5. On foot in the hold, with the classic HUD bar up: portrait, three vital bars, compass, interaction-mode icon. The mouse is the cursor scheme (`mouse_control_mode` 0): a 3 by 3 steer grid while the left button is held, arrows doing the same regions, the arrow drawn on the bar. View mode (`mouse_control_mode` 1) is reachable from Options and looks with the mouse.
6. Doors and levers in the hold open in grab mode by the cursor ray (right button while the weapon is sheathed). The centre-activate key does the same from the crosshair. Info, steal, and talk change the mode byte, the icon, and the status line. They do not open talk, shop, or crime screens.
7. Health, fatigue, and magicka are real numbers. A jump spends 11 fatigue. A hard landing applies the classic fall-damage expression. The bars use `hud_draw`'s integer expression.
8. Options (Escape, or the options strip) shows Continue, Exit Game, the fullscreen bit, the head-bob bit (stored; the bob offset may be a later edit in the same movement file), and Cursor versus View. Continue returns to the hold with the world stilled while the screen was up.

The original slice did not include these screens. Inventory, the sheet and spellbook are now implemented below; rest, the automap, the travel map, transport, the status pop-up, combat, creature AI and the quest machine remain for later slices. A click on those buttons plays click sound 203, sets the status line where the decomp does, and leaves the world as it was. Weapon-ready flips the flag and plays the equipped weapon's sound; it does not draw a swing. Creatures are placed and their pictures load. They do not wake, chase, or strike.

`title_menu` calls `quests_start_initial` after the landing. Slice 1 does not run quests. The owner of `play/title.c` reads that function once: if it puts a message on screen before the player can walk, that one box is slice 1 and is named in the module comment; if it only sets flags, it is skipped and the comment says so.

## 2. Authority

Cite the decomp. Do not paste lifted functions into this repo. The lifter's pointer casts are not bugs to keep. One example, so it is not copied: `click_activate` in `src/lifted/intrface.c` picks at the cursor, and in cursor mode the pick is six pixels down and right of `mouse_x`/`mouse_y` (the hotspot). The Y argument is written as a cast of an address. Implement the hotspot. Do not pass a pointer as a coordinate.

| Behavior | Decomp |
| --- | --- |
| Title, race map, class list, background, name, then the dungeon | `src/hand/func_0003A3CE.c` `title_menu` |
| Where a new game stands | `src/lifted/init.c` `newgame_place_player`; `src/lifted/maplogic.c` `dungeon_load` |
| Starting items | `src/lifted/intro.c` `chargen_give_starting_equipment` |
| HUD draw, vital bars, portrait overlay | `src/hand/func_0005CE17.c` `hud_draw`; `src/hand/func_0005D151.c` `hud_draw_compass`; `src/hand/func_0005DB95.c` `hud_draw_heading_strip` |
| HUD clicks | `src/lifted/icons.c` `hud_update`, `hud_buttons_click`, `hud_portrait_overlay_index` |
| Button table | `config/names.csv` rows for `hud_buttons` (confirmed): 11 rects, strict `>` / `<` |
| Mode cycle | `src/hand/run_00042F0F.c` `interaction_mode_cycle`: `(mode + step) & 3`, then a status line |
| Steer keys and the 3 by 3 grid | `src/lifted/intrface.c` `intrface_key_region`, `intrface_set_regions`, `player_movement_update`; `docs/state.md` "Input, movement and the interface"; `docs/play.md` |
| Activate | `src/lifted/intrface.c` `click_activate` |
| Jump speed | `src/lifted/intrface.c` `player_compute_jump_velocity` |
| Options shell | `src/lifted/options.c` `options_frame`, `options_draw` |
| What the keys are by default | `docs/play.md` "Keys"; the 38-action `key_map` (`docs/state.md`, address `D_00195EB0`) |

XnGine calls in those files (`xn_draw_image`, `xn_mouse_poll_clamped`, `engine_pick_object`, `xn_render_frame`, VID playback) are replaced by the gfall calls in section 7. Do not reimplement them.

`kludge_fix_dungeon_door` runs inside the decomp's `dungeon_load`. Slice 1's doors are the ones `world_load` already builds. That kludge is not re-coded, and `doors.c` is not edited to match it. If a hold door disagrees with the decomp, write it down; do not change the shared door builder in this slice.

## 3. Ordered steps

ENGINE goes first and is short: confirm section 7, report "no engine change", and stop. GAME must not start the Makefile until that report is in, so a real gap can land before the exe is wired. The pure functions in steps 2–4 do not need the exe and can be written during the review.

1. ENGINE reads section 7 against the headers and reports to the supervisor. No source edit on a confirmation.
2. GAME: host-tested pure functions. Steer regions and key-to-region (`play/steer.c`). Vital-bar height, fall damage, jump speed and the 11-point fatigue cost (`play/vitals.c`). HUD hit tests with the strict inequalities (`play/hud.c`). Title and option hit tests (`play/title.c`, `play/options.c`). Marker-6 choice given a plain list of marker ids and positions (`places/place.c`).
3. GAME: character record and chargen state machine (`play/chargen.c`, `tables/tables.c`), driven by scripted clicks in the host test, including the Battlemage path and Escape back to the title. No pictures yet: the test feeds classic 320×200 coordinates and reads the mode and the character.
4. GAME: movement policy on top of `body` (`play/move.c`). Host test on a fake scene the engine's body tests already know how to build, or on the hold if `make hostcheck` can see ARENA2 (it can: hostcheck already loads the hold). Walk, stop at a wall, jump, land, crouch. Creatures are not stepped.
5. GAME, after step 1: `apps/ogfall/seam.c` and `apps/ogfall/main.c`, and the Makefile target in section 6. `make ogfall-check` passes the PE check. `make hostcheck` still passes, including the new file.
6. GAME: draw the title, chargen, HUD, and options through `pic_*`, with the seam loading the IMG, CIF, BSS, and font bytes. Fullscreen bit hides the bar and uses the heading strip. Cursor mode and view mode both work in the hold. A door in grab mode swings.
7. Supervisor accepts slice 1 when section 1 is true. Nobody commits unless the user asks.

## 4. GAME engineer

Owns `src/game/ogfall/` and `apps/ogfall/`. Does not edit `src/engine/`, `src/game/dagger/`, `apps/gfall/`, or `docs/pony97.md`. Does not include a dagger header from anywhere under `src/game/ogfall/`.

Fill the stubs. Do not add a second tree. `formats/formats.c` stays empty apart from its startup: slice 1 has no format parser of its own.

### Files

| File | Decomp behavior | Engine calls |
| --- | --- | --- |
| `apps/ogfall/main.c` | The frame loop only. Window, `input_read`, `ogfall` frame, `gfx_frame`. | Same shape as `apps/gfall/main.c` and `apps/pony97/main.c`: `WinMainCRTStartup` via `win_startup`, `mem_init`, dt from the performance counter clamped to a tenth. |
| `apps/ogfall/seam.c` | The only translation unit that includes dagger. Loads the hold, pictures, classes, race text, the click sound, and editor markers. | See section 5. |
| `play/play.c` | Which screen has the input. World runs only in mode 0. | `gfx_set_pause` while a screen is up. `gfx_set_extra` draws the HUD. `hud_set(0)` so the engine's position line stays off. `gfx_set_use_key(0)` so E is not "use". |
| `play/screen.c` | Classic 320×200, origin top-left, over the view. | `docs/pictures.md`: a 320×200 picture at 2× is `pic_draw(id, 0, 440, 640, 400)` on 640×480. Mouse hits divide back into classic pixels, letterbox included. |
| `play/title.c` | `title_menu` through the race map's return to the title, the load button, and exit. | `gfx_prepare(0)`, `gfx_frame(cam, 0)`, `input_set_pointer(1)`, `input_pointer`. |
| `play/chargen.c` | The rest of `title_menu`: gender, class list, generated background, name, face, attributes, skills, reflexes, summary. | Pictures and `smallfont` pixels handed in by the seam. Clicks are classic coordinates. |
| `tables/tables.c` | The class and race fields chargen and the jump formula read. Plain structs the seam fills. | None. |
| `play/kit.c` | `chargen_give_starting_equipment`. Gold is already 100. | None. No inventory screen. |
| `places/place.c` | Marker 6 after marker 8. | None. The seam passes `{record, x, y, z}`. |
| `play/hud.c` | `hud_draw`, `hud_update`, `hud_buttons_click`, `hud_portrait_overlay_index`, `interaction_mode_cycle`. | `pic_draw` / `pic_draw_part` from `gfx_set_extra`. |
| `play/steer.c` | `intrface_set_regions`, `intrface_key_region`, the 3×3 grid in `docs/state.md`. | Reads `input_pointer` and the rebound actions. Does not look. |
| `play/move.c` | `player_movement_update` and `player_compute_jump_velocity`, on the engine body. | `body_init`, `body_place`, `body_step`, `body_jump`, `body_landed`, `body_set_size`, `rig_first`, `walker_set_game`. |
| `play/activate.c` | `click_activate` for a door, a lever, and a dungeon click that hits nothing. | `interact_request_use_from` along the ray through the hotspot or the centre. `camera_basis`, `camera_focal`, `df_view_w`, `df_view_h`. |
| `play/options.c` | Continue, Exit, the two setting bits, Cursor/View. | `input_set_pointer` follows the mode. Fullscreen is `game_settings` bit 0, the same word `hud_draw` already tests. |
| `play/vitals.c` | The bar expression, fatigue max, fall damage, jump cost. | None. `play/hud.c` and `play/move.c` call it. |
| `play/msg.c` | The one status line `interaction_mode_cycle` and `hud_status_set` write. | `ui_string` or a `smallfont` blit into a picture. |

### HUD

`hud_buttons` is eleven rectangles on the 320×200 screen, hit with `mouse_x > x0 && mouse_x < x1 && mouse_y > y0 && mouse_y < y1`. From the confirmed names:

| Index | Rect (classic) | Click |
| --- | --- | --- |
| 0 | (5,159)–(41,194) portrait | Sheet. Slice 1: sound 203 only. |
| 1 | (83,154) spellbook | Spellbook. Sound only. |
| 2 | (131,154) mode | Left button `step +1`, right button `step -1`. `(mode + step) & 3`. Status line. |
| 3 | (178,154) inventory | Sound only. |
| 4 | (225,154) weapon | Toggle the ready flag. Play the weapon sound. No swing. |
| 5 | (83,177) magic item | Sound only. |
| 6 | (131,177) transport | Sound only. |
| 7 | (178,177) map | Left automap, right travel map. Sound only. |
| 8 | (225,177) rest | Sound only. |
| 9 | (71,154)–(82,199) options | Open options. |
| 10 | (274,154)–(319,199) status | Sound only. |

A press remembers the index. A release fires only on that same index, then plays sound 203 (`icons.c`). While the pointer is on the bar (`mouse_y >= 154`), the bar is shown, and no screen is up, steering from the mouse stops; a steer key still wins (`hud_update` returns 1 only when `steer_key_region` is −1). View mode with the cursor not active ignores bar clicks.

`hud_draw` (`src/hand/func_0005CE17.c`): with fullscreen off, blit the bar, the mode icon, the portrait overlay, the portrait centred near (23, 176), the compass. Vital bars are 4× up to 32 pixels, sitting on y 161, at x 49 / 57 / 65. Fullscreen (`view_flags` bit 0) skips the bar and the compass and draws the three bars at `x - 40`, plus `hud_draw_heading_strip`. Keep the integer expression as it associates, left to right, `<<` and `/` equal:

- health: `(((health << 8) / max_health) << 5) / 256`
- fatigue: max is `(STR + END) << 6`, then `(((fatigue << 8) / max) << 5) >> 8`
- magicka: numerator is `magicka + spell_points_bonus`, denominator is max magicka, same shape as health

Portrait overlay from `hud_portrait_overlay_index`, first match: health under a tenth (frame 2), under a spell (frame 0), poisoned (frame 3), diseased (frame 1). Slice 1 has no spell, poison, or disease, so only the low-health frame can appear. Keep the order in the function anyway.

Compass frame is `(yaw & 0x7ff) >> 6` into `CMPA00I0.BSS` (`hud_draw_compass`). The weapon-ready and detect-arrow decorations are drawn when those flags are set. Detect stays clear. Ready shows its frames.

Pictures the seam should load, which this repo already reads: `TITL00I0.IMG` and the chargen screens `img_load` accepts, `MAIN00I0.IMG` / `MAIN01I0.IMG` (bar and mode icons), `MAIN03I0.IMG`–`MAIN05I0.IMG` (the three bars), `MAIN02I0.IMG` (magic-item list, unused beyond the click), `MCOL00I0.CIF` (portrait overlays), `CMPA00I0.BSS`, `COMPASS.IMG` and `COMPBOX.IMG` for the fullscreen strip, `ARROW.RAW` for the pointer. Confirm each name against the decomp's `disk_read_file` site before binding it. The decomp wins a disagreement.

### Controls

Default keys are the decomp's, not the engine's WASD defaults (`docs/play.md`). Rebind with `input_bind` at startup. Forward and back are the arrows, not W. W is the travel map and does nothing in this slice. Turn left and right are the arrows. Shift with a turn is a slide. J jumps, D crouches, P runs. Page Up and Page Down are the float keys (the hold is dry; still bind them). The activate-centre action is its own key (action 18 in `player_movement_update`), not E. E aborts a spell in the decomp and does nothing here. Right button activates in cursor mode while the weapon is sheathed; it does not swing. A readies the weapon (action 32).

Regions, matching `intrface_key_region` and `docs/state.md`:

| Region | Cell | Keys |
| --- | --- | --- |
| 0 | forward-left | forward + turn left |
| 1 | forward | forward |
| 2 | forward-right | forward + turn right |
| 3 | turn left | turn left |
| 4 | centre | no key; the mouse stops here |
| 5 | turn right | turn right |
| 6 | slide left | slide left, or turn left + strafe |
| 7 | back | back |
| 8 | slide right | slide right, or turn right + strafe |

The grid is the 3D view, three by three (`intrface_set_regions`: width/3, height/3, plus the view's origin). The bar is not part of the grid. `hud_update` returning 1 skips the rest of `player_movement_update`, so the mouse does not also steer.

View mode: `input_set_pointer(0)` while the cursor is inactive, so the mouse recentres and `mouse_dx` / `mouse_dy` keep coming. Apply that motion in the game to yaw, and to pitch clamped to a quarter turn (the ±256 of a 2048-step circle in `docs/port.md`, "VIEW mode"). Do not use `Input.look_dx`: it adds arrow steps and is suppressed in pointer mode. When the cursor is active, `input_set_pointer(1)` and the mouse moves the arrow instead. Toggle on the transition only. A toggle flushes motion (`input.h`).

Cursor mode stays on `input_set_pointer(1)` for the whole dungeon. The game draws the arrow. The engine must not also look.

### Movement

The engine body is the collider. +Y is up. Do not port `object_apply_gravity` or classic's Y sign.

`player_compute_jump_velocity`: `((SPD + STR) / 2) + 50`, then add `jumping * that / 100`. The decomp then negates and shifts left 8 because its velocity is in 1/256 and its jump is negative. Take the magnitude before that negate. Convert to engine units per second in one function in `play/move.c`, and host-test the integer magnitude and the conversion separately. Acrobat (`conditions` bit `0x10000`) doubles the impulse. Class flag bit 2 adds half again. Both use the integer magnitude. A jump from the ground spends 11 fatigue (`fatigue_add(-11)`), sets airborne, and does not re-steer until landing.

Fall damage, on the classic-scaled impact speed, only when it is positive: `d = ((v / 256) / 40) - 7`, then `d * d / 2`, applied to health. Map `body_landed`'s downward speed into that `v` with the same conversion, reversed. Host-test the expression on known `v`, not by eyeballing a fall.

Other rules in `player_movement_update` that slice 1 keeps:

- Airborne: keep the speed from takeoff. Do not read the steer region again until the ground.
- View mode, on the ground, with the decomp's key 23 held: halve that speed, then subtract one. Integer. The key is whatever action 23 is in `key_map`; bind it, do not drop the decrement.
- Crouch is `body_set_size` to half height, and the step while crouched is the smaller one (`step_size` 5 against 10 in the classic loop). Ask `body_can_stand` before standing.
- Run is the faster steer speed. The decomp's run key is P.
- Water and climbing are in the same function. The hold is dry (`dungeon_water_level` 10000 means none) and the start is a floor, so they are not acceptance tests. Still code the water enter (sound, vertical velocity cleared, sink when carried weight is over 250 and float is not held) against `scene_water_body`, so a wet block is not special-cased away. Climbing in the decomp needs a wall-hit flag the body does not report. Approximate with "asked to move, feet barely moved" (under about 5 classic units). Do not add a collide flag. If the approximation cannot see a scrape, leave climbing out of the acceptance test and say so in `play/move.c`.

`walker_set_game` to the player's body so the viewer's walker does not move a second body. The game calls `body_step` itself. `gfx_frame` does not.

### Activation

Grab mode, weapon sheathed, right button, or the centre-activate key: cast from the eye through the hotspot (cursor mode) or the view centre (the centre key, and view mode's centre). `interact_request_use_from` on a hit usable. Doors and levers then animate because `world_load` already built the movers and `gfx_frame` steps them. A miss does nothing. There is no spell missile in this slice, so the fall-through in `click_activate` that fires one is skipped.

Info mode uses the same ray and, for a door, the description the usable already carries (`interact_say` if that string exists). Steal and talk do not create crimes or conversations. They still change the cursor drawn for that region (`cursor_draw` in the movement function).

### Character

A small record in `tables/tables.c`: name, race, gender, the eight attributes and their bases, skills, health and max, fatigue, magicka and max, level 1, gold, reflexes (default 2, "average"), the class flags the jump reads, the equipped right-hand item the ready-sound needs. The seam copies class rows out of `classcfg` into this struct. Chargen writes it. The HUD and the jump read it. Do not lay it out as FALL.EXE's 634-byte character.

The province screen picks a race by the palette index under the cursor (`title_menu` reads the map image's pixel). That index is not a rectangle. Keep it. The picture on screen is `MAP100I0.IMG` under `MAP.PAL` (both shifted as `title_menu` shifts them), with "Please select your home province..." drawn at classic (160, 16) in inks 145 and 156. `TAMRIEL2.IMG` is only the click mask. Its pixels are 0 or a race id (1 Breton through 8 Argonian), and under `MAP.PAL` those colours are near black, so it is not drawn. A pixel 1..8 opens the race confirm over that map: TEXT.RSC `2000 + D_0017B4B8[pixel - 1]`, drawn by `msgbox_show_rsc` (kind 5) on `SPOP.RCI`'s parchment. Yes and No are `BUTTONS.RCI` frames 3 and 4 at x 112 and 176 (`msgbox_draw_buttons`). Their Y is `msgbox_render`'s, from the line count, not a fixed 153. Yes, or Y, goes to the gender box: `chargen_popup_choice(2200, 7, 8, MAP100I0, scan 50, scan 33)`, the same parchment, Male and Female frames 6 and 7, or M and F. No, N, or Escape returns to the map. Escape on the map returns to the title. Pixel 0 and anything past 8 stay on the map.

## 5. How the hold is loaded

`src/game/ogfall` cannot include `src/game/dagger`. `tools/check-layers` rejects that. It allows `apps/` to include both, because the cross-game check only fires when the including file is under `src/game/<name>/`.

The smallest legal seam is `apps/ogfall/seam.c`. It includes dagger. Nothing else does.

```
apps/ogfall/main.c
  includes engine, game/ogfall/play/play.h, apps/ogfall/seam.h
apps/ogfall/seam.c
  includes engine, game/dagger/formats/bind.h, game/dagger/places/world.h,
  game/dagger/formats/img.h, cif.h, smallfont.h, dungeon.h or rdb.h
  for the marker list, game/dagger/tables/maindat.h, classcfg, textrsc
  and calls world_load / world_as_level / world_start / world_creatures_step
src/game/ogfall/**
  includes engine and other ogfall headers only
```

`seam_open` does, in order:

1. `maindat_start`, because LEVELFALL does and the place's creatures read those tables. The cook writes `DATA\MAIN.DAT` beside the exe, the same file GFALL uses.
2. `dagger_bind_loaders` (also called inside `world_load`).
3. `world_load`, which is Privateer's Hold: `dungeon_load` reads `MAPDITEM.017` and the place named Privateer's Hold (`src/game/dagger/formats/dungeon.c`).
4. `world_as_level` into the `Level` the frame draws. `world_start` for marker 8 else 10. Then the marker-6 override from editor markers, passed into `ogfall` as plain numbers.
5. Load the HUD and menu pictures and copy the bytes `pic_load` needs. Install `FONT0003` (and any other face chargen uses) through `smallfont_load`, which is plain C. `fnt_install` is Win32 and stays out of hostcheck; the seam may call it on the target for the engine's `ui_string` face.
6. Resolve sound 203 and the weapon-ready sound to `SoundClip` pointers and hand those pointers to play. Play calls `sound_play`. It does not include `sndfile.h`.

Each dungeon frame, before `gfx_frame`, the seam calls `world_creatures_step` so creature pictures load. Play does not call it and does not step creature AI.

Do not move `src/game/dagger/formats` in this slice. A shared lower layer would rewrite every dagger include and the Makefile groups. That is a later decision, and it belongs to the supervisor. Do not copy IMG or BSA parsers into `ogfall/formats`.

Link `DAGGER_SRC` (formats, tables, places) and not `PLAY_SRC`. Places do not include play, so this link is legal. The exe carries town and stream code it does not call. Leave `DAGGER_SRC` whole rather than splitting it.

`world_load` also builds doors, lights, water, and creature entities. Slice 1 uses those. It does not call anything in `src/game/dagger/play/`.

## 6. ENGINE engineer

Slice 1 needs no new engine call. Confirm the list, tell the supervisor, and stop. Do not add a HUD, a menu, a player, a classic integrator, an unproject, a message box, or a climb flag "to make the port cleaner".

Already there, so an engine change is forbidden:

| Need | Call |
| --- | --- |
| Window, loop, ARENA2 beside the exe | `win_startup` |
| Menu before any place | `gfx_prepare(0)`, `gfx_frame(cam, 0)`, `pic_set_room`, `pic_load`, `pic_draw`, `pic_draw_part`, `pic_update` |
| Hold on the card | `gfx_prepare(level)`, `gfx_prime`, `gfx_fade_in`, `gfx_hold` |
| HUD over the view, engine position line off | `gfx_set_extra`, `hud_set(0)` |
| World still under a screen | `gfx_set_pause` |
| Classic activate, not E | `gfx_set_use_key(0)`, `interact_request_use_from`, `interact_aim_from` |
| Doors already in the level | movers stepped inside `gfx_frame` |
| Walk | `body_*`, `rig_first`, `walker_set_game` |
| Cursor versus look | `input_set_pointer`, `input_pointer`, `mouse_dx` / `mouse_dy`, `input_bind` |
| Status text | `ui_string`, or glyphs into a picture via `smallfont` (dagger, through the seam) and `pic_update` |
| Click and weapon sounds | `sound_play` on a clip the seam resolved |
| Creature pictures | `world_creatures_step` from the seam |
| Water depth | `scene_water_body` |

A ray through a screen point is `camera_basis` plus `camera_focal` and the view size. That is game arithmetic in `play/activate.c`.

Coloured classic text is a blit into an indexed picture (`smallfont_draw` already takes an ink index). Do not add `ui_string` colours.

If a picture `img_load` cannot read is on the acceptance path, that is a dagger format limit (uncompressed IMG only, `src/game/dagger/formats/img.h`), not an engine gap. Stop and tell the supervisor before editing `img.c`. GFALL shares that file.

If the review finds a real hole, the change has to be something play cannot do above the engine: a missing Glide state, a body result the game cannot measure, or a picture the card cannot show. Sketch it as one function next to the existing header, with a host test, and wait. Do not land it in the same edit as game code.

## 7. Layer diagram

```
apps/ogfall/main.c          window, loop
apps/ogfall/seam.c          only file that includes both games
        |                           |
        v                           v
src/game/ogfall/play/       HUD, menus, movement policy, character
src/game/ogfall/places/     marker-6 choice on plain numbers
src/game/ogfall/tables/     class and race rows the seam filled
src/game/ogfall/formats/    empty. no parser
        |
        v
src/engine/                 gfx, input, pic, font, body, interact, sound

src/game/dagger/places/world.c     linked, called only from seam.c
src/game/dagger/formats + tables   linked, called only from seam.c
src/game/dagger/play/              not linked. not included.
```

A file includes its own layer and below. `ogfall/play` may include `ogfall/places`, `ogfall/tables`, `ogfall/formats`, and `engine`. It may not include `apps` or `game/dagger`. `seam.c` is an app, so it may include both games and the engine.

## 8. Shared files

Tell the supervisor before editing any of these. Another session may have them open.

| File | Who | Rule |
| --- | --- | --- |
| `Makefile` | GAME, after ENGINE's "no change" report | Add `OGFALL_SRC`, `build/OGFALL.EXE`, `ogfall-check` using the existing `pe_check`. Sources: `apps/ogfall/main.c`, `apps/ogfall/seam.c`, `src/game/ogfall/**/*.c`, `$(ENGINE_SRC)`, `$(DAGGER_SRC)`. Not `$(PLAY_SRC)`. Hostcheck gains `tests/check_ogfall.c` and the ogfall objects, the same way play tests are linked, and still excludes `fnt.c`. Do not retarget `all` until the supervisor agrees; `ogfall-check` can be a named target first. |
| `tests/check_ogfall.c` | GAME | New file. Do not edit `check_play.c` or `check_dagger.c`. |
| `AGENTS.md` | GAME, one line, when the exe exists | The layout list gains OGFALL as a program. Not in the planning pass. Not `docs/game.md`. |
| `docs/ogfall.md` | Whoever changes slice-1 behavior | This plan. Say so first. |
| `docs/game.md` | Nobody this slice | GFALL's design. |
| `docs/pony97.md` | Nobody | Dirty on `OGFALL`. Do not revert it, stage it, or reformat it. |
| `src/engine/**`, `src/game/dagger/**` | ENGINE only if section 7's review finds a hole; otherwise nobody | Supervisor first. GAME never. |
| `tools/check-layers` | Nobody | The rule already allows a second game and an app seam. |

## 9. Host tests and the target

Host (`make hostcheck`, plain C, no Glide, no `windows.h` under `src/game/`):

- Steer: each key combination in section 4 maps to the region number. A point in each ninth of a 320×200 view maps to that cell. `mouse_y >= 154` with no steer key suppresses the mouse region.
- HUD: the eleven rects. A press on 2 and a release on 2 cycles `0 → 1`. A release on a different index does not. The edge pixel (`x == x0`) misses, because the test is strict.
- Bars: the three integer expressions, including a zero max (the decomp draws nothing when the numerator is not positive; do not divide by zero, match that skip).
- Fall damage and jump magnitude on hand values. Fatigue 11. Acrobat doubles. Class flag bit 2 adds half.
- View-mode key 23: halve, then subtract one.
- Title hits: S, L, E, and the three rectangles. Province pixel index selects the race. Escape returns.
- Chargen: the `newgame.txt` click sequence yields a male Breton Battlemage, gold 100, the bonus points where they were clicked, reflexes 2.
- Marker 6: a list with a 6 picks the nearest 6; a list without one keeps marker 8.
- One body step in the hold: the start is on the floor, a short walk does not pass a wall, a jump leaves the ground and `body_landed` fires.

Target only (`make ogfall-check`, then the exe on the machine): PE32, 80386, subsystem 4.0, KERNEL32 and USER32 only, `glide2x.dll` not imported, no SSE. The title picture, the HUD on the glass, a door swinging, click sound 203, the compass frame tracking yaw. Host tests do not see Glide.

`src/game/ogfall` stays gnu90, no SSE, no CRT calls, no `windows.h`. The app file holds the Win32 entry, as GFALL's `main.c` does.

## 10. Later slices

Name them so slice 1 does not grow into them:

1. Combat. `weapons.c`, `damage.c`, `monster.c`, the swing CIF, creatures waking. The ready flag and its sound already exist.
2. Inventory and the sheet. `inven.c`, `sheet.c`. The items and the numbers already exist.
3. Rest, automap, travel map, transport, status, magic-item list. Each is the button that slice 1 only clicks.
4. Spellbook and cast. `spells.c`, `pickbook.c`, `runspell.c`. The missile fall-through in `click_activate` belongs here.
5. Saves. The title's Load button gets real slots. `loadsave.c`.
6. Quests, if `quests_start_initial` was the flag-only kind. `quests.c`.
7. Leaving the hold. `maplogic.c` transitions, the exit door's other side. Slice 1 stays inside the one `world_load`.

## 11. Risks

- Branch `OGFALL` in `/home/shade/git/gfall` is the working tree. `docs/pony97.md` is modified and is not this work. Do not revert it. Do not stage it. Do not commit anything from this plan unless the user asks.
- Stay on `OGFALL`; commit and push only when requested by the user. Do not rebase or reset `main`.
- Other sessions use other worktrees. Do not edit those checkouts. Stage by name if a later commit is requested. Never `git add -A`.
- `world_load` is GFALL's place builder. Geometry, doors, and creature placement follow it. Player rules, HUD, and menus follow the decomp. Where those disagree (the dungeon-door kludge, climbing flags), record the disagreement. Do not "fix" either side in slice 1.
- Linking all of `DAGGER_SRC` pulls town and stream objects into `OGFALL.EXE`. They must have no missing symbols without `PLAY_SRC`. If a place file calls a play symbol, stop and tell the supervisor. Do not satisfy it by linking `PLAY_SRC` and do not satisfy it by including dagger from `ogfall`.
- `maindat_start` shares `DATA\MAIN.DAT` with GFALL when both run beside the same tree. That is the same cook, not a second format.
- The engine's default bindings are WASD and E-to-use. Forgetting `input_bind` and `gfx_set_use_key(0)` would ship GFALL's keys under a classic HUD. The host test should fail if forward is still W.
- Compressed IMG. If a title or chargen screen fails `img_load`, stop. Do not write a second decoder under `ogfall/formats`.
- Marker 6 may or may not exist in the hold. The choice function covers both. Do not change `world_start`.

## Current implementation and verification

OGFALL is a playable second program built by `make ogfall-check`. The existing
Makefile also includes it in `all` and `build/gfall.iso`. `make hostcheck` runs
its tests alongside GFALL's; `make ogfall-host` runs the OGFALL checks alone.
Nothing under `src/engine` or `src/game/dagger` was changed for this continuation.

The creation route is title -> province -> race confirmation -> gender -> class
list or ten class questions -> generated or answered background -> reputation
summary -> name -> face -> attributes -> skills -> reflexes -> editable summary
-> Privateer's Hold. Every creation screen paints live values onto classic's
indexed artwork. `TAMRIEL2.IMG` is only the province mask; `MAP100I0.IMG` is drawn.

`chargen.c` follows `title_menu`, `generate.c`, `run_0009169B.c`,
`func_0009063D.c`, `func_0009190C.c`, and `generate_00091B80.c` in the local
byte-matched decomp. The 40 button rectangles were checked against the relocated
`chargen_buttons` table at 0x18801C in FALL.EXE 1.07.213.

Attributes roll inclusively from the class minimum through minimum+10, capped
at 100; the pool rolls 6..14. This replaces the earlier fixed ten-point mock.
Each of eight rows can be selected; its diamond allocates or refunds points down
to the rolled minimum. Save Roll, Load Roll and Reroll work. OK requires the
pool to be empty. The saved Watcom-compatible random stream gives these inclusive ranges; see
the persistent-state section below for its verified multiplier and mask.

Skills receive 3..6 only when biography left them at zero, then the classic
25/15/10 primary/major/minor additions. Each group has six allocation points.
Any of its rows can receive points and refund only its allocated bonus. OK
requires all three groups to be spent. The final summary lets the player move
those bonuses, rename, change the face and reflexes, or restart at the map.
Starting health is 25+HP/level and spell points use init.c's integer multiplier
table (D_001788D3), rather than simply copying Intelligence. Names hold 31
characters; the app accepts spaces and preserves Shift/Caps Lock letter case.

The class questionnaire reads all forty texts from TEXT.RSC 9000, chooses ten
without repetition, counts A/B/C through the biased answer-kind table
(0x18520C), and offers the class determined by CLASSES.DAT and class_result_ids.
A/B/C answers; the strip at y=135..182 scrolls with the keyboard arrows or by
clicking just above/below it. The original animated scroll and answer cels are
not played.

The custom editor uses CUST00I0, CUST01I0/CUST02I0, CUST03I0, CUST08I0 and
CUST09I0. It accepts a 14-character class name, twelve distinct skills,
attributes balanced around 400 (10..75 each), HP/level (1..30), up to seven
advantages and seven disadvantages with their subchoices, and five balanced
reputations. Its 16.16 costs, special flags, conflicts between resistance or
immunity and weakness/tolerance, and advancement dagger follow custom.c and
run_000553B2.c. Specials can be removed by clicking their listed entry. Class
names/costs are verified values from 0x18084E/0x18092E and the pointer tables at
0x185370/0x1853A0/0x18549C/0x1854C8. The custom class remains in memory; no class
file is written. Its background uses career_nearest_class's weighted skill match.

BIOG questions now apply the shared reader's skill, gold, item, social reputation
and reaction effects. Generated backgrounds apply twelve random answers;
manual backgrounds reject empty answer boxes. Returning to the choice restores
the pre-background character before another run. Gold begins at 100 **before**
these effects and can therefore be greater at the summary. Background gifts are
retained as group/index/material rows. The starting kit adds the classic
sex-specific clothes, the class weapon (custom included), the Archer's bow and
24 arrows, and a spellbook item. Gifts now become live inventory records and
the book receives the class's starting spells (see the inventory section).
Faction changes and written biography text remain for subsequent slices. The HUD face is now the chosen character portrait.

The intro videos are skipped. Summary OK emits exactly the app's enter-world
request, and the app loads the existing Privateer's Hold Level: world_start's
marker 8 (else 10), then the nearest marker 6 if present. The created character
is retained through loading. The engine handles drawing, collision and doors.
Quests and creature AI are not started. Other HUD screens, combat and leaving the Hold retain the slice-1 limits
described above; saves and Options are now implemented as documented below.

Menus reserve 384 KB of picture room. HUD pictures are loaded only on entering
the Hold; title/map/choice and composition pictures are freed first. Only one
palette-specific composition occupies the card at once. An input revision lets
idle creation frames reuse that picture without recomposing 64,000 pixels;
`pic_update` sends only changed tiles. Menu data is process-lived. The host's
fake P_8 card checks that the composed screens actually draw in this budget.

`tests/check_ogfall.c` covers province selection, the standard path, all eight
attribute and twelve skill selections, bonus refunds and OK gates, saved rolls,
manual biography effects and restarting it, the ten distinct class questions,
custom class selection/skills/specials/reputations, summary rename/reflexes,
starting vitals, and summary -> app enter request -> actual Hold floor -> jump
(11 fatigue) -> landing. It exports the actual composed indexed surfaces as
`build/ogfall-{biography,name,face,attributes,skills,reflexes,summary,question,custom}.ppm`.
The screenshots are host composition checks; Glide output, pointer feel, audio,
and speed on the Pentium/Voodoo hardware still require a target playtest.

### Windows 98 stack growth

The target fault at `004037AA` matched the first call in `seam_present` in the
2026-11-07 build: its prologue subtracted `0x3CDC` bytes from ESP without a
probe. GCC had inlined the question screen's 15,360-byte automatic strip into
that function, so even the title's early return entered with this large frame.
The PE commits only 4 KB of its reserved stack initially. Skipping the guard
page left the call's return-address push on an uncommitted page. Linux host
tests do not exercise Windows stack growth. The marker-6 loader also has a
21 KB frame, so moving only the strip would leave a later instance of the bug.

OGFALL now overrides the common `-mno-stack-arg-probe` with
`-mstack-arg-probe`. Its statically linked libgcc `___chkstk_ms` helper touches
each intervening page before reserving a large frame. This adds no DLL import
or CRT startup; the same Pentium-MMX and Win98 PE checks apply. The question
strip also uses static process-lived scratch to avoid a large per-frame stack
allocation. Other programs retain their existing build flags.

`make ogfall-check` runs `tools/check-stack-probes` against the final executable's
disassembly. It rejects page-sized immediate reservations such as those in the
crashing build; probed frames call the helper before subtracting EAX from ESP.
Negative immediates that release space are excluded. The executable depends
on the Makefile as well as its sources so this target's compiler-flag changes
trigger a relink.

## Target feedback fixes and the first persistent-state slice

Implemented on 2026-11-07; host-tested, with the new Win98/Voodoo playtest still
pending. The earlier slice specification above describes the starting scope.

The class and custom-picker lists now draw the original five-pixel beveled
thumb at x=241, following `picklist.c`'s track and thumb sums. Track clicks,
clamped dragging, held-arrow repeat and release cancellation share the same
list state. TEXT.RSC 307's reflex help uses the existing SPOP parchment composer,
with `generate.c`'s kind-4 positioning (bottom anchor 125), instead of painting
words over the province map.

The HUD has its own full-width, bottom-anchored transform. At 640x480 the
320x46 bar occupies y=0..91, with no lower letterbox; creation menus keep their
centered picture transform. World-pointer conversion and pointed activation
use the world/HUD boundary from the same geometry. The fake-card test measures
the actual HUD quads, as well as pointer round trips at four resolutions.

`intrface_steer` in `src/lifted/intrface_00081EF4.c` requires the left button
for Cursor-mode mouse steering. Hovering is now passive. Held steering uses
the within-cell weights; keyboard regions retain full weight. View mode's UI
pointer cannot steer. Regions 2/5 turn right, independently of 0/3 turning
left. Downward-positive input is subtracted from upward-positive pitch; Home
centers it and the classic +/-256 limit remains. Native floor-scene checks
exercise all nine regions at four headings, mirrored forward turns, vertical
look, limits and Home. Input reset cancels captured scrollbar input and skips
the discontinuity frame. The app skips gameplay while the console owns input;
focus restoration contributes no paused wall time.

### Session ownership

`play/state.c` now owns the live `OgCharacter`; tables own only class/rule rows.
`OgState` contains no pointers: the character, copied career (including custom
restrictions), base attributes, skill-use counters, equipment IDs, control/HUD
settings, RNG, game time, modal stack, place identity and player pose. Summary
OK starts this session after granting the kit. The Hold adapter supplies its
actual region/location index, and movement updates the session pose.

The first bounded object pool has 256 rows. Its original record-kind numbers
and parent relationships follow `init_player_records` in `src/lifted/init.c`:
local-world and nonworld roots, movement, eye, player entity, inventory/storage
containers and quest/options/logbook/bank roots. Starting gifts receive item
IDs under the inventory. Item statistics, equipping, books and loot now use
these records; bank contents, quests and their consumers remain later work. An engine
entity/usable slot or texture handle is never a saved game ID.

`object_new_id` in `src/lifted/object.c` supplies the ID convention: a high
16-bit group with low IDs 10000..62999, wrapping and skipping live IDs in both
roots. Reparenting rejects missing owners and cycles; deletion rejects occupied
containers, equipped items and mandatory session records. Exhaustion returns
failure without replacing a live row. Loading validates the complete ownership
graph and all required references before replacing the session.

The random stream now matches the Watcom routine at code VA 0x0009DC25,
verified from FALL.EXE's instructions: `seed = seed * 1103515245 + 12345`, then
`(seed >> 16) & 0x7fff`. Seed zero is valid. Creation and gameplay share this
saved stream, as the original does. The former chargen generator omitted the
15-bit mask, so a previous seed can produce different creation rolls now.

### Time, modes and notifications

`time_update_realtime` and `time_pass_minutes` in `src/lifted/tamriel.c` are the
reference. The session counts BIOS ticks at 1193182/65536 per second, with 90
ticks to a game minute; integer remainders survive frame boundaries. Modal
screens pause time and discard the unfinished minute when opened, matching the
original's paused BIOS-clock baseline. Explicit elapsed-time requests are
separate from realtime frames. A request above 200000 minutes becomes one,
as in the original. The initial 523530 minutes comes from `func_0004ED36`,
not `game_reset`'s different debug/reset value.

`og_state_date` derives the original 3E 400 epoch, 360-day year and continuous
weekday directly. The engine receives the date and time with its automatic
scale set to zero, avoiding double advancement. Its absolute-year weekday
index has a different epoch; OGFALL calendar UI must use `og_state_date`.
Weather decisions and time-driven gameplay maintenance are not implemented by
this foundation; those systems must consume session time when introduced.

The bounded modal stack uses the original game mode values. Options owns one
push/pop even if asked to open twice. Notifications use original quest event
numbers where defined; elapsed-minute (256) and place-change (257) are OGFALL
notifications. One synchronous observer lives outside saved state; the last
event and serial live inside it. Minute notifications are chronological and
see the final destination clock, as original elapsed-time maintenance does.
This is the event contract, not an implemented quest interpreter or a claim
that every original maintenance handler already runs.

### OGFALL's save format and location

`play/save.c` writes **`ogfall/saves/S0001.SAV`** through `S9999.SAV`, relative
to the executable directory. It never writes GFALL's `saves` folder. These
are OGFALL saves; DOS Daggerfall import/export compatibility is out of scope.
Future OGFALL versions may explicitly reject incompatible files.

The current version 3 has a 24-byte header: eight-byte `OGSAVE\r\n` magic, little-endian
32-bit version, payload length, FNV-1a payload checksum and a reserved zero.
The payload serializes named fields explicitly as little-endian 32-bit
integers, fixed character arrays and IEEE-754 binary32 positions/fractions.
It does not dump native structs, pointer values or compiler padding. Readers
bound the file to 128 KB, require exact length, check the version/checksum,
validate the candidate session, then replace the live state atomically.

A write goes to a same-directory `.TMP`, closes it, rereads and validates it,
then uses the engine's Win98-safe replacing rename. An interrupted replacement
with a missing `.SAV` can recover its `.SAV.OLD`. A failed temporary write
preserves the old save. This is error-safe replacement, not a guarantee against
power loss or storage-controller caching.

The initial state APIs are now connected to the title/Options menus and Hold
reconstruction described below. Version 3 adds item statistics, names, pictures,
container keys/positions and owned spell records. Version 2 saves are upgraded
and their starting clothing and spell contents materialised once. Effects,
factions and quest state will extend the schema as their systems are built.

`tests/check_ogstate.c` verifies ownership/cycle/reference validation, bounded
IDs and wrap, RNG reference values and continuation, modal pause/resume,
frame-independent timer accumulation, chronological minute events, two
independent characters, custom career preservation, settings and pose, disk
round trips at the required path, failed writes, corrupted/truncated/future
files and interrupted-rename recovery. Tests use `build/ogfall/saves` and clean
their fixtures; they never touch player saves. `make hostcheck` includes these
checks along with the existing GFALL suite.

## Options, controls and resuming the Hold

Implemented and host-tested on 2026-11-07; acceptance on Win98/Voodoo is pending.
The Name screen now sends a click on its visible OK button through the same
validation/transition as Enter, including renaming from the summary.

Options uses OPTN00I0's original hit rectangles and draws its live checkbox and
slider values. Sound and Music change the engine's effect and MIDI volumes
immediately; held drags stay captured outside the three-pixel track and clamp
at either end. Detail retains its value and indicator without changing renderer
detail, as requested. Full Screen hides the stone HUD and draws the original
COMPASS/COMPBOX heading strip; F10 also toggles it. Head Bobbing now changes the
camera using the original **int32** samples at FALL.EXE 0x18767C:
`0,2,4,6,8,6,4,2,0,0` over the 1000-tick walking cycle. It does not move the
collision body and stops in air, water and the relevant condition or at rest.

CONTROLS opens CNFG00I0, not a scheme toggle. Its 38 cells and four bottom
buttons follow `controls_buttons` at 0x17B808. The defaults translate the original
scan-code table at 0x18484C into Windows virtual keys, including F5/F6 for sheet
and inventory. Click a cell, release the opening mouse button, then press a key
or mouse button. Escape cancels capture. Duplicate bindings are red and prevent
Continue until resolved; Defaults restores keys only. Unsupported joystick input
has an explicit notice. A capture cannot reinterpret its own mouse press as a
second grid click.

MOUSE opens CNFG01I0 with the original bounds at 0x17BA0C: Cursor/View, horizontal
and vertical sensitivity, turn rate and reverse vertical. The selected scheme
and sensitivity affect actual movement/look, not only the labels. The native
CNFG00I1 overlay is used for View-mode labels on the key screen. A menu's closing
click must be released before it can steer or activate in the world. Console
and focus resets cancel pending captures and contribute no simulation delta.

### Preferences

`play/config.c` owns **`ogfall/settings.cfg`**, beside the executable and outside
ARENA2. Like GFALL's config, it has comment headings and readable `name=value`
lines: `resolution=640x480`, `fov=75`, `full_screen=off`, `head_bob=on`,
`mouse=cursor` (or `view`), `reverse_y=off`, audio volumes in percent,
`detail` (0..127, stored only), `sensitivity_x`/`sensitivity_y` (1..15), and
`turn_rate` (0..15). Switches also accept yes/no or 1/0. Keys use the names
shown by the controls: `key.forward=Up`, `key.jump=K`,
`key.slide_left=Left Shift`, `key.swing=Right Button`. Names are case insensitive;
`Digit 2` distinguishes the keyboard's 2 from an old numeric VK code.
The previous numeric format (including `view_flags`, `mouse_mode`, `width`,
`height` and numeric bindings) is still read and rewritten in the readable form
on Continue. CRLF output opens cleanly in Windows 98 Notepad. Supported resolution pairs
are 320x240, 400x300, 512x384, 640x480 and 800x600; the engine can refuse an
unsupported card mode and fall back. Resolution changes in this file take effect
at startup. The Options panel does not claim a resolution selector it lacks.

Missing keys keep their defaults. CRLF, surrounding whitespace, blank lines and
whole-line `#`/`;` comments are accepted. Malformed/unknown entries and invalid
ranges reject the whole candidate, retaining defaults with an explicit log
message. Continue writes a temporary file, verifies it and uses the engine's
Win98 replacing rename; a missing CFG can recover its `.OLD`. Write failures
are reported without trapping the player in Options. Global preferences are
reapplied after loading a character, so an older save does not replace current
bindings or visual/audio choices. Escape, F10 and the console retain UI ownership.

### Save/load screens and reconstruction

The title's L/Load and Options Load open LOAD00I0's six real slots. Options Save
adds SAVE00I0's header. Click a slot to select it, then the header action or Enter;
a second click within 350 ms also confirms. Tab and arrows move the selection.
Saving opens a 31-character label prompt with working Accept/Reject buttons.
Cancel/Escape returns to Options or the title, as appropriate; Escape in a label
prompt first cancels editing. Empty, damaged and unsupported saves are identified
without replacing the current session. Every valid slot has its label and an
80x50 RGB565 preview when available. The app captures the world before opening
Options, keeping the panel itself out of the preview.

The UI uses `ogfall/saves/S0001.SAV` through `S0006.SAV`. The numeric codec API
still permits 1..9999; the classic screen intentionally exposes six. **Version
3** retains the label/preview, pointer-free body/motion snapshot, HUD state and
up to 512 keyed world-change rows, and adds inventory/container details. Its
maximum file size is 128 KB. Version 2 upgrades are tested; version 1 files are
incompatible and explicitly rejected; DOS save
compatibility remains out of scope. The writer normalizes transient modal state
so a load resumes the world.

Before a save, `seam_capture_world` calls the existing world's keep adapter;
OGFALL copies stable keys, door endpoints/locks and entity changes into its
session, with an explicit capacity failure. Motion includes feet, safe position,
vertical/horizontal velocity, crouch, ground/air state, heading/pitch, eye easing
and bob phase. On load the app releases graphics references, closes the old
place's temporary heap and texture cache, installs the saved keep, rebuilds the
Hold and restores the body/camera. UI assets are read before the place heap and
pictures retain their own copies, so they survive repeated loads. The departed
world is not saved over the incoming keep. Inventory and book screens now use
that state; combat, quests and the remaining rules are outside this slice.

The compact compass now keeps one BSS frame on the card and updates it only
when heading changes. This replaces 32 resident copies and makes room for all
six RGB previews within the existing 384 KB picture allocation. Closed previews
and the Mouse panel are freed. These are no longer needed when walking.

`tests/check_ogui.c` drives the real controller: Name OK, both audio sliders and
a captured drag, fullscreen/bobbing indicators, key capture/cancel/duplicates,
Cursor/View/sensitivity/reverse, defaults/config round trips, all nine native
PNTER.CIF arrows/X, save naming, six previews actually drawn on the fake card,
and a saved door/player restored after rebuilding the Hold. It also execs a
fresh host process that loads through the title and checks character, position
and door state. Fixtures live under `build/ogfall-ui-data` and are removed.
The compositor exports `build/ogfall-{options,controls,mouse,save,save-name,load}.ppm`.
Binary checks still require the Win98 subsystem/import/CPU restrictions and
OGFALL's stack probes; native tests do not certify target pointer feel or speed.


## Inventory, sheet, spellbook and books

The CFG writer now emits comments, named keys and on/off switches, as described
under Preferences. `build/ogfall-settings.cfg` is a host-generated example.
Old numeric preferences are retained when read and rewritten on Continue.

### Source and ownership

`tables/itemdefs.c` and its generated `itemdata.h` supply all 288 original item
templates, group/index maps, clothing slot/style/dye tables, material modifiers,
body-part/shield rules, starting-spell IDs, effect names and loot matrices.
`tools/ogfall-data.py` reproduces the data from FALL.EXE using the reference
checkout's LE reader; it is an authoring tool, not a build dependency. Runtime
OGFALL reads ARENA2 in place and does not require the decomp checkout.

The behavior is translated from the reference's `src/hand/func_0005DE74.c`
(item construction), `src/hand/run_00095F82.c` and `src/lifted/inven.c`
(equipment and inventory), `src/lifted/equip.c` and `equip_0005FA0E.c`
(materials and loot), `src/lifted/kludge.c` (starting gear/spells),
`src/hand/func_0003B6CF.c` and `src/lifted/sheet.c` (sheet),
`src/lifted/pickbook*.c` (spellbook), and `src/lifted/book*.c` (books).
The imported original data is separate from GFALL's rules and tables.

`play/inventory.c` operates on stable object IDs. Item records retain group,
index, name, material, current/max condition, quantity, quarter-kilogram weight,
value, inventory/dropped picture, armor type, dye and book ID. Equipment uses
the original 27 slot numbers, including paired accessories, clothing layers,
two weapon hands and shields. A two-handed weapon clears the other hand;
class restrictions and broken equipment are checked. Taking an overweight item
leaves ownership intact. Gold transfers into the character's gold exactly once.
Removing an equipped item clears its slot before reparenting. The body receives
the actual carried weight for its water/movement rules.

Starting clothing is worn, while weapons begin in the pack, matching
`starting_equipment_give`'s equip argument. The Archer receives one long bow,
an iron battle axe and a separate stack of 24 arrows: index 17 is the bow,
18 is arrows. The class weapon's original fixed starting value is retained.
Clothing styles and dyes and metal recolouring use original lookup data.

A spellbook is a real group-27/index-0 item. Its type-9 spell records are its
children, seeded from `starting_spells_give`, including the custom-class magic
skill check. Moving the book moves access to its spells without copying them.
A stored book keeps its contents; the player's spellbook screen cannot use it.
Nothing recreates or rerolls those contents on an ordinary save/load.

### Screens and input

`play/possessions.c` owns input and transient screen state. F6/the pack button
opens the four-tab inventory on INVE00/01, with the paper doll, twelve accessory
slots, live armor values, two scrolling item columns, Info/Equip/Remove/Use and
the Gold prompt. The right column is the searched container or a dropped pile.
Info displays the item's actual weight, condition, value and quantity. Use on
clothing changes its style; Use on the spellbook or a readable book opens that
screen. A paper-doll click uses the owner of the actual composed pixel.

F5/the portrait opens INFO00's sheet: name, race, saved career, level, gold,
fatigue, health, encumbrance and attributes. The four skill buttons use the
saved career, including custom classes; miscellaneous skills scroll. Rename,
attribute/vital inspection and the inventory/spellbook links work. Affiliations
and log currently report their empty state; History shows creation/reputation
information, not a reconstructed written biography. Level advancement and guild
membership systems remain later work.

Backspace/the spell HUD button, the sheet's Spellbook button or Use on the item
opens SPBK00. It lists the book's actual spells from SPELLS.STD, their icons,
elements, targets and effect names. Selection, scrolling, moving up/down and
deletion update the persistent records. The original SORT artwork
has no handler in the reference's nine-button table; this slice preserves that.
Casting/effect simulation, spell making and enchantments are not part of this
inventory slice.

`formats/book.c` owns a bounded file reader for BOOKS/BOKnnnnn.TXT: a 234-byte
header, page count and absolute offsets. The reader checks page count, offsets,
file size and token operands. The original reads up to 16000 bytes from a page
offset until token 246; some shipped books continue beyond the following page
index, so that offset is not used as an artificial terminator. The renderer
honors line/font/color/centering/x-position/picture tokens, with the original
font-slot mapping (slot 4 = FONT0003) and each font's own space width. Page
arrows, Page Up/Down, Go To Page and Exit work on BOOK00's parchment. Exit closes
the file and returns to inventory.

All these screens pause simulation and use classic menu pointer coordinates,
including the app's native input path. Nested books/spells balance the mode
stack and return to their parent. Escape first dismisses a prompt or popup.
Closing a modal consumes its mouse click until release. Status text from the
world cannot draw over a book. Book opening/page turns and button clicks use
the original sound IDs.

The adapter composes one 320x200 picture on revision changes. Idle frames do
not allocate, reread artwork or decode item pictures. A short-lived composition
heap owns decoded body/item pictures; uncapped archives newly read there are
forgotten before that heap is deleted. Fonts, interface artwork and the
container-icon CIF are read before the Hold's temporary heap. The composed
picture remains within the existing 384 KB card allocation, alongside the HUD.
The world's reserved creature-image slots are still synchronised when their
pictures stream in; adding treasure does not freeze the copied image table.

### Dungeon loot and persistence

The Hold adapter adds activation bounds for authored TEXTURE.216 flats and
random-treasure marker 199/19. Random markers settle onto the floor and currently
use the common 216/0 pile picture; fixed flats retain their own art. Grab mode's
normal activation ray opens the pile through the same wall/reach checks as
other usables. Info mode names it. Each pile has a stable place/thing key and
one persistent container record, including when emptied. Reopening cannot
reroll its contents or duplicate taken items.

The dungeon's original one-based table selector is converted to a zero-based
loot matrix. Generation retains the gold-by-level rule, the original `<=`
chance test, level scaling only for categories 2..5, and halving repeats.
Implemented mundane groups use the original templates/materials; books use the
installed catalogue. MAGIC.DEF enchanted loot, potion/recipe treasure and the
full book rarity selection remain for the magic/loot continuation. Empty first
rolls are retried up to 32 times; object capacity stays bounded. Corpse loot
will connect when OGFALL combat and corpse ownership are implemented.

Remove/Gold creates a keyed ground container near the player. The adapter draws
one usable pile while it holds items; taking the last item removes that drawn
pile without discarding its saved ownership key. Piles and contents, equipment,
styles/dyes, gold, names, and spell ordering/deletions are version-3 saved state.
The schema remains OGFALL's own, under `ogfall/saves`.

### Verification

The host suite exercises all readable key names and old numeric preferences;
actual inventory/sheet/spellbook/book clicks; card picture allocation; idle
allocation/time stability; Archer bow/arrow quantities; equipment and drop/take
identity; carry rejection and single gold credit; storing/recovering a spellbook;
all installed book pages; real Hold treasure activation; no reroll on reopen;
visible dropped-pile creation/removal; exact disk round trips; and a fresh process
loading an equipped, looted character through the actual title menu. A version-2
wire fixture verifies upgrading and resaving without duplicating starting spells.
The host writes `build/ogfall-*.ppm` for visual inspection. Win98/Glide playtest
acceptance remains necessary; host images are the composed indexed surfaces.

### Target follow-up: sound IDs and the two hands

The 2026-11-07 target report found grunts on UI clicks, a whoosh when opening
inventory/the sheet, and two equipped swords with only one visible on the doll.
These were adapter/presentation defects, not a rule limiting the player to one
one-handed weapon.

`sound_play` in the reference's `src/lifted/sound.c` loads a DAGGER.SND **record
ID**. The shared reader's `sndfile_clip` takes a **directory position** instead.
The seam must translate with `sndfile_by_id` first, for every OGFALL sound.
For example, click ID 203 is position 360; position 203 is monster record 10292.
OpenBook ID 237 is position 384; position 237 is monster record 10410. Unknown
IDs stay silent. Do not change the reader's positional API: GFALL's soundscape
uses those positions deliberately.

The previous controller also played OpenBook on every possessions screen.
`inventory_open` in `src/lifted/inven.c` and ordinary `sheet_open` in
`src/lifted/sheet_0003B1F3.c` do not play an entry sound. Preserve that silence
for F6/F5; HUD clicks still play their normal click. The spellbook and readable
book play ID 237, and a book page turn plays 205. A future level-up sheet has a
separate sound in the reference, not an ordinary-open sound.

`inv_equip_item` in `src/hand/run_00095F82.c` and `inv_equip_in_slot_pair` in
`src/lifted/inven.c` put the first one-handed weapon in slot 19 (right), the
second in 21 (left), and replace the right when both are occupied. Two-handed
weapons clear the left; a shield in the left removes a conflicting two-handed
weapon. OGFALL's existing slot rules already follow these cases.

The missing drawing rules are in `src/hand/func_0005978F.c`,
`paperdoll_add_item`: weapons use the female archive one below the male archive
(except shared bow archives 432/433); a right-hand one-handed weapon uses the
next image record; the left-hand one-handed weapon gets five added to its draw
layer. Both hands must be drawn and written into the pick mask. Apply these
changes to presentation values only, preserving the saved inventory image.
Equal draw layers are inserted before existing entries, as in the original.

Host regression checks compare the seam's actual mixed PCM with independently
identified archive entries, check silent ordinary screen entry, render/pick both
Longsword variants for both genders, preserve saved image IDs, and exercise
third-weapon, two-handed and shield replacement. The host emits
`build/ogfall-dual-swords-male.ppm` and `build/ogfall-dual-swords-female.ppm`.
The corrected sounds and hand artwork still need target listening/playtesting.

## Inventory completion — 2026-11-08

The ordinary inventory and spellbook-item continuation is implemented. Combat,
casting/effects, enchanted-object generation, potion/recipe systems, commerce,
wagon access, skill advancement and quests remain their respective roadmap
systems; inventory does not fabricate their records or effects.

### Rules and descriptions

`play/describe.c` expands the original TEXT.RSC item records rather than a
generic stats box: 1000 armor, 1001 weapons, 1003 miscellaneous items, 1004
empty soul traps, 1007 letters of credit, 1009 book title/author and 1011 arrows.
Material, armor modifier, weapon damage, condition, worth and quarter-kilogram
weight follow `inv_item_info` and the named `parse.c` macros. The condition
thresholds are 1/5/15/40/60/75/91/101, not equal fifths. Helm/shield material
text is empty in the original. Magical powers, filled traps, poisons, potions,
recipes, quest letters and property descriptions will use their corresponding
record types when those systems are added.

Book rarity follows `item_init_book(item, index)` and `book_pick_random`:
the item's tier 0..3 filters the installed books by the signed header value
at offset 228 minus one. It does not use player level. The authoring extractor
retains that metadata alongside book IDs; the book reader supplies full title
and author for Info without opening a reading screen.

Painting Info reproduces `item_info_painting`: a saved seed chooses one of 180
PAINT.DAT entries, its four ten-byte lists choose TEXT.RSC 6100/6200/6300/6400
phrases, and record 250 supplies the caption pattern. `formats/lore.c` bounds
those reads and NAMEGEN.DAT's set directory. The artist follows the original
first/surname rules for banks 0..7, including Redguard suffixes and Nord `sen`.
The original instructions confirm that `%an` draws gender before bank.
The selected `APAINT.CIF`..`WPAINT.CIF` frame is decoded only when composing
the changed screen and freed afterward. Inspection uses a local Watcom RNG
seed, giving stable art/captions without reseeding the live world simulation.

### Pack, equipment and transfers

The original group-to-tab table places books, gold, jewellery and ordinary
miscellaneous items under Clothing & Misc; gems and ingredients use their
original Ingredients tab. `og_inventory_rows` still queries all ownership;
`og_inventory_list` hides worn items from the player's list while preserving
their weight and ownership. The doll/accessory slots remain their access path.
Info/Equip/Remove/Use follow `inven_000945C0.c` and `inven_000946D4.c` on either
column: Equip on loot takes and equips it, Remove takes it, and Use reads a
ground book or changes a clothing style without taking it. Book contents remain
available only while the spellbook is carried. The small inventory arrows use
their actual 48..63 / 184..199 button rows; the stone between is inert.

`inv_merge_arrows` has no general quantity prompt. Dropping moves the complete
stack; taking merges into one carried stack and clamps it to **199**, deleting
the incoming stack including overflow. That original loss of excess arrows is
preserved deliberately. The carried stack can be updated even when the object
pool is full. Gold's amount prompt, carry checks, stable item identity, empty
container persistence and dropped-pile removal remain in place.

Equipment sounds use the original per-weapon table and armor/clothing/jewellery
IDs. Weapon-ready sound follows the equipped weapon's index. Cloaks now add the
first style as a back layer at order zero, matching `paperdoll_draw`, while
retaining their front style and owner mask. Cycling a clothing style rewinds
within its contiguous original style range. Neither layering nor hand-specific
art changes the saved inventory picture.

### Owned spells and UI

An owned type-9 record stores its editable name and icon; its index refers to
immutable SPELLS.STD effect parameters. The header strip edits at most 24 name
characters. Left/right icon clicks move forward/backward through 69 icons.
Subtype names come from the original tables. The three effect boxes show the
correct 1200+ help record, expanded with that effect's duration, chance and
magnitude parameters. Long descriptions page on parchment with Page Up/Down,
Enter/Space or a click; Escape dismisses the popup before its parent screen.

The scrollbar supports dragging, track paging, arrow repetition and Page Up/Down
with shared drawn/hit geometry. Up/Down exchange adjacent owned spells and
rebuild from the first row, as `spellbook_build_list` does. Delete is immediate
in the original; the earlier confirmation prompt has been removed. SORT is
still artwork without an original button-table entry. Creating a spell compacts
old order gaps before appending and respects the original 128-spell limit.
No idle screen rereads artwork or allocates picture data.

### Save and verification boundary

Schema **4** adds an explicit spell-icon field to each object record and uses
the existing name field for owned spells. Version-2/3 saves are decoded with
their original wire sizes and upgraded before validation; existing spell
definitions seed their editable name/icon once. A later schema-4 load preserves
edits, ordering and deletions. Saves remain under `ogfall/saves` and preferences
under `ogfall/settings.cfg`, with no DOS-save compatibility requirement.

Host tests cover original description macros/thresholds, painting stability,
Take-and-Equip and worn-list filtering, common-book rarity and ground reading,
whole-stack transfers/merge overflow and a full object pool, actual spell-name
and icon clicks, reordering/deletion, complete Paralysis help and page bounds, exact save
round trips, independent version-2/3 wire fixtures, and a fresh process loading
edited spells and 199 arrows through the actual title Load screen. Screenshots
include `build/ogfall-{item-info,painting-info,spellbook-edited,spell-effect-help,spell-effect-help-long}.ppm`.
These checks establish host behavior and composition; Win98/Glide appearance,
sound, input and performance still need the target playtest.

## Weapons and dungeon combat — 2026-11-08

`weapon.c` owns active hand, right-button gestures, the five-frame melee clock,
and bow draw/release/reload. `combat.c` owns hit chance, damage, equipment wear
and injury feedback; `dungeon.c` owns creature combat, health and swept arrows.
`anim.c` interprets the original ASCR frame, branch, wait, record and event
opcodes used by MONSTER.BSA. `foegear.c` builds `monster_give_equipment`'s three
tiers as persistent items on each eventual corpse. No GFALL combat code is
linked into OGFALL.

The reference is the byte-matched `/home/shade/git/daggerfall-decomp`, especially
`weapon_start_swing`, `weapon_player_update` (0x72E85), `weapon_bow_update`,
`weapon_melee_strike` (0x733AD), `weapon_arrow_update`, `damage_resolve_attack`,
`damage_roll_to_hit`, `item_wear_from_hit`, `monster_init` (0x78566),
`monster_give_equipment`, `ai_creature_think` and XnGine's ASCR interpreter
(0xC013B onward). `tools/ogfall-data.py` verifies FALL.EXE's reference checksum
and extracts the weapon CIF/skill/swing tables, body-part distribution, monster
stats, signed armor bytes, WORD damage pairs, corpse pictures and original
animation scripts into `tables/combatdata.h`. The authoring tool is not a build
or runtime dependency.

Controls use the configured actions: **A** readies/lowers, **S** switches hands,
and holding the right button while making a deliberate mouse stroke swings.
Motion belongs to the weapon during that gesture, so View mode does not turn
the camera with the stroke. A quiet held button does not swing. Normal swings
strike at frame 2; the unarmed kick's script strikes at frame 4. One strike
spends 11 fatigue. Swing frames wait `(115 - SPD) * 3` milliseconds. The original
16 sectors select six swing groups and the original hit/damage modifiers.
An upward thrust is +10 to hit/-4 damage; a downward chop is -10/+4.

A readied bow draws to frame 3 and waits for a right-button press. Release
fires at frame 5, consumes one carried arrow and spends 11 fatigue. Reload is
`(100 - SPD) * 10 + 1000` milliseconds. Empty bows lower with a message. The
render-dependent original bow/arrow steps use a 60 Hz time base here: arrows
travel 40 units per tick, at most 2048 units from launch. Segment sweeps resolve
the nearer wall or target, so a long render frame cannot tunnel through walls.
Arrows hitting a creature are retained in its loot; incoming arrows enter the
player's inventory. Recovery respects the original 199-arrow merge cap.
Incoming damage is attributed to the shooter; the original `weapon_arrow_update`
passes the player as both participants for a hostile arrow. That original
self-attack bug is not reproduced.

The app adapter decodes WEAPON##.CIF, mirrors the active left hand and applies
OGFALL's extracted item-color remap. It keeps one mutable 320x200 cutout on the
card, updating changed tiles only; no per-frame CIF allocations. Weapon art is
anchored immediately above the HUD, or at the bottom in full screen. Creature
attack, hurt, ranged and corpse pictures are supplied alongside walking art.
The original ARCH3D **99800** arrow mesh has 16 reserved transient draw slots.
The engine's optional `Scene.solid_models` prefix excludes them from physical,
activation and sight rays while retaining them in normal textured rendering.

Health is rolled once using each monster's original dice and bonus. Skill,
material, proficiency, race, body-part armor, backstab, critical-strike and swing
terms feed the damage path. Hit chance is clamped to 3..97; deliberately retain
the original attacker's-Dodging term. Mundane armor protects its body parts,
shields cover their listed parts, and successful hits wear weapons and struck
armor. Broken player gear is unequipped. Creatures react to hits, become
hostile, can pursue and strike, show hurt/blood and leave nonsolid lootable
corpses. Player damage updates live health bars and a red hit flash. Death stops
play and offers Load, return to title, or Quit. Cancelling Load returns to those
choices; an empty save directory cannot strand the player.

Schema **4** remains sufficient. Entity `Keep.game` bytes carry a combat marker,
alert flag and current/max health. Engine keep fields carry position and corpse
pose; corpse containers and enemy gear are ordinary owned objects. The player
entity record's flag stores the active hand. Transient swings, arrows and blood
are cleared on load. Save remains `ogfall/saves`; settings remain
`ogfall/settings.cfg`. The app saves entity state directly with `keep_save`:
`world_keep_save` would overwrite an OGFALL kill from the borrowed builder's
private loading-state array. During reload the builder sees live entities first,
then OGFALL restores its own health/death state, allowing corpse art to load.

Host checks cover original data/formula cases, ASCR strike events (including a
werewolf's two), swing/kick hit timing, fatigue, hands, bow ammunition, projectile
flight and wall blocking, nonblocking visual meshes, injury and death. A fresh
process loads a save made after killing a real Hold creature through the actual
title Load screen, verifies its visible corpse and loot usable, and captures it
again. Fake-card previews are `build/ogfall-weapon-{swing,fist,bow}.ppm`.

This completes the first weapon/combat slice, not every creature behavior.
Remaining here: original wall-following/sensing/fleeing and inter-creature teams,
creature spellcasting and disease/poison/curse hit effects, enchantment/artifact
strike effects, collision damage from knockback, exact human/guard special
initialization, the original death movie, rest encounters and full dungeon survival.
The motor currently uses engine bodies and direct pursuit; bow and arrow timing
has the explicit time-base adaptation above. Target appearance, sound and
crowded-combat performance still require the Win98/MMX233/Voodoo playtest.

## Combat playtest repairs — 2026-11-08

A stationary right click can activate treasure with a melee weapon drawn.
Mouse motion belongs to the swing; a readied bow keeps its release click.
The configured Activate Cursor key also picks in Cursor mode. Corpse use boxes
have a real horizontal basis, avoiding an infinite horizontal pick volume.

Small creatures' step height is bounded by a quarter of their height, keeping
their upper collision ellipsoid nonzero. Flying pursuit passes its vertical
velocity through the body's sweep instead of changing feet coordinates outside
collision. Floor contact and gravity continue during attack and hurt poses.

OGFALL opens and closes the MIDI player alongside sound effects. The Hold's
existing soundscape chooses its dungeon music from MIDI.BSA. Creature voices
use `monster_ambient_sound` (0x6310D) and `monster_play_sound` (0x633BC): numbered
DAGGER.SND records `10000 + mobile_id*10`, with the idle/pursuit/close variants
and guard record 11461. Voices are positional within 1024 units. The current
motor spaces them with a 2–6 second timer rather than a render-frame random
trial; attack calls have a two-second minimum spacing. Class confirmation plays
`title_menu`'s record 217. Device output still needs target testing.

The death screen restores retained title/menu pictures before starting again.
The Save menu and save writer reject a dead character. The reader rejects older,
checksum-valid dead saves with an explanation, leaving the current state intact.
No save schema change is needed; saves stay under `ogfall/saves`.

The spellbook retains the inventory or character sheet underneath it. Opened
from the HUD, its outside pixels are transparent over the world. Inventory item
images use `size_fit` (0x7D19B) and `inv_draw_item_image`'s inset, including the
original fixed-point rounding and shrink-only rule. Container header art also
fits its header cell, instead of spilling into the item list.

`tgm` toggles OGFALL's process-local god mode; `tgm on/off` also work. It restores
health, stamina and magicka, blocks player damage (including falls), suppresses
stamina debits, and makes reachable, unobstructed player attacks connect despite
hit rolls or material immunity. Arrows are still consumed and walls still block
attacks. `og_magicka_cost`/`og_magicka_spend` provide the zero-cost protected path
for casting; actual spellcasting and drowning/breath simulation remain future
work. God mode is not serialized or written to settings.

Regression checks cover drawn-weapon pickups, nested spellbook backgrounds,
class confirmation sound, the Hold's dungeon song, small-creature floor contact,
flying pursuit into a floor, empty-save death recovery, old dead saves, icon
fitting, god-mode resource protection and repeated certain hits. UI previews
include `build/ogfall-{spellbook,death,title-after-death,dungeon-loot}.ppm`.

## Pile clicks, creature variants and Rest — 2026-11-08

`intrface.c`'s `steer_center` calls `click_activate` for a left click in the centre
X region. That path now works with a drawn weapon. The arrow regions continue
to steer; right-click activation and View mode retain their existing routes.
The regression places the player at standing height outside a real Hold pile's
hit box, projects the pile onto the screen and drives its left click through
`og_frame`, rather than testing a ray whose origin is already inside the pile.

The walking archive is also the source for a creature's attack, hurt and ranged
pictures and its ASCR variant (`monster_init`, 0x78566). Combat art is shared by
kind **and** archive. Ranged records 20 and 25 are both loaded. Thus a male rogue
does not become female when struck, and a shooting creature keeps its proper
animation. Classic's template flags intentionally allow ranged attacks on
several humanoid classes even though their generated loot includes a melee
weapon; this capability remains. The adapter reads animation timing from the
same selected archive.

Flying creatures use `body_fly_step`, a full-body three-axis sweep without the
walker's floor search or stair snapping. Pursuit, knockback and idle contact use
that same path. Enemy impacts, misses/parries, body falls and arrow releases
are positional sounds, just like voices; they no longer use the flat UI path.
Initial voice timers are staggered. A real DAGGER.SND clip is tested at near,
middle and out-of-range distances; the engine's attenuation formula is unchanged.

`rest.c` owns Rest's input and hourly recovery. The campfire and Rest key open
REST00I0; timed rest and loitering use REST02I0, until healed uses REST01I0.
For a while accepts 1–99 hours; loitering accepts 1–3. Escape or Stop cancels.
Each hour passes 60 game minutes; loitering does not heal. `rest_recover`
(0x71606) supplies Medical/Endurance health recovery, one eighth of base fatigue,
one eighth of maximum magicka unless the class forbids recovery, and a Medical
use. Rest is refused in water, off the ground or with hostile creatures nearby.
Running rest checks that enemy condition again. Its interface is a keyed overlay
over the world and HUD; previews are `build/ogfall-rest-{choice,running,healing}.ppm`.
Random rest encounters, disease/curse updates and six-hour skill advancement are
still pending alongside their respective systems. No save-format change is needed.

## Dungeon locks and teleport arrivals — 2026-11-08

`play/doors.c` owns ordinary dungeon lock rules. Grab activation reports the
original difficulty message; Steal activation picks the lock. The source is
`lockpick_door` (0x1345E), `lock_show_difficulty` (0x138E4) and `door_try_open`
(0x7425E). For ratings 1–19, the pick chance is Lockpicking +
(character level − lock rating) × 5, clamped to 5–95, tested against an inclusive
0–100 roll. Each real attempt trains Lockpicking once. A failure remembers the
skill value and prevents another roll until that value changes, including after
a save/restart. Success unlocks and opens the door with sound 60. Ratings 20 and
above resist ordinary picking and do not train the skill. Original difficulty,
failure and success text is extracted into `tables/doordata.h` by the existing
authoring tool; boosted skill hints are bounded to the last valid table entry.

When a melee hit frame strikes no creature, the centre ray can strike a door,
as `weapon_melee_strike` (0x733AD) does. The door must be within 100 horizontal
world units, reachable by the ray and unobstructed by a nearer wall. Unlocked
doors open or close. Bashing a locked door trains Stealth, sounds PlayerDoorBash
(9) at the door and tests `roll(1,100) <= 20 − lock rating`; ratings 20 and above
cannot break. Bow attacks stay on the projectile path. Levers and lifts are not
lockpick/bash targets. Doors and their chains refuse another use while moving.

A keyed mover's game bytes use marker `0xd1`, byte 1 for a recorded failure,
bytes 2–3 for the underlying lock rating, and little-endian bytes 4–7 for the failed Lockpicking value. Successful attempts
do not overwrite the last failed value. The existing `Keep.lock` and `at_end`
retain unlocked/open/closed states. Schema 4 already carries these fields and
game bytes; no format change or DOS-save compatibility is needed. All saves
remain under `ogfall/saves`.

OGFALL now takes the engine's pending teleport into its own body, camera and
saved pose before movement, combat or a newly opened modal. Original link action
14 (`run_0006480F.c`) preserves the player's yaw and pitch; OGFALL therefore
ignores the borrowed builder's target-facing value. The builder supplies a
standing-eye target over its selected floor. Arrival preserves crouch and uses
that floor, resets movement/fall tracking and clears the previous safe position.
The actual game body is registered for touch triggers, including its crouched
height. The engine's arrival guard prevents immediate return through a paired
pad. Closing a place discards an unconsumed arrival.

The only engine change is generic persistence: teleport movers retain changed
locks and game bytes, while their motion endpoints remain ignored. The engine
contains no lockpicking, bashing, character, skill or original-action rules.

Tests cover probability boundaries, skill training, blocked/out-of-range rays,
magical locks, retry suppression, saving a picked door while closed, and paired
touch teleports with a crouched body and retained view angles. The fresh-process
Hold combat round trip additionally locks two real Hold doors in its fixture,
picks one and fails the other, then checks both outcomes after rebuilding the
place. The unmodified Hold is not assumed to contain two naturally locked doors.
Generic engine tests cover a teleport lock, retained game bytes and removal of
unchanged rows.

The following gameplay slice adds trigger dispatch, ordinary traps and
non-motion effects, creature alert reactions and original motion timing, below.
Secret-door activation and magical Open/Lock spells remain.
The teleport path is host-tested with synthetic paired pads; target acceptance
in a dungeon containing portals remains.

Verification: `make ogfall-host` and `make -j2 all` pass, including the full
engine/game host suite, fresh-process Hold reload, Win98 PE/CPU validation and
OGFALL stack-probe checks. `build/gfall.iso` contains the rebuilt OGFALL.EXE.
The full-suite teleport fixture explicitly settles arrival state left by prior
engine tests before entering its first pad; no target playtest is claimed.


## Dungeon gameplay systems — 2026-11-09

This extends the playable Hold; it does not complete every dungeon system.
Rules stay in `src/game/ogfall/play/`. The borrowed place adapter exposes raw
model actions and supplies geometry, keys and art. It does not decide OGFALL's
skill, encounter, trap or survival rules.

### Sensing and navigation

`ai.c` translates `ai_stealth_check` (`monster_0006328F.c`),
`ai_move_toward_target`, its octant lookup and corridor centering (`monster.c`).
Detection shares the original once-per-game-minute Stealth roll, distance limit,
momentum and odd-minute checks. Attacks and successful door use/picking or
bashing alert creatures. The attacked source wakes regardless of its check.
Awake enemies turn gradually, pursue through collision bodies, try the original
axis directions and corridor grid when blocked, and run the give-up check.
Ranged enemies turn into their firing arc instead of stalling while misaligned.

Creature health Keep rows marked `0xc1` now also hold blocked/direction/turn
state, give-up, stuck, detected and previously encountered flags. Earlier health
rows remain readable. Navigation still uses the engine's collision solver and
frame-time movement rather than importing XnGine. It is not a pathfinder:
original axis navigation can oscillate against an isolated wall. Fleeing, teams,
inter-creature targets, illusion effects and complete human/guard initialization
remain. Movement around real stairs and crowded rooms needs target acceptance.

### Action chains

`actions.c` owns original link dispatch from `run_0006480F.c`. Triggers 1–7
match their event; 8 accepts use/touch/melee/projectile (2/3/5/6), and 9 accepts
use/touch (2/3). Door pick success/failure emit 7/4, melee misses and arrow wall
impacts reach action models, and touch callbacks retain their provenance.
Linked ordinary actions start concurrently. A riddle defers its following chain
until a case-insensitive accepted answer; cancellation/wrong answers stop it.

Implemented actions are motion 1–8, no-ops 10/13/29, text 11, riddle 12,
teleport 14, lock/open/unlock/close 15–20, pulsed damage 21, elemental direct
damage 22–25, magicka drain 28 and sound 30. Text uses TEXT.RSC 8600+parameter;
riddles use 5400+parameter and answers 5656+parameter. The answer is bounded to
16 characters. Both use the existing parchment composition over the world.
Doors retain their underlying rating through unlock/relock, including an action
changing that rating before a save. Grab/Info/Talk use the same ordinary door
path; Steal picks. The original recursive key check is implemented too: the
unshipped item slot 27/3 must have both value and message matching the location
ID (the callback ignores lock level); a successful opening consumes it. Shipped
Daggerfall does not generate this unused key slot.
Damage actions call the original direct-damage path, which does not perform the
spell bundle's separate resistance roll. `tgm` protects damage and magicka.

Motion duration uses the original BIOS clock (18.2065 Hz); directional actions
2–7 use 50 ticks. Geometry and collision still move through generic movers.
Keep game bytes 8–15, marked `0xa2`, save the link's active/reverse/begun flags,
elapsed BIOS ticks, remaining pulse milliseconds and normalized motion fraction.
Partial movers resume without replaying start sounds or starting their chain.
Pulse clocks are per link here; the original shares a damage/poison/disease timer.

Kinds 31 (quest flag), 99 (special) and 129/130 (combination) remain explicit
logged gaps. Kinds 9 and 26/27 and magical Open/Lock are now connected by the
magic batch below. Secret-door-specific rules remain. This interpreter does not yet
cover arbitrary flat actions or all original object flags.

### Progression and survival

`progress.c` follows `rest_close`, `raise_skills`, `skill_ready_to_advance`,
`level_skill_sum` and the sheet's level-up code. Rest checks skill uses only
when more than 360 game minutes have passed. Thresholds include original skill
factors, the class's 16.16 advancement multiplier, reflex adjustment and
1.04 raised to the current level. Each eligible skill rises at most once per
check, with the original mastery cap. The career sum selects a level change;
a gain adds the original random health/Endurance amount and 4–6 attribute points.
The sheet allocates/refunds points above its saved opening floors, refuses to
discard unspent points, and highlights raised skills. Intelligence updates the
stored magicka ceiling. Allocations, skill highlights and the baseline survive
save/load. Guild training and affliction-related skill adjustments remain.

`survival.c` and `move.c` follow `fatigue_update`, `breath_update` and
`player_movement_update`. Ordinary/running/climbing minutes cost 11/88/22
fatigue; a non-Argonian's failed Swimming check costs 44 instead of 11.
Exhaustion near an enemy is fatal; otherwise it passes an hour and applies
rest recovery. Action fatigue also respects `tgm`.

Swimming and levitation use collision-constrained free movement; up/down moves
at 80 world units per second. Heavy swimmers sink. Underwater breath starts at
half Endurance, capped at 50, and uses the BIOS timer; Argonians skip alternating
decrements. Water Breathing and god mode protect against drowning. Remaining
breath draws in the original right-edge bar and persists with its clock phase.

Climbing requires an actual near-head-on wall contact and the original hold
delay, rather than mistaking a short ordinary walking step for a blocked move.
It trains Climbing, moves at one-third speed, checks failure by the original
minute/race rules, and stops when facing away or clearing the wall at foot
height. Saved climbing state restores with the physical body. Guild breath
bonuses, broader condition effects and exact ledge handling on the target remain.

### Rest encounters and persistence

`encounter.c` follows `mplace.c`'s table windows, rolled land slots 1–6,
12-minute rest checks, 1-in-36 interruption roll and hidden 384–768-unit spawn
ring. Loitering does not roll encounters. Table preparation seeds from the
location ID and restores the original saved `rand()` result; it does not consume
hundreds of gameplay rolls. Encounter tables and navigation/skill constants are
extracted from the verified FALL.EXE by `tools/ogfall-data.py`.

The adapter prepares encounter art before the card opens the place. Matching
combat poses are shared; absent walk/idle pictures are decoded into separate
slots without activating distant placed enemies. Invisible resource references
let the existing renderer budget those slots. No image decode/upload is added
to an encounter frame. Live actors use the engine spawn pool and join the same
OGFALL AI, combat, equipment and corpse-loot loops as placed creatures.

Eight persistent encounter records retain unique keys, kind, walking archive
(including humanoid sex) and initial position. Their Keep rows retain movement,
health, death and AI. Failed placement creates no saved actor; failure to restore
a saved actor fails place opening. Schema 5 explicitly serializes progression,
survival, stealth time and encounters (348 added wire bytes), validates their
ranges/identities/archives and upgrades schemas 2–4. Saves remain OGFALL's own
files under `ogfall/saves`.

Adaptations: encounter capacity is eight; no dead-slot reclamation is implemented.
The engine's floor/visibility search replaces XnGine's search. Failed placement
currently happens before rolling combat stats, unlike the original. Rest still
advances/recoveries by whole hours, stopping on its first successful encounter.
Other places and their encounter tables/actors await travel integration.

### Verification and acceptance

`make -j2 all` passes: full engine/game host checks, generated-data checks,
layer checks, Win98 PE/import/CPU restrictions, stack-probe validation and ISO
packaging. `make ogfall-host` also passes. `build/gfall.iso` contains OGFALL.EXE.
Host cases cover trigger matrices, concurrent chains, trap/mover phase reload,
right/wrong riddles, touch provenance, six-hour thresholds, saved level points,
wall contact/climbing, swimming, breath/drowning, fatigue collapse and encounter
placement/combat/codec validation. A fresh process loads an actual Hold save with
both a killed placed creature and a killed rest encounter, checking physical
positions, corpse art and loot usables. Painting captions now expand their nested
male/female name macros while keeping inspection's RNG local.

No new target run is claimed. On the MMX233/Voodoo1, accept wall navigation and
climbing/ledge behavior, underwater controls/breath, rest interruption and
save/restart, traps/riddle input, level allocation, and the extra art's RAM/VRAM
and frame-time cost. The magic batch below adds creature spells and ordinary
poison/disease. Curse systems, enchantment strike effects, knockback collision
damage, the special actions listed above,
the original death movie and the wider world are still unfinished.

## Magic and ailments — 2026-11-09

OGFALL now casts its owned standard spells. The Cast key/HUD opens selection;
inventory Use of the spellbook keeps its editor. Selecting a spell closes the
book before payment/casting, so Recall and Dispel can open their own modal.
Self spells apply immediately; touch/ranged/area spells wait for a mouse press.
Recast and Abort are connected. Abort does not refund. Status (key or HUD)
shows active effects and ailments. Each cast trains its effect schools and
raises original event 73. Dead characters cannot cast through the console.

### Rules and source

The reference is the byte-matched `/home/shade/git/daggerfall-decomp` and the
verified `FALL.EXE`, not GFALL's rules. `tools/ogfall-magic-data.py` verifies the
executable SHA-1 against `config/fall.sha1`, then extracts the effect metadata,
43 cost rows, schools/formulas/target multipliers, element bits and sound IDs,
12 poison timing rows, 17 disease records, creature spell/disease lists and
undead/daedra categories into `tables/magicdata.h`. Runtime needs only ARENA2.
Regenerate with:

```
python3 tools/ogfall-magic-data.py /home/shade/git/daggerfall-decomp daggerfall/DAGGER/FALL.EXE
```

`magic.c` follows `spells.c:spell_cost`, the seven cost formula functions at
0x3937B–0x3988A, `runspell.c:cast_spell_on`, `spell_resist_check`,
`spell_compute_values`, and the effect/end handlers in `spfx.c`, `maplogic.c`
and `hand/run_0008A550.c`. Preserve integer division order, signed 16-bit cost
subtotals, five-point minimum cost, chance byte wrapping and 16-bit durations.
Resistance returns a damage percentage in 5% steps, including class flags,
Breton/Nord bonuses, caster level and elemental protection. Absorption,
reflection and Spell Resistance precede the elemental roll.

Effects include damage/continuous damage, healing, attribute drain/transfer and
fortification with exact applied-delta undo, paralysis/Free Action, cures,
regeneration, elemental resistance, Shield's consumable point pool, illusion
conditions, pacify/charm, disintegration, banishment, Soul Trap, dispels and
movement conditions. Matching group/subtype/target extends duration without
applying the bonus twice. A zero-round effect ends on the following minute,
as `book.c:spell_tick_spell` does. Spell and ailment ticks are interleaved by
minute during time passage. Normal invisibility/chameleon/shadow break on a
hostile act; the original undead/daedra list and 4%/8% detection rolls remain.

Open is consumed by an attempted non-Steal activation of a locked door. Wizard
Lock closes an open door and gives it the caster's level as its lock rating.
Their original condition bits and saved chance are represented by effect rows.
Recall sets or consumes a saved anchor with position and view; it currently
returns only within the loaded place. Dispel lists spell bundles and removes
the selected bundle, with the original self-cast exemption and level chance.
Soul Trap creates an owned type-20 soul child in an empty carried gem on death;
Info expands TEXT.RSC's `%hs` to the trapped creature's name.

`casting.c` follows `pickbook.c`'s payment/bonus/recast behavior, `runspell.c`'s
launch/area rules, `hand/func_0005C448.c`'s 400-unit/sec flight and 2048-unit
camera leash, and `monster.c`/`equip.c` for creature spell selection and casting.
Missiles sweep their initial 110-unit advance and subsequent segments against
walls and creatures. Cursor aiming shares activation's projection; touch uses
the pointed direction within 160 units. Casting consumes its click, preventing
a simultaneous door/loot activation. Creature lists use original level tiers,
spell schools at 80, initial magicka `level*10+100`, cast poses and costs.
Dungeon actions 9, 26 and 27 now emit spells, poisons and diseases at their
one-second pulse cadence (`hand/run_0006480F.c`).

### Sprites, sound and budgets

The adapter decodes all five elements' six `*00C6.CIF` casting frames once at
startup, draws both mirrored hands at their stored offsets, and shifts them
46 pixels for the HUD bar. Casting/weapon pictures release each other's card
allocation. Missile and impact art comes from TEXTURE.375–379 records 0/1,
prepared with the place before play; firing decodes no pictures. Cast and
impact IDs come from the executable's `spell_cast_sounds` and
`spell_impact_sounds`, played through the existing spatial sound hook.
Active-effect icons use ICON00I0, expire with the effects and blink near their
end; hostile spells sit above the HUD or at y=177 in fullscreen. MCOL00I0's
four portrait overlays now reflect low health, magic and symptomatic ailments,
in original priority order.

The generic engine adds only `Entity.glow` (full-bright drawing, no gameplay)
and increases the fixed spawn pool from 16 to 32. OGFALL bounds active effects
at 96, ailments at 32, magical actors at 64 and missiles/bursts at eight. The HUD
shows at most 24 distinct spell bundles. Encounter/blood/missile pools and the
Light candle fit without per-frame allocation. Light uses the original candle
sprite 192 units ahead; illumination is currently an ambient lift, restored on
expiry/close, because the renderer bakes static light decals. This is explicitly
a rendering adaptation, not an exact moving local light.

### Poison, disease and persistence

`ailments.c` follows `disease.c:disease_infect`, `poison_apply`, `poison_tick`,
`spfx.c:spfx_disease_daily`, recovery/cure handlers and
`damage.c:damage_monster_hit_effects`. All twelve poisons have their original
onset/course, stat combinations and beneficial side effects. Ordinary diseases
roll their original affected-stat damage once at a day boundary; reversible
attribute losses are tracked separately. Recovery restores tracked attributes;
Cure Disease/Poison clears the matching records. Group-0 inventory drugs consume
the whole item into poison types 136–139 (`inven.c:inv_use_item`). Rats, bats,
mummies and vampires transmit ordinary diseases from the original lists;
spider/scorpion strikes cast the original paralysis spell. Level-one protection
and player class immunities apply even when a debug command skips resistance.

Preserve the source's unusual infection comparison: `disease_infect` and
`poison_apply` reject a resistance result of **100**, whereas ordinary spell
bundles interpret 100 as full damage. Do not silently invert it. Adaptations:
ailment stat changes affect their actual victim instead of the original global
player pointer, and Transfer restores its actual caster; tracked drain is the actual applied change, preventing recovery
overshoot at the attribute floor. Beneficial poison changes recover by their
tracked sign. Biography infection modifiers and special curse infections remain
unimplemented. God mode prevents health/fatigue/magicka loss through these paths.
Rest stops after a fatal tick and cannot revive the player with hourly recovery.

Schema 6 appends 11,368 explicit wire bytes for magic, ailments, in-flight
missiles, spent actor vitals and the Recall anchor. No pointers or SpawnIds are
saved. Versions 2–5 upgrade with empty magic state. Validation covers ranges,
known spell/ailment IDs, owned ready/last spell references, unique actor keys,
finite missile/anchor positions and directions. Deleting a book spell clears
ready/recast references. Loading reconstructs visuals and creature modifiers
without paying, training or applying player buffs again. Saves stay under
`ogfall/saves`; preferences stay in `ogfall/settings.cfg`.

Console testing: `learn 7` adds Wizard's Fire, `cast 7` pays/readies it normally;
`learn 64`/`cast 64` exercises healing. `poison 128` or `disease 0` applies an
ailment without its resistance roll but retains immunity/level-one protection.
Use a level-two-or-higher character when testing infection. `tgm` keeps its
existing resource protections and zero player spell cost.

### Verification and remaining work

`make hostcheck`, `make ogfall-host` and `make -j2 all` pass. The full native
suite reports `hostcheck ok`; the cross-build passes every PE/import/CPU check
and OGFALL stack-probe validation without compiler warnings. The ISO is
`build/gfall.iso`. `make all` packages and checks the programs; the native
suites above are run explicitly.

The focused host suite covers golden costs, duration extension/expiry, buffs
across saves, resistance, shields, reflection/absorption, paralysis and racial
immunity, illusion visibility, Dispel, Open/Lock, Recall, Soul Trap descriptions,
all twelve poison outcomes, drug use, disease day ticks, cures, fatal Rest,
creature casting/magicka persistence and malformed-save rejection. A fresh
process restores actual Hold magic and latent poison. Art checks decode all five
casting animations and resolve original cast/impact sound IDs. The fire-frame
preview is `build/ogfall-casting-fire.ppm` (transparent regions shown black).

Sequencing decision: spell making/purchasing, enchantments/magical items and
potion systems are deferred until people and guild services are implemented.
Integrate them through their NPC providers and membership/rank restrictions
(roadmap milestone 7); they are not the next dungeon magic tasks.

No target run is claimed. Still missing: those deferred systems,
artifacts (including Azura's Star), special vampirism and
lycanthropy infections/transformations, cross-place Recall, Detect/Identify's
world/inventory integration, owned spell name/icon propagation into active
instances, biography modifiers, exact original collision shapes and moving
local lighting. Nonfunctional original handlers (including wall spells and
several unused effects) remain stubs. Tongues and similar condition bits await
their dialogue/world systems. Test all five elements, aiming, walls/area damage,
mobility, monster casts, cures, interrupted Rest and save/restart on the target,
including audio balance, RAM/VRAM use and frame times.

## Opening the world — 2026-11-09

The Hold's actual exit now opens the streamed wilderness. Towns, dungeon
exteriors and building interiors use the existing placement/stream APIs; building
exits return to the door entered, with its facing and a walkable landing.
`goto Daggerfall`, `goto Sentinel`, or `goto <region> <location>` changes place
without spending time or gold. Name lookup is case-insensitive, preferring exact
matches; an ambiguous name lists numbered choices. `where` prints place identity,
map pixel and stream origin. Close other game screens before using `goto`.
The catalogue contains the 15,251 real map locations, including taverns.

M/HUD Map opens the indoor or town automap; in wilderness it opens the travel
map. W/HUD Travel opens the world/province maps outdoors. The rebound travel key
also works from the town map. Map input pauses simulation, consumes closing
clicks, and does not leak into attacks. Indoor geometry uses `gfx_automap` and
engine seen-model rows, with the chrome's transparent window fitted to OGFALL's
letterboxed coordinates. Town maps use the RMB cells; both show a player arrow.
Find, ambiguous results, province pages, location filters, confirmation, travel
options and the day countdown are connected. The optional illness confirmation
uses TEXT.RSC 1010; declining preserves options, money and time.

### Boundaries and engine use

`play/travel.c`, `journey.c`, `maps.c`, `ground.c` and `worldstate.h` own OGFALL's
rules and saved data. Only `apps/ogfall/seam.c` includes the existing dagger
builders and map presenters. The map's callbacks supply OGFALL money, discovery,
question text and journey planning; no GFALL player or save is constructed.
GFALL's original map entry delegates to `travelmap_player.c`, retaining its
own atlas, ride, bank and journey behavior. OGFALL's executable links the
presenters, canvas and widgets, rather than GFALL's gameplay implementation.

The placement adapter adds an opt-in whole-dungeon policy, leaving GFALL's
streamed-dungeon default unchanged. OGFALL currently retains its whole-place
creature/action pipeline indoors; outdoors it uses `wild`/`stream` and `wres`.
Townspeople stay in the existing pedestrian pool and are not reinterpreted as
OGFALL dungeon enemies. They walk through the builder's motor. Outdoor far clip
is 4096 units with radius one, a 2 ms streaming budget, and the engine's texture
residency/terrain/sky/weather/audio paths. Seasonal and mesh-budget reloads run
under the loading hold and prime resources before resuming.

A place change releases graphics references before ending the old heap.
Persistent keep rows are captured before closing; missiles are place-bound and
cleared on departure. Reopening reconstructs the place, creatures, actions and
pictures before restoring the walking body. Stream origin changes shift the
body, safe point, saved pose, arrows and spell missiles exactly once; engine
spawns shift through `spawn_shift`. This avoids saving an old local position
against a new origin, including when a menu is opened immediately afterward.
Recall saves the place descriptor, return door and origin with its anchor;
cross-place and rebased outdoor returns use the same queued exact-pose landing.
An old anchor without sufficient place identity is refused instead of opening
the wrong place.

Outdoor drops are keyed by map pixel and quantized position inside it, with
small saved coordinates. This avoids float precision loss and key collisions
between separate stream origins. Four nearest nonempty piles within 320 units
use the existing spawn pool and acquired TEXTURE.216/0 art. Picking uses the
pointed ray within 128 units, bounded by walls and nearer usables. Empty piles
stop drawing. Items survive unloading, travel and saves; no SpawnId is saved.
Four piles plus eight encounters, eight blood effects, eight missiles and Light
fit the existing 32-slot budget. No engine-specific Daggerfall logic was added.

### Original journey rules

Reference: byte-matched decomp `src/hand/func_0009CEC4.c` (`travel_route`),
`func_0009C3E7.c` (`travel_begin_trip`), `src/lifted/travel.c` and
`src/hand/func_0003C610.c` (`health_status_add`).
`tools/ogfall-world-data.py` verifies FALL.EXE's SHA-1 before extracting terrain
modifiers and climate categories into `tables/traveldata.h`:

```
python3 tools/ogfall-world-data.py /home/shade/git/daggerfall-decomp daggerfall/DAGGER/FALL.EXE
```

Preserve the original strict-greater minor-axis route step, including its
diagonal endpoint quirk. Foot/horse/cart transport factors are 256/128/192;
truncate `factor*102/256` before multiplying by `512-terrain_modifier` and
then dividing by 256. Sea pixels take 51 minutes by ship or 255 without it.
Camping multiplies each pixel by 300/256. Reckless travel halves total time
before both the displayed estimate and pricing. Inn costs use
`5*((ceil(minutes/60)-sea_pixels)/24)+5`; ship fares use
`25*(sea_pixels/24+1)`. These are the original integer sums, including the
unusual subtraction of sea pixels from hours, rather than GFALL's DFU sums.
Inventory horse/cart records supply the transport modifier in original scan
order. Travel advances OGFALL time and ailment/effect ticks, spends gold, applies
cautious recovery or retains reckless fatigue, and performs skill advancement.
A cautious ordinary arrival waits to 06:10 if it is night; daylight-damaged
classes wait to 18:10 when necessary. A fatal journey remains fatal.

Schema 7 appends two explicit 48-byte place descriptors (current and Recall),
map pixel, 2048 discovery bytes, a count and up to 8192 seen-model rows at 20
bytes each. The fixed world suffix is 2156 bytes before its variable map rows;
the codec's bounded buffer is 512 KiB. Schemas 2–6 upgrade to empty world/map
state and the original Hold default. Saves remain `ogfall/saves`; no DOS
compatibility is required. Existing object/keep budgets remain 256/512 and are
not a claim of unlimited full-world persistence.

### Verification and remaining scope

Native coverage includes fixed original journey sums (terrain, horse/cart,
camping, reckless, ocean fare and diagonal stepping), actual Hold exits,
Daggerfall/tavern lookup, building doors both ways, indoor seen polygons,
Find/confirm/options/Begin, illness cancellation, time advancement, outdoor
save/load in a fresh process, real stream-origin changes, cross-place/outdoor
Recall, and outdoor dropped-item activation/revisit/persistence, and fresh-process missile
restoration under a changed origin. Composed map
pictures are written under `build/ogfall-*-map.ppm` and
`build/ogfall-travel-*.ppm`. The combined host runner launches OGFALL's suite in
a fresh process: GFALL and OGFALL are separate executables and must not share
command registrations or gameplay globals during tests.

This opens the world; it does not complete every world system. Remaining work
includes original map notes/markings and exact indoor camera controls, streamed
large-dungeon population support, outdoor combat/encounters, riding and wagon
storage, owned ships/guild travel privileges, NPC services/law/quests and an
audit of world terrain/climate/population decisions against the decomp. Ground
shape and pedestrian presentation currently follow GFALL's efficient builders.
Outdoor arrow motion/collision works, but its 3D mesh still needs a dynamic-model
rendering path compatible with streamed scene storage. Spell making/buying,
enchantments and potions remain deferred until people/guild services. Target
appearance, frame time, memory and the complete travel loop still require the
Win98/233/Voodoo playtest.

Final verification for this batch: `make ogfall-host`, `make hostcheck` and
`make -j2 all` pass without compiler warnings, including every PE/import/CPU
check and OGFALL stack probes. The rebuilt ISO is `build/gfall.iso`; target
hardware acceptance remains separate.


## Standing: factions, reputation and regions (2026-11-10)

People and law, slice 1 ([ogfall-people.md](ogfall-people.md)). `play/standing.c`,
plain C, no Daggerfall headers, host-tested in `tests/check_ogstanding.c`.

Source: `faction_add_reputation`, `faction_change_reputation`,
`faction_add_reputation_r`, `faction_propagate_reputation` (`src/lifted/talk.c`
1734..1800), `reputation_decay` (`src/lifted/tamriel.c` 392..417) and
`faction_find_type_in_region` (names.csv); the notes are `docs/ai-social-math.md`,
"Faction reputation".

- **The table** is static data: `FACTION.TXT`'s 366 records, handed in by the seam
  (`load_factions`) as `OgFactionDef`, linked into the original's tree (children in
  file order, siblings by `next`). Only reputation and power change while playing, in
  `OgStanding` inside the saved state, with each region's legal reputation and price
  adjustment.
- **A reputation** is a sum held in -100..100 (the original stores a short: past 100 it
  stops, below -100 it is 65436, which is -100).
- **A change** goes to the faction's root (or to faction 108's own line: the walk up stops
  there), then down the root's whole subtree, the faction itself in full and every other
  node at half by an arithmetic shift (-5 halves to -3); faction 844 and a temple (id 240)
  follow the player's membership by guild bits 64 and 128; then each ally takes half and
  each enemy minus half. Memberships come with the guilds slice (`og_standing_set_membership`).
- **Decay** is one point toward zero for every region's legal reputation (not while a court
  is open) and every faction's reputation; a value under -100 is set to 100 first and comes
  out at 99, as the original does. Nothing calls it yet: `time_pass` does it each 161280
  game minutes (`OG_DECAY_MINUTES`), to be wired when the clock hooks are.
- **Adaptations and limits.** Regions start at 1000 (normal prices) and no legal standing:
  the original's start values are not in the C. The politics tick (powers, alliances, war,
  regional prices by `region_update_prices`) is not implemented; it needs the extern tables
  its notes list. `faction_find_type_in_region` is the first of a type in the region, else
  the first of the type with no region.
- **Saves** are schema 8: standing is written before the world state (ready, the table's
  count, a reputation and power per faction, a legal reputation and price per region).
  Schemas 2..7 load with the defaults.


## Trade: the price chain (2026-11-10)

People and law, slice 2, first part. `play/trade.c`, plain integer C, host-tested in
`tests/check_ogtrade.c` against numbers worked by hand from the decomp's notes.

Source: `trade_region_price`, `trade_base_price`, `trade_adjust_price`, `trade_total_buy`,
`trade_total_sell`, `trade_total_repair`, `trade_make_offer` (`src/lifted/inven.c` 1054..1345),
`trade_add_buy_price`, `trade_add_repair_cost` (`src/lifted/generate.c` 264..304) and
`trade_steal_button`; notes in `docs/economy-math.md`.

- A shop is `OgShop`: the region's price adjustment (from standing), the building's quality
  and type, today's holiday and the guild scale. A basket is the sum of its lines: a thing is
  its value, a stack of arrows its value times its count, and the weaponsmith's holiday 49
  (weapons) or a temple's 29 halve a line by a shift.
- The chain is the region's adjustment, the quality's, doubled (`og_trade_base_price`), then
  the buyer's side by Mercantile (skill 14) and Personality, and for buying and repair the
  guild scale out of 256. Selling takes the other side of the adjustment with no scale.
  A neutral shop (quality 10, region 1000, both at 50) quotes 112 to buy and 100 to sell for
  goods worth 100.
- Repair is a tenth of an item's value (three quarters if enchanted), at least 1, an empty
  sum 1; identification at the Mages Guild is `(value * 25) >> 8` with no skill; the offer box
  picks TEXT.RSC 260..262 (263..265 selling) by how the price compares with the total.
- Not built yet: the shelves' stock (`shop_generate_stock` reads group tables by building type
  that are extern in the C and must be read from FALL.EXE), buildings' identity and
  `holiday_today`, the haggling window (its constants are extern doubles and no caller exists
  in the decomp), and the screen.


## Trade: the shelves (2026-11-10)

Slice 2, second part. `play/shelf.c`, with `tables/shopdefs.c` generated from FALL.EXE
(`tools/ogfall-data.py` writes `shopdata.h`: the five group lists at 0x186639, 0x186678,
0x186774, 0x186870 and 0x1868D9, 21 building types each). Source: `shop_generate_stock`
(`src/lifted/equip.c` 755), `interior_stock_shelves` and `furniture_is_container`
(`src/lifted/engsupp.c`), `building_load_interior` (`src/lifted/maplogic.c` 372).

- **Which models are shelves:** interior model 418, or 410 when its variant is a container
  (3, 4, 7, 8, 50, 51 or 32..38).
- **A shelf** draws its groups from the building type's list, whose width and entries depend on the
  shelf's variant (0..1: 3 groups, 2..3 and 4..10: 12, 11..14: 5, 15 and up: 11). One group is
  picked a shelf, then an item of it, then more while a chance that starts at 100 and halves each
  time keeps coming up (`rand() % 100 > chance` ends it): about 1.6 items a shelf, 100 shelves of a
  general store made 164 in the host test. Clothing is the player's sex's group; a book's rarity
  is `(quality + 3) / 5`. The items are made by the same routine as loot (`og_item_create`, a
  random kind and material by the player's level).
- **Restocked at every entry:** the original clears the shop's container and stocks all its
  shelves again each time the building is entered, so nothing is kept between visits.
- **Not built:** magic items (group 4), artifacts (5) and group 11 wait for the magic-item and
  enchantment systems; the enchanted and soul-trap shelves of the Mages Guild and the potions
  shelf (`shop_stock_magic`, `shop_stock_potions`) are separate callers and wait too. The shop
  container itself and its owner flag are made when the trade screen is.


## Trade: a shop session and the holidays (2026-11-10)

Slice 2, third part. `play/market.c` (the session), `tables/holidaydefs.c` (generated from
FALL.EXE: `holiday_days` at 0x17CA24 and `holiday_regions` at 0x17CA8E, 53 holidays), tested in
`tests/check_ogmarket.c` on a live session. Source: `inventory_frame` and `inventory_close`
(`src/lifted/inven.c` 386..500), `trade_settle_offer`, `trade_make_offer`, `inv_claim_items`,
`holiday_today` (`src/lifted/parse.c` 1920).

- **A session** is one of buy, sell or repair, with the shop's `OgShop` and a container: the shelves
  when buying, the empty counter otherwise. Buying moves a clicked shelf item into the pack marked
  unpaid (record flag 32); the basket is every unpaid item carried. Selling puts a pack item on the
  counter. Pay asks for the offer text (260..262, 263..265 selling) and the price does not change
  with the answer; a yes takes or gives the gold, marks the basket as theirs or frees the counter,
  uses Mercantile once and plays 204; a yes without the gold is refused with text 454. Leaving
  returns the unpaid to the shelves and the counter to the pack.
- **`holiday_today`** is a year of 518400 minutes (360 days), the last five days having none; a
  holiday is for a region when its table byte is 255 or the region + 1. It comes from the 53-entry
  tables, not GFALL's sixteen.
- **Adaptations and limits:** an arrow stack is priced by its count (the original keeps the count
  in the condition field). Repair has no enchanted-item case yet, as there are no enchantments. The
  session does not draw anything and is not wired to a screen: the inventory's shop layout
  (INVE08/10/12 columns), the building's quality, type and the holiday from the town (the seam's
  `travel_building`), the offer box and the haggling are the next part. Nothing here is saved:
  an open shop is a screen, not a state.


## Trade: the shop screen (2026-11-10)

Slice 2, fourth part. The inventory as a shop: `og_possessions_trade` (`play/possessions.c`), the seam's
painting and shelf hook (`apps/ogfall/seam.c`: `paint_inventory`, `shelf_used`, `offer_say`), tested by
scripted clicks in `check_ogmarket_screen`, which also writes `build/ogfall-shop-basket.ppm` and
`build/ogfall-shop-offer.ppm`.

Source: `inventory_draw` (`src/hand/func_00092ED8.c`), `inventory_load_images` and `inventory_frame`
(`src/lifted/inven.c`), and the original's `inv_mode_buttons` table, read out of FALL.EXE at 0x188281
(five modes of seven rects and handlers).

- **The column.** A shop uses `INVE08I0.IMG` (buy), `INVE10I0` (sell) or `INVE12I0` (repair) over the
  inventory at the picture's own place, with `INVE09/11/13` for the lit button and `SHOP00I0.IMG` as the
  cost panel. Its buttons, strictly inside x 226..256: Wagon 14, Info 36, Select 58, Steal 112 (buying
  only), Buy / Sell / Repair 134, Clear 156, Exit 182 (selling and repair have no Steal, so theirs are
  one up the column as in the table). The panel shows the base total (not the price) at (77,15) and the
  gold at (107,15).
- **Clicks.** Info says what an item is. Select moves it: buying takes a shelf item into the pack unpaid
  and puts an unpaid one back; selling puts a pack item on the counter and takes one off it. Buy asks the
  offer: a yes-or-no box with the shopkeeper's words and Accept and Reject (Enter and Escape too); Accept
  pays, and without the gold shows TEXT.RSC 454. Clear returns the basket or the counter; Exit (or the
  inventory key) leaves and returns everything unpaid. Steal does nothing yet (slice 7).
- **The offer's words** are TEXT.RSC 260..265, one variant picked at random, its macros filled in by the
  dagger reader: `%a` the price, `%cn` the town. `%cpn` (the shop's name, `building_name` in the
  decomp's names.c) is not built, so a shop is "This shop". The original draws its variant with one
  `rand()` only when a record has several; this uses the reader's own draw, so the random stream differs.
- **Shelves in the world.** A shelf usable from the dagger interior (DFU's shelf list; its variant is the
  model number's last two digits, now carried in the usable's tag) opens a buy session over that shelf's
  stock, made the first time it is used in a visit and discarded with the visit. Adaptation: the decomp
  stocks the models whose id is 418, or 410 with a container variant (`interior_stock_shelves`), which is
  not the dagger's shelf list; the two should be reconciled when interior models are read from the RMB
  blocks directly. A building's quality and type come from the town (`travel_building`), the price from
  the region's standing, the holiday from the clock. Opening hours and a closed shop are left to the door.
- **Not done:** the haggling window; Steal; selling and repair are reachable by the session API but no
  shopkeeper opens them yet (that is talk, slice 6); a save made inside a shop would leave the shelf
  containers behind as objects.


## The bank (2026-11-10)

People and law, slice 3. Rules in `play/bank.c`, tested in `tests/check_ogbank.c`; the screen is a page of
the possessions controller (`og_possessions_bank`) painted by the seam (`paint_bank`); the console's
`bank [region]` opens it until bankers talk. Saves are schema 9 (an account a region, the house and the ship).

Source: `bank_deposit_gold`, `bank_withdraw_gold`, `bank_deposit_letter`, `bank_withdraw_letter_of_credit`,
`bank_borrow`, `bank_sell_house`, `bank_sell_ship` (`src/lifted/bank.c` 218..410), `bank_repay_loan`
(`src/hand/func_0006BF30.c`), `bank_draw` (`src/hand/func_0006BA50.c`), `gold_can_carry`
(`src/lifted/support.c` 900), `macro_dat_date` (`src/lifted/parse.c` 448); the notes are
`docs/economy-math.md` ("Bank, houses, ships, loans"); the button rectangles (`bank_buttons`, 0x186E24) and
the date's names (`tables/datedefs.c`, generated) are read out of FALL.EXE.

- **Accounts** are a region's balance, loan owed, due minute and flags. A deposit moves gold 1:1 (454 if the
  gold is not carried); a withdrawal over the balance asks (290) and takes what there is, then must fit the
  carry limit (a hundredth of the gold in weight against four times the capacity: "You cannot carry so much
  gold."). A letter of credit (item group 27, index 2) is at least 100, costs its value and a fee of
  value / 100 + 1 (293, 292 when short), and depositing every letter asks (291).
- **Loans:** at least 100, at most 50000 a level (296, 295), one at a time (289), a tenth added at once to
  what is owed, due in 518400 game minutes; repayment is trimmed to what is owed (294), takes the balance
  first and then the gold carried, and clears the due day when it reaches nothing. Selling a house or a ship
  asks (298, 299) and pays the price less 15% into the account.
- **The screen:** `BANK00I0.IMG` over the world with the balance at (197,19), the gold (coins and letters) at
  (203,29), a loan's amount at (143,39) and due date at (119,49) ("Turdas the 4th of Morning Star"), the typed
  amount at (157,152). The buttons are the original's, strictly inside their rectangles. An amount is typed
  and Enter makes it; a question is the same Accept/Reject box as a shop's offer.
- **Not built:** buying a house or a ship (their lists come from the town's blocks and the port, with the
  wider world: for now Buy says there is none, or that one is owned); the world's effects of a loan overdue
  (`loan_due_penalty`, `loan_collectors_update`, whose notes say they only ever read the first account, and
  whose collector monster table is extern); a banker to talk to. The 10% is all there is: the decomp has no
  later interest.


## Lodging: rooms, meals and where rest is allowed (2026-11-10)

People and law, slice 4. `play/lodging.c`, tested in `tests/check_oglodging.c` against numbers worked by
hand; saves are schema 10 (rooms held, the last meal). Source: `tavern_room_offer`, `tavern_room_pay`,
`tavern_rent`, `tavern_extend_room`, `tavern_room_rented`, `tavern_room_days_left`, `tavern_buy_food`,
`tavern_food_button` (`src/lifted/tavern.c`), `rest_allowed`, `rest_room_expired` (`src/lifted/rest.c`
197..244) and `rest_update` (`src/hand/func_0007193D.c`); the food list, its prices and the original's own
lines are read out of FALL.EXE (`tables/lodgingdata.h`, generated).

- **A room** is the building's, rented by the day: 7 gold a day through the buyer's side of the price
  adjustment alone (no region, base or scale term: a neutral buyer pays 3 a night). A stay across day 46
  (Heart's Day) loses a night, a single night there is free ("Room is free due to Heart's Day."), a local
  knight stays free ("The room is free for a knight such as you."), and a room cannot be kept past 350 days
  (record 16). Paying takes the gold alone (454 when short); asking again adds the days to the room held
  (`tavern_extend_room`), else it is rented from now. A room's days left are rounded up; past its time it
  is expired and a sleeper in it is woken with "Your time for this room has expired."
- **Meals:** eleven, priced 1 to 3 gold, each giving twice its price in health; Sun's Rest (holiday 37)
  halves the price (at least 1), holiday 1 is free; none within 240 minutes of the last ("You are not
  hungry.").
- **Where a bed is allowed** (`og_rest_allowed`): the dungeon and the wilds always; in a town's streets only
  where its location kind lets a sleeper be (the original refuses sleep as vagrancy otherwise: crime 8, the
  watch and record 17); inside, a tavern needs a rented room ("You have not rented a room here."), the
  knights' hall (faction 41) a member, a house the player's own, anything else is trespass (crime 8). The
  seam fills the place (`rest_site`); the rest screen asks when the first or second button is pressed
  (`og_rest_set_site`, `may_sleep`), as `rest_update` does, and checks a tavern room's expiry each frame.
- **Not built:** crime 8 and the watch (the law slice), moving the sleeper to their room's marker (the
  building's interior markers), the knights' membership, a ship berth, the tavern's menu screen and room
  dialogue (talk), and an outdoor location's kind: the dagger's location records do not carry it, so every
  town counts as a town and a keep or a ruin refuses a sleeper that the original lets be.


## Law: crimes and the court (2026-11-10)

People and law, slice 5, the rules. `play/law.c`, tested in `tests/check_oglaw.c`; saves are schema 11 (the crime
noted and each region's punishment flags). The crime table is read out of FALL.EXE (`tables/lawdata.h`, generated:
`crime_reputation_loss` 0x179EAC, `crime_fine_table` 0x179EE0, `crime_names` 0x184229). Source:
`crime_reputation_penalty`, `court_reputation_restore`, `court_restore_vitals`, `court_frame` (`src/lifted/crime.c`),
`court_open` (`src/hand/func_00020C87.c`), `prison_serve_sentence` (`src/lifted/crime_000217E7.c`), `gold_spend`,
`gold_total`, `gold_can_afford` (`src/lifted/support.c`), `reputation_decay` (`src/lifted/tamriel.c`); notes in
`docs/ai-social-math.md` ("Crime, fine, jail, legal reputation").

- **A fine** is the crime's row `{multiplier, base, least, most}` taken at the region's legal standing: base minus
  standing times multiplier when the standing is below nothing, plus it when above (both raise it), held between
  least and most (smuggling and piracy have a most of 0, so they fine nothing). Each forty of it is a coin: 40 gold
  or 3 days of prison. Short of gold, the days take what the gold cannot.
- **Severity:** a bad standing (below nothing) makes a banishment likelier: two rolls of 1..100 against half the
  standing's size and against its size, each up to 75; assault at -100 is a banishment seven times in eight. Else the
  usual fine and days.
- **The court** is a state machine (`og_court_open`, `og_court_accept`, `og_court_plead`) that says what the original
  says (TEXT.RSC 8050 the question, 8064 the plea, 8052, 8055, 8060, 8062, 8063) and what to do to the world. Guilty:
  half the fine and half the days; paid from coins and letters (`og_gold_spend`: a letter that covers it is lessened,
  else the letters in order, then the coins); a short purse gives up everything and adds a day for each forty short
  and one. A banishment or a death sentence sets the region's flag and puts the player in the street or past it (marker
  kinds 8 and 4). A plea is Personality and the skill over two plus the standing, 5 to 95: success is an acquittal
  (marker 12); a failure is the sentence with the fine halved above a roll of 75 and doubled below 25 (and, as the
  original does, not charged: only its days are served). A sentence given restores the standing a quarter of the
  loss less a point (assault gives back 3), and the original does it a second time when prison days are served. The
  Dark Brotherhood (crimes 4 and 5) and the Thieves Guild (crimes 1..3 and 12) clear a case at once when their rank is
  at least a roll of 0..19 (the decomp's code, which reads the other way round in its own notes).
- **Reputation:** a guard's blow costs the crime's loss off the standing and half of it off the region's people
  (`og_crime_penalty`); the legal standing and every faction drift a point toward nothing each 161280 game minutes
  (`og_standing_decay`, now called from the clock), not while a sentence is being served (`og_law_serving`).
  Resting where it is vagrancy or trespass notes crime 8.
- **Not built:** the guards (`guards_summon`, `people_check_witnesses`, their team and level, the line-of-sight
  witnesses) and what calls the court (a guard's arrest, `crime_guards_or_court`'s health-to-1 and surrender); the
  court's and prison's screens (`COURT00I0`, `PRIS00I0`, "N days until freedom") and the marker moves; the crimes'
  callers (a witnessed pickpocketing, lockpicking, a theft: with steal, slice 7); the legal standing's words
  (`legal_reputation_ranges`, with the character sheet). Playing the court needs the guards first.


## Law: the guards, the arrest and the court screen (2026-11-10)

People and law, slice 5, the play. `play/law.c` (guards and arrest), the court as a page of the possessions
controller (`og_possessions_surrender`, `og_possessions_arrested`), painted by the seam, tested in
`check_oglaw_guards` (it writes `build/ogfall-court-surrender.ppm`, `-ask.ppm`, `-sentence.ppm`). Source:
`guards_summon` (`src/hand/func_00040C87.c`), `guards_timer_tick`, `guards_are_present` (`src/lifted/text.c`
758..826), `damage_apply` (`src/lifted/qkey.c` 527..589), `crime_guards_or_court`
(`src/hand/func_00040FC1.c`), `court_open` and `court_frame`; the court's words are TEXT.RSC 15 and 8050..8064.

- **Summoning:** immediate makes two to five guards near the player (`rand_range(2, 5)`); deferred lets a guard who
  saw it call the watch at once, and anyone else who saw it start a timer of five to ten seconds. Nothing in a
  dungeon, and nothing once there are more than ten creatures. Vagrancy and trespass at rest call it.
- **The blow:** with guards about, the first blow at the player takes the crime's loss off the standing and is the
  offer to surrender (record 15, Yes or No); it is asked once until the guards are gone. A blow that brings the
  player to nothing among the guards is `crime_guards_or_court(0)`: below -20 it is death, between -20 and 0 a coin's
  toss, else the court. Yes to the offer is the court with `surrendered` (no death).
- **The screen:** the court sits over `CORT01I0.IMG`. The accusation (8050) asks Guilty or Not guilty; Guilty is the
  sentence (half the fine and days, the purse spent), Not guilty the plea (8064): the first button argues with
  Etiquette, the second with Streetwise (one use of the skill, then the roll). The records follow, a click at a time
  (8052, 8055, 8060, 8062, 8063 or a guild's 550 and 551), then the days pass one at a time (the standing does not
  drift while they do), 240 minutes more as `court_close` does with the fatigue kept, and the player is put out.
  The buttons' labels (the original's are pictures 16..19 of BUTTONS.RCI, with keys g, n, l and d) are ours.
- **Adaptations and limits:** no guard is made in a town yet: the streets' creature art and AI are the engine's
  dungeon ones and are not prepared for the watch, so the spawn hook says none and a summons makes nobody (the
  rules, the timer and the court are all tested with a fake world). A guard seen by a person is not
  modelled (`people_check_witnesses` needs the pedestrians' lines of sight). The held-back first blow is lost
  rather than dealt after a No. Release puts the player out in the street from a building and leaves them where they
  stand otherwise (no marker kinds 8, 4 or 12 to move to). The prison's screen (`PRIS00I0`, "N days until
  freedom") is not drawn: the days pass at once. `%prn` says "the prison" or a variant of record 8100.


## Talk: who talks and how they greet (2026-11-10)

People and law, slice 6, the rules. `play/talk.c` and the faction relation in `play/standing.c`, tested in
`tests/check_ogtalk.c` on FACTION.TXT's real factions. The greetings' records (9 relations of three), the chance
to know (8 kinds of question by 5 speech styles) and the attitude each kind adds are read out of FALL.EXE
(`tables/talkdata.h`, generated). Source: `talk_start` (`src/lifted/talk.c` 356), `talk_open`
(`src/hand/func_00014AE8.c`), `talk_faction_greeting`, `talk_roll_attitude`, `talk_roll_knows` (talk.c 1378..1457),
`talk_faction_relation` (`src/hand/func_00018BDC.c`), `faction_has_ally`; notes in `docs/ai-social-math.md`
("Dialogue disposition").

- **Who talks:** a thing is "not a person" (7204); a person flagged not to talk, or with a disposition under -20,
  refuses (7205). The disposition is the reputation of the faction that speaks for them (the first of type 2, 7 or 9
  up the tree from their own), a local temple's rank, and the player's reputation with that group's social class (the
  equipment's reaction enchantments are not there yet).
- **The greeting** is by the faction (its own for a regional person or a ruler's, else the group's) and the player's
  relation to it: 0 the same faction, 1 a shared parent, 2 an ally, 3 an enemy, 4 a shared enemy, 5 a shared ally,
  6 and 7 the allies and enemies of those, 8 none. The score is the relation's (+15, +10, 20, +5, +5, -5, -5) plus the
  faction's reputation and the player's with its social class; against a roll of -10..5 it is the allied greeting when
  the reputation is 30 or more, the ordinary one, or the hostile one, which closes the talk (a group at -15 always
  greets hostile). A person of no faction's greeting says a word by the disposition (7206 under 0, 7207 under 10, 7208
  under 30, 7209 more). A guard seeing the player speak to the underworld (social group 4) is crime 7 and the watch, and
  no talk.
- **Attitude** (cold, plain, warm) rolls Etiquette for the polite tone or Streetwise for the blunt, 10 on a success and
  -5 else, plus the question's kind and Personality / 5, against 0..20, cached per tone; the skill is used the first time
  the tone is rolled. Two quirks of the original are kept: the blunt tone tests the neutral tone's cache for that first
  use, and the third cache is not cleared between conversations. **Knowing** is a roll seeded by the person and the
  row, 1..20 against the table's number plus 10 for their speech style (a commoner and a merchant swapped); factions 806
  and 842 always know.
- **Not built:** the window itself (`CHAT00I0`-like pictures and the question lists: places by category, people,
  things, work, news; the answers' wording and directions; quest topics; copying to the logbook), a rumour's first
  words (`rumor_pick_news`), the prostitute's offer, creatures' talk by language, and the guild memberships and
  equipment enchantments that feed the relation and the reaction. A person's own faction comes from the town's people
  (`pedestrian_*` and the building keepers), which the world side does not give OGFALL yet.

## People and law, slice 7: steal (rules)

`play/steal.c` is `pickpocket_attempt`, `trade_steal_button` and the shelf click's alarm roll (decomp `steal.c`, `inven.c`,
`click.c` 975), with the original's own lines read out of FALL.EXE into `tables/stealdata.h`.

- A pocket (`og_steal_pocket`): Pickpocket (15), a creature's level against the player's at five a level, held 5..95, so
  even a master is seen one try in twenty. Seen: crime 12 and the watch; a townsperson is not tried again, a creature's company
  wakes. Else a third of the time there is nothing (TEXT.RSC 8999), else 1 to 5 gold. One use of the skill each try.
- The shop (`og_market_steal`, the Steal button in Buy): Pickpocket less a 32nd of the basket's base total and a quarter of
  the pack's weight, with no floor, so a heavy pack always fails. Seen: a use of Stealth (16), crime 13, the watch, and the
  screen closes with the goods back on the shelves. Else the basket is the player's, unpaid and unflagged.
- A shelf (`og_steal_shelf`): after record 37's yes, seen when a roll of 0..255 reaches 108 less Pickpocket. The original
  has it this way round (a higher skill is seen more); it is kept. Crime 13 and the watch.

Not wired: GFALL's shelf still opens a buying session directly (an adaptation: the original buys through the shopkeeper's
panel, which comes with talk), so record 37's question and the alarm roll wait for that panel and for the person to
steal from. `tests/check_ogsteal.c` holds the chances, the crimes and the kept goods.

## People and law, slice 6 (continued): the window's answers

`play/talk.c` now holds what `talk_button_okay` decides, over a world the caller describes:

- Topics: `og_talk_category` is `building_category` (the bank's type 3 is "Banks", an unlisted type "General", a house only when
  it is a quest site); `og_talk_topics` is `talk_build_place_topics` (houses and types past 16 out, nearest first by the original's
  bubble sort, the "Regional" entry last and outside the sort); `og_talk_has_category` is `town_has_category`.
- The answer (`og_talk_answer`): "tell me about" gives the news once a conversation, except for factions 806 and 842, whose person
  is asked again and again (record 1457 once it was given; no knowing roll is made then). Work answers by attitude (8075..8077) or
  8078 when there is none to give. "Regional" is record 28. Everything else is the answer table's record by knowing, speech style
  and attitude (`tables/talkdata.h`, from FALL.EXE).
- The directions (`og_talk_hint`, `talk_hint_text_id`): records 10 and 11 for a regional topic, 7332 (and the building goes on the
  town map) when it is within 2048 units or a one-in-four recall while out in the streets, else 7333.

Not built: the window's screen, `rumor_pick_news` (it reads a rumour file the original's region events write, which OGFALL has no
world to produce, so there is no news yet), the quest topics, and the people to talk to (slice 8). Tests are in `tests/check_ogtalk.c`.

## People and law, slice 8 (first part): names

`play/names.c` is the original's name generator and signs, over NAMEGEN.DAT bytes the seam loads at start (`og_names_set_data`) and the
words in `tables/namedata.h` (from FALL.EXE):

- `og_name_generate` / `og_name_seeded`: `name_generate` by bank. Banks 1, 8, 9 and 10 have a given name alone, the Nords (2) two pieces
  and "sen", the rest a given name and a surname (pieces 4 and 5); the male and female forms differ as `name_generate_first` has them,
  each with the original's own chance rolls (`rand() % 100 < 75`, `% 50 < 25`). Seeding keeps the stream's next value, seeds, and
  seeds again with the kept value (`name_generate_seeded`), so naming leaves the stream advanced by exactly one draw.
- `og_region_bank`: the bank each of the 62 regions names its people from (the table at FALL.EXE 0x1841E3).
- `og_npc_name` (`npc_display_name`): an individual faction's own name, else a generated one seeded by the person's record id (a twin by its
  name seed and own sex bit).
- `og_building_name` (`building_name`): shops by type ("%s %s" of a first and a last word list), taverns, "The Bank of <region>", the palace,
  the city wall, a guild's hall (or the Brotherhood's and the Thieves' once joined), a quest site's "The <resident> Residence", else
  "Residence". The shop and tavern words carry macros (`%ef`, `%cn`), which the game's expander says (`og_names_set_expand`); with none
  they stay raw.

Not built: the person in the view (`people_in_view` gives an id and a sex), the town's building list with each building's name seed (the RMB
building record's first word, not yet read by `rmb.c`), the person's faction and schedule, the portrait and the talk screen on top.
Tests are in `tests/check_ognames.c`; the names themselves are not checked against the original's output, only their shapes and seeding.

## People and law, slice 8 (continued): the person in the view

`og_activate_set_person` hands the game a use that found no usable: `apps/ogfall/seam.c`'s `person_used` asks the dagger layer who stands
in the view (`people_in_view_at`, a number and a sex, within `TALK_REACH`), and the people of the region are that region's faction of type 15.
By the hud's mode:

- Info: the person's generated name (`og_npc_name`, the region's bank, their number as the seed).
- Talk: `og_talk_start` and `og_talk_open` (who talks, the greeting by faction relation or the mood's word), the greeting said in a message page.
  The window itself (the lists, the answers) is the next part. No guild memberships are passed yet, so no player belongs to a faction.
- Steal: a pocket within 128 units (`og_steal_pocket`), once a person: a failed try puts the number on a short list and that person is not tried again.

`TownBuilding` now carries each building's `name_seed` (`rmb_building_seed`, the first word of the 26-byte record), so `og_building_name`
has what it needs. The test is in `tests/check_ogworld.c` (Daggerfall's streets: info, then talk opening a message page).

## People and law: the talk window and the shopkeeper's panels

`play/talkwin.c` is the window's behaviour (`talk_update`, `talk_draw`, `talk_buttons` at FALL.EXE 0x179B78) without drawing; the seam paints it
(`paint_talk`) on TALK01I0.IMG, with TALK02I0 (the tabs greyed) and TALK03I0 (the two lit ways of asking) as the original layers them. A page of its
own (`OG_VIEW_TALK`) routes the keys and clicks to `og_talkwin_click` and `og_talkwin_key`; Goodbye, or Escape on the categories, ends it.

- The conversation: the tone (polite, normal, blunt) chooses the question's record (7225 + tone for a place, 7231 + tone for the news,
  7212 + tone for a topic, 7211 for work); the question is wrapped to 111 pixels and OKAY logs it (marked, in the dull colour) and then says the
  answer, the log wrapped to 112 and scrolled to its end. A person asked for news is told once (1457 after), except factions 806 and 842.
- Where is: the categories the town has (a category picked by Enter or OKAY, even one the town lacks), then the places of that category nearest first by
  their sign (`og_building_name`), "Previous List" as the first row; "Regional" lists the temples, orders, guilds and shops asked after in other towns
  (the original's 28, the orders only in their own region). Tell me about: "Any news?" then the 34 organisations. People and Things stay empty (no quests).
- The world the window asks for (`OgTalkWinWorld`): the town's buildings and their distances (the original's `xn_math_approx_dist2d` is not in the tree:
  the longer side and half the shorter), the sign over a door, the words said with %key, %loc, %di and %hnt, and the font's width.

Met people follow `click_pedestrian` and `click_npc` (not what I first wrote): in the street, Steal tries a pocket (the person is marked before the try,
so each is tried once, win or lose) and every other mode, info too, talks. A person standing in a building: info names them; within 256 units the rest
offer a shopkeeper's panel or talk. A panel is `REPR01I0.IMG` (Repair item, Talk, Sell, Exit) for armorers, weaponsmiths and general stores, or `GNRC01I0.IMG`
(Talk, the service row, Exit) for banks ("Banking" opens the bank) and for alchemists, bookstores, clothing and gem stores and pawn shops ("Sell"). Sell and
Repair open the inventory in its trade mode over a fresh counter (`og_possessions_trade`). The armorer and the general store test the faction number's bit 8
where the others test the person's flag, as the decomp has it (`npc_click_service`).

Shop shelves, corrected: in the original a shelf (`shelf_open_stock`) opens the buying screen when the shop is open and a plain container (everything
free to take, so theft) when it is closed. Record 37, "private property", with its alarm roll, belongs to the furniture in other people's houses
(models 418 and 410), not to a shop's shelves. Neither the closed shop nor the house furniture is built yet.

Not built: guild halls and temples (`guild_service_dispatch`),
the tavern's panel (`tavern_open`), the quest people, `rumor_pick_news`, the regional search (`talk_find_regional`, so a regional question answers "no one"), the
copy to the logbook, and the list's horizontal scroll (kept in the state, not painted). Tests: `tests/check_ogtalkwin.c` over a stand-in town, and
`tests/check_ogworld.c` in Daggerfall's streets (a person met, the window's buttons, the panels), writing `build/ogfall-talk0..2.ppm` and `service0..1.ppm`.

### The shopkeeper's flag

`formats/interior.c` reads a block's inside itself (`og_interior_people`; the decomp's `rmb_add_people` and `block_flat`): a person's picture, faction and
flag byte. Bit 4 of the block's flag is the record's bit 8, the shopkeeper (`og_interior_shopkeeper`). The dagger layer keeps the picture and faction
but drops the flags, and OGFALL does not take its reading of a block: the only thing taken from it is the block's bytes (`town_block_bytes`, a copy of
one RMB). The seam finds the person met among the building's by picture and faction. Checked on Daggerfall: 647 buildings, 682 people, every
bank, alchemist, armorer and weaponsmith with a shopkeeper, no house with one.

## People and law, slice 9: guilds

`play/guild.c` is the membership model (schema 12: `OgGuilds`, eight memberships, the Thieves' and the Brotherhood's crime counts and invitations) and its rules,
from `guild_join_or_promote`, `guild_join_check`, `guild_rank_for_skills`, `guild_count_crime`, `guild_check_invitations` and `guild_kind_of_faction`; the messages, the skills that
rank each guild and the steps come from FALL.EXE (`tables/guilddata.h`).

- A guild has a kind: 0 the Dark Brotherhood, 1 the Mages, 2 the Fighters, 3 the Thieves, 64 | k an order (68..77), 128 | k a temple (142..149); the table has 22 guilds (kind & 63).
  An empty slot is `used == 0`, since a cleared state would otherwise be a Brotherhood member (kind 0).
- Joining wants the hall's reputation not negative (record 1 of the guild if it is), the best of its skills at 22 and the next at 4 (record 0), and then asks (record 2); the yes
  makes a rank-0 member and says the first rank's line. A knight cannot join a second order, nor a temple member a second temple.
- The rank is the lesser of a tenth of the hall's reputation, the step the best skill reaches (22, 30, 38 .. 94) and the step the next best reaches (4, 8 .. 40), at most 9. It is
  looked at on each visit, at most once in 28 days: up is the next rank's line, a negative reputation is expulsion (record 668, the membership gone, the guild's count back to nothing).
- The Thieves count a pocket that paid (wired in `steal.c`), the Brotherhood a kill (the kill is not wired); at 6 and 15 an invitation falls due three days on, taken outdoors
  (`og_guild_invitation`; delivering the letter is not built).
- Memberships feed the rest: the talk rules get the factions the player belongs to and a local temple's rank, the court the ranks that free a Brotherhood or Thieves member from a crime,
  a local knight rests free in an order's rooms, and the standing rules find the player's temple (`og_standing_set_membership`).

The hall (`hall_enter` in the seam): npc_click_service sends the Thieves' and the Brotherhood's people to their guild when the player is a member (a Brotherhood stranger only
talks) and every person of a guild hall (type 11) or temple (14) to the menu. The menu is `GILD00I0.IMG` for a stranger and `GILD01I0.IMG` for a member, with Join (shut to a member), Talk,
the person's service label (159,71) and Exit. `og_guild_service` is the original's gating by guild, the person's own faction and the rank: the rank refusal (record 3100),
"reserved for members only", the temple's healing (from a rank each, Akatosh and Zenithar always), the Mages' restore of a caster's magicka, a stranger's training, donation, blessing,
cure and quest. Built: Talk, a donation (a number, a chance of one point of the hall's reputation, records 702 and 703), the Fighters' repairs at a member's price, the Thieves' fence
(a sale at half). Everything else answers "That service is not built yet." A promotion's message is shown in place of the menu (the original shows it, then the menu).

Two small controls came with it: a yes or no over the world (`og_possessions_ask`) and a typed number (`og_possessions_number`), both on the message page.

Not built: training, buying and making potions, soul gems, spells and magic items, the Daedra, teleporting, blessings, cures, the temples' quests, the knights' armour and house,
the Brotherhood's and Thieves' invitation letters, the guild healing's attribute restore and disease cleanup, and the rank-4 knights' effects beyond rooms. Tests: `tests/check_ogguild.c`
(the rules, through a save in `check_ogstate.c`) and the menu, the yes or no and the number in `tests/check_ogworld.c`, writing `build/ogfall-guild0..1.ppm`.

### Guild services: training, cures, blessings, healing

`guild.c` holds the rules, the seam the flows (`hall_*`):

- **Training** (`training_offer`, `training_update`): a trainer's skills by their faction (the twelve lists are in `tables/guilddata.h`), refused within 720 minutes of the last
  lesson (record 4023); the price is the level times 100, four times for a stranger, asked in record 8; a list of the skills; over 50 is refused (4022); else the gold is spent,
  10 to 20 times the skill's advance (`skill_advance_multipliers`) uses are added (`og_skill_add_uses`, held 0..20000), record 5221 is said and 180 minutes pass.
- **Cure** (`guild_cure_diseases`): 250 gold a disease (record 30 if there is none); Akatosh's members pay (10 - rank) / 10; the price goes through the shop's chain and the offer's words.
- **Blessing** (`guild_buy_blessing`): none at Akatosh's, one at a time (the original answers 454, "no gold", when one is held), 100 gold through the chain, rank + 10 points of the
  temple's skill, attribute or the region's legal standing for rank + 4 days. The points added are held at 100 but the amount kept is the whole, and the legal gift is taken back from
  region 0 (the record never names a region), as the decomp has it. An old one ends when time passes (`og_blessing_expire` in `og_state_pass_minutes`).
- **Heal** (`guild_heal`): health to the most; if an attribute is down the offer (403) to put them back, the conditions cleared and the ailments cured. The spells' effects on
  the player (the original's type 9 children) are not cleared. When a question is up the service waits for its answer.
- A watchman killed in a dungeon is counted by the Brotherhood (`guild_count_crime(6, 1)`); a pedestrian's death (6, 5) comes with the streets' people being fought.

Controls on the message page grew a list popup (`og_possessions_list`) and up to four queued messages. Tests: `tests/check_ogguild.c` (training, blessings) and the Fighters'
trainer driven by clicks to a lesson in `tests/check_ogworld.c`. Cure and blessing through the shop's chain need a building to stand in and are not driven by the tests.

### The tavern, the knights' gifts and opening hours

- **The innkeeper** (`inn_act`, `TVRN00I0.IMG`: Room, Talk, Food and drinks, Goodbye at x 99..220, y 51, 60, 69, 78): a person of a tavern who keeps it (the block's shopkeeper flag) offers it.
  Room asks the days (record 5102, or 5100 with the hours left, `%dwr` = minutes left / 60), then `og_room_offer`'s rules (record 16 past 350 days, free to a local knight and on Heart's
  Day, else 7 gold a day through the buyer's skills with the offer's words) and `og_room_take`. The bed marker the room sends a sleeper to is not read (room 0). Food is a list of eleven,
  refused within 240 minutes of the last meal.
- **Knights' gifts** (`og_knight_armor`, `og_knight_house_allowed`): one piece of plate a rank, a random one of seven, in the metal the rank names (rank 0 wraps the byte and gets 2),
  records 461 and 463. The house waits until the towns' houses are OGFALL's own (their price is five times the largest model's radius, which the model layer holds).
- **Opening hours** (`play/hours.c`, from `building_is_open`): a type opens and shuts by FALL.EXE's table; a guild's own members enter their hall at any hour (the Fighters and Mages from
  rank 6). A shop's shelves now sell when it is open and open as a plain container, the goods free to take, when it is shut (`shelf_open_stock`). The doors do not ask yet.

### Potions and soul traps

`play/potion.c` and `tables/potiondata.h`, `souldata.h` (from FALL.EXE):

- The twenty recipes (`potion_recipes`, 109 bytes each: ingredients, value, the potion's own 89-byte spell). Each spell is registered with the spell table (`og_spell_register`,
  under `1000 + recipe`), so a potion is cast like any spell of the caster. Stamina is 25 gold, Purification 500; Healing's spell is group 10, sub 8 and heals on drinking.
- A potion is a glass bottle (group 1, index 1) priced at its recipe's value, named "Potion of ...", with the recipe (one more than its number) in the item's message field.
  Using a bottle in the pack drinks it (inven.c 900): the spell lands on the player at the player's own level and the bottle is gone.
- `guild_buy_potions` (a hall's potion seller, a stranger's too where the original lets them): the counter is emptied, stocked with quality + 1 bottles of random recipes
  (`shop_stock_potions`) and opened for buying. The soul-gem seller is the same with `shop_stock_soul_traps`: quality / 2 + 1 traps at 5000, a quarter holding a soul of a random kind
  that has a value (the 43 values are FALL.EXE's), added to the price.
- Not built: the potion maker (`potionmaker_mix`: the cauldron, ingredients matched to a recipe by `potion_match_recipe` or an unknown brew made into a spell by
  `potion_mix_unknown`, which wants the ingredients' potency from `item_make`'s side effects and the spell maker's tables), the alchemists' own shelves of potions, magic items
  (`shop_stock_magic`, the enchantment tables), the spell shop's window, the Daedra and teleporting. Tests: `tests/check_ogpotion.c`.

### The potion maker and the alchemists' shelves

- **Ingredients** are the pack's items whose template has bit 0 of its flags (`og_ingredient`). Each lends, from its template: a key (`variants`) the recipes match by, a potency (`rarity`,
  +0x28, now read into `g_rarity`), and, to an unknown brew, an effect (`draw_order`) and a subtype (`capacity`, +0x1E, only for an ingredient). The cauldron (`OgCauldron`) holds eight, no two of a kind.
- **Mixing** (`og_brew`): the twenty recipes in order; a recipe matches when each ingredient of it is matched by key and the potency (the matched cauldron ingredients' added, the others taken off,
  less the recipe's own, halved, plus 5) is at least 1; the first match makes its potion, worth its recipe's value. Failing them, `potion_mix_unknown`: the effects the ingredients lend
  sorted largest first, merged by effect (potencies added as bytes), sorted by potency, the weakest dropped (those past three added to a pool, and a second that is under half the first, a
  third under half the second) and any at or below the pool; no effect left makes nothing. The brew is a spell of "Unknown Powers", element magic, on the caster, each effect's duration,
  chance and magnitude from its settings (duration 1+1/level, chance 70+1/level, magnitude 10..1 plus 1 a level; the rest of the original's spell maker record, which it left as it was, is
  nothing). The bottle remembers the effects (a message with bit 30 set and ten bits an effect) and a drink makes the spell fresh (id 2999) and casts it.
- **Quirk kept:** using the ingredients up empties the cauldron's cells but not its keys (only taking one out by hand sets its key to 254), so a second Mix at once matches the same recipe
  with nothing in the pot; opening the maker again clears it. The test holds it.
- **Recipes** are items (group 27, index 4) holding a recipe (`rand() % 20` when made, `og_item_create` now does it); Recipes in the window loads the pack's ingredients for a known recipe
  into the cauldron, or says "You do not have the ingredients required." (the cauldron's cells emptied first, its keys not, as above), or lists them if there are several.
- **The window** (`MASK00I0.IMG`, `OG_VIEW_POTION`): the pack's ingredients three across from x 16 (twelve from the scroll, three at a time), the cauldron two across from x 221, Recipes
  (169,26), Mix (169,42), scrolls (6,30) and (6,156), Exit (290,178). A guild's potion maker opens it (record 34 if the player has no ingredients).
- **Alchemists' shelves:** the original stocks them from the ordinary group tables (ingredients and bottles); a potion is never on them. What it adds is a quarter chance of a recipe item
  (`item_add_to_container(27, 4)`) when a shelf is first opened, and a horse and a cart (group 23, 1 and 0) on each general store shelf; both are now added.
