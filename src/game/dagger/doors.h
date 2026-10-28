#ifndef DF_DOORS_H
#define DF_DOORS_H

#include "game/dagger/rdb.h"

/*
 * What can be used in a Daggerfall place, and what moves: the engine's
 * Usable and Mover lists (engine/scene/interact.h), built from the
 * blocks as Daggerfall Unity reads them. Read docs/interact.md.
 *
 * - A dungeon's hinged doors (models whose list letters are DOR, DDR,
 *   NEW or CAV, but not the red brick 72100) swing a quarter turn in
 *   1.5 s, locked by their starting lock; a house's (its door list) the
 *   same, unlocked.
 * - A dungeon's action records: models that slide or turn when used, or
 *   when the one before them in the chain starts. Text, spells, traps
 *   and the like relay the chain and do nothing more yet.
 * - Passages, the faces of a model painted with a door: a building's
 *   (archive 74) from the street into it and from inside back out; a
 *   dungeon's way in (56, the ruins' 331) from outside; its way out (95)
 *   from inside.
 * - People: the people lists of a town and of a house.
 */

/* Usable.what of a passage: where it leads. */
enum {
    DOOR_NONE,
    /* From the street into building tag & 0xff of block tag >> 8. */
    DOOR_ENTER,
    /* From inside a building back to the street. */
    DOOR_LEAVE,
    /* From outside into the place's dungeon. */
    DOOR_DUNGEON,
    /* From the dungeon out to the place. */
    DOOR_EXIT
};

/* What the place is, so which passages it has. */
enum {
    PLACE_DUNGEON,
    PLACE_TOWN,
    PLACE_INSIDE
};

/* Daggerfall Unity's reach: a door or a lever 128 units (3.2 m), a person 256. */
#define DOOR_REACH 128.0f
#define TALK_REACH 256.0f

typedef struct DoorBuild {
    /* In: the place's models (written as they move), what the file said of each, its block. */
    Instance *inst;
    const ModelAct *act;
    const df_u8 *block;
    int ninst;
    const Flat *flat;
    int nflat;
    /* A dungeon's editor markers and their blocks: a teleport's target may be one. 0 for none. */
    const Marker *mark;
    const df_u8 *mark_block;
    int nmark;
    int place;
    /* Out, with room for cap of each. */
    Usable *use;
    int nuse;
    int use_cap;
    Mover *mover;
    int nmover;
    int mover_cap;
} DoorBuild;

/*
 * Fill b->use and b->mover from the place, after its flats are final
 * (they are used by index). Returns 1; 0 when out of memory for the
 * work list. The movers' start poses are taken (mover_init).
 */
int doors_build(DoorBuild *b);

/*
 * Where to stand to come out of passage u: d units out from its face
 * along its normal, eye above its foot, facing out. yaw as Camera.yaw.
 */
void doors_landing(const Usable *u, float d, float eye,
                   float *x, float *y, float *z, float *yaw);

#endif
