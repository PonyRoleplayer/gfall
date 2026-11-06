#ifndef DF_KEYS_H
#define DF_KEYS_H

/*
 * The player's settings, kept in gfall.cfg beside the exe as text, key=value
 * a line (config.h): every binding, the pointer scheme, the screen's mode,
 * the graphics, the sound and the play options. Read at the start and written by
 * every Apply of the Settings screens. A missing file is written at the first
 * start from the defaults; no older format is read. Read docs/game.md (Menus,
 * The settings file).
 *
 * Escape stays the menu and is not bound here; the console's key is
 * the engine's.
 */

/* Default bindings (WASD, E, right Cursor swing, left View attack, turn/look)
 * and Cursor scheme. Existing action ids remain append-only. */
void keys_defaults(void);
/* gfall.cfg over the defaults (and a gfall.cfg written when there is none): 1 when a file was read. */
int keys_load(void);
/*
 * The screen's mode gfall.cfg holds, for the program to pass to gfx_set_mode before the card opens (it
 * is read again with the rest by keys_load once it has): 1 when the file holds one. Nothing else is applied.
 */
int keys_peek_mode(int *w, int *h);
/* Every setting as it is now into gfall.cfg: 1 when written. */
int keys_save(void);

#endif
