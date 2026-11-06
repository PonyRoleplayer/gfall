#ifndef DF_CAST_H
#define DF_CAST_H

#include "game/dagger/play/effects.h"
#include "game/dagger/play/projectile.h"

/*
 * Casting: the spell made ready, the spell points paid, and where it goes by its
 * target. Read docs/game.md (Magic) before editing.
 *
 * As classic and DFU have it, a spell is chosen in the book and is then ready;
 * the attack button casts it (the weapon stays away while one is ready), the
 * cast key readies the last one again, and Escape or choosing a weapon puts it
 * away. Casting takes its spell points when it goes, by the cost for the player's
 * skill (spells.c), and trains the school of each effect that took hold. Nothing is
 * taken for a cast that cannot go: a silenced caster ("You are silenced."), too
 * few spell points ("You don't have the spell points."), a touch spell with
 * nothing within 3 m.
 *
 *   the caster       lands on the player at once.
 *   touch            on the creature the view rests on within 3 m, at once.
 *   at range         a missile flies (projectile.c, 25 m/s, 8 s at most, no
 *                    gravity) and lands on the creature it meets; a wall or its
 *                    range ends it with nothing.
 *   area round       every living creature within 4 m with no wall between, at once.
 *   area at range    a missile as above, and where it ends (a creature, a wall or
 *                    the end of its range) it bursts: every creature within 4 m.
 *
 * A burst is not at the player, whatever it stands beside; the caster's own
 * spells reach others only. Plain C over effects.h and combat.h: hostcheck
 * casts in the streamed hold.
 */

/* Touch reaches 3 m; a burst 4 m; the missile starts an arm's length (0.9 m) out. */
#define CAST_TOUCH 120.0f
#define CAST_BURST 160.0f
#define CAST_ARM 36.0f
#define CAST_BURST_MAX 16

/* A new game: nothing ready, nothing last. */
void cast_new_game(void);
/*
 * s made ready for p: 1 when it is. Not when p is silenced or has fewer spell points than s costs
 * (the line says which); the spell kept is a copy. A ready spell is put away by cast_cancel.
 */
int cast_ready(const Spell *s, const Player *p);
int cast_is_ready(void);
const Spell *cast_ready_spell(void);
/* The spell last cast, or 0: what the cast key readies again. */
const Spell *cast_last_spell(void);
/* Put away. */
void cast_cancel(void);

/*
 * The ready spell cast from the eye along the unit dir: 1 when it went (its points spent, the
 * spell no longer ready, remembered as the last). 0, nothing spent and still ready, when it could
 * not go (the line says why). scene is the place's (creatures and walls).
 */
int cast_fire(Player *p, const Scene *scene, const float *eye, const float *dir);
/*
 * A spell's missile ended (projectile_step's end, read by shots.c): lands, or bursts. How many
 * effects took hold.
 */
int cast_arrived(const ProjectileEnd *end, Player *p, const Scene *scene);

/* The spell cast by missile entry (its copy), or 0 when none flies there: for the missile's picture. */
const Spell *cast_flying(int entry);

#endif
