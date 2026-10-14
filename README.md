# gfall

One Daggerfall room, drawn with Glide 2, for Windows 98 and a Voodoo 1. Built from Linux. The disc does not carry `glide2x.dll`. The guest uses the 3dfx driver already installed on the machine.

## Build

From the repo root:

```
sh tools/check-deps
make
```

`make` runs that check again, then builds both discs.

| Command | Result |
| --- | --- |
| `make hostcheck` | Native test only. No Windows, no Glide. Writes `build/tex0.ppm`. |
| `make` | `build/spintri.iso` and `build/gfall.iso`. |
| `make clean` | Deletes `build/`. |

`make hostcheck` needs `gcc`, `make`, and `daggerfall/DAGGER/ARENA2`. A full `make` also needs the i686 MinGW cross compiler, `i686-w64-mingw32-objdump`, `xorriso`, and `file`. If something is missing, `sh tools/check-deps` names it and the Debian package. `sh tools/check-deps host` checks the native test only.

On Debian or Ubuntu:

```
sudo apt install build-essential file gcc-mingw-w64-i686 binutils-mingw-w64-i686 xorriso
```

## Run

Mount `build/gfall.iso` and run `D:\GFALL.EXE`. WASD moves, the mouse looks, Q is up, E is down, Escape quits. `build/spintri.iso` is the Glide text demo: `D:\SPINTRI.EXE`.

Do not copy `glide2x.dll` onto either disc. The system driver is the one that draws the 3dfx splash.
