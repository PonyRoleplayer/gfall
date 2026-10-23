# AGENTS.md

## Project
- **Goal:** Recreate Daggerfall for Windows 98 on period hardware (Pentium MMX 233, 64 MB RAM, Voodoo 1, AWE64). Renders via 3dfx Glide 2; **glide2x.dll is loaded at runtime**.
- **Status:** Single-room block viewer (GFALL) plus tech demo (SPINTRI). This is a "from scratch" recreation for the 90s, not a port of the original source.
- **Reference:** Prefer public sources for formats and behavior: the [UESP wiki](https://en.uesp.net/wiki/Daggerfall:Daggerfall) (file formats, mechanics), plus community writeups such as the Daggerfall Unity docs and Interkarma's format notes. Do not treat `daggerfall/decompiled/` as authoritative; that decompilation is unreliable.

## Build (Linux, cross-compile)
**Required tools:** `i686-w64-mingw32-gcc` (or gcc-mingw-w64-i686-win32), `i686-w64-mingw32-objdump`, `xorriso`, and native `gcc` (for hostcheck). These are **not present** in this environment; build artifacts already exist in `build/`.

**Primary targets:**
- `make` — Runs `check`, builds `build/spintri.iso`, runs `gfall-check`, `renderfall-check`, and `levelfall-check`, builds `build/gfall.iso`.
- `make hostcheck` — **Only test.** Native build + run (`build/hostcheck`) against `daggerfall/DAGGER/ARENA2`. Writes `build/tex0.ppm`. Does not touch Glide or Windows.
- `make gfall-check` — Cross-builds `GFALL.EXE` and validates: PE32, 80386 GUI, Win95/NT4 subsystem/version, only `KERNEL32.dll` and `USER32.dll` imports, `LoadLibraryA`/`GetProcAddress` present, **no `glide2x.dll` import**, and rejects SSE/P6-specific instructions (cmov/fcmov/fucomi/fcomi/movaps/movss/addps/xmm).
- `make renderfall-check` / `make levelfall-check` — Same validator for `RENDERFALL.EXE` and `LEVELFALL.EXE`.
- `make check` — Same validator for `SPINTRI.EXE`.
- `make clean` — Removes `build/`.

**Execution (target):** Mount the ISO and run `D:\SPINTRI.EXE`, `D:\GFALL.EXE`, `D:\RENDERFALL.EXE`, or `D:\LEVELFALL.EXE`. **Do not copy glide2x.dll onto discs.** Escape quits.

## Codebase layout
- `game/daggerfall/main.c` — `GFALL.EXE` entry. Same relationship to the engine as `tools/renderfall/main.c`: it links engine and dagger code and brings `WinMainCRTStartup`. Loads Privateer's Hold and hands a `Level` to `gfx.h`.
- `src/engine/core/` — `df.h` types, endian helpers, x87 `df_sin`/`df_cos` (no libm). `mem.c` is the heap (stdlib malloc if `DF_HOST`, Win32 heap + CRT `memcpy`/`memmove`/`memset`/`strlen` replacements if not).
- `src/engine/platform/` — `win32.c` (`_WinMainCRTStartup`, content dir beside the exe), `input.c`, `fs.c` (POSIX if `DF_HOST`, Win32 otherwise).
- `src/engine/assets/` — VFS over `fs`, loader registry, engine formats: mesh, texture, palette, level.
- `src/engine/scene/` — placed meshes, billboards, transforms, visibility. Models and sprites whose box misses the view, or sits past the fog or the far clip, are not drawn. See `docs/culling.md`.
- `src/engine/gfx/` — `gfx.h` is the call the game makes. `glide.c` loads glide2x.dll at runtime (plain/`@N`/`_name@N`), `texcache.c` (one full image per picture, no mip chain, no per-frame upload), `camera.c`, `draw.c`.
- `src/engine/ui/` — HUD, text, and the console. Text draws a glyph sheet the game installed. With no sheet, the engine rasterizes the 8x8 Terminal strike (not the 8x12 stock OEM face). The built-in 8x8 face draws when that raster fails. The console is `docs/console.md`.
- `src/engine/ui/shot.c` — BMP screenshots. Reads the front buffer, writes the file beside the program, and draws the fading note. `console.c` only dispatches `screenshot` and pumps F12. Read `docs/console.md` before editing `shot.c`.
- `src/engine/ui/build.c` — the compile stamp in the bottom left. The Makefile passes the local time of the link as `BUILD_STAMP` (`YYYY-MM-DDTHH:MM:SS`). `version` turns the drawing on and off. Read `docs/console.md` before editing `build.c`.
- `src/game/dagger/` — BSA, ARCH3D, RDB, RMB, COL, TEXTURE.###, FONT0000.FNT, Privateer's Hold in `dungeon.c`/`world.c` (`MAPDITEM.017`: `S0000999.RDB` at the origin tile plus four `B` border blocks, camera on editor marker 10), outdoor locations in `rmb.c`, and `player.c` (WASD/Q/E, mouse look).
- `tools/levelfall/main.c` — `LEVELFALL.EXE`. Indexes every indoor (`MAPDITEM`) and outdoor (`MAPPITEM`) place at startup, filters by name, and flies through one. The fly camera and the titled panels are `docs/viewers.md`. Same window and Glide path as renderfall.
- `tools/hostcheck.c` — runs `check_engine.c` then `check_dagger.c`. Engine tests cover transforms, camera, visibility, loaders, VFS. Dagger tests assert the hold (models, sprites, start marker), the dungeon list (4232 records, Privateer's Hold in region 17), the outdoor list (15251, Daggerfall region 17 block `WALLAA02.RMB`, one farmstead loads), texture POT/square tiling, and write `build/tex0.ppm`. **Update the dagger test when world/loading changes.**
- `docs/dungeons.md` — dungeon record layout, climate, and which loader to call. Read this before editing `dungeon.c` or `world.c`.
- `docs/viewers.md` — RENDERFALL and LEVELFALL list UI, keys, and the temp heap. Read this before editing either viewer.
- `docs/culling.md` — block spans, baked boxes, and ARCH3D face cull. Read this before editing `scene.c`, the far clip, or fog.
- `docs/console.md` — the engine console, its log, and the commands that change a live frame. Read this before editing `console.c`.
- `docs/memory.md` — heap, the shared mesh cache, and the 2MB texture card. Read this before editing `world.c`, `texcache.c`, or `glide.c` uploads.
- `daggerfall/DAGGER/ARENA2/` — Game data used at runtime and by hostcheck. Not copied onto the discs. The executables still look for `ARENA2` beside themselves.
- `third_party/glide2/include/` — Glide 2 headers.

## Critical constraints
- **Platform:** Windows 98, PE32 GUI, 80386, subsystem 4.0. No C runtime: `-nostdlib`, custom entry `_WinMainCRTStartup`. Only `KERNEL32.dll`/`USER32.dll` may be imported. `glide2x.dll` imported dynamically.
- **CPU:** `-march=pentium-mmx -mfpmath=387 -mno-sse`. The check forbids SSE/cm* and `xmm`-using insns — do not introduce them.
- **C:** `-std=gnu90` (C90 + GNU extensions). Math uses x87 inline asm (`df_sin`, `df_cos`, `fsqrt` in arch3d). Avoid `libm` calls.
- **Memory:** Win32 heap in `mem.c` (not C runtime malloc). Host uses libc malloc. Keep allocation consistent with the target side.
- **Textures (Voodoo 1, 2MB TMU):** Must be square and power-of-two. Non-square/non-POT source -> tiled into `pot_w x pot_h` (power-of-two square), with subrect `span_w x span_h`; `pot_w % span_w == 0`. `glide.c` uploads `pot` with `cutout` flag to pack. hostcheck enforces and sums bytes < 1500000.
- **Privateer's Hold:** The one loaded dungeon. Block list comes from `MAPDITEM.017`; tiles are 2048 units; the camera starts at marker record 10. Model faces store texture keys 119/120/122/123/124. The game builds one table from the location id and that place's climate: each key draws an offset 0..4 (a draw of 2 becomes 4) and adds it to the climate's crypt base. This hold is woodlands, so the keys become 323/322/319/322/320. Door set 74 (records 0..2), sewer set 168, and cave set 47 use the same family (374, 368, and 347 when the record exists). Sprites are not remapped. Asset paths use backslashes and an `ARENA2` directory beside the exe. That directory is not on the disc.
- **No SEH, no DLLs except those above.** Keep minimal imports.

## Conventions
- **Build first, test fast:** Use `make hostcheck` to verify logic on the host; then `make gfall-check` if changing Win32/Glide code paths.
- **Trust executable sources:** If public docs (UESP, format writeups) and this repo disagree about what the program does, trust Makefile + headers + source + existing build artifacts. This repo is instruction-heavy in its Makefile/checks.
- **Don't commit generated:** `build/` is build output. Keep `daggerfall/DAGGER/ARENA2` as-is (game data). `.gitignore` is standard C/MingW.
- **Minimal changes:** This aims for period accuracy. Avoid modern C or features that would trip the check scripts. If you modify `src/game/dagger/world.c` (hardcoded geometry/data counts), update `tools/check_dagger.c` accordingly.
- **Style:** gnu90 C. Comment non-obvious intent, format quirks, and constraints — this codebase is vibe-coded and has to be reviewable. Follow existing naming (snake_case). Keep Windows `WINVER`/`_WIN32_WINNT`/`WIN32_LEAN_AND_MEAN` in Win32/Glide files.
- **Document the change:** Behavior a later edit has to preserve goes in `docs/` before the commit, and the layout list above gains a line if the doc is new. Update the existing note (`docs/culling.md`, `docs/memory.md`, `docs/dungeons.md`, `docs/viewers.md`) when the change is one of those. A commit that changes what the frame draws, what is loaded, or a budget the next change must respect, without that note, is incomplete.

## Debugging
- `build/*.dump` from objdump (PE info, imports) are produced during checks. Useful to inspect.
- `hostcheck` prints counts, bounds, texture info and writes `build/tex0.ppm`. The hardcoded expectations reflect the exact room kept.
- Glide binding is defensive (tries multiple name mangling) — don't "fix" it for modern toolchains.
