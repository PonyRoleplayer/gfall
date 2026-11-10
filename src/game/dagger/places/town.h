#ifndef DF_TOWN_H
#define DF_TOWN_H

/*
 * A town's buildings, from its blocks' files: what each is and where it stands
 * in the town's own frame, the one a house is loaded in (world_load_inside).
 * For asking the way, and for a shop's quality. Read docs/game.md (People).
 *
 * Reading them means reading every block of the town, so it is done when first
 * asked for and kept until another town is asked about.
 *
 * Plain C: hostcheck runs it.
 */

/* The largest town, Daggerfall, has 64 blocks of up to 32 buildings. */
#define TOWN_BUILDINGS 2048

typedef struct TownBuilding {
    /* Which block and which record in it: what a door's tag names. */
    short block;
    short record;
    short type;
    short quality;
    /* The sign's seed, the building record's first word. */
    unsigned short name_seed;
    int faction;
    float x;
    float z;
} TownBuilding;

/* The buildings of the town at region and index, and how many; 0 when it does not read. */
const TownBuilding *town_buildings(int region, int index, int *count);
/*
 * The town's automap: each block's 64 x 64 bytes from its RMB file, laid out
 * as the town's blocks are, north up, into out (TOWN_MAP_MOST bytes): 0 where
 * nothing stands, a building's type + 1 where one does, other values for its
 * props. *w and *h the map's size. 0 when the town does not read.
 */
#define TOWN_MAP_SIDE 512
#define TOWN_MAP_MOST (TOWN_MAP_SIDE * TOWN_MAP_SIDE)
int town_automap(int region, int index, unsigned char *out, int *w, int *h);
/* The name of the town last read. */
const char *town_name(void);
/*
 * The bytes of one of the town's blocks (an RMB, copied: they stay until the next call), for a game that reads
 * what is inside a building itself. 0 when the town or the block does not read, or the block is too big.
 */
const unsigned char *town_block_bytes(int region, int index, int block, unsigned *size);
/* One of them, by what a door's tag names; 0 for none. */
const TownBuilding *town_building(int region, int index, int block, int record);

#endif
