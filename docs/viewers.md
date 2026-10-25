# Viewers

`RENDERFALL.EXE` (`tools/renderfall/main.c`) lists `ARCH3D.BSA` and orbits one model. `LEVELFALL.EXE` (`tools/levelfall/main.c`) lists every indoor and outdoor place and flies through one. Both are PE32 GUI programs on the gfall disc. `ARENA2` is not on the disc. They read it from the directory beside the exe. Neither imports `glide2x.dll`. `comctl32.dll` is `LoadLibrary`'d for `SysListView32`; it is not in the import table. `make renderfall-check` and `make levelfall-check` are the same PE checks as `gfall-check`.

Read `docs/dungeons.md` before changing what a level is. This file is the UI and the camera.

## List

Both viewers use the same child-window pattern. Copy it; do not invent a second list control.

- `win_startup(..., "/ARENA2", title, 1)` makes a 640x480 GUI window with a system cursor. `gui == 0` is the borderless game window.
- The list is `SysListView32`, report view, single select, full row select. Columns are inserted with `LVM_INSERTCOLUMNA`. Header clicks arrive as `WM_NOTIFY` / `LVN_COLUMNCLICK` through `win_notify_hook`. Double-click is `NM_DBLCLK`. The Open button is `WM_COMMAND` through `win_command_hook`.
- The parent `wnd_proc` eats `WM_KEYDOWN`, so a child sees keys only when it has focus. Esc is polled with `GetAsyncKeyState`. Enter and a double-click open. The title-bar cross posts `WM_QUIT` (`win_closed`). Esc does not. On the list, Esc clears the find box when it has text, and quits when it is empty.
- `WM_MOUSEWHEEL` does not scroll a list-view on this target. Both viewers turn it into `WM_VSCROLL` line steps.
- Renderfall sorts its row array, then deletes and reinserts items. `lParam` is the index after the sort.
- Levelfall indexes every place once, into `g_rows` (about 4232 indoors and 15251 outdoors). The list is `LVS_OWNERDATA`: `LVM_SETITEMCOUNT` plus `LVN_GETDISPINFOA`, so a keystroke does not insert thousands of rows. `g_view` is the filtered, sorted subset. The find box is an `EDIT`. The kind box is a `CBS_DROPDOWNLIST` (All / Indoors / Outdoors). Match is a case-insensitive substring. An exact name is selected. Up and down from the find box move that selection. Focus starts in the find box. On `LVN_GETDISPINFO`, copy into `pszText` only when it is a real buffer with a sane `cchTextMax`. Selection sends `LPSTR_TEXTCALLBACK` (`(char *)-1`); writing there is an illegal operation. Point `pszText` at a static cell buffer instead. That buffer only has to live until the next dispinfo.

## Opening a level

Wrap the open in `mem_temp_begin` / `mem_temp_end`. On Win32 that is a second heap; destroying it frees the dungeon, the textures, and the climate read. On the host the temp heap is a no-op, so hostcheck leaks, which is already the case for `world_load`.

Order when leaving the view:

1. `input_end` (levelfall) or `win_mouse_hook(0)` (renderfall)
2. `gfx_shutdown`
3. `texture_cache_clear` — the TEXTURE file cache is a static table of pointers. Clear it before the temp heap dies or the next level reads freed bytes.
4. `mem_temp_end`
5. `show_list(1)` and `input_clear_quit`

The HUD uses the engine face: the stock OEM fixed font, rasterized once into a sheet on the first `gfx_prepare`. `GFALL.EXE` replaces that sheet with `FONT0000.FNT`. These viewers do not, so a tool does not read the game font. The place index is filled inside one temp heap, then `mem_temp_end`, so the map files are not kept. The `DunRef` array itself is allocated before that. A failed open shows `dungeon_error()`, `world_error()`, or `gfx_error()` and stays on the list. `texture memory is full` used to mean the Voodoo's 2MB filled during `gfx_prepare`. Pictures are one full image each. A picture that does not fit is left unbound. See `docs/memory.md`. Esc in the view sets the quit flag; clear it before waiting on the list again or the next Esc test trips immediately. The title-bar cross sets `win_closed` and posts `WM_QUIT`, including from the fly view, so that path leaves the process instead of returning to the list.

