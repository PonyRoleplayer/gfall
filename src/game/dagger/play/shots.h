#ifndef DF_SHOTS_H
#define DF_SHOTS_H

#include "engine/scene/scene.h"
#include "game/dagger/play/character.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/player.h"
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
 *
 * A spell's missile is seen as a billboard of the engine's spawn pool (spawn.h): its element's
 * picture, TEXTURE.375..379 record 0, five frames a second, put where the missile is each frame
 * and let go where it ends, with a burst (record 1, 15 frames a second) for 0.6 s. The pictures are
 * made resident once for the stream (wres_acquire; they are small); outside a stream a spell
 * flies unseen. An arrow has no picture (DFU's is a 3D model).
 */

/* Everything in the air gone: a new game, a place left. */
void shots_clear(void);
/*
 * A burst of an element's own picture where a spell lands, for 0.6 s (DFU's post-impact
 * life): TEXTURE.375 fire, 376 cold, 377 poison, 378 shock, 379 magic, record 1. Nothing is
 * shown where the pictures cannot be made resident (outside a stream).
 */
void shots_flash(const Scene *scene, int element, const float *at);
/*
 * An arrow loosed from the eye along unit dir, from the bow bow (copied, for
 * the hit). 1 when it flies, 0 when eight already do.
 */
int shots_arrow(const float *eye, const float *dir, const Item *bow);
/*
 * One frame: what flies moves dt seconds in scene; an arrow that reaches a
 * creature strikes it for the player with the hand weapon, a spell's missile
 * lands or bursts (cast.h). How many ended.
 */
int shots_frame(float dt, const Scene *scene, Player *player);

#endif
