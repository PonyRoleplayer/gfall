#ifndef DF_RIDE_H
#define DF_RIDE_H

#include "game/dagger/play/player.h"

/*
 * Riding: on foot, on a horse, or driving a small cart, as Daggerfall Unity's
 * TransportManager has it. Read docs/game.md (The wider world, Riding).
 *
 * The HUD's transport button mounts the horse if the player owns one, else the
 * cart, and dismounts. Only outdoors: going into a building or a dungeon gets
 * down. Riding is faster and cannot run (rules_ride_speed). The riding picture
 * (MRED00I0.CFA, the cart's MRED01I0.CFA) has four frames, a new one every
 * 0.125 s while the player moves, the first while they stand.
 *
 * Plain C: hostcheck runs the rules; the game draws the picture.
 */

enum {
    RIDE_FOOT,
    RIDE_HORSE,
    RIDE_CART
};

#define RIDE_FRAMES 4
#define RIDE_FRAME_TIME 0.125f

/* What the transport button does outdoors (or not): the new way of going, and what to say; 0 to say nothing. */
const char *ride_toggle(Player *p, int outdoors);
/* Down, whatever it was: into a building, a dungeon, a new game. */
void ride_get_down(Player *p);
/* The picture's frame now, as the player moves or stands, dt seconds on. */
int ride_frame(int moving, float dt);
/* The way the travel map's journey goes for a player: on a horse if they own one, else their cart, else on foot (journey.h). */
int ride_journey(const Player *p);

#endif
