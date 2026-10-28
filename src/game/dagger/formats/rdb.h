#ifndef DF_RDB_H
#define DF_RDB_H

#include "engine/scene/scene.h"

/*
 * One RDB block after the records have been turned into scene entities.
 * neditor counts editor markers. They are not entities.
 */

/*
 * Archive 199. Not drawn. Record 10 is a block's start (the new-game
 * start in the hold); 15 a random creature, 16 a fixed one; 8 where a
 * visitor comes in. The flat's resource bytes after its picture, as
 * Daggerfall Unity's RdbFlatResource reads them: a creature marker's
 * gender (fixed) or slot (random) in flags, its type in id's low byte
 * (fixed), passive when action is 99; a start marker's water level in
 * sound (8 units each, 0 for none). docs/entities.md.
 */
typedef struct Marker {
    float x, y, z;
    df_u16 record;
    df_u16 flags;
    /* FactionOrMobileId: magnitude, then the sound byte above it. */
    df_u16 id;
    df_u8 action;
    /* RDB: the object's offset in its block file, by which an action's next names it (a teleport's target). */
    df_u32 pos;
} Marker;

/*
 * What a block's file says of one model beyond where it stands, beside
 * Block.inst: what world.c needs for its doors and actions. Read
 * docs/interact.md.
 */
typedef struct ModelAct {
    /* RDB: the object's offset in its block file; an action names the next by it. */
    df_u32 pos;
    df_u32 id;
    /* RDB: the model list's three letters (DOR a door, DDR a double door, EXT the exit). */
    char tag[4];
    /* RDB: the trigger, low bits; for a door, its starting lock in the top nibble. */
    df_u32 trigger;
    /* RDB: a record id in DAGGER.SND, 0 for none. */
    df_u8 sound;
    /* RDB action record: flags 0 is none; then the axis, time and size of it. */
    df_u8 flags;
    df_u8 axis;
    df_u16 duration;
    df_u16 magnitude;
    df_i32 next;
    /* RMB: the building (subrecord) it belongs to, -1 for the block's own. */
    df_i16 record;
    /* RMB interior: 1 for a hinged door from the building's door list. */
    df_u8 hinged;
} ModelAct;

/* Flat.tag of a person (an RMB people record): this bit, and the faction under it. */
#define FLAT_PERSON 0x8000

typedef struct Block {
    Instance *inst;
    /* One per inst, zeroed but for what the loader knows. */
    ModelAct *act;
    int ninst;
    int inst_cap;
    int ntris;
    Flat *flat;
    int nflat;
    int flat_cap;
    Marker *marker;
    int nmarker;
    int mark_cap;
    int neditor;
    /* RDB object type 2. RMB blocks have none. */
    Light *light;
    int nlight;
} Block;

struct Bsa;

/*
 * One ARCH3D mesh for the whole place. A block used to keep its own
 * copy, so a town parsed the same house once per block.
 */
#define ARCH_CACHE 4096

typedef struct ArchEnt {
    df_u32 id;
    Mesh *mesh;
} ArchEnt;

Mesh *arch_cached(ArchEnt *cache, int *ncache, struct Bsa *arch, df_u32 id);
/*
 * Meshes arch_cached reads go into h (0, the default: the current heap).
 * A streamed world keeps them in a heap of their own, apart from the
 * chunk being loaded (docs/streaming.md).
 */
struct MemHeap;
void arch_cache_heap(struct MemHeap *h);

/* Allocate the slots this block will fill. Caps are the tally, not 2048. */
int block_reserve(Block *block, int ninst, int nflat, int nmark);

Block *rdb_load(const df_u8 *rdb, df_u32 size, struct Bsa *arch,
                ArchEnt *cache, int *ncache);
/*
 * A block's editor markers alone, in rdb_load's order, into out (cap):
 * no models parsed, no memory. How many. A streamed dungeon draws its
 * creatures from them, block by block, as a whole load does.
 */
int rdb_markers(const df_u8 *rdb, df_u32 size, Marker *out, int cap);

#endif
