# Frame profile

Where a frame's time goes, measured on the machine it runs on. Read this before editing `src/engine/core/prof.c` or the stage marks in `draw.c`, or before setting a budget anywhere (`docs/streaming.md` sets its own from these numbers).

## How it is measured

The Pentium's time-stamp counter (`rdtsc`, Pentium and later, about 11 cycles a read) counts the CPU's cycles. `prof_begin` and `prof_end` read it round each stage of `draw_frame`; a stage marked twice in a frame adds up. Always on: about 30 reads a frame, a few hundred cycles.

The cycles become milliseconds by calibration, not by an assumed clock: each frame `draw_frame` hands `prof_frame` a performance-counter reading, and over the first quarter second of frames the two are compared (gaps longer than a second, a load or the console, are left out). A 233 shows 233 MHz.

Each stage is summed over a window of about half a second of frames; at its end the mean and the worst single frame are kept. The panel and the console show the last full window.

## The stages

| Stage | What is in it |
|---|---|
| `frame` | The whole of `draw_frame`, the stages below inside it |
| `sim` | The clock and the weather, the walker, the use key and the movers |
| `sound` | `ambience_frame`: the soundscape, the voices' gains, mixing and the card's refill |
| `sky` | The sky pass |
| `cull` | `scene_visible_models`: the frustum and the occlusion cells |
| `models` | Drawing the visible models |
| `decals` | The light decals |
| `windows` | The lit windows' night pass |
| `sprites` | `scene_visible_sprites` and drawing them |
| `precip` | Rain and snow |
| `overlay` | The debug views, the use line, the HUD, a viewer's panels, notes, the build stamp |
| `swap` | `grBufferSwap` |
| `stream` | The world stream's loading and uploads (`docs/streaming.md`), when there is one |
| `play` | The game's own frame, outside `draw_frame`: its rules, bodies and motors, its HUD's sums. The game brackets it with `prof_begin(PROF_PLAY)` and `prof_end`; 0 in the viewers. The game aims at 2 ms or less on the 233 (`docs/game.md`) |

**The card works alongside.** The Voodoo draws from a command FIFO while the CPU goes on. A stage's time is the CPU's own work (transform, clip, project, set up), plus any time it waited because the FIFO was full. The card's fill shows mostly in `swap`, where the CPU waits for the frame to finish before the buffers turn, and with `vsync` on, for the screen's refresh too. So: a big `models` is CPU-bound geometry; a big `swap` with `vsync off` is fill (too many pixels, too many blended passes).

The time outside `frame` (the viewer's own loop, Windows' messages) is the difference between `frame` and the frame time in the fps corner.

## Where to read it

- `prof` in the console: each stage's mean and worst, in ms, with the calibrated clock.
- LEVELFALL: P shows the profile panel, top centre (`profpanel=1` in `levelfall.cfg` opens it at start), and each F12 bookmark logs a `# prof` line with the means.

## The time spare before the retrace

`prof_spare_ms(vsync, fallback_ms)` is the one budget worked out here, the time a stream may load in, because it reads the last frame's stages. `gfx_stream_budget(fallback_ms)` is the same with the card's vsync setting, and it is what a program calls. The last frame's work is `frame` less `swap`. The sum finds the first 60 Hz retrace that work can make with 2.5 ms over (the 1 ms floor and the 1.5 ms margin), and gives what is left before it less the margin, held to 1..8 ms. A frame that would miss its retrace counts to the next one, since it shows there anyway. With vsync off, before calibration, or with no frame timed, the answer is the caller's `fallback_ms`, as each program chooses its own. `tests/check_engine.c` holds the sums (`check_prof_spare`).

## Baseline

To be measured on the 233 with the Voodoo 1 (LEVELFALL, 640 × 480, `vsync off`): Privateer's Hold at the start marker, Daggerfall's square at noon and at night, a Daggerfall street at far 1400 and at 8192.
