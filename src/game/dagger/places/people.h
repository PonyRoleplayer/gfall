#ifndef DF_PEOPLE_H
#define DF_PEOPLE_H

#include "engine/gfx/camera.h"
#include "engine/scene/entity.h"

/*
 * Townspeople walking a streamed town's streets, as Daggerfall Unity's
 * PopulationManager and MobilePersonMotor have them. Read
 * docs/entities.md before editing.
 *
 * In a town, by day, up to 24 people for every 16 of its blocks (96 at
 * most) come and go round the camera: each appears on a free walking
 * cell (wild_nav) within PEOPLE_SPAWN of it, and goes past PEOPLE_RECYCLE,
 * at dusk, or stuck; but never in sight within PEOPLE_POP (behind the
 * camera, or farther). Each walks cell to cell, 64 units, north, south,
 * east or west, liking roads, then grass, dirt, stone, and stops to face
 * the camera when it stands close and still. A guard one time in 32;
 * else a man or a woman in one of four outfits of the climate's people.
 *
 * The people are entities (engine/scene/entity.h) in a pool of
 * PEOPLE_MAX, which the scene reads to draw (Scene.entities). Their
 * pictures are the stream's (wres), read one a call before anyone wears
 * them. Plain C: hostcheck runs it.
 */

#define PEOPLE_MAX 96
/* World units: where they appear round the camera, where they go, the range they may pop in sight. */
#define PEOPLE_SPAWN 6144.0f
#define PEOPLE_RECYCLE 6000.0f
/*
 * A guard spawned for a crime (people_spawn_guard) comes into being this far from the player, out of
 * sight if a cell there allows: near enough to be on them in a few seconds, as DFU's are, not the
 * whole street's width away.
 */
#define PEOPLE_GUARD_NEAR 700.0f
#define PEOPLE_GUARD_FAR 1600.0f
#define PEOPLE_POP 4800.0f
/* A street citizen's health. A guard's is the city watch's, rolled when struck. */
#define PEOPLE_CITIZEN_HP 1

/*
 * Whoever the view from cam rests on within reach, drawn and in front of it:
 * *female and a number for who they are. 0 for no one. A person in the street
 * talks, and the game asks this when the use key finds no usable. Pitch,
 * visible height and the nearest model wall are respected, including a
 * cursor-derived aim camera. One cast per use, none for passive pointing.
 */
int people_in_view(const Camera *cam, float reach, int *female, unsigned *who);
/* The same, and in *dist how far along the view they stand: one cast serves a nearer reach and a farther. */
int people_in_view_at(const Camera *cam, float reach, int *female, unsigned *who, float *dist);

/* The stream is open: its scene draws the people. */
void people_begin(struct Scene *scene);
void people_end(void);
/* Once a frame, before it draws: who comes and goes, and everyone's step, secs since the last. */
void people_update(const Camera *cam, float secs);
/*
 * Before the first frame (a stream opened, a door back out): in a town
 * by day, every outfit of its people read and its whole population out
 * round the camera, as if they had been walking all along, not coming
 * one a tick and their pictures one a frame after it shows.
 */
void people_fill(const Camera *cam);
/*
 * The picture slots a settlement's people of climate wear (every outfit
 * and the guard's, walking and idle: 54), registered, into out; how
 * many. A town's block chunk holds them, so they are read under the
 * stream's budget when the town first comes in range (wild.c).
 */
#define PEOPLE_SLOTS 54
int people_slots(int climate, df_u16 *out, int cap);
/* 1 for a location type (WORLDMAP_*) whose streets have walkers: a city, a hamlet, a village. */
int people_settle(int type);
/* The origin moved (the stream's rebase): everyone moves back with it. */
void people_shift(float dx, float dz);
/* People walking now, and the pictures of them read so far. */
int people_count(void);
int people_pictures(void);

/* The pool entry, or 0. A dead one is still the same pointer until the slot is reused. */
Entity *people_at(int index);
int people_index(const Entity *e);
/* 1 for a living street citizen, or a living guard (archive 399). */
int people_is_citizen(const Entity *e);
int people_is_guard(const Entity *e);
/* A citizen's is PEOPLE_CITIZEN_HP. A guard's is -1 until a blow rolls the city watch's. */
int people_health(const Entity *e);
void people_set_health(Entity *e, int health);
/* 1 when the watch has marked this living person hostile to the player. */
int people_hostile(const Entity *e);
void people_set_hostile(Entity *e, int on);
/* The street motor walks them again. A corpse stays held. */
void people_unhold(Entity *e);
/* Dead, and gone: a citizen has no corpse picture. */
void people_fell(Entity *e);
/*
 * A killed guard stays, drawn as the city watch's corpse (ENEM 146,
 * TEXTURE.380 record 1). 1 when that picture is on them.
 */
int people_lay_corpse(Entity *e);
/* 1 for a guard's body still in the pool. */
int people_is_corpse(const Entity *e);
/*
 * One blood splash at the body's middle: record of TEXTURE.380, a billboard
 * in a free slot, 10 frames a second. Not a person. 0 with no slot or picture.
 */
Entity *people_blood(float x, float y, float z, int record);
/* 1 for that splash. */
int people_is_effect(const Entity *e);
/* The nearest living guard, or -1. The pool is the town round the camera. */
int people_nearest_guard(float x, float z);
/*
 * One guard on a walking cell round cam, out of sight when a cell allows it.
 * The same pictures the population already wears (archive 399). -1 when the
 * town has no free slot or no cell. Does not start them chasing: the game does.
 */
int people_spawn_guard(const Camera *cam);
/* The street motor leaves index alone. A guard is given the attack records. */
void people_hold(int index);

#endif
