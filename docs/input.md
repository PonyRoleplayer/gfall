# Input

What `input_read` gives a program once a frame: the movement, the look, quit, and a game's actions. Read this before editing `src/engine/platform/input.c` or the use key in `draw.c`.

## The snapshot

`Input` is one frame:

- `move_f`, `move_r`, `move_y`: forward minus back and right minus left, from those actions' keys (W, S, D, A unless rebound), and Q minus E, fixed (a flying viewer's).
- `look_dx`, `look_dy`: the mouse's pixels since the last read, plus a fixed step a frame for each arrow held.
- `mouse_held`, `mouse_pressed`, `mouse_released`: the physical left and right buttons, by `INPUT_MOUSE_LEFT` and `INPUT_MOUSE_RIGHT`, independent of action bindings and Windows' swapped primary button.
- `mouse_dx`, `mouse_dy`: physical mouse movement since the last read, in logical 640 x 480 pixels, right/down positive. No arrow steps; available in pointer mode too.
- `reset`: a discontinuity since the preceding read, consumed once; cancel an existing gesture before interpreting any press/release bits (see below).
- `quit`: Escape (unless a game took it, below), or the window's close.
- `held`, `pressed`, `released`: the game's actions, one bit each (`INPUT_BIT(INPUT_ATTACK)`). Held is down now; pressed went down this frame, released went up. A game reads edges from these, once a frame, and never polls a key itself. The window remembers each key and button going down and up (`input_note_key`, from `WM_KEYDOWN` / `WM_KEYUP` and the mouse-button messages). A click that goes down and up between two reads is still one press and one release that read: the poll alone used to miss it, so a short click did nothing. A key already held does not press again (a repeat is not a new press). Escape is that same edge. With `input_set_escape_quits(0)` it is `INPUT_MENU` once per press. The engine does not start a game from it. The same notes feed `mouse_pressed` and `mouse_released`, so a short physical click is one press too.
- `wheel`: the mouse wheel's whole notches since the last read, above 0 rolled away from the player. The window adds Windows' units to it (`WM_MOUSEWHEEL`, 120 a notch, `input_on_wheel`), and part of a notch from a finer wheel waits for the rest. With the console open it is 0, and what was rolled is dropped. A viewer with its own use for the wheel (LEVELFALL's field of view, the lists) takes the message from the queue before `win_pump`, so `Input` never sees it. The edge rule is `input_fold_edges`, and hostcheck holds it. The normal hostcheck does not build `input.c`; the focused input harness below compiles its real implementation against a small Win32 stand-in.

With the console open nothing reaches the program: no movement, no look, nothing held, and what was held is released that frame, so a held attack lets go.

## The actions

| Action | Default | For |
| --- | --- | --- |
| `INPUT_ATTACK` | left button | a swing: held to draw back, released to strike |
| `INPUT_USE` | E | using what the view rests on (`interact_request_use`) |
| `INPUT_RUN` | Shift | the game scales the walk it gives the walker |
| `INPUT_JUMP` | Space | `walker_jump` |
| `INPUT_CHARACTER` | C | the character screen |
| `INPUT_INVENTORY` | I | the inventory |
| `INPUT_MAP` | M | the map |
| `INPUT_REST` | R | rest |
| `INPUT_MENU` | Escape | the game's menu, with `input_set_escape_quits(0)` |
| `INPUT_FORWARD` | W | the walk, `move_f` |
| `INPUT_BACK` | S | the walk, `move_f` |
| `INPUT_LEFT` | A | the strafe, `move_r` |
| `INPUT_RIGHT` | D | the strafe, `move_r` |
| `INPUT_SWAP_HANDS` | H | swapping what is in the hands (Daggerfall Unity's key); the engine only reports it |
| `INPUT_CURSOR` | Tab | the game's request to toggle a free HUD pointer; the engine only reports it |
| `INPUT_SWING` | right button | a rebound gesture, separate from legacy `INPUT_ATTACK` |
| `INPUT_TURN_LEFT` / `INPUT_TURN_RIGHT` | Left / Right | keyboard turning, interpreted by the game |
| `INPUT_LOOK_UP` / `INPUT_LOOK_DOWN` | Page Up / Page Down | keyboard pitch, interpreted by the game |
| `INPUT_LOOK_CENTER` | Home | a request to center the view |
| `INPUT_FREELOOK` | Alt | independent look while held, interpreted by the game |
| `INPUT_TRAVEL_MAP` | V | the travel map (Daggerfall Unity's key), apart from `INPUT_MAP`, which a game may give to its automap; the engine only reports it |

`input_bind(action, vk)` sets an action's key: a Windows virtual key (`VK_SHIFT`, `VK_LBUTTON`, a letter's capital). It refuses, returning 0, Escape on any action but `INPUT_MENU` and the console's key (the one left of 1, by its scan code on this layout): those are the menu's and the console's. `input_binding(action)` reads a key back. The actions are numbered in a fixed order and new ones go at the end, because a game saves its bindings by number (the test holds `INPUT_FORWARD` at 9, `INPUT_SWAP_HANDS` at 13 and `INPUT_CURSOR` at 14). A bindings file older than an action has no row for it, so that action keeps the engine's default key.

### A bind screen

`input_key_pressed()` returns a key or mouse button that went down since its last call, the lowest virtual key if several did, or 0. A screen that says "press a key" calls it every frame while it waits.
- **First frame:** its first call, or one after a frame it was not called, only notes what is already down. So the click that opened the screen is not taken as the key.
- **A short press:** one the poll missed is still returned, from the same notes as `pressed`, once. `input_fold_edges` is that rule, and hostcheck holds it (a finished click, a held key, a repeat, a second press).
- **What it skips:** the console's key, the left and right copies of Shift, Ctrl and Alt (Windows 98 reports the plain ones), and everything while the console is open. Escape comes back like any key, for the screen to treat as cancel.
- **Cost:** it reads 254 keys a call, only while a screen waits.

`input_key_name(vk, dst, cap)` names a key for display: "W", "Space", "Left Button", "Shift", "F12", "Num 7", or "Key 233" for one with no name. It is our own table (`keyname.c`), the same on any layout and read by hostcheck. Punctuation is named by a US board's marks. E is also `move_y`'s down for a flying viewer; a game on foot does not read `move_y`.

`input_set_escape_quits(1)`, the default, is the viewers': Escape quits. With 0, Escape never quits: it is `INPUT_MENU`, and the game opens its menu and decides itself when to quit. Either way, an Escape that just closed the console is not counted (`console_take_escape`). Two places see Escape, and both obey the setting: `input_read`, and the window's own key message in `win32.c`, which asks for the quit (`input_escape_quits`). Before that second check, a game's Escape quit the program on its key-down before `input_read` ever saw it as `INPUT_MENU`.

## A game's pointer

`input_set_pointer(1)` is for a game's menus or free HUD pointer. Mouse look normally recentres the cursor after every move. In pointer mode the cursor stays where the mouse puts it, still hidden and held to the window, and nothing is looked: `look_dx` and `look_dy` read 0 for the mouse and the arrows alike. `input_pointer(&x, &y)` gives where it is in screen pixels, in `ui_string`'s frame: from the lower left, 0..639 and 0..479, scaled from the window's client size. The game draws its own arrow there (Daggerfall's is a picture, `docs/pictures.md`).

`INPUT_CURSOR` reports a held/pressed/released bit, as any action does; it never changes pointer mode itself. A game may take its pressed edge during play and call `input_set_pointer(!input_pointer_on())`. Pointer mode does not pause: `gfx_set_pause` remains separate, so a free HUD pointer can leave the world running while a menu can stand it still. The game owns which screens accept the toggle and how a pointer click is used. Movement actions and the wheel still reach it in pointer mode; only look is suppressed.

A HUD or menu takes `mouse_pressed & INPUT_MOUSE_LEFT`: rebinding an attack or swing does not change its physical click. The action bits remain available for game actions, including a keyboard-bound swing. Escape is unchanged: with `input_set_escape_quits(0)` it is `INPUT_MENU`, and closes a menu.

`input_set_pointer(0)` goes back to mouse look. Both ways start from the window's middle, so look starts with no jump. A game that used a menu pointer chooses the mode to resume when play starts.

`input_begin` captures the mouse and clips it to the window, and the cursor stays blank while that capture is held (`input_grabs_cursor`): `WM_SETCURSOR` draws none, so the system arrow does not sit over the client. A button-up does not release that capture. It used to, on every left or right button-up, which dropped the capture `input_begin` had taken and let the cursor leave the window: the system arrow showed around it, and a click out there never arrived. The drag hook (`win_mouse_hook`, a viewer's orbit) still captures for its own button and releases on the way up, and the button-up is delivered to the hook before that release so the synthesized move is not a drag.

## The use key

The engine's own use key, polled by `draw_frame` (`use_edge`: the left button or E), is the viewers'. A game whose left button attacks turns it off with `gfx_set_use_key(0)` and, on its `INPUT_USE` edge, calls `interact_request_use()`: the next frame uses what the middle of the view rests on, as the engine's key would (`docs/interact.md`). With the key on, a request is taken as well.

## The game's pause

`gfx_set_pause(1)`, for a game's screens: the world stands still as it does under the console (no clock, weather, flames' frames, walker, doors or use; `gfx_paused` says so), but the frame is still drawn under the screen and the keys still reach `input_read`. See `docs/console.md`.

## Physical mouse motion and transitions

Raw motion is independent of the view. In pointer mode `WM_MOUSEMOVE` differences are accumulated between client positions without recentering; in mouse-look mode they are accumulated from the client center before the normal warp. `input_read` scales the sum once per axis to 640 x 480, retaining the signed fractional remainder. A 1280 x 960 client therefore reports two physical pixels as one logical pixel, including a slow drag spread across several frames. `mouse_dy` is positive down, unlike the pointer's bottom-left Y coordinate. Motion carries no button meaning: the game decides which held action makes it a swing or independent look. All the new actions remain held/pressed/released in pointer mode, even though legacy `look_dx/dy` still read zero there. The engine applies no rotation or gesture threshold.

Each distinct virtual key is polled at most once inside `input_read`. A 256-byte lazy cache, cleared once per snapshot, shares the result among movement, physical buttons and bound actions; a right button bound to both swing and attack cannot disagree within a snapshot. No heap allocation, floating-point conversion or per-message scale division is needed. The bind screen's separate all-key scan is unchanged. New fields are appended to `Input`; a caller zeroing `sizeof(Input)` initializes them, and every path through `input_read` writes them.

`input_mouse_reset` drops pending raw movement, its fractional remainder and the previous pointer position. The next pointer message establishes a new baseline rather than spanning a discontinuity. Pointer toggles and `input_begin` instead know the center they just warped to and establish it directly. Own warp notifications are ignored without discarding real movement already accumulated. `input_look` keeps its old mouse-look-only contract and never consumes a game's raw motion.

`Input.reset` is 1 on the next `input_read` after a boundary: `input_begin`/`input_end`, an explicit `input_mouse_reset`, a focus/capture change, a pointer-mode toggle, or entering/leaving console/focus suppression. Several boundaries before a read coalesce into one flag. Every read, including a suppressed one, writes and consumes the pending flag; a later read is 0 unless another boundary occurred. Merely remaining under the console or unfocused creates no further resets. The first read after `input_begin` is always marked; own cursor warps and setting an unchanged pointer mode are not boundaries. Viewer `input_look` does not consume this signal.

The game checks `reset` **before** treating any press or release as an action. Releases are still reported exactly as before: a focus loss releases a held RMB, but this is a cancellation, not a deliberate bow shot or click-release swing. The game blocks its controller and cancels its weapon/gesture state when marked, deciding itself when new input may start. This also cancels a gesture on capture loss while the physical button remains held, even when there is no release edge at all. The engine neither invokes game functions nor hides or fabricates button edges for this purpose. A zero-initialized `Input` has reset 0.

The window calls `input_on_focus` on focus gain/loss, and `input_mouse_reset` on capture loss/cancellation and console-consumed keys. A console opened and shut between reads thus cannot leave an old gesture waiting. While unfocused or under the console, snapshots report no held actions/buttons or motion, release the previous held bits once, and discard the wheel. Focus changes also clear pending look. Reacquiring focus while a physical button is held starts a fresh pressed edge on the next snapshot; the game must ignore input while its own screen is inactive as usual.

Between `input_begin` and `input_end`, the window retains session capture across ordinary button-up. Releasing that capture on each button-up would flush the final motion of a short gesture before the snapshot saw it. An actual capture transfer still flushes, and GUI viewers without an input session retain their existing per-drag capture and release. No new action changes pointer mode, pause, camera position or rotation itself.

### Focused input regression harness

Run independently of the Makefile (no Windows or Glide required):

```
gcc -std=gnu90 -Wall -Wextra -Werror -DDF_HOST -Itests/input_win32 -Isrc tests/check_input.c src/engine/platform/input.c src/engine/platform/keyname.c -o /tmp/gfall-check-input
/tmp/gfall-check-input
```

`tests/input_win32/windows.h` supplies only the Win32 declarations this module uses. The harness feeds physical key states and client moves into the actual `input.c`, including synchronous cursor-warp notifications. It holds action ids/defaults, one poll per key, physical clicks independent of bindings, pointer-mode action edges, scaled motion and fractional carry, legacy look, console releases, focus/capture resets, and begin/end/toggle boundaries. `tests/check_engine.c` also holds the saved action ids in normal hostcheck. The real Win32 message dispatch and hardware remain covered by cross-builds and target testing.
