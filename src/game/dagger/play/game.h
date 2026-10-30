#ifndef DF_GAME_H
#define DF_GAME_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"
#include "engine/platform/input.h"

/*
 * The game's play state: one frame of it, between the program's input
 * and its gfx_frame. Read docs/game.md (A frame of play) before editing.
 *
 * The game's states: classic's title, then its start menu, before any
 * place (the engine's screen frame, gfx_frame with no level); then play
 * in the hold, where Escape opens the options over the stilled world,
 * and from them the controls. Places and saves come with later phases.
 */

/* What a frame asks of the program. */
enum {
    GAME_QUIT,
    GAME_GO_ON,
    /* Load the place to play: the start menu's new game. */
    GAME_START
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
 * A place to play in, streamed by block (dunstream.c): the player at its
 * start (x, y, z the eye), the overlay and use handler hooked up. The
 * game steps the stream each frame (GAME_STREAM_MS).
 */
void game_enter(const Level *level, Camera *cam, float x, float y, float z, float yaw, float pitch);
/* Milliseconds a frame gives the dungeon's stream: blocks coming in, lights cut a face at a time. */
#define GAME_STREAM_MS 3.0f
/* One frame: GAME_*. dt is real seconds. */
int game_frame(Camera *cam, const Input *in, float dt);
void game_shutdown(void);

#endif
