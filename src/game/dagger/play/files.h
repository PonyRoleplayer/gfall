#ifndef DF_FILES_H
#define DF_FILES_H

#include "game/dagger/play/saves.h"

/*
 * The saved games' screen: the body of the settings shell's first tab, built
 * of ui.h's widgets. Read docs/game.md (Saves, The list).
 *
 * On the right a list: each character a row, and under them their saves,
 * newest first, each with the real time it was saved. On the left the
 * snapshot of the save selected, and where and when it was left off.
 * Underneath, LOAD (also a double click on a save), SAVE (only from the
 * game: a new save of the character playing) and DELETE (a save, or a
 * character with every save they have: the button asks once more, as the
 * shell's Exit Game does). The wheel and the arrows scroll; Up and Down move
 * the selection, Enter loads, Delete deletes.
 *
 * Plain C: it draws through ui.h, so hostcheck drives it by clicks and keys
 * and rasterizes it. It reads and deletes through saves.h. The snapshot's
 * pixels are kept here (files_thumb) for menu.c to put on the card.
 */

/* What a click or a key asks of the menu. */
enum {
    FILES_NONE,
    /* files_chosen is the path of the save to load. */
    FILES_LOAD,
    /* A new save of the character playing. */
    FILES_SAVE
};

/* The ids its controls answer to in the shell's regions. A row is FILES_ID_ROW and its index in the list. */
#define FILES_ID_LOAD 1990
#define FILES_ID_SAVE 1991
#define FILES_ID_DELETE 1992
#define FILES_ID_UP 1993
#define FILES_ID_DOWN 1994
#define FILES_ID_ROW 2000

/*
 * Reads the saves and builds the list. can_save: the player is in a game and may save. current is the id of
 * the character playing (0 for none): theirs is selected first, else the first character's newest save. select
 * is the path of a save to select, or 0.
 */
void files_open(int can_save, df_u32 current, const char *select);
/* Draws the body (in the area the shell leaves for it) and makes its regions; hover is the id under the pointer. */
void files_draw(int hover);
/* A click on a control or row of this screen's ids at time now (a cycle count, per_ms of them a millisecond, 0 if not known). */
int files_click(int id, df_u32 now, df_u32 per_ms);
/* A key (a virtual key): Up, Down, Enter, Delete. */
int files_key(int vk);
/* The wheel's notches (above 0 away from the player). */
void files_wheel(int notches);
/* The path of the save chosen after FILES_LOAD. */
const char *files_chosen(void);
/* A line to show in the screen's own note, or 0. */
const char *files_note(void);
void files_say(const char *line);

/*
 * The snapshot of the save selected: SAVE_THUMB_W x SAVE_THUMB_H in RGB 565, 0 when it has none. files_thumb_serial
 * changes whenever it does, so menu.c sends a new picture only then. Its rectangle on the shell's 640 x 480.
 */
const df_u16 *files_thumb(void);
df_u32 files_thumb_serial(void);
void files_thumb_rect(int *x, int *y, int *w, int *h);

/* For the tests: the list's rows, which is selected, and whether DELETE has been asked once. */
int files_rows(void);
int files_selected_row(void);
int files_row_is_character(int row);
int files_confirming(void);

#endif
