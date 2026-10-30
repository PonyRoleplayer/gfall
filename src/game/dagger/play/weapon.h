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
 * ramp, DATFALL's colours). Each frame goes on the card cropped to its
 * drawn pixels (cif_bounds) and is drawn at its place moved by the crop.
 * On the card: the armed picture, and the one frame of the swing
 * showing, sent when it shows (docs/pictures.md).
 */

/* Bare hands' file. */
#define WEAPON_FISTS_CIF 10

/* The item in hand, or 0 for bare hands: its file read, if it is not the one held. */
void weapon_hold(const Item *it);
/* A random swing, unless one runs: its swing (1..6), or 0. */
int weapon_attack(void);
/*
 * A swing by the mouse (DFU's WeaponManager, classic's gesture). Each frame the
 * attack button is held, what the mouse moved; pressed and released as the
 * Input says. When a second's motion comes to WEAPON_GESTURE counts, the swing
 * goes that way; a press and release with no such motion swings at random,
 * as a click did. The swing begun's group, or 0. A bow takes no gesture: it
 * draws on the press, holds, and looses on the release.
 */
#define WEAPON_GESTURE 32
/*
 * Whether the attack button is to be ignored this frame: a screen was up this
 * frame, or it was and the button has not been let go since. Without it the click
 * that closes a screen swings when let go.
 */
int weapon_gate(int screen_up, int held);
int weapon_gesture(int pressed, int held, int released, int dx, int dy, float dt);
/* The swing a motion gives: a mouse's dx (right) and dy (down), 1..6 by the swing groups. */
int weapon_swing_of_motion(int dx, int dy);
/* The swing on by dt seconds at this Speed. */
void weapon_step(float dt, int speed);
/* 1 while a swing runs. */
int weapon_swinging(void);
/* The swing running (1 down, 2 down-left, 3 left, 4 right, 5 down-right, 6 up), 0 at rest. */
int weapon_swing(void);
/*
 * 1 once a swing, the step its hit frame comes up: frame 2 of a melee
 * swing, 5 of a bow's shot (DFU's FPSWeapon.GetHitFrame). The game
 * strikes then (combat.c). Taken: the next call says 0.
 */
int weapon_hit(void);
/* 1 when the file held is the bow's. */
int weapon_is_bow(void);
/* The picture to draw and its place on classic's screen; 0 when nothing shows (a bow at rest, no file). */
int weapon_shown(int *pic, int *left, int *top, int *w, int *h);
/* Its pictures and file let go. */
void weapon_free(void);

#endif
