#ifndef DF_ENEMIES_H
#define DF_ENEMIES_H

#include "game/dagger/formats/rdb.h"

/*
 * The creatures a dungeon's blocks place: which kind stands at each
 * editor marker, and the pictures of each kind. Daggerfall's rules as
 * Daggerfall Unity has them (RDBLayout, EnemyBasics, RandomEncounters),
 * read for what they do. Read docs/entities.md before editing.
 *
 * A kind is Daggerfall's mobile id (DFU's MobileTypes): 0..42 the
 * monsters (a rat 0, a lich 32), 128..146 people of a class (a mage
 * 128, a knight 145, the city watch 146). Whether one is friend or foe
 * is the game's later business; here they only exist, where the file
 * puts them. Plain C: hostcheck runs it.
 */

/* Mobile ids. */
#define ENEMY_KINDS 147
#define ENEMY_RAT 0
#define ENEMY_SLAUGHTERFISH 11
#define ENEMY_GHOST 18
#define ENEMY_WRAITH 23
#define ENEMY_DREUGH 41
#define ENEMY_LAMIA 42
#define ENEMY_FIRST_CLASS 128

/* Editor records: a random creature, a fixed one. */
#define ENEMY_RANDOM_MARKER 15
#define ENEMY_FIXED_MARKER 16

/* A block's water level when it has none: classic's 10000, below everything. */
#define ENEMY_DRY 10000

/* Its pictures' archive (TEXTURE.###), -1 for no such kind. */
int enemy_archive(int kind, int female);
/* 1: it has standing pictures (records 15..19); else it stands in its walking ones. */
int enemy_has_idle(int kind);
/* 1: it flies, hanging where the marker is (an imp, a bat, a harpy, a dragonling). */
int enemy_flies(int kind);
/* 1: a ghost or a wraith, which float where the marker is as flyers do. */
int enemy_spectral(int kind);
/* 1: it lives in water only (a slaughterfish, a dreugh, a lamia). */
int enemy_aquatic(int kind);

/*
 * A dungeon's draws, before its markers: classic seeds its rand with the
 * location id and draws 256 kinds from the dungeon's encounter table
 * (DUN_TYPE_*) and 256 from the underwater one; a random marker takes
 * the one in its slot. level is the player's, which steers the draws
 * toward the table's start (1 for now: no player yet).
 */
void enemy_dungeon(int location_id, int dungeon_type, int level);

/*
 * The creature at an editor marker, given its block's water level
 * (classic's: from the block's start marker, 8 units a step, down
 * positive; ENEMY_DRY for none): its kind, and 1 in *female for a
 * woman of a class. -1 when the marker places none: not a creature
 * marker, a fixed one of the invalid kind 99 or of no pictures, or a
 * water dweller where no water is. *passive is 1 when the file says it
 * attacks only when attacked (its action byte, 99); the game decides
 * what that means.
 */
int enemy_at(const Marker *mk, int water, int *female, int *passive);

/* A block's water level from its start marker (record 10), classic's sense; ENEMY_DRY for none. */
int enemy_water(const Marker *marks, int n);

#endif
