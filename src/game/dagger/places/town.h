#ifndef DF_TOWN_H
#define DF_TOWN_H

#include "engine/core/df.h"

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
/* The price of the house for sale in a block of the town: five times its largest model radius (0 when it does not read). */
int town_house_price(int region, int index, int block);
/* The name of the town last read. */
const char *town_name(void);
/*
 * The bytes of one of the town's blocks (an RMB, copied: they stay until the next call), for a game that reads
 * what is inside a building itself. 0 when the town or the block does not read, or the block is too big.
 */
const unsigned char *town_block_bytes(int region, int index, int block, unsigned *size);
/* One of them, by what a door's tag names; 0 for none. */
const TownBuilding *town_building(int region, int index, int block, int record);

/*
 * A building's identity everywhere: what the player owns (the house), what is kept of it (its chests and cupboards: container keys), who is let in. One number from the town's map place and the building's block and
 * record, so no two buildings of a town share one (the key was the record alone, and every block has record 0), and the same number is the bank's house, the entry's building and the things' place key.
 * The top two bits are set (a building's, as ground piles have another), the rest a hash of the four.
 */
#define BUILDING_KEY_KIND (3u << 30)
/* Where the editor marker `record` (199) of the town's block 0 stands in the town's own frame (the corner of the blocks the origin). 0 when there is none. */
int town_marker(int region, int index, int record, float *x, float *z);
df_u32 town_building_key(int region, int index, int block, int record);

#endif
