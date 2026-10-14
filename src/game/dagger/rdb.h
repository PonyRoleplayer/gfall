#ifndef DF_RDB_H
#define DF_RDB_H

#include "engine/scene/scene.h"

/*
 * One RDB block after the records have been turned into scene entities.
 * neditor counts editor markers. They are not entities.
 */

typedef struct Block {
    Instance *inst;
    int ninst;
    int ntris;
    Flat *flat;
    int nflat;
    int neditor;
} Block;

struct Bsa;

Block *rdb_load(const df_u8 *rdb, df_u32 size, struct Bsa *arch);

#endif
