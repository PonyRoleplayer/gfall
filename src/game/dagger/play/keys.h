#ifndef DF_KEYS_H
#define DF_KEYS_H

/*
 * The player's key bindings: saved in DATA\KEYS.DAT beside the exe as a
 * record file (docs/data.md), one KEYS record an action (id INPUT_*, its
 * Windows virtual key), one MOUS row for Cursor or View (mouse.h),
 * CTLS for that same preference (control.h), and one OPTS row for full
 * screen, head bobbing and FOV (viewopt.h). Applied with input_bind,
 * mouse_read, control_read_settings and viewopt_read at start. A missing or damaged
 * file keeps classic's defaults. Read docs/game.md (Menus).
 *
 * Escape stays the menu and is not bound here; the console's key is
 * the engine's.
 */

#define KEYS_FILE "DATA\\KEYS.DAT"

/* Default bindings (WASD, E, right Cursor swing, left View attack, turn/look)
 * and Cursor scheme. Existing action ids remain append-only. */
void keys_defaults(void);
/* DATA\KEYS.DAT's bindings and scheme over the defaults: 1 when it read. */
int keys_load(void);
/* Every binding and the scheme into DATA\KEYS.DAT: 1 when written. */
int keys_save(void);

#endif
