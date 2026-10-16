# Viewers

`RENDERFALL.EXE` (`tools/renderfall/main.c`) lists `ARCH3D.BSA` and orbits one model. `LEVELFALL.EXE` (`tools/levelfall/main.c`) lists every indoor and outdoor place and flies through one. Both are PE32 GUI programs on the gfall disc, next to `ARENA2`. Neither imports `glide2x.dll`. `comctl32.dll` is `LoadLibrary`'d for `SysListView32`; it is not in the import table. `make renderfall-check` and `make levelfall-check` are the same PE checks as `gfall-check`.

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

`fnt_install` runs once, before any temp heap, so the glyph sheet stays on the main heap across opens. The place index is also filled inside one temp heap, then `mem_temp_end`, so the map files are not kept. The `DunRef` array itself is allocated before that. A failed open shows `dungeon_error()`, `world_error()`, or `gfx_error()` and stays on the list. Esc in the view sets the quit flag; clear it before waiting on the list again or the next Esc test trips immediately. The title-bar cross sets `win_closed` and posts `WM_QUIT`, including from the fly view, so that path leaves the process instead of returning to the list.

Glide clear stays black (the default). Fog uses that color. `gfx_set_fog` turns the distance table on or off; the default is on, and a call before `gfx_startup` is applied when the device opens. Renderfall reads `renderfall.cfg` beside the exe (`background=R,G,B`, `grid=1` or `0`, `fog=1` or `0`) and F toggles fog for the rest of the run. Levelfall reads `levelfall.cfg` beside the exe for `background`, `fog`, and `far`, each as `key=value`. A missing file is written on first run. Nothing on the disc is a cfg. `far` is the clip distance in world units (`gfx_set_far`); the engine default is 1400 and the cap is 16384. Levelfall's own default is 8192. Fog, when on, goes solid at about four fifths of that distance, in the clear colour.

## Cameras

Renderfall does not fly. Left drag and the arrow keys orbit. Right drag, W/S, PgUp/PgDn zoom. R resets. G toggles the scale grid. F toggles fog. The model stays centered.

Levelfall flies. Do not route it through `player_tick` or `input_read`. Those are the game: WASD is yaw-flat, Q/E are vertical, and the arrow keys are look. `input_look` returns only the recentered mouse delta. `input_begin` / `input_end` capture the cursor; do not also call `win_blank_cursor` or the show-cursor count drifts.

Spectator step, in `fly_step`:

- Mouse changes yaw and pitch. Positive pitch looks up. Pitch clamps to about ±89 degrees (`1.55334303`).
- W/S move along the aim: `(sin yaw * cos pitch, sin pitch, cos yaw * cos pitch)`. Looking up and holding W climbs.
- A/D and Left/Right strafe on the horizontal right vector `(cos yaw, 0, -sin yaw)`.
- Up/Down arrows move on world Y, ignoring pitch.
- Speed is 720 units/second (the game's 12 units per vsync at 60 Hz). Shift multiplies by 4. Delta time is capped at 100 ms.

The HUD (`hud_draw`, already called from `gfx_frame`) prints position, yaw, pitch, and fps (F3). Levelfall's own overlay is the dungeon name plus the F1 help lines.

## Game controls, unchanged

`player_tick` is still what `GFALL.EXE` uses. hostcheck locks it: yaw 0 moves W toward +Z and D toward +X, Q raises Y, E lowers Y, mouse-right increases yaw. A change to spectator flight belongs in `tools/levelfall/main.c`, not in `player.c`.
