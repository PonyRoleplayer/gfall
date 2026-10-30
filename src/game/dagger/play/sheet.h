#ifndef DF_SHEET_H
#define DF_SHEET_H

#include "game/dagger/play/player.h"

/*
 * Classic's character sheet (INFO00I0.IMG), in memory: who the player is,
 * their attributes, their skills, and the paper doll. Read docs/game.md
 * (Levelling) before editing.
 *
 * When a new level is due it opens at once and is where the level is taken:
 * the level number, the health, and 4 to 6 points to place on the
 * attributes (DFU's DaggerfallCharacterSheetWindow). It cannot close until
 * they are placed, unless every attribute is at its most.
 *
 * The buttons are DFU's rectangles. Logbook, History, Spellbook and
 * Affiliations come with what they show. Plain C: hostcheck drives it.
 */

/* What a click asks of the game. */
enum {
    SHEET_STAY,
    SHEET_LEAVE,
    /* Its Inventory button: on to the inventory screen. */
    SHEET_INVENTORY
};

/* Opens over the player; a level due is taken now. 0 when a picture is missing. */
int sheet_open(Player *p);
void sheet_close(void);
void sheet_compose(void);
const df_u8 *sheet_pixels(void);
int sheet_changed(void);
/* A click at classic's x, y: SHEET_*. */
int sheet_click(int x, int y);
/* 1 when it may close: no points left to place. If not, a note says why. */
int sheet_may_leave(void);
const char *sheet_note(void);
/* Points still to place. */
int sheet_points(void);

#endif
