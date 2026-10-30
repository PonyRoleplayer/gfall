#ifndef DF_MENU_H
#define DF_MENU_H

#include "engine/platform/input.h"

/*
 * Classic's menus, one open at a time, each its own picture where classic
 * puts it and its buttons where DFU's windows put them (StartWindow,
 * PauseOptionsWindow, ControlsWindow). Read docs/game.md (Menus).
 *
 *   MENU_TITLE     TITL00I0.IMG, until a key or a click
 *   MENU_MAIN      PICK03I0.IMG: load a game, start a new one, exit
 *   MENU_OPTIONS   OPTN00I0.IMG over the stilled world: save, load, exit,
 *                  controls, continue
 *   MENU_CONTROLS  CNFG00I0.IMG: each action's key, click one to bind it
 *
 * A menu's picture is on the card only while it is open (pic_load on
 * open, pic_free on close). The pointer is classic's ARROW.RAW. The
 * game turns the engine's pointer mode on while a menu is open.
 */

enum {
    MENU_NONE,
    MENU_TITLE,
    MENU_MAIN,
    MENU_OPTIONS,
    MENU_CONTROLS
};

/* What a menu frame asks of the game. */
enum {
    MENU_STAY,
    MENU_START,
    MENU_QUIT,
    MENU_CLOSED
};

/* The pointer's picture, before the first gfx_prepare. */
int menu_setup(void);
void menu_shutdown(void);
/* Opens a menu, closing the one open. */
void menu_open(int which);
int menu_current(void);
/* One frame of the open menu's input: MENU_*. */
int menu_frame(const Input *in);
/* The open menu and the pointer, from the game's gfx_set_extra. */
void menu_draw(void);
/* Classic's pointer alone at classic's x, y, for a screen that is not a menu (screens.c). */
void menu_draw_pointer(int x, int y);

#endif