Glide clear stays black (the default). Fog uses that color. `gfx_set_fog` turns the distance table on or off; the default is on, and a call before `gfx_startup` is applied when the device opens. Renderfall reads `renderfall.cfg` beside the exe (`background=R,G,B`, `grid=1` or `0`, `fog=1` or `0`) and F toggles fog for the rest of the run. Levelfall reads `levelfall.cfg` beside the exe for `background`, `fog`, `far`, `fov`, `time`, `weather`, `open`, `cam`, and the sound keys below, each as `key=value`. A missing file is written on first run; an older file without the new keys still loads. `fov` is applied once at startup and stays for the process, like the wheel. `time=21:30` (or `time=21`) sets the world clock once at startup with `clock_set_minute`; the clock then runs at its timescale, and only while a world draws, so the first open shows that hour. `weather=rain` (any of the seven kinds) calls `weather_set` and then `weather_force`: the place opens in that weather with no blend, and no roll of the odds replaces it until `weather auto` in the console. A place load only sets the odds, so the held kind survives it. Sound: `soundpanel=1` or `0` opens the sound panel at start, `sndrows=0..8` (5) is how many sources it lists, and `sndview=0..4096` (0) is the engine's marker range at start. `sound=`, `music=` (1 or 0), `sndvol=`, `musvol=` (0..100) are the engine's switches, applied after `sound_startup` and `music_startup` so a volume reaches the open device; left out, the engine's defaults stay, and the result is logged as one line. `open=indoor,R,I` or `open=outdoor,R,I` (region, then the `DunRef` index) selects that row and flies it before the list waits; an unknown place logs `open=: no such place` and stays on the list. `cam=x,y,z,yaw,pitch` (world units, degrees) replaces the start marker for that first open only, and is ignored without `open`. Nothing on the disc is a cfg. `far` is the clip distance in world units (`gfx_set_far`); the engine default is 1400 and the cap is 16384. Levelfall's own default is 8192. That is far past what a dungeon shows: GFALL's default 1400, with fog solid at 1170, is the game's own view. In the bank's vault hall in Daggerfall's castle, 8192 drew up to 5,800 front faces and about 3 screens of fill at 8 fps on a 233 and Voodoo; at 1400 the same views are 800 to 1,000 faces and about 1.75 screens, and the worst room ran at 35. Time a view at the distance the game will use, with `covers` off and the panels hidden (F1); see `docs/console.md` for `vsync` and `stats`. Fog, when on, goes solid at about four fifths of that distance, in the clear colour.

## Cameras

Renderfall does not fly. Left drag and the arrow keys orbit. Right drag, W/S, PgUp/PgDn zoom. R resets. G toggles the scale grid. F toggles fog. The model stays centered.

Levelfall flies. Do not route it through `player_tick` or `input_read`. Those are the game: WASD is yaw-flat, Q/E are vertical, and the arrow keys are look. `input_look` returns only the recentered mouse delta. `input_begin` / `input_end` capture the cursor; do not also call `win_blank_cursor` or the show-cursor count drifts.

Spectator step, in `fly_step`:

