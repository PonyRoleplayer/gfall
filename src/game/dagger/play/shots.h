#ifndef DF_SHOTS_H
#define DF_SHOTS_H

#include "engine/scene/scene.h"
#include "game/dagger/play/character.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/projectile.h"

/*
 * What the player looses, and where it goes: the game's half of
 * projectile.c. Read docs/game.md (Combat, a bow; Magic, the projectile).
 *
 * A bow's release launches an arrow (shots_arrow) that flies along the view
 * at PROJ_ARROW_SPEED. Each frame shots_frame moves what flies, asking
 * combat's ray whether a wall is in the way and combat's aim whether a
 * creature is entered, and an arrow that reaches a creature strikes it as
 * the old instant shot did, by combat_missile_strike, with the bow in hand
 * when it lands (DFU's arrow carries nothing of the bow), or, if that
 * hand holds something else by then, the bow it was loosed from, kept with it.
 *
 * A hit wakes and wounds the creature, so it is the game's: ProjectileEnd
 * carries only where the arrow went.
 */

/* Everything in the air gone: a new game, a place left. */
void shots_clear(void);
/*
 * An arrow loosed from the eye along unit dir, from the bow bow (copied, for
 * the hit). 1 when it flies, 0 when eight already do.
 */
int shots_arrow(const float *eye, const float *dir, const Item *bow);
/*
 * One frame: what flies moves dt seconds in scene; arrows that reach a
 * creature strike it for pc with the hand weapon of pack, feet being the
 * player's. How many ended.
 */
int shots_frame(float dt, const Scene *scene, Character *pc, Inventory *pack, const float *feet);

#endif
