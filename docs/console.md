# Console

The engine console is `src/engine/ui/console.c`. Screenshots are `src/engine/ui/shot.c`. The console calls `shot_save` and `shot_pump`. It does not read the frame buffer or write the file. `GFALL.EXE`, `RENDERFALL.EXE`, and `LEVELFALL.EXE` all get it, because `draw_frame` presents it and `input_read` / `input_look` pump it. A game does not install it.

The key immediately left of 1 toggles it. That is scan code `0x29`, turned into a virtual key with `MapVirtualKey`, so the physical key is the same on a US or ABNT board. The panel slides down from the top over about 200 ms, and slides back up when toggled again. Escape closes it and does not quit. The press that closes it is ignored until the key comes up, so a held Escape does not also leave the view.

While the panel is on screen, including during the slide, the world is not drawn. The frame that was already on the front buffer is copied once, through `grLfbReadRegion`, and written back to the back buffer each console frame. The 3D pass, the HUD, and texture uploads do not run. The camera does not move: `input_read` and `input_look` report no motion, and the viewer loops skip their own keys. Rendering resumes on the first frame after the panel has shut. A command changes engine state immediately. The picture shows that change when the console closes.

The background of the panel is a black wash at alpha 88, so the grabbed frame stays visible. Text uses the engine face, the 8x8 Terminal strike, unless a game installed its own sheet.

## Log

`console_log` appends one line. The buffer is 48 lines of 96 characters. Past that, the oldest line is dropped. Callers cut their own text to one line. Do not allocate a line per message.

The grab is one 640 by 480 picture, 16 bits per pixel, allocated on the main heap the first time the console opens and kept. See `docs/memory.md`.

## Commands

A line is a word and an optional argument. Letters are taken as lower case. Enter runs it. With no argument, a command prints its current value.

| Command | Effect |
|---|---|
| `fov` | Horizontal field of view, 45..120. Default 75. |
| `fog` | `on` or `off`. |
| `cull` | `on` or `off`. Off lists every model and sprite. |
| `covers` | `on` or `off`. On draws the wall test. Green is one face. Red is faces that meet and were joined. A gap between them stays open. Default on. |
| `version` | `on` or `off`. On draws the compile stamp in the bottom left: `build YYYY-MM-DDTHH:MM:SS`, the local time of the link. Default on. With no argument it prints the stamp. |
| `far` | Far clip in world units. Clamped to the engine cap, 16384. |
| `faces` | `on` or `off`. Off draws ARCH3D faces the normal would drop. |
| `stats` | Those values, plus models, sprites, faces kept, and faces hidden on the last world frame. |
| `pos` | Camera position and yaw, pitch in degrees. |
| `screenshot` | Writes the current picture as a 24-bit BMP beside the program, not inside `ARENA2`. The argument is the file name. `.bmp` is added when it is missing. A file of that name is replaced. |
| `clear` | Drops the log. |
| `help` | The list above. |

F12 takes the same picture without opening the console. The file is `screenshot_YYYY-MM-DDTHHMMSS.bmp`, 24-hour local time, year through second. A second press in that same second replaces the file.

The name is drawn at the top right as `name taken...`. It stays solid for about 700 ms, then fades out over about 1.2 s. The saved BMP is the frame that was already on screen, so the note itself is not in that file.

The read is one 640 by 480 picture at 16 bits, allocated on the first shot and kept. The buffer's first row is the top of the glass. The BMP stores the bottom row first, so the file is written from the last row upward. Rows are expanded to 24-bit as they are written. The note's plate uses the same box as the glyphs: `ui_string` takes the top of the line, and the plate's bottom is one line below that. See `docs/memory.md`.

`fov` and `cull` move the slopes in `docs/culling.md`. Pictures are one full image each. See `docs/memory.md`.
