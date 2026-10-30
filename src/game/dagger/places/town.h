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
    int faction;
    float x;
    float z;
} TownBuilding;

/* The buildings of the town at region and index, and how many; 0 when it does not read. */
const TownBuilding *town_buildings(int region, int index, int *count);
/* The name of the town last read. */
const char *town_name(void);
/* One of them, by what a door's tag names; 0 for none. */
const TownBuilding *town_building(int region, int index, int block, int record);

#endif
