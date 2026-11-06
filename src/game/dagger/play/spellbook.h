#ifndef DF_SPELLBOOK_H
#define DF_SPELLBOOK_H

#include "game/dagger/play/spells.h"

/*
 * The spellbook screen, classic's SPBK00I0.IMG (DFU's DaggerfallSpellBookWindow): the
 * spells in a list on the left, the one chosen on the right with its icon, its target
 * and element and each effect's parameters, what it costs the player in spell points,
 * and the buttons Delete, Up, Sort and Down under the list. Read docs/game.md (Magic).
 *
 * A click on a spell chooses it; a click on the chosen one, or a double click, or Enter,
 * readies it (the screen closes and the game makes it ready, cast.h); the arrows
 * and the wheel or Page Up and Down scroll; Up and Down move the chosen spell in the book
 * (those two buttons), Sort puts the book in order by name, and Delete takes the chosen
 * spell out (asked first by a second click, as the shell's Exit Game is). Exit
 * or Escape closes. Plain C over canvas.h: hostcheck drives it by clicks and writes
 * a picture.
 *
 * The book it works on is the game's (spells.h), changed in place; the character is for costs.
 */

enum {
    SPELLBOOK_STAY,
    SPELLBOOK_LEAVE,
    /* Leave with a spell chosen to ready: spellbook_chosen. */
    SPELLBOOK_READY
};

/* The screen's own rectangles on classic's 320 x 200, for the screen and the test. */
#define SPELLBOOK_ROWS 13
#define SPELLBOOK_ROW_H 10

/* 0 when a picture is missing. The book and the character are kept by pointer until it closes. */
int spellbook_open(Spellbook *book, const Character *pc);
void spellbook_close(void);
void spellbook_compose(void);
const unsigned char *spellbook_pixels(void);
int spellbook_changed(void);
/* A click at classic's x, y; a key (a virtual key); the wheel's notches (above 0 away from the player). */
int spellbook_click(int x, int y);
int spellbook_key(int vk);
void spellbook_wheel(int notches);
/* The spell to ready after SPELLBOOK_READY: its index in the book. */
int spellbook_chosen(void);

/* For the tests: the one chosen (-1 for none), the first row shown, and whether Delete has been asked once. */
int spellbook_selected(void);
int spellbook_scroll(void);
int spellbook_confirming(void);

#endif
