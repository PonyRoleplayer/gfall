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

Glide clear stays black (the default). Fog uses that color. `gfx_set_fog` turns the distance table on or off; the default is on, and a call before `gfx_startup` is applied when the device opens. Renderfall reads `renderfall.cfg` beside the exe (`background=R,G,B`, `grid=1` or `0`, `fog=1` or `0`) and F toggles fog for the rest of the run. Levelfall reads `levelfall.cfg` beside the exe for `background`, `fog`, `far`, `fov`, `open`, and `cam`, each as `key=value`. A missing file is written on first run; an older file without the new keys still loads. `fov` is applied once at startup and stays for the process, like the wheel. `open=indoor,R,I` or `open=outdoor,R,I` (region, then the `DunRef` index) selects that row and flies it before the list waits; an unknown place logs `open=: no such place` and stays on the list. `cam=x,y,z,yaw,pitch` (world units, degrees) replaces the start marker for that first open only, and is ignored without `open`. Nothing on the disc is a cfg. `far` is the clip distance in world units (`gfx_set_far`); the engine default is 1400 and the cap is 16384. Levelfall's own default is 8192. Fog, when on, goes solid at about four fifths of that distance, in the clear colour.

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

- `place`, top left: the name, indoor or outdoor, the region file, and the climate. The title band says `f1` or `f1 hide`.
- `camera`, `block`, and `frame` sit in a row along the bottom. Camera is x y z, yaw (and the axis it faces), pitch, and the field of view. Block is the file under the camera (`outside` when the tile is empty), the cell, and the grid span. Frame is visible models and flats over the place totals, fog, the far clip, and `tex L/T NK`: pictures on the card over the level's image count, and their bytes. The frame title band carries fps. The bottom row is four lines tall; camera and block leave the last one blank.
- The frame panel turns red (`WARN_R/G/B`) when something was silently not drawn right. `CAP` after models or flats means the count reached `DRAW_VIS_MAX` (2048, `draw.h`) and the rest of the list was cut; `cull off` in a city does this. `unfit N` after tex means N pictures had no room on the 2MB card (`texcache_stats`). Those stay unbound, and their faces wear whatever texture was bound before them. Do not drop either warning to tidy the panel.
- F1 opens three more panels down the right: `record` (location, climate, family, counts), `draw`, and `keys`. They start closed.
- `draw` uses the console's words, so a label is the command to type: `cull`, `occlude`, `covers`, `faces`, `fog`, each with its key, then `faces kept`, `faces hidden`, `occluders` (faces written into the occlusion cells), and `occluded` (models and sprites those cells dropped). `faces on` drops planes whose normal points away. Keep the labels in step with the console's names and `docs/console.md`.

F5 `cull`, F6 `occlude`, F7 `covers`, F8 `faces`, F9 `fog` flip that engine switch in the fly view and log `name on` or `name off` to the console. The picture changes on the next frame, which the console cannot do, because it freezes the frame. These are engine state, so they stay flipped on the next level and in the console. F10 is not used: Windows takes it for the menu. Key edges are tracked while the console is up, so a key held through it does not fire when it shuts.

F12 is the engine screenshot (`shot_pump`, inside the frame). Levelfall polls the same edge right after `gfx_frame` and writes a bookmark to `levelfall.log`: `bookmark` with `shot_status()` (the file name), then `open=`, `cam=`, and `fov=` as cfg lines, then two `#` lines with the switches and the frame's counts. Pasting the three cfg lines into `levelfall.cfg` reopens that view. The pose and counts are snapped each loop before the camera moves and before the frame resets the counts, because the shot is the front buffer, the frame before. A tap shorter than one frame can reach one poll and not the other; the bookmark then names the previous shot.

The key immediately left of 1 toggles the engine console. See `docs/console.md`. While that panel is up, including while it is sliding, these loops do not move the camera and do not toggle fog, the grid, help, or the F5-F9 switches. Escape closes the console. A second Escape, after it has shut, leaves the view. F12 writes a BMP beside the program. The name fades in the top right.

## Game controls, unchanged

`player_tick` is still what `GFALL.EXE` uses. hostcheck locks it: yaw 0 moves W toward +Z and D toward +X, Q raises Y, E lowers Y, mouse-right increases yaw. A change to spectator flight belongs in `tools/levelfall/main.c`, not in `player.c`.
