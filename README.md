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
| `make levelfall-check` | `build/LEVELFALL.EXE`, the dungeon viewer. |
| `make` | `build/spintri.iso` and `build/gfall.iso`. The gfall disc also carries `RENDERFALL.EXE` and `LEVELFALL.EXE`. |
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
| View | G | Show or hide the scale grid. |
| View | F | Turn distance fog on or off. |
| View | F1 | Show or hide the instruction message. |
| View | Esc | Close Glide and return to the list. |

The list line is the record id and its size in bytes. `renderfall.log` is written next to the exe. So is `renderfall.cfg`. A missing file is written with the defaults, one `key=value` per line. `background=128,128,128` tints the clear (and the fog). `grid=1` draws the scale planes, `grid=0` hides them. `fog=1` uses the distance table, `fog=0` turns it off. A line that is absent leaves that setting at its default (fog on). F changes fog until the program quits; the file is read only at startup.

`D:\LEVELFALL.EXE` indexes every indoor and outdoor place at startup (a `MAPDITEM` dungeon, or a `MAPPITEM` location of `RMB` blocks). Type in the find box; a full name selects that place. The Show box limits the list to indoors or outdoors. Enter or a double-click loads the selection. Indoors start on editor marker 10 when the entrance block has one, otherwise on the entrance block's first marker, otherwise at the center of the bounds. Outdoors start above the roofs, looking down.

| Where | Key | Action |
| --- | --- | --- |
| List | Type | Filter by name. An exact name is selected. |
| List | Up / Down | Move the selection. From the find box too. |
| List | Show | All, indoors, or outdoors. |
| List | Enter, or double-click | Open the selected place. |
| List | Esc | Clear the find box. Quit when it is already empty. |
| List | Title-bar cross | Quit. |
| View | Mouse | Look. |
| View | W / S | Fly along the aim, including up and down when the camera is pitched. |
| View | A / D | Strafe. |
| View | Arrow keys | Move up, down, left, and right. Left and right strafe. Up and down are world vertical, independent of pitch. |
| View | Shift | Fly faster. |
| View | F1 | Show or hide the instruction message. |
| View | Esc | Close Glide and return to the list. |

The columns are kind, the region file number (`017` is the file that holds Privateer's Hold and the city of Daggerfall), the name, and the block count. Click a column header to sort. `levelfall.log` is written next to the exe. So is `levelfall.cfg`, the first time the program runs, if that folder can be written. The disc does not carry a cfg. The written defaults are `background=128,128,128`, `fog=1`, and `far=8192`. `far` is how far geometry is drawn, in world units. The engine otherwise stops at 1400, and will not go past 16384. `fog 0` turns distance fog off for the next launch. A large town loads. Faces near the camera use the real texture. Faces farther than about 8192 units, or past what the 2MB texture card can hold, stay a 16 by 16 stand-in until you fly closer. Format notes are in `docs/dungeons.md`, `docs/viewers.md`, and `docs/memory.md`.

Glide still has to come from the 3dfx driver already installed on the machine. If `glide2x.dll` is missing, the list still opens and the log names the failure when you press Enter; the desktop is restored. A black view with a good log line means the mesh loaded and the camera is on it: press R, or pick another id. Magenta faces are missing `TEXTURE.###` records, not a skipped polygon. Do not copy `glide2x.dll` onto either disc. The system driver is the one that draws the 3dfx splash.
