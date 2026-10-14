# AGENTS.md

## Project
- **Goal:** Recreate Daggerfall for Windows 98 on period hardware (Pentium MMX 233, 64 MB RAM, Voodoo 1, AWE64). Renders via 3dfx Glide 2; **glide2x.dll is loaded at runtime**.
- **Status:** Single-room block viewer (GFALL) plus tech demo (SPINTRI). This is a "from scratch" recreation for the 90s, not a port of the original source.
- **Reference:** Prefer public sources for formats and behavior: the [UESP wiki](https://en.uesp.net/wiki/Daggerfall:Daggerfall) (file formats, mechanics), plus community writeups such as the Daggerfall Unity docs and Interkarma's format notes. Do not treat `daggerfall/decompiled/` as authoritative; that decompilation is unreliable.

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
- `src/main.c` — `gfall_main(void *)` entrypoint; platform calls it from `WinMainCRTStartup`. Loads one Daggerfall room and hands a `Level` to `gfx.h`.
- `src/engine/core/` — `df.h` types, endian helpers, x87 `df_sin`/`df_cos` (no libm). `mem.c` is the heap (stdlib malloc if `DF_HOST`, Win32 heap + CRT `memcpy`/`memmove`/`memset`/`strlen` replacements if not).
- `src/engine/platform/` — `win32.c` (`_WinMainCRTStartup`, content dir beside the exe), `input.c`, `fs.c` (POSIX if `DF_HOST`, Win32 otherwise).
- `src/engine/assets/` — VFS over `fs`, loader registry, engine formats: mesh, texture, palette, level.
- `src/engine/scene/` — placed meshes, billboards, transforms, visibility query (everything is visible, in storage order).
- `src/engine/gfx/` — `gfx.h` is the call the game makes. `glide.c` loads glide2x.dll at runtime (plain/`@N`/`_name@N`), `texcache.c` (2MB TMU), `camera.c`, `draw.c`.
- `src/engine/ui/` — HUD primitives.
- `src/game/dagger/` — BSA, ARCH3D, RDB, COL, TEXTURE.###, the one room in `world.c` (`N0000012.RDB`, model 62022 at (896,-512,1152), 2 torches TEXTURE.210 record 6), and `player.c` (WASD/Q/E, mouse look).
- `tools/hostcheck.c` — runs `check_engine.c` then `check_dagger.c`. Engine tests cover transforms, camera, visibility, loaders, VFS. Dagger tests assert the room (1 model, 12 tris, 2 flats, 21 editor markers), texture POT/square tiling, and write `build/tex0.ppm`. **Update the dagger test when world/loading changes.**
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
- **Trust executable sources:** If public docs (UESP, format writeups) and this repo disagree about what the program does, trust Makefile + headers + source + existing build artifacts. This repo is instruction-heavy in its Makefile/checks.
- **Don't commit generated:** `build/` is build output. Keep `daggerfall/DAGGER/ARENA2` as-is (game data). `.gitignore` is standard C/MingW.
- **Minimal changes:** This aims for period accuracy. Avoid modern C or features that would trip the check scripts. If you modify `src/game/dagger/world.c` (hardcoded geometry/data counts), update `tools/check_dagger.c` accordingly.
- **Style:** gnu90 C. Comment non-obvious intent, format quirks, and constraints — this codebase is vibe-coded and has to be reviewable. Follow existing naming (snake_case). Keep Windows `WINVER`/`_WIN32_WINNT`/`WIN32_LEAN_AND_MEAN` in Win32/Glide files.

## Debugging
- `build/*.dump` from objdump (PE info, imports) are produced during checks. Useful to inspect.
- `hostcheck` prints counts, bounds, texture info and writes `build/tex0.ppm`. The hardcoded expectations reflect the exact room kept.
- Glide binding is defensive (tries multiple name mangling) — don't "fix" it for modern toolchains.
