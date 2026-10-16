# Console

The engine console is `src/engine/ui/console.c`. `GFALL.EXE`, `RENDERFALL.EXE`, and `LEVELFALL.EXE` all get it, because `draw_frame` presents it and `input_read` / `input_look` pump it. A game does not install it.

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
| `far` | Far clip in world units. Clamped to the engine cap, 16384. |
| `faces` | `on` or `off`. Off draws ARCH3D faces the normal would drop. |
| `tex` | `auto`, `tiny`, or `sharp`. Tiny never binds a full map. Sharp wants every full map, still dripped by the budget. |
| `budget` | Bytes of full-size maps sent in one frame. 0 pauses new uploads. The cap is 524288. |
| `stats` | Those values, plus models, sprites, faces kept, and faces hidden on the last world frame. |
| `pos` | Camera position and yaw, pitch in degrees. |
| `clear` | Drops the log. |
| `help` | The list above. |

`fov` and `cull` move the slopes in `docs/culling.md`. `tex` and `budget` are the drip in `docs/memory.md`.
