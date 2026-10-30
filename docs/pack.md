# The pack: a generic inventory

`src/engine/scene/pack.[ch]` is what someone or something carries: a player's bag, a chest, a body. `src/engine/ui/packview.[ch]` is a list screen over one pack or two side by side. Both belong to the engine and know no game: no item names, no rules, no pictures. Both are plain C, and hostcheck runs them (`check_pack`).

No game calls them yet. GFALL's own `play/inventory.c` is separate and unchanged.

## The catalogue

A game gives each pack a table of `PackItem`, from its own data, and keeps that table alive:

| Field | Means |
| --- | --- |
| `id` | the item's number |
| `name` | what the list screen shows |
| `weight` | each, in the game's unit |
| `stack_max` | the most in one stack, at least 1 |
| `slot` | where it is worn, `0..PACK_SLOTS-1`, or `PACK_NO_SLOT` |
| `stat[PACK_STATS]` | what wearing it adds; the game says what each means |

## The pack

- **Stacks:** `pack_add` fills the item's stacks of the same tag, then starts new ones, up to `PACK_MAX`. It returns how many fit. A tag is the game's own number for one thing's state, such as its condition; stacks with different tags stay apart.
- **Weight:** `max_weight` limits what `pack_add` and the moves let in; 0 means no limit.
- **Wearing:** `pack_equip` puts a stack in its item's slot, and whatever was there comes off but stays in the pack. From a stack of several, one is split off to wear. A worn stack is always one thing, and nothing merges into it. `pack_remove` keeps the slots pointing at the right stacks as stacks go.
- **What it adds up to:** `pack_stat(p, k)` sums stat `k` over everything worn. A game's own rules read it: that is how equipment changes its combat.
- **Moving:** `pack_move` moves up to a count between two packs under the same catalogue, as far as the other's limits allow. `pack_move_all` moves everything it can, money too: looting a body or emptying a chest.
- **Money:** `Pack.money`; `pack_pay` takes nothing when there is not enough.
- **Saving:** a `Pack` holds no pointer but its catalogue, so it can be written as bytes. `pack_bind` gives it the catalogue again after loading. The catalogue's ids must still mean the same things.

## The list screen

`PackView` holds a cursor and a scroll position for each side. It holds no keys and no font:

- The game turns its input into `PACKVIEW_UP`, `DOWN`, `SWITCH`, `USE` (wear or take off), `MOVE` (one across) and `MOVE_ALL`, and calls `packview_command`.
- The game passes its text call to `packview_draw`, for example `ui_string`. Sides take half the width each, the heading first, then a line for each thing shown; the screen's y counts up, so each line is drawn lower than the last.
- A line reads `>*Sword x1  3.0`: the cursor, worn, the name, the count, the stack's weight.

A list that grows shorter scrolls back, so no rows stand empty while things sit above them.

## Not here

Pictures, dragging with the mouse, sorting, and anything a game decides for itself: what stats mean, what a body holds, prices, condition wearing down, or what happens when something is used.
