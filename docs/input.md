# Input

What `input_read` gives a program once a frame: the movement, the look, quit, and a game's actions. Read this before editing `src/engine/platform/input.c` or the use key in `draw.c`.

## The snapshot

`Input` is one frame:

- `move_f`, `move_r`, `move_y`: W minus S, D minus A, Q minus E.
- `look_dx`, `look_dy`: the mouse's pixels since the last read, plus a fixed step a frame for each arrow held.
- `quit`: Escape (unless a game took it, below), or the window's close.
- `held`, `pressed`, `released`: the game's actions, one bit each (`INPUT_BIT(INPUT_ATTACK)`). Held is down now; pressed went down this frame, released went up. A game reads edges from these, once a frame, and never polls a key itself.

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

`input_bind(action, vk)` sets an action's key: a Windows virtual key (`VK_SHIFT`, `VK_LBUTTON`, a letter's capital). E is also `move_y`'s down for a flying viewer; a game on foot does not read `move_y`.

`input_set_escape_quits(1)`, the default, is the viewers': Escape quits. With 0, Escape never quits: it is `INPUT_MENU`, and the game opens its menu and decides itself when to quit. Either way, an Escape that just closed the console is not counted (`console_take_escape`).

## The use key

The engine's own use key, polled by `draw_frame` (`use_edge`: the left button or E), is the viewers'. A game whose left button attacks turns it off with `gfx_set_use_key(0)` and, on its `INPUT_USE` edge, calls `interact_request_use()`: the next frame uses what the middle of the view rests on, as the engine's key would (`docs/interact.md`). With the key on, a request is taken as well.

## The game's pause

`gfx_set_pause(1)`, for a game's screens: the world stands still as it does under the console (no clock, weather, flames' frames, walker, doors or use; `gfx_paused` says so), but the frame is still drawn under the screen and the keys still reach `input_read`. See `docs/console.md`.
