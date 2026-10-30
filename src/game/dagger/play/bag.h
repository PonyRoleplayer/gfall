#ifndef DF_BAG_H
#define DF_BAG_H

#include "engine/scene/entity.h"
#include "game/dagger/play/player.h"

/*
 * Classic's inventory screen, in memory: what it shows and what a click on
 * it does. Read docs/game.md (Loot and the inventory) before editing.
 *
 * Every piece is classic's own picture at classic's place, the rectangles
 * Daggerfall Unity's DaggerfallInventoryWindow gives:
 *   INVE00I0.IMG  the screen itself, its tabs and buttons in grey
 *   INVE01I0.IMG  the same in gold: the chosen tab and button are cut from it
 *   INVE06I0.IMG, INVE07I0.IMG  a list's arrows, green or red
 *   ITEM00I0.IMG  the stone behind the small panel that names things
 *   FONT0003.FNT  the labels
 *
 * The whole screen is composed into one 320 x 200 picture of palette
 * indices, and only when something on it changed. A frame of the open
 * screen sends nothing to the card and draws one picture, however much is
 * in the lists, where drawing each piece would bind and blend dozens of
 * cutouts a frame and hold each on the card. See docs/pictures.md for what
 * a picture costs.
 *
 * The left of the screen is the paper doll (doll.h). The two lists are
 * the player's pack and, on the right, what they are searching (loot.h): a
 * body, a heap of treasure, a pile; or, with nothing searched, what they are
 * dropping, which lands as a pile at their feet when the screen closes
 * (pile.h). An item worn is on the doll, not in the list.
 *
 * Plain C, drawing nothing and reaching no card: hostcheck runs it, and
 * screens.c puts the result on the screen.
 */

/* What a click asks of the game. */
enum {
    BAG_STAY,
    BAG_LEAVE
};

/*
 * 1 when there is anything to find on what is being searched. A heap of
 * treasure with nothing on it goes, there and then.
 */
int bag_finds_things(Entity *searched, int level);
/*
 * Opens the screen over the player's things, and searched's beside them
 * when it is not 0. Its pictures are read into heaps of its own, gone at
 * bag_close. 0 when a picture is missing.
 */
int bag_open(Player *p, Entity *searched);
/* Closes it: what was dropped lands, what was searched keeps what is left. */
void bag_close(void);
/* Composes the screen, whatever bag_changed says; the pixels are bag_pixels'. */
void bag_compose(void);
/* The composed screen: 320 x 200 palette indices under ART_PAL.COL, row 0 at the top. */
const df_u8 *bag_pixels(void);
/* 1 once since the screen last composed if something on it changed. */
int bag_changed(void);
/* The pointer at classic's x, y: the small panel names what it is over. */
void bag_point(int x, int y);
/* A click at classic's x, y: BAG_*. */
int bag_click(int x, int y);
/* A line to show the player, or 0. */
const char *bag_note(void);

#endif
