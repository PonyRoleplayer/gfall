#ifndef DF_FOES_H
#define DF_FOES_H

#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "game/dagger/play/player.h"

/*
 * A dungeon's creatures awake: they see the player, chase, and strike,
 * by Daggerfall Unity's EnemySenses, EnemyMotor and EnemyAttack read for
 * what they do. Read docs/game.md (Creatures awake) before editing.
 *
 * A creature sleeps where its marker stood until it sees the player
 * (within FOES_SIGHT, in front of it, no wall between their eyes) or is
 * struck (foes_alert). A passive one (its marker's) wakes only when
 * struck. Awake, it has a mind: a body of its size under gravity (none
 * for a flyer), a give-up count, a melee timer. It turns toward the
 * player 20 degrees a classic tick, walks at (Speed + 150) / 39.5 m/s
 * to melee range, and swings when the timer and a roll on its Speed
 * let it; its swing lands on its attack pose's third frame if the
 * player is still in reach and before it. Out of sight it goes where it
 * last saw the player, and after 200 classic ticks unseen it gives up,
 * standing where it is.
 *
 * Senses run on classic's ticks, about 18 a second (FOES_TICK): a few
 * sleepers looked at a tick, each mind's sight every sixth. Moving and
 * the attack's frames run every frame. At most FOES_MINDS are awake.
 *
 * The console's tai switches them off: no one wakes, moves or strikes.
 */

#define FOES_MINDS 24
/* Classic's update: the system timer's 0.0549254 s. */
#define FOES_TICK 0.0549254f
/* Sight: 4096 classic units; melee reach, DFU's 2.25 m. */
#define FOES_SIGHT 4096.0f
#define FOES_MELEE 90.0f
#define FOES_GIVE_UP 200

void foes_set_on(int on);
int foes_on(void);
/* e struck or otherwise roused: awake now, after the player, passive or not. */
void foes_alert(Entity *e);
/*
 * One frame of every mind against player p in scene: senses on the
 * ticks dt brings, moving and striking. 1 when a blow killed the player.
 */
int foes_frame(Player *p, const Scene *scene, float dt);
/* Every mind let go: a new place, or the player back at the start. */
void foes_reset(void);
/* Minds awake now. */
int foes_awake(void);

#endif
