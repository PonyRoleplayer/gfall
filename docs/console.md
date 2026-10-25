# Console

The console is three files:

| File | Holds | Builds |
|---|---|---|
| `src/engine/core/log.c` | The log: levels, the line ring, the `printf` subset, the file sink. | Every exe and hostcheck. |
| `src/engine/ui/cmd.c` | The command table, variables, parsing, the typed line, history, Tab completion, and the built-in commands that need nothing from the engine. | Every exe and hostcheck. |
| `src/engine/ui/console.c` | Win32 and Glide: the panel, its keys, `exec`, `logfile`, and the commands that reach into the engine. | The three exes. |

Screenshots are `src/engine/ui/shot.c`. The console calls `shot_save` and `shot_pump`. It does not read the frame buffer or write the file. `GFALL.EXE`, `RENDERFALL.EXE`, and `LEVELFALL.EXE` all get the console, because `draw_frame` presents it, `input_read` / `input_look` pump it, and `win_pump` hands it the keys. A game does not install it. A game may add commands with `cmd_add` or `cmd_var`.

## Panel

The key immediately left of 1 toggles it. That is scan code `0x29`, so the physical key is the same on a US or an ABNT board. The panel slides down from the top over about 200 ms and covers half the screen. It slides back up when toggled again. Escape closes it and does not quit. An Escape that closed it is ignored until the key comes up, so a held Escape does not also leave the view. `input_read` and the window's `WM_KEYDOWN` both ask `console_take_escape` before they quit.

While the panel is on screen, including during the slide, the world is not drawn. The frame that was already on the front buffer is copied once, through `grLfbReadRegion`, and written back to the back buffer each console frame. The 3D pass, the HUD, and texture uploads do not run. The camera does not move: `input_read` and `input_look` report no motion, and the viewer loops skip their own keys. Rendering resumes on the first frame after the panel has shut. A command changes engine state immediately. The picture shows that change when the console closes.

The background is a black wash at alpha 88, so the grabbed frame stays visible. Text uses the engine face, the 8x8 Terminal strike, unless a game installed its own sheet. The newest log line is just above the prompt and older lines go up. When the view is scrolled back, the bottom line is a row of `^` marks, and new lines do not move the view.

## Keys

Keys arrive as window messages, not polled. `win_pump` gives every message to `console_message` before `TranslateMessage`. With the console open, an editing key is kept, any other key is translated into `WM_CHAR` and not delivered, and the character is typed. So the keyboard layout, Shift, and the keyboard's own repeat all work, and a key pressed and released between two slow frames is not lost. The toggle key is kept untranslated, so it never types a character or starts a dead key. Alt combinations are never kept. The console takes keys only while some frame loop has pumped it in the last 500 ms. The viewers' lists use their own message loops and never reach it.

| Key | Effect |
|---|---|
| Characters | Typed at the cursor. Printable ASCII only. |
| Enter | Runs the line. It is logged as `> line` and kept in the history. |
| Up / Down | Walk the history, newest first. Down past the newest gives back the line you were typing. |
| Tab | Completes the command name at the cursor, or the name after `help` or `toggle`. Several matches extend to their common start, then list. |
| Left / Right, Home / End | Move the cursor. |
| Backspace / Delete | Remove before / at the cursor. |
| Ctrl+U | Empty the line. |
| Ctrl+L | Empty the log. |
| PgUp / PgDn | Scroll the log half a panel. Ctrl+Home goes to the oldest line, Ctrl+End to the newest. |
| Escape | Close. The line is kept for next time. |

The history keeps 32 lines of up to 95 characters, for the process. A line equal to the newest entry is not added again.

## Commands

A line is a command name and its arguments, separated by spaces. Double quotes keep spaces inside one argument. `;` separates commands on one line. `//` starts a comment, and so does a `#` at the start of a command. Names are not case sensitive. `help` lists every name. `help name` explains one. `find word` searches names and help text.

A variable prints its value when typed alone, and takes a new one as an argument. A number is clamped to the range. An on/off switch takes `on`, `off`, `1`, `0`, `yes`, `no`, or `toggle`, and `toggle name` does the same. Every variable prints what it holds after the change.

