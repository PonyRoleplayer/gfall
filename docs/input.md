# Input

What `input_read` gives a program once a frame: the movement, the look, quit, and a game's actions. Read this before editing `src/engine/platform/input.c` or the use key in `draw.c`.

## The snapshot

`Input` is one frame:

- `move_f`, `move_r`, `move_y`: forward minus back and right minus left, from those actions' keys (W, S, D, A unless rebound), and Q minus E, fixed (a flying viewer's).
- `look_dx`, `look_dy`: the mouse's pixels since the last read, plus a fixed step a frame for each arrow held.
- `quit`: Escape (unless a game took it, below), or the window's close.
- `held`, `pressed`, `released`: the game's actions, one bit each (`INPUT_BIT(INPUT_ATTACK)`). Held is down now; pressed went down this frame, released went up. A game reads edges from these, once a frame, and never polls a key itself.
- `wheel`: the mouse wheel's whole notches since the last read, above 0 rolled away from the player. The window adds Windows' units to it (`WM_MOUSEWHEEL`, 120 a notch, `input_on_wheel`), and part of a notch from a finer wheel waits for the rest. With the console open it is 0, and what was rolled is dropped. A viewer with its own use for the wheel (LEVELFALL's field of view, the lists) takes the message from the queue before `win_pump`, so `Input` never sees it. The Input path has no host test, because `input.c` is not built for the host.

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

`input_bind(action, vk)` sets an action's key: a Windows virtual key (`VK_SHIFT`, `VK_LBUTTON`, a letter's capital). It refuses, returning 0, Escape on any action but `INPUT_MENU` and the console's key (the one left of 1, by its scan code on this layout): those are the menu's and the console's. `input_binding(action)` reads a key back. The actions are numbered in a fixed order and new ones go at the end, because a game saves its bindings by number (the test holds `INPUT_FORWARD` at 9 and `INPUT_SWAP_HANDS` at 13). A bindings file older than an action has no row for it, so that action keeps the engine's default key.

### A bind screen

`input_key_pressed()` returns a key or mouse button that went down since its last call, the lowest virtual key if several did, or 0. A screen that says "press a key" calls it every frame while it waits.
- **First frame:** its first call, or one after a frame it was not called, only notes what is already down. So the click that opened the screen is not taken as the key.
- **What it skips:** the console's key, the left and right copies of Shift, Ctrl and Alt (Windows 98 reports the plain ones), and everything while the console is open. Escape comes back like any key, for the screen to treat as cancel.
- **Cost:** it reads 254 keys a call, only while a screen waits.

`input_key_name(vk, dst, cap)` names a key for display: "W", "Space", "Left Button", "Shift", "F12", "Num 7", or "Key 233" for one with no name. It is our own table (`keyname.c`), the same on any layout and read by hostcheck. Punctuation is named by a US board's marks. E is also `move_y`'s down for a flying viewer; a game on foot does not read `move_y`.

`input_set_escape_quits(1)`, the default, is the viewers': Escape quits. With 0, Escape never quits: it is `INPUT_MENU`, and the game opens its menu and decides itself when to quit. Either way, an Escape that just closed the console is not counted (`console_take_escape`). Two places see Escape, and both obey the setting: `input_read`, and the window's own key message in `win32.c`, which asks for the quit (`input_escape_quits`). Before that second check, a game's Escape quit the program on its key-down before `input_read` ever saw it as `INPUT_MENU`.

## A menu's pointer

`input_set_pointer(1)` is for a game's menus. Mouse look normally recentres the cursor after every move. In pointer mode the cursor stays where the mouse puts it, still hidden and held to the window, and nothing is looked: `look_dx` and `look_dy` read 0 for the mouse and the arrows alike. `input_pointer(&x, &y)` gives where it is in screen pixels, in `ui_string`'s frame: from the lower left, 0..639 and 0..479, scaled from the window's client size. The game draws its own arrow there (Daggerfall's is a picture, `docs/pictures.md`).

A click is an action's press: `INPUT_ATTACK`'s, the left button, unless rebound. Escape is unchanged: with `input_set_escape_quits(0)` it is `INPUT_MENU`, and closes a menu.

`input_set_pointer(0)` goes back to mouse look. Both ways start from the window's middle, so look starts with no jump. A game turns it off when play starts.

## The use key

The engine's own use key, polled by `draw_frame` (`use_edge`: the left button or E), is the viewers'. A game whose left button attacks turns it off with `gfx_set_use_key(0)` and, on its `INPUT_USE` edge, calls `interact_request_use()`: the next frame uses what the middle of the view rests on, as the engine's key would (`docs/interact.md`). With the key on, a request is taken as well.

## The game's pause

`gfx_set_pause(1)`, for a game's screens: the world stands still as it does under the console (no clock, weather, flames' frames, walker, doors or use; `gfx_paused` says so), but the frame is still drawn under the screen and the keys still reach `input_read`. See `docs/console.md`.
