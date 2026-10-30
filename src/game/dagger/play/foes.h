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
 * to melee range, and swings when the timer has run out and a roll of
 * Speed comes under Speed / 8 + 6; its swing lands on its attack
 * pose's third frame if the player is still in reach, height and all, before it
 * and no wall between. Out of sight it goes where it
 * last saw the player, and after 200 classic ticks unseen it gives up,
 * standing where it is.
 *
 * Beyond sight (EnemySenses, EnemyMotor). A creature that knows of the player
 * hears them within FOES_HEARING, through doors but not walls, and keeps
 * after them. A sleeper within FOES_STEALTH may notice them through anything:
 * once a game minute, a roll against 2 x (distance x Stealth / 1024), so the
 * nearer the likelier and the better the skill the less. Having lost the player
 * it walks on past where they were last, along the way they were going, a step
 * further each time. One that can opens an unlocked door in its way.
 * A blow knocks a creature back, standing it still while it slides.
 *
 * Senses run on classic's ticks, about 18 a second (FOES_TICK): a few
 * sleepers looked at a tick, each mind's sight every sixth. Moving and
 * the attack's frames run every frame for a creature near the player. One
 * far off moves every second or fourth frame, by the time it was owed, so
 * it covers the same ground for fewer collision sweeps (foes_due). At most
 * FOES_MINDS are awake.
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
/* Hearing: 25 m. A sleeper's notice of the player: within 1024 classic units. A door opened from 2 m. */
#define FOES_HEARING 1000.0f
#define FOES_STEALTH 1024.0f
#define FOES_DOOR_REACH 80.0f
/* Past these a creature moves less often (foes_due): 12.8 m and 25.6 m, ours, each well past any blow. */
#define FOES_NEAR 512.0f
#define FOES_FAR 1024.0f

void foes_set_on(int on);
int foes_on(void);
/* e struck or otherwise roused: awake now, after the player, passive or not. */
void foes_alert(Entity *e);
/* e knocked back from the point from, at this speed in classic's units: it slides, and acts again when that dies away. */
void foes_knock(Entity *e, const float *from, int speed);
/* The nearest sleeper within FOES_ROUSE_REACH of the point wakes and comes: what a rest's encounter is, there being no creatures to conjure. 1 when one did. */
#define FOES_ROUSE_REACH 2048.0f
int foes_rouse_nearest(const float *at);
/* The chance in 100 a sleeper this far off does NOT notice someone of this Stealth (FormulaHelper.CalculateStealthChance). */
int foes_stealth_chance(float distance, int stealth);
/*
 * One frame of every mind against player p in scene: senses on the
 * ticks dt brings, moving and striking. 1 when a blow killed the player.
 */
int foes_frame(Player *p, const Scene *scene, float dt);
/* Every mind let go: a new place, or the player back at the start. */
void foes_reset(void);
/* Minds awake now. */
int foes_awake(void);
/* The furthest any blow at the player has come from since foes_reset, feet to feet, height and all. */
float foes_blow_reach(void);
/*
 * Whether mind number index, this far from the player in units, moves on
 * frame number frame. Near, always; past FOES_NEAR every second frame;
 * past FOES_FAR every fourth. The index staggers them, so far minds do
 * not all land on one frame. Time not moved is not lost: the next move
 * takes it all.
 */
int foes_due(float distance, int frame, int index);

#endif
