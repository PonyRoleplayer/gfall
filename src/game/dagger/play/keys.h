#ifndef DF_KEYS_H
#define DF_KEYS_H

/*
 * The player's key bindings: saved in DATA\KEYS.DAT beside the exe as a
 * record file (docs/data.md), one KEYS record an action (id INPUT_*, its
 * Windows virtual key), and applied with input_bind at start. A missing
 * or damaged file keeps classic's defaults. Read docs/game.md (Menus).
 *
 * Escape stays the menu and is not bound here; the console's key is
 * the engine's.
 */

#define KEYS_FILE "DATA\\KEYS.DAT"

/* Classic's keys (WASD, E to use, the left button to swing...) on every action. */
void keys_defaults(void);
/* DATA\KEYS.DAT's bindings over the defaults: 1 when it read. */
int keys_load(void);
/* Every action's binding into DATA\KEYS.DAT: 1 when written. */
int keys_save(void);

#endif
