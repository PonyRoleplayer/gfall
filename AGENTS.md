# AGENTS.md

## Project
- **Goal:** Recreate Daggerfall for Windows 98 on period hardware (Pentium MMX 233, 64 MB RAM, Voodoo 1, AWE64). Renders via 3dfx Glide 2; **glide2x.dll is loaded at runtime**.
- **Status:** Single-room block viewer (GFALL) plus tech demo (SPINTRI). This is a "from scratch" recreation for the 90s, not a port of the original source. See `daggerfall/decompiled/NOTES.txt` for reference, but do not build against it.

## Build (Linux, cross-compile)
**Required tools:** `i686-w64-mingw32-gcc` (or gcc-mingw-w64-i686-win32), `i686-w64-mingw32-objdump`, `xorriso`, and native `gcc` (for hostcheck). These are **not present** in this environment; build artifacts already exist in `build/`.

**Primary targets:**
- `make` — Runs `check`, builds `build/spintri.iso`, runs `gfall-check`, builds `build/gfall.iso`.
- `make hostcheck` — **Only test.** Native build + run (`build/hostcheck`) against `daggerfall/DAGGER/ARENA2`. Writes `build/tex0.ppm`. Does not touch Glide or Windows.
- `make gfall-check` — Cross-builds `GFALL.EXE` and validates: PE32, 80386 GUI, Win95/NT4 subsystem/version, only `KERNEL32.dll` and `USER32.dll` imports, `LoadLibraryA`/`GetProcAddress` present, **no `glide2x.dll` import**, and rejects SSE/P6-specific instructions (cmov/fcmov/fucomi/fcomi/movaps/movss/addps/xmm).
- `make check` — Same validator for `SPINTRI.EXE`.
- `make clean` — Removes `build/`.

**Execution (target):** Mount the ISO and run `D:\SPINTRI.EXE` or `D:\GFALL.EXE`. **Do not copy glide2x.dll onto discs.** Escape quits.

## Codebase layout
- `src/main.c` — `gfall_main(void *)` entrypoint; platform calls it from `WinMainCRTStartup`.
- `src/df.h` — Shared types, endian read helpers, `df_sin/df_cos` use **x87 inline asm** (no libm). Respect when adding math.
- `src/platform/` — Win32 and host I/O: `win32.c` sets `_WinMainCRTStartup` and gets module root (CD dir → uses `.../ARENA2`), `input.c`, `fs.c` (POSIX if `DF_HOST`, Win32 otherwise), `mem.c` (stdlib malloc if `DF_HOST`, Win32 heap + CRT `memcpy/memmove/memset/strlen` replacements if not).
- `src/assets/` — Daggerfall formats: `bsa.c` (BSA archives), `arch3d.c` (ARCH3D.BSA meshes, `fsqrt` via x87), `texture.c` (source tiling into POT square), `palette.c`, `rdb.c` (RDB block data).
- `src/render/` — `glide.c` (dynamic loading of glide2x.dll; exports resolved by plain/`@N`/`_name@N` to work with retail driver), `texcache.c` (2MB TMU), `camera.c`, `draw.c`, `hud.c`.
- `src/game/` — `world.c` hardcodes **one** room from `N0000012.RDB` (room model 62022, position (896,-512,1152), 2 torches TEXTURE.210 record 6). `player.c` (WASD/Q/E, mouse look).
- `tools/hostcheck.c` — End-to-end host validation: rotation/camera/math, parses ARENA2, asserts exact hardcoded expectations (1 model, 12 tris, 2 flats, 21 editor markers), checks texture POT/square tiling, projects screen points, writes `build/tex0.ppm`. **Update this when world/loading changes.**
- `daggerfall/DAGGER/ARENA2/` — Game data used at runtime/for tests (baked into ISOs).
- `third_party/glide2/include/` — Glide 2 headers.

## Critical constraints
- **Platform:** Windows 98, PE32 GUI, 80386, subsystem 4.0. No C runtime: `-nostdlib`, custom entry `_WinMainCRTStartup`. Only `KERNEL32.dll`/`USER32.dll` may be imported. `glide2x.dll` imported dynamically.
- **CPU:** `-march=pentium-mmx -mfpmath=387 -mno-sse`. The check forbids SSE/cm* and `xmm`-using insns — do not introduce them.
- **C:** `-std=gnu90` (C90 + GNU extensions). Math uses x87 inline asm (`df_sin`, `df_cos`, `fsqrt` in arch3d). Avoid `libm` calls.
- **Memory:** Win32 heap in `mem.c` (not C runtime malloc). Host uses libc malloc. Keep allocation consistent with the target side.
- **Textures (Voodoo 1, 2MB TMU):** Must be square and power-of-two. Non-square/non-POT source -> tiled into `pot_w x pot_h` (power-of-two square), with subrect `span_w x span_h`; `pot_w % span_w == 0`. `glide.c` uploads `pot` with `cutout` flag to pack. hostcheck enforces and sums bytes < 1500000.
- **Single-room world:** Hardcoded to `N0000012.RDB`, specific model/flats/start (must not change lightly). Asset paths use backslashes/ARENA2 root (in CD root: `ARENA2/` sits next to `.EXE`).
- **No SEH, no DLLs except those above.** Keep minimal imports.

## Conventions
- **Build first, test fast:** Use `make hostcheck` to verify logic on the host; then `make gfall-check` if changing Win32/Glide code paths.
- **Trust executable sources:** If behavior differs between docs (e.g. README/NOTES) and code/Makefile, trust Makefile + headers + source + existing build artifacts. This repo is instruction-heavy in its Makefile/checks.
- **Don't commit generated:** `build/` is build output. Keep `daggerfall/DAGGER/ARENA2` and `daggerfall/decompiled` as-is (data/reference). `.gitignore` is standard C/MingW.
- **Minimal changes:** This aims for period accuracy. Avoid modern C or features that would trip the check scripts. If you modify `src/game/world.c` (hardcoded geometry/data counts), update `tools/hostcheck.c` accordingly.
- **Style:** gnu90 C. No comments unless asked. Follow existing naming (snake_case). Keep Windows `WINVER`/`_WIN32_WINNT`/`WIN32_LEAN_AND_MEAN` in Win32/Glide files.

## Debugging
- `build/*.dump` from objdump (PE info, imports) are produced during checks. Useful to inspect.
- `hostcheck` prints counts, bounds, texture info and writes `build/tex0.ppm`. The hardcoded expectations reflect the exact room kept.
- Glide binding is defensive (tries multiple name mangling) — don't "fix" it for modern toolchains.