| Variable | Range | Effect |
|---|---|---|
| `fov` | 45..120 | Horizontal field of view. Default 75. |
| `far` | 16..16384 | Far clip in world units. Default 1400. LEVELFALL starts at 8192. |
| `fog` | on/off | Distance fog. |
| `cull` | on/off | Off lists every model and sprite. |
| `occlude` | on/off | Off keeps the frustum and skips the occlusion step, to compare a frame. Default on. See `docs/culling.md`. |
| `covers` | on/off | Washes the occlusion cells of the last frame green. A model or sprite whose box lands only on green, and is behind it, was not drawn. Default off. |
| `faces` | on/off | Off draws ARCH3D faces the normal would drop. |
| `vsync` | on/off | On waits for the retrace on each swap. Default on. |
| `wbuffer` | on/off | On is the W-buffer, depth from 1/w. Off is the 16-bit Z-buffer, to compare. Default on. See `docs/glide.md`. |
| `mipmap` | on/off | On uploads mip chains, off single levels. Both are budgeted to fit the card. A change sends the level again on the next frame. Default on. See `docs/glide.md`. |
| `lights` | on/off | Dungeon lights: ambient tint and light decals. Off draws every place full bright. Default on. See `docs/lighting.md`. |
| `ambient` | 0..255 | Brightness away from lights in a lit place. Default 112. |
| `lightrange` | 1..8 | How many times its radius a light reaches. Cuts the decals again. Default 3. |
| `timescale` | 0..2000 | Game seconds a real second. Default 12; 0 stops the clock. |
| `sky` | on/off | The sky behind an outdoor place. Off draws on the viewer's background. Default on. |
| `daylight` | on/off | Sun and moon light outdoors, and the street lamps. Off draws outdoors full bright. Default on. |
| `lightview` | 0..2048 | Squares and rings for the lights within this many units of the camera, the 64 nearest: yellow when its decals were drawn, orange when not. 0 (default) is off. Try 512. |
| `planar` | on/off | On sends each face as one planar polygon, when `glide2x.dll` has `grDrawPlanarPolygonVertexList`. Off sends its triangles. Reads off when the DLL lacks it. Default on. |
| `hud` | on/off | GFALL's position and fps corners. LEVELFALL draws its own panels and turns this off in the view. |
| `fps` | on/off | The fps corner alone. F3 flips it too. |
| `logfile` | on/off | Copies the log to `console.log` beside the program. See Log. |

`vsync` matters when reading the fps. The card runs at 60 Hz with two colour buffers. A third does not fit in a Voodoo 1's 2 MB frame buffer at 640x480 with depth. With vsync on, a frame that takes 17 ms waits for the next retrace and shows at 30 fps, so the corner's half-second average reads values such as 35 or 45 when frames alternate between one and two retraces. With vsync off the swap is immediate, the picture tears, and the counter reads what the scene costs. Use `vsync off` and `stats` to time a view.

| Command | Effect |
|---|---|
| `quit`, `qqq`, `exit` | Leave the program, as the title-bar cross does. In RENDERFALL and LEVELFALL this leaves from the view too, not just back to the list. |
| `stats` | Frame time: mean and worst over the last whole second of world frames, swap to swap, with the vsync state. Then models, sprites, and faces drawn on the last world frame, faces turned away, the occlusion counts (`occluders`, faces written into the cells, and the boxes they hid), the lights, outdoors the hour and how lit the lamps are, and the view variables. |
| `pos` | Camera position, and yaw and pitch in degrees. |
| `time [hh:mm]` | The world clock and its timescale; with an hour, sets it: `time 21`, `time 6:45`. The clock runs only while the world draws, so it stops while the console is open. See `docs/daylight.md`. |
| `setpos x y z [yaw [pitch]]` | Moves the camera. Whole world units and degrees. Works where the program gave the console its camera: GFALL and LEVELFALL's view. RENDERFALL orbits a model and offers none. |
| `screenshot [name]` | Writes the current picture as a 24-bit BMP beside the program, not inside `ARENA2`. `.bmp` is added when it is missing. A file of that name is replaced. No name uses the F12 name. |
| `exec file` | Runs the commands in a file beside the program, as if typed, without echoing them. `.cfg` is added when the name has no dot. A file may exec another, four deep. Up to 4095 bytes are read. |
| `glinfo` | The `glide2x.dll` path and export spelling, the board, the frame buffer and TMU sizes and revisions, the mode (depth buffer, vsync, planar polygons), and the texture room. |
| `texmem` | Pictures on the card, their bytes, how many did not fit, whether they are chains, how many the budget shrank, and the TMU room left. |
| `mem` | Bytes allocated on the engine's main and temp heaps, and the machine's free physical memory. |
| `log [error|warn|info|debug]` | How much the engine logs. Default `info`. |
| `version [on|off]` | Prints the compile stamp, `build YYYY-MM-DDTHH:MM:SS`, the local time of the link. on/off sets whether it is drawn in the bottom left. Default on. |
| `echo text` | Prints the text. |
| `history` | The lines typed, oldest first. |
| `clear` | Empties the log. |
| `help [name]`, `find word` | See above. |

