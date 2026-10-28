#ifndef DF_GAME_H
#define DF_GAME_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"
#include "engine/platform/input.h"

struct World;

/*
 * The game's play state: one frame of it, between the program's input
 * and its gfx_frame. Read docs/game.md (A frame of play) before editing.
 *
 * P1 is a single state, play, in one place: the player on foot, the use
 * key, the creatures read as they come near, Escape to pause. Places,
 * screens and saves come with later phases.
 */

/* The card's room for the game's pictures (pic_set_room), set before the first place. */
#define GAME_PICTURE_KB 256

/*
 * Before the first place is prepared: the keys as the game wants them,
 * the overlay's pictures, the card's room for them.
 */
void game_setup(void);
/* A place to play in: the player at its start, the overlay and use handler hooked up. */
void game_enter(struct World *world, const Level *level, Camera *cam);
/* One frame: 0 when the player quits. dt is real seconds. */
int game_frame(Camera *cam, const Input *in, float dt);
void game_shutdown(void);

#endif
