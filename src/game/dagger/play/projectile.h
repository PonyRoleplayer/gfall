#ifndef DF_PROJECTILE_H
#define DF_PROJECTILE_H

#include "engine/core/df.h"

/*
 * Things that fly: a bow's arrow, a spell's missile. Read docs/game.md
 * (Magic, the projectile) before editing.
 *
 * A projectile is a point with a velocity, a range and an optional pull of
 * gravity, in the scene's world units (40 to the metre). Each frame it moves
 * along its velocity, and the segment it covers is asked two questions of the
 * world: is a wall in the way, and does it enter a target. The nearer answer
 * ends it there; neither moves it on; the range run out ends it where it
 * is. A segment, not a point, is tested, so a slow frame never lets a missile
 * through a thin wall or a body.
 *
 * What the world is is the game's: the two answers come through the hooks
 * of ProjectileWorld (the game gives combat's ray and creature test), so
 * this file knows no scene, no creature, no sprite, and hostcheck drives it
 * on a world made of a wall and a ball. What a hit does is the game's too:
 * projectile_step reports each end, and the game strikes the arrow's
 * creature or bursts the spell.
 *
 * Nothing is allocated: PROJ_MAX in a static table. A launch with no free
 * entry fails, and the game says nothing (the oldest of eight arrows in
 * the air is a long way off).
 */

#define PROJ_MAX 8

/* What kind of thing flies (the game's picture and hit rule follow it). */
enum {
    PROJ_NONE,
    PROJ_ARROW,
    PROJ_SPELL
};

/* Who loosed it: the player's hits creatures, a creature's the player. */
enum {
    PROJ_FROM_PLAYER,
    PROJ_FROM_FOE
};

/* How a projectile ended. */
enum {
    PROJ_HIT_TARGET,
    PROJ_HIT_WALL,
    PROJ_EXPIRED
};

/* Speeds and ranges, in units a second and units: DFU's missile 25 m/s for 8 s; an arrow a little faster, to sight. */
#define PROJ_UNITS_PER_M 40.0f
#define PROJ_ARROW_SPEED (40.0f * PROJ_UNITS_PER_M)
#define PROJ_ARROW_RANGE 4096.0f
#define PROJ_SPELL_SPEED (25.0f * PROJ_UNITS_PER_M)
#define PROJ_SPELL_RANGE (8.0f * PROJ_SPELL_SPEED)

typedef struct Projectile {
    int kind;
    int from;
    /* The game's own number: an arrow's cargo, a spell's place in its list. */
    int tag;
    float at[3];
    /* Units a second. */
    float vel[3];
    /* Units a second squared, pulling down (y); 0 for a missile that flies true. */
    float gravity;
    /* Range left, in units. */
    float left;
} Projectile;

/* What to launch: the direction is a unit vector; the speed is units a second. */
typedef struct ProjectileShot {
    int kind;
    int from;
    int tag;
    float at[3];
    float dir[3];
    float speed;
    float gravity;
    float range;
} ProjectileShot;

/*
 * The world's two answers for a segment from a (length len along unit dir):
 *   wall:   1 when a wall crosses it, its distance along in *t.
 *   target: whatever the segment enters first (the game's handle for it) and
 *           its distance along in *t, else 0. It sees the projectile, for who
 *           flew it.
 * Either may be 0: no walls, no targets.
 */
typedef struct ProjectileWorld {
    void *user;
    int (*wall)(void *user, const float *a, const float *b, float *t);
    void *(*target)(void *user, const Projectile *p, const float *a, const float *dir, float len, float *t);
} ProjectileWorld;

/* One end of a flight, for the game to act on. */
typedef struct ProjectileEnd {
    /* The entry it flew in, as projectile_launch gave it: what a game keeps beside a flight (an arrow's bow). */
    int entry;
    int result;
    int kind;
    int from;
    int tag;
    /* Where it ended. */
    float at[3];
    /* Its direction of flight at the end, a unit vector. */
    float dir[3];
    /* What it struck, for PROJ_HIT_TARGET. */
    void *target;
} ProjectileEnd;

/* One launched; its entry, or -1 when all of PROJ_MAX are flying. */
int projectile_launch(const ProjectileShot *shot);
/*
 * Every flying one moved dt seconds. The ends of this step go in ends (at
 * most max of them, the rest lost: the game gives PROJ_MAX); how many.
 */
int projectile_step(float dt, const ProjectileWorld *world, ProjectileEnd *ends, int max);
/* All gone: a place left, a new game. */
void projectile_clear(void);
/* How many fly, and entry i (0 to PROJ_MAX - 1) for a picture, or 0 when it is free. */
int projectile_count(void);
const Projectile *projectile_get(int i);

#endif
