#ifndef DF_REST_H
#define DF_REST_H

#include "game/dagger/play/player.h"

/*
 * Classic's rest (REST00I0.IMG, REST01I0.IMG, REST02I0.IMG), in memory, as
 * Daggerfall Unity's DaggerfallRestWindow has it. Read docs/game.md (Rest)
 * before editing.
 *
 * Three ways, chosen on the panel: for a while (a number of hours typed, 99
 * at most), until fully healed, and loitering (the hours pass, nothing heals).
 * Time passes ten game minutes a step, and each whole hour of rest gives back
 * health, fatigue and spell points and is a use of Medical. A creature awake
 * stops it, and refuses it to begin with (foes_awake).
 *
 * Plain C, drawing nothing: hostcheck drives it. screens.c puts it on the
 * screen and gives it the frame's time.
 */

/* What a frame or a click asks of the game. */
enum {
    REST_STAY,
    REST_LEAVE
};

/* 0 when a picture is missing. */
int rest_open(Player *p);
void rest_close(void);
void rest_compose(void);
const df_u8 *rest_pixels(void);
int rest_changed(void);
/* A click at classic's x, y: REST_*. */
int rest_click(int x, int y);
/* A key by its virtual key code: the hours typed, Enter, Escape. As a click. */
int rest_key(int vk);
/* The frame's real seconds: time passes while it runs. As a click. */
int rest_step(float dt);
/* The line a rest ended with, for the game to say once it has closed; 0 when none. */
const char *rest_said(void);
const char *rest_note(void);

#endif
