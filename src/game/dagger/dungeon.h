#ifndef DF_DUNGEON_H
#define DF_DUNGEON_H

#define DUN_NAME_MAX 13
#define DUN_MAX_BLOCKS 64

typedef struct DunBlock {
    char name[DUN_NAME_MAX];
    int x;
    int z;
    int start;
} DunBlock;

typedef struct Dungeon {
    char title[32];
    DunBlock block[DUN_MAX_BLOCKS];
    int nblocks;
    /* LocationNum in the dungeon record, and the climate id of its map pixel. */
    int location_id;
    int climate;
} Dungeon;

/* Privateer's Hold from MAPDITEM.017. */
int dungeon_load(Dungeon *out);
const char *dungeon_error(void);

#endif
