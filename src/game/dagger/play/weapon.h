#ifndef DF_WEAPON_H
#define DF_WEAPON_H

#include "game/dagger/play/inventory.h"

/*
 * The weapon in hand, drawn over the view as classic draws it: its
 * WEAPON##.CIF (WEAP's cif; bare hands WEAPON10.CIF), the armed picture
 * at rest and a swing's frames while it swings, each at its own place on
 * classic's 320 x 200 screen (cif.h). Read docs/game.md (Combat).
 *
 * For now a click swings one of the file's swings at random (classic's
 * six: down, down-left, left, right, down-right, up), not the mouse's
 * direction; a swing runs its frames at classic's rate
 * (rules_swing_frame_time) and the armed picture comes back.
 *
 * The material recolours the picture (cif_metal on ART_PAL's grey
 * ramp, DATFALL's colours). On the card: the armed picture, and the one
 * frame of the swing showing, sent when it shows (docs/pictures.md).
 */

/* Bare hands' file. */
#define WEAPON_FISTS_CIF 10

/* The item in hand, or 0 for bare hands: its file read, if it is not the one held. */
void weapon_hold(const Item *it);
/* A random swing, unless one runs: its swing (1..6), or 0. */
int weapon_attack(void);
/* The swing on by dt seconds at this Speed. */
void weapon_step(float dt, int speed);
/* 1 while a swing runs. */
int weapon_swinging(void);
/* The picture to draw and its place on classic's screen; 0 when nothing shows (a bow at rest, no file). */
int weapon_shown(int *pic, int *left, int *top, int *w, int *h);
/* Its pictures and file let go. */
void weapon_free(void);

#endif
