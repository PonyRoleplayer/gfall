#ifndef DF_ENTBUILD_H
#define DF_ENTBUILD_H

#include "engine/scene/entity.h"

/*
 * A game's say in who stands in a place, as a builder makes it (the entity plan's load hooks, docs/entities.md). The builders are world.c (a building's inside, a whole dungeon), dunstream.c (a
 * dungeon's blocks as they stream in) and, later, wild.c (a town's chunks). The people a builder makes are entities; before they are drawn the game is asked, for each, to change it or drop it
 * (EntityBuildHook), then to add its own (EntityAddHook): a quest's person takes over one of a building's people, or stands on a quest marker. A hook runs while the builder works and may call
 * entbuild_marker_nth, entbuild_slot and entbuild_person. Plain C: hostcheck runs it.
 */

/* What the file object was. */
enum {
    /* An RMB people record (a building's person). */
    ENT_SRC_PEOPLE = 1
};

typedef struct EntBuild {
    /* PLACE_INSIDE, PLACE_DUNGEON ... (world_place). */
    int place;
    /* A stream chunk's, else 0. */
    int cx, cz;
    /* The town or dungeon's region and index (dungeon_load_at's / exterior_load_at's). */
    int region, index;
    /* A building's RMB block and record in its town, or a dungeon's block (record -1); -1 for neither. */
    int block, record;
    /* ENT_SRC_*, and the object's id low word as the original numbers it (0: not worked out; a game matches MAPS quest sites by it). */
    int source;
    df_u32 file_id;
    /* The nth of count the builder made here (EntityBuildHook; for the add hook, how many stand), and where the first stands (has_first) for a game that stands its own people beside it. */
    int nth, count;
    int has_first;
    float first_x, first_y, first_z;
} EntBuild;

/* Change e (its picture, who it is, where it stands), or return 0 to drop it. */
typedef int (*EntityBuildHook)(const EntBuild *b, Entity *e);
/* Write the game's own into out (cap of them) and say how many. */
typedef int (*EntityAddHook)(const EntBuild *b, Entity *out, int cap);
void entbuild_set_hooks(EntityBuildHook change, EntityAddHook add);

/*
 * What a builder gives a hook while it runs (entbuild_begin, entbuild_end): a picture read and placed by its file and image, with its slot and sizes (the pose's slot, w and h); and the n-th editor
 * marker of a record in the building or block being built (199.11 is a quest's), in the frame the builder's entities are in.
 */
typedef struct EntBuilder {
    int (*slot)(int file, int image, int *slot, int *w, int *h);
    int (*marker)(int record, int n, float *x, float *y, float *z);
} EntBuilder;
void entbuild_begin(const EntBuilder *builder);
void entbuild_end(void);
int entbuild_slot(int file, int image, int *slot, int *w, int *h);
int entbuild_marker_nth(int record, int n, float *x, float *y, float *z);

/* A person of a building or a dungeon as an entity: static, usable by hand, not solid, one idle picture facing the viewer; what is their faction (tag & 0x7fff), tag the flat they stand as. */
void entbuild_make_person(Entity *pe, int slot, int w, int h, int tag, int file, int image, float x, float y, float z);
/* While a hook runs: out made so, its picture placed by the builder. 1 when made. */
int entbuild_person(Entity *out, int file, int image, int tag, float x, float y, float z);

/* The builders' calls: the change hook on e (1 keep, 0 drop; 1 when none is set) and the add hook. */
int entbuild_change(const EntBuild *b, Entity *e);
int entbuild_add(const EntBuild *b, Entity *out, int cap);
int entbuild_have_hooks(void);

#endif
