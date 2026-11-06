#ifndef DF_MENU_H
#define DF_MENU_H

#include "engine/platform/input.h"
#include "engine/core/df.h"

/*
 * Title/start/death retain classic's pictures. Escape opens settings.c's
 * native 640x480 stone shell: Load/Save, Keybindings, Graphics, Audio, and Gameplay.
 * Text is runtime glyphs, never baked into panel art. Read docs/game.md
 * (Menus) for drafts, Apply, persistence and the card budget.
 *
 * MENU_OPTIONS hosts Load/Save and Graphics; MENU_CONTROLS hosts bindings.
 * The game owns pausing and pointer mode. All menu clicks use physical
 * mouse edges, independent of attack bindings. ARROW.RAW remains the pointer.
 */

enum {
    MENU_NONE,
    MENU_TITLE,
    MENU_MAIN,
    MENU_OPTIONS,
    MENU_CONTROLS,
    MENU_DEATH
};

/* What a menu frame asks of the game. */
enum {
    MENU_STAY,
    MENU_START,
    MENU_QUIT,
    MENU_CLOSED,
    /* The game's to do; the menu stays open until the game says otherwise. */
    MENU_SAVE,
    MENU_LOAD,
    /* The death screen was dismissed. */
    MENU_DEATH_OVER
};

/* The pointer's picture, before the first gfx_prepare. */
int menu_setup(void);
void menu_shutdown(void);
/* Opens a menu, closing the one open. */
void menu_open(int which);
/* A line under the open menu, until the next click: "Game saved." (on the saved games' tab, in its own note). */
void menu_say(const char *line);
/*
 * Before opening the shell: whether the player is in a game (the saved games' screen then has a SAVE button) and
 * the id of the character playing, whose saves are selected first.
 */
void menu_files(int can_save, df_u32 current);
/* After MENU_LOAD: the path of the save chosen. */
const char *menu_chosen_path(void);
/* After the game saved from MENU_SAVE: the path it went to (0 if it failed); the list shows it. */
void menu_files_saved(const char *path);
int menu_current(void);
/* One frame of the open menu's input: MENU_*. */
int menu_frame(const Input *in);
/* The open menu and the pointer, from the game's gfx_set_extra. */
void menu_draw(void);
/* Classic's pointer alone at classic's x, y, for a screen that is not a menu (screens.c). */
void menu_draw_pointer(int x, int y);
/* Classic world arrow/X, centered on its hotspot; invalid shape uses ARROW.RAW. */
void menu_draw_world_pointer(int shape, int x, int y);

#endif
