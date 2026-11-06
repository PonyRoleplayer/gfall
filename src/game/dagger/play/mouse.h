#ifndef DF_MOUSE_H
#define DF_MOUSE_H

#include "engine/core/df.h"

/*
 * Cursor or View, classic's mouse option on the controls screen. Stored
 * as one MOUS row in DATA\KEYS.DAT beside the key bindings (keys.c writes
 * and reads it with them). Read docs/game.md (Menus).
 *
 * Cursor keeps the pointer: the mouse does not turn the view. View is
 * mouse look. A menu or a screen shows the pointer either way; play uses
 * the saved mode. No row means Cursor, classic's own.
 *
 * Plain C. It does not bind a key and is built for the host.
 */

/* Cursor, as at the start and after Default. */
void mouse_defaults(void);
/* 1 cursor, 0 view. */
int mouse_cursor(void);
void mouse_set_cursor(int on);
/*
 * 1 when the engine's pointer should be up: a menu or a screen is open,
 * or play is in cursor mode. 0 is mouse look.
 */
int mouse_pointer(int ui_up);

#endif
