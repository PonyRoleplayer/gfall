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
| `make renderfall-check` | `build/RENDERFALL.EXE`, the ARCH3D model viewer. |
| `make` | `build/spintri.iso` and `build/gfall.iso`. The gfall disc also carries `RENDERFALL.EXE`. |
| `make clean` | Deletes `build/`. |

`make hostcheck` needs `gcc`, `make`, and `daggerfall/DAGGER/ARENA2`. A full `make` also needs the i686 MinGW cross compiler, `i686-w64-mingw32-objdump`, `xorriso`, and `file`. If something is missing, `sh tools/check-deps` names it and the Debian package. `sh tools/check-deps host` checks the native test only.

On Debian or Ubuntu:

```
sudo apt install build-essential file gcc-mingw-w64-i686 binutils-mingw-w64-i686 xorriso
```

## Run

Mount `build/gfall.iso` and run `D:\GFALL.EXE`. WASD moves, the mouse looks, Q is up, E is down, Escape quits. `build/spintri.iso` is the Glide text demo: `D:\SPINTRI.EXE`.

`D:\RENDERFALL.EXE` sits on the same disc and reads the same `ARENA2` tree. It lists every record in `ARCH3D.BSA`. Enter (or a double-click) loads that one model. Faces use the texture archive and record stored on the face. A face whose texture did not load is flat magenta.

| Where | Key | Action |
| --- | --- | --- |
| List | Up / Down, Home / End | Move through the model ids. |
| List | Enter, or double-click | Open the selected model. |
| List | Esc | Quit. |
| View | Left drag, arrow keys | Pan around the centered model. Releasing them leaves it where it is. |
| View | Right drag, W / S, PgUp / PgDn | Zoom. |
| View | R | Center the camera again. |
| View | F1 | Show or hide the instruction message. |
| View | Esc | Close Glide and return to the list. |

The list line is the record id and its size in bytes. `renderfall.log` is written next to the exe.

Glide still has to come from the 3dfx driver already installed on the machine. If `glide2x.dll` is missing, the list still opens and the log names the failure when you press Enter; the desktop is restored. A black view with a good log line means the mesh loaded and the camera is on it: press R, or pick another id. Magenta faces are missing `TEXTURE.###` records, not a skipped polygon. Do not copy `glide2x.dll` onto either disc. The system driver is the one that draws the 3dfx splash.
