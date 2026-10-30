#ifndef DF_DUNSTREAM_H
#define DF_DUNSTREAM_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"
#include "engine/scene/entity.h"
#include "game/dagger/formats/dungeon.h"

/*
 * A dungeon streamed by block (docs/streaming.md, Dungeons): each RDB
 * block a chunk of 2048 units, named by its place in the dungeon's
 * layout, loaded round the camera (3 x 3) and dropped as it walks on.
 * The engine streams; this is the chunk source, as wild.c is the
 * world's. Read docs/streaming.md before editing.
 *
 * A block's chunk holds what a dungeon loaded whole holds of it, by the
 * same rules (world.c): its models, painted by the dungeon's picture
 * table (swaps.c), its flats standing at their centres, its lights (the
 * flames' flickering), its torches' and animals' sounds, its doors,
 * levers and teleports (doors_build), its creatures (enemies.c's draws,
 * standing on their floors), and its water (its start marker's level
 * over its flooded models). Its doors and creatures are keyed as a
 * whole load's are, and the stream keeps what the player changed.
 *
 * Beyond a whole load, a block's treasure stands as entities, not flats,
 * so that the game can search them and they can go when emptied: its
 * random treasure markers, and its fixed TEXTURE.216 flats. Each block
 * also holds DUNSTREAM_SPARES spare things, not drawn, for the game to put
 * down where it likes: a pile the player drops. Being entities of the
 * block, they keep their place across origin moves and when the block
 * goes and comes back, with no list of the game's to move.
 *
 * One stream at a time, in the current temp heap; pictures and meshes
 * by session (wres.c).
 */

/*
 * Opens a stream of dungeon over level, with its origin at the start
 * block. The start: the start block's enter marker, 8 (or its marker 10, its first marker,
 * or the block's middle) in the stream's frame. 0 when the dungeon
 * cannot be streamed (no start block, no files).
 */
int dunstream_open(const Dungeon *dungeon, Level *level, float *x, float *y, float *z, float *yaw, float *pitch);
/* The camera's ring, loaded whole before the first frame (under the hold). */
void dunstream_fill(Camera *cam);
/* One frame: blocks in and out round the camera, within budget_ms. */
void dunstream_update(Camera *cam, float budget_ms);
void dunstream_close(void);
/*
 * The creature kind (ENEM's id) of an entity the stream made, by its key,
 * and in *female whether it was drawn a woman; -1 for one it did not make.
 */
int dunstream_kind(const Entity *e, int *female);
/* 1 when its marker says it attacks only when attacked (enemy_at's passive). */
int dunstream_passive(const Entity *e);
/* 1 for an entity that is one of a block's treasure piles. */
int dunstream_treasure(const Entity *e);
/* 1 for an entity that is one of a block's spare things, used or not. */
int dunstream_spare(const Entity *e);
/* How many spare things a block holds. */
#define DUNSTREAM_SPARES 4
/* The byte of a spare's keep row (Keep.game) that says it is in use: set, it comes back drawn with its block. */
#define DUNSTREAM_SPARE_USED 15
/* The open dungeon's type (Dungeon.dungeon_type), which names its treasure's loot table; -1 with none open. */
int dunstream_dungeon_type(void);
/* A block's side in units: a chunk of the stream, and what stream_origin counts in. */
#define DUNSTREAM_BLOCK 2048.0f
int dunstream_active(void);

#endif