- Mouse changes yaw and pitch. Positive pitch looks up. Pitch clamps to about ±89 degrees (`1.55334303`).
- W/S and Up/Down move along the aim: `(sin yaw * cos pitch, sin pitch, cos yaw * cos pitch)`. Looking up and holding W climbs. Holding both a letter and its arrow does not go faster.
- A/D strafe on the horizontal right vector `(cos yaw, 0, -sin yaw)`.
- Left/Right turn. Positive yaw turns right. The rate is `TURN_RATE` (1.44 radians/second), the game's arrow look of 8 pixels per frame at 60 Hz, and it does not speed up with Shift.
- Page Up and Page Down pitch at that same rate. Page Up looks toward the sky. The position does not change. Pitch still clamps at about ±89 degrees.
- The wheel changes `camera_set_fov`. One notch is 120. Forward (positive) lowers the field by `FOV_STEP` (5 degrees). Back raises it. The camera clamps to 45..120. The value stays for the rest of the process. Take the wheel messages in the fly loop, before `win_pump`, or the list's scroll path is not the one that runs.
- Speed is 720 units/second (the game's 12 units per vsync at 60 Hz). Shift multiplies by 4. Delta time is capped at 100 ms.

`hud_set(0)` while the fly view is up, so the engine corner readout does not stack on this one. `hud_set(1)` on the way out. GFALL never calls `hud_set`.

The fly view is a 640 by 480 picture. Text is the engine face: the 8x8 Terminal strike, or the built-in 8x8 if that raster fails. Panels size from `ui_line_height`. Do not draw a second face, and do not cover the centre with a key list.

Each panel is a dark body, a title band in the same hue, and a 1px border in the bright colour. White text sits on that. Place is amber, camera is blue, block is green, frame is grey.

- `place`, top left: the name, indoor or outdoor, the region file, the climate, and the time. The title band says `f1` or `f1 hide`. The time line is `time hh:mm`, the timescale (`x12`, or `stopped` at 0), then what lights the place at that hour: `sun` or `moon` from `daylight_at`, and `lamps N` (`light_lamps`, 0..255) while the street lamps are on. `full bright` when `daylight` is off. Indoors it ends `unused`, because nothing indoors reads the clock (`docs/daylight.md`). The last two lines are the weather (`weather_state`, `docs/weather.md`):
  - `weather rain`, or `weather cloudy>rain 40%` while one kind blends into the next, then `held` when a kind is forced or `next 5h` (`next 45m` under an hour), the game time to the next roll. Indoors it ends `unused`: the weather runs, nothing shows it. With the weather off it ends `off`, drawn clear.
  - What it does now, outdoors and on: `fog 2400` where its fog goes solid (`clear` for none), then `cloud`, `rain`, `snow`, `storm` in percent when above 0, and `view N`, the far clip the weather chose, when `weathersim` is on. Empty indoors or with the weather off. Single spaces between the values: a thunderstorm with `view` is the widest line, and it has to clear the F1 panels.
  - The place panel is six lines tall whatever the lines hold, so it does not jump as the weather changes. It no longer shares the bottom row's height.
- `sound`, left, under place (purple, title band `f2`). F2 shows and hides it; it starts open. Read `docs/sound.md` for what the words mean.
  - `out primary 11025  lead 150ms`: the way onto the card (`audio_mode_name`), its rate, and the mix lead. `out none, silent` when no card opened (`sound_ready`).
  - `fx on 80  voices 5/16`: effects on or off, their volume, voices busy of `SOUND_VOICES`.
  - `music on 80  DUNGEON5.HMI`: music on or off, its volume, the song (`stopped` when chosen but not playing, as with no device).
  - `synth  notes 17/32  cut 0`: the MIDI device kind (`music_kind`), the most notes at once in the song over `musvoices`, and the notes the limit cut.
  - `sources 53  heard 4  loops 4/6`: the place's emitters, those in reach last frame, and loops held of `AMBIENCE_LOOPS`.
  - `sndview 1024  f4`: the engine's emitter markers (`ambience_set_view`), or `off`. LEVELFALL does not draw markers of its own; the squares, words and rings are the engine's `sndview`.
  - Then the `sndrows` nearest sources, nearest first: the word (`Emitter.label`), distance over reach, `loop` or `now` (now and then, `every > 0`), and `play` (a voice has it, `ambience_emitter_playing`), `near` (in reach with no voice: a loop past the six nearest, or a call waiting) or `out`. One pass keeps the shortest list on squared distance; only the rows shown take a square root (x87 `fsqrt`). The panel is `6 + sndrows` lines whatever is near, so it does not jump. `no sources here` when the place has none.
- `camera`, `block`, and `frame` sit in a row along the bottom. Camera is x y z, yaw (and the axis it faces), pitch, and the field of view. Block is the file under the camera (`outside` when the tile is empty), the cell, and the grid span. Frame is visible models and flats over the place totals, fog, the far clip, and `tex L/T NK`: pictures on the card over the level's image count, and their bytes. The frame title band carries fps. The bottom row is four lines tall; camera and block leave the last one blank.
- The frame panel turns red (`WARN_R/G/B`) when something was silently not drawn right. `CAP` after models or flats means the count reached `DRAW_VIS_MAX` (2048, `draw.h`) and the rest of the list was cut; `cull off` in a city does this. `unfit N` after tex means N pictures had no room on the 2MB card (`texcache_stats`). Those stay unbound, and their faces wear whatever texture was bound before them. Do not drop either warning to tidy the panel.
- F1 opens three more panels down the right: `record` (location, climate, family, counts), `draw`, and `keys`. They start closed.
- `draw` uses the console's words, so a label is the command to type: `cull`, `occlude`, `covers`, `faces`, `fog`, each with its key, then `faces kept`, `faces hidden`, `occluders` (faces written into the occlusion cells), and `occluded` (models and sprites those cells dropped). `faces on` drops planes whose normal points away. Keep the labels in step with the console's names and `docs/console.md`.

F5 `cull`, F6 `occlude`, F7 `covers`, F8 `faces`, F9 `fog` flip that engine switch in the fly view and log `name on` or `name off` to the console. The picture changes on the next frame, which the console cannot do, because it freezes the frame. These are engine state, so they stay flipped on the next level and in the console. F10 is not used: Windows takes it for the menu. Key edges are tracked while the console is up, so a key held through it does not fire when it shuts.

F2 shows or hides the sound panel. F4 steps `sndview` off, 512, 1024, 2048, 4096, off, and logs `sndview N` to the console; a cfg range between the steps goes on to the next one up. Both are tracked like F5-F9 while the console is up.

The left button or E uses what the middle of the view rests on (`docs/interact.md`): a door swings, a lever starts its chain, a person says their faction. A passage ends the view and `view_row` opens where it leads, on its own temp heap, with the card left open: the last frame holds, dimmed, under "Loading...", and the next place fades in from black (`gfx_hold`, `gfx_fade_in`). It leads to a house's inside from the street, the street from inside, the dungeon below a town, or the outside above a dungeon. Only Esc or a failed load closes Glide. The landings, and what happens to a passage with nowhere to go, are in `docs/interact.md`. Esc goes back to the list from any of them. The place panel's kind reads `inside` in a house, and F12 does not bookmark there, since `open=` names a list row. F11 steps the engine's `useview` 256, 512, 1024, off, and logs `useview N`; it is tracked like F2 and F4. The keys panel lists both.

F12 is the engine screenshot (`shot_pump`, inside the frame). Levelfall polls the same edge right after `gfx_frame` and writes a bookmark to `levelfall.log`: `bookmark` with `shot_status()` (the file name), then `open=`, `cam=`, `fov=`, `time=`, and `weather=` as cfg lines, then `#` lines with the switches and the frame's counts, and one with the song, voices, sources, heard, loops and `sndview`. That sound line is read when F12 is seen, not snapped, because the sound runs on its own clock. Pasting those five cfg lines into `levelfall.cfg` reopens that view. `weather=` is the kind arriving; a bookmark taken half way through a change reopens with that kind already there. The pose and counts are snapped each loop before the camera moves and before the frame resets the counts, because the shot is the front buffer, the frame before. A tap shorter than one frame can reach one poll and not the other; the bookmark then names the previous shot.

The key immediately left of 1 toggles the engine console. See `docs/console.md`. While that panel is up, including while it is sliding, these loops do not move the camera and do not toggle fog, the grid, help, or the F5-F9 switches. Escape closes the console. A second Escape, after it has shut, leaves the view. F12 writes a BMP beside the program. The name fades in the top right.

## Game controls, unchanged

`player_tick` is still what `GFALL.EXE` uses. hostcheck locks it: yaw 0 moves W toward +Z and D toward +X, Q raises Y, E lowers Y, mouse-right increases yaw. A change to spectator flight belongs in `tools/levelfall/main.c`, not in `player.c`.
