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
 * The sounds are TransportManager's too: while moving, the horse's hooves
 * loop (HorseClop at half speed or less and half as loud, HorseClop2 faster),
 * or the cart's rattle; a whinny every 2 to 40 seconds while mounted. Standing
 * still, in the air or paused, the loop stops. DFU raises the pitch running:
 * the engine's voices have no pitch, and a rider cannot run here.
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
/* The loop a rider's movement plays: SND_*, or -1 on foot. slow: at half speed or less. */
int ride_hoof_clip(int riding, int slow);
/* A frame of the riding sounds: moving on the ground or not, slow or not, dt seconds on. On foot or paused (moving 0), quiet. */
void ride_sound(int riding, int moving, int slow, float dt);
/* The loop stopped while the world stands (a screen, the console), the whinny's count kept. */
void ride_quiet(void);
/* The way the travel map's journey goes for a player: on a horse if they own one, else their cart, else on foot (journey.h). */
int ride_journey(const Player *p);

#endif
