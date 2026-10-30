#ifndef DF_BAGVIEW_H
#define DF_BAGVIEW_H

#include "engine/platform/input.h"
#include "game/dagger/play/bag.h"

/*
 * The inventory screen on the card: bag.c composes it in memory, and this
 * puts the result up as one picture, gives it the pointer and the clicks,
 * and takes it down. Read docs/game.md (Loot and the inventory).
 *
 * The picture goes back on the card only when bag.c says something
 * changed, so a frame of the open screen costs its quads and no download.
 */

/* Opens it for the player, with a body's things beside theirs when body is not 0. */
void bagview_open(Player *p, const Entity *body);
void bagview_close(void);
int bagview_is_open(void);
/* One frame's pointer, clicks and keys. 1 when the screen asks to close. */
int bagview_frame(const Input *in);
/* The screen and the pointer, from the game's gfx_set_extra. */
void bagview_draw(void);

#endif
