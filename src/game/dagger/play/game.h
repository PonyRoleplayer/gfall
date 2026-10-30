#ifndef DF_GAME_H
#define DF_GAME_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"
#include "engine/platform/input.h"
#include "game/dagger/places/travel.h"

/*
 * The game's play state: one frame of it, between the program's input
 * and its gfx_frame. Read docs/game.md (A frame of play) before editing.
 *
 * The game's states: classic's title, then its start menu, before any
 * place (the engine's screen frame, gfx_frame with no level); then play
 * in a place, where Escape opens the options over the stilled world,
 * and from them the controls; saving and loading, one slot; the death
 * screen; the inventory, the sheet and rest. Going from place to place is travel.h's.
 */

/* What a frame asks of the program. */
enum {
    GAME_QUIT,
    GAME_GO_ON,
    /* Load the place to play: a new game, or a saved one, as the game has set up. */
    GAME_START,
    /* A passage was used: load where it leads, the player going on as they are (travel.h). */
    GAME_TRAVEL
};

/* The card's room for the game's pictures (pic_set_room), set before the first place. */
#define GAME_PICTURE_KB 256

/*
 * Before the first gfx_prepare: the keys as the game wants them and as
 * DATA\KEYS.DAT binds them, the HUD's and the menus' pictures, the
 * card's room for them; the title open.
 */
void game_setup(void);
/* 1 while a place is being played (the program draws it), 0 on a screen before one. */
int game_playing(void);
/* 1 when the frame should draw the place: in play, and no screen covers all of it. */
int game_world_shows(void);
/*
 * A new or a loaded game begins in the place just opened (travel.h): the
 * player made from what game_frame set up, at this pose, the overlay and the
 * use handler hooked up. The game steps the place each frame (travel_update).
 */
void game_enter(const Level *level, Camera *cam, const TravelPose *at);
/* The player arrives in the place just opened through a passage: the same player, at this pose. */
void game_arrive(const Level *level, Camera *cam, const TravelPose *at);
/* The height of the player's eye over their feet, for travel_land. */
float game_eye(void);
/* Milliseconds a frame gives a stream when the spare time to the retrace cannot be worked out (game.c's stream_budget). */
#define GAME_STREAM_MS 3.0f
/* One frame: GAME_*. dt is real seconds. */
int game_frame(Camera *cam, const Input *in, float dt);
void game_shutdown(void);

#endif
