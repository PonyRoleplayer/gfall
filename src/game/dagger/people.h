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
#define PEOPLE_POP 4800.0f

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

#endif