`autoexec.cfg` beside the program runs on the first frame, when the device is open, if it exists. A missing file is silent, except for a debug line. For example:

```
// timing a view on the 233
vsync off; far 2800
logfile on
```

## Log

`log_print(level, fmt, ...)` and `log_text(level, s)` append one line. A line at `LOG_PRINT` is a console reply and is always kept. `LOG_ERROR`, `LOG_WARN`, `LOG_INFO`, and `LOG_DEBUG` are kept only up to the level `log` chose. A line above the level costs one compare, so a debug line need not be guarded. Error and warning lines start with `error: ` and `warning: `. The formatter is a `printf` subset: `%d %i %u %x %c %s %f %%`, `-` and `0` flags, a width, and a precision. `%f` takes at most four decimals. It is not used per frame.

The ring is 256 lines of 95 characters, static. Past that, the oldest line is dropped. A longer line is cut, not wrapped. Tabs and newlines inside a line become spaces. Write one line per call. Do not log from a per-frame loop except on a change of state, as the draw cap warning does.

What the engine logs:

| Level | When |
|---|---|
| error | `glide2x.dll` missing, an entry point missing, no board, `grSstWinOpen` failed, the palette or texture count rejected, an allocation that failed (with its size and heap), `logfile` unable to write. |
| warn | Pictures that did not fit on the card. A visible list that reached `DRAW_VIS_MAX`: once when it does, again only after it drops below. An `exec` file cut at 4095 bytes or nested too deep. |
| info | The `glide2x.dll` path and export spelling, the board, the mode and texture room when the device opens. Each level's models, blocks, sprites, and pictures, and how many pictures went on the card. Each `exec`. |
| debug | A world frame that took over 100 ms, at most once a second. A missing `autoexec.cfg`. |

`logfile on` writes `console.log` beside the program. It replaces the old file, writes the compile stamp, then every line already in the ring, oldest first, so the Glide start-up is in the file even when `autoexec.cfg` turns it on. From then on every kept line is written as it is logged. There is no flush per line: Windows keeps written data when the program dies, and a flush is a disk wait on Windows 98. LEVELFALL and RENDERFALL also keep their own `levelfall.log` and `renderfall.log`. Those are the viewers' records, not this log.

The grab is one 640 by 480 picture, 16 bits per pixel, allocated on the main heap the first time the console opens and kept. See `docs/memory.md`.

## Screenshots

F12 takes the same picture as `screenshot` without opening the console. The file is `screenshot_YYYY-MM-DDTHHMMSS.bmp`, 24-hour local time, year through second. A second press in that same second replaces the file.

The name is drawn at the top right as `name taken...`. It stays solid for about 700 ms, then fades out over about 1.2 s. The saved BMP is the frame that was already on screen, so the note itself is not in that file.

The read is one 640 by 480 picture at 16 bits, allocated with `mem_keep` on the first shot and kept, so it outlives a viewer's temp heap. The buffer's first row is the top of the glass. The BMP stores the bottom row first, so the file is written from the last row upward. Rows are expanded to 24-bit as they are written. The note's plate uses the same box as the glyphs: `ui_string` takes the top of the line, and the plate's bottom is one line below that. See `docs/memory.md`.

LEVELFALL also flips `cull`, `occlude`, `covers`, `faces`, and `fog` with F5 to F9, without opening the console, so the frame changes as you watch, and its `draw` panel uses these command names. Its F12 also writes a bookmark to `levelfall.log`. See `docs/viewers.md`.

`fov`, `cull` and `occlude` change the cull in `docs/culling.md`. Pictures are one full image each. See `docs/memory.md`.
