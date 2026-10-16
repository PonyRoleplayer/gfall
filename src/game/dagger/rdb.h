#ifndef DF_RDB_H
#define DF_RDB_H

#include "engine/scene/scene.h"

/*
 * One RDB block after the records have been turned into scene entities.
 * neditor counts editor markers. They are not entities.
 */

/* Archive 199. record 10 is the new-game start. Not drawn. */
typedef struct Marker {
    float x, y, z;
    df_u16 record;
} Marker;

typedef struct Block {
    Instance *inst;
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

/* Allocate the slots this block will fill. Caps are the tally, not 2048. */
int block_reserve(Block *block, int ninst, int nflat, int nmark);

Block *rdb_load(const df_u8 *rdb, df_u32 size, struct Bsa *arch,
                ArchEnt *cache, int *ncache);

#endif
