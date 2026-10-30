#ifndef DF_SCREENS_H
#define DF_SCREENS_H

#include "engine/platform/input.h"
#include "engine/scene/entity.h"
#include "game/dagger/play/player.h"

/*
 * The screens composed in memory, on the card: the inventory (bag.c), the
 * character sheet (sheet.c), character creation (create.c) and rest (rest.c). One is
 * open at a time. Each composes one 320 x 200 picture, and this puts it up,
 * gives it the pointer, the clicks and the keys, and takes it down. Read
 * docs/game.md (The inventory screen, What it costs the 233).
 *
 * The picture goes back on the card only when the screen says something
 * changed, so an open frame costs its quads and no download. It takes the
 * place of a menu's picture in GAME_PICTURE_KB, as the controls screen does.
 */

/* What a frame asks of the game. */
enum {
    SCREENS_STAY,
    /* It closed. */
    SCREENS_CLOSED,
    /* Creation finished, the character made (create_result), still open until the game has read it. */
    SCREENS_MADE,
    /* Creation skipped for the default character (create_default). */
    SCREENS_SKIPPED,
    /* A save slot chosen (screens_slot), closed. */
    SCREENS_SLOT
};

/* Opens one, closing any other. Nothing opens when its pictures are missing: is_open says. */
void screens_open_bag(Player *p, Entity *searched);
void screens_open_sheet(Player *p);
void screens_open_create(void);
void screens_open_rest(Player *p);
void screens_open_slots(int saving);
/* The slot chosen and the name typed, after SCREENS_SLOT; whether it was to save. */
int screens_slot(void);
const char *screens_slot_name(void);
int screens_slot_saving(void);
void screens_close(void);
int screens_is_open(void);
/* One frame's pointer, clicks and keys: SCREENS_*. */
int screens_frame(const Input *in, float dt);
/* What a rest just ended said, once, for the game to show now the world is back; 0 for nothing. */
const char *screens_said(void);
/* The screen and the pointer, from the game's gfx_set_extra. */
void screens_draw(void);

#endif
