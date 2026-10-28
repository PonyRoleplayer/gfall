#ifndef DF_WORLDMAP_H
#define DF_WORLDMAP_H

#include "engine/core/df.h"

/*
 * Daggerfall's world map, for streaming the world (docs/streaming.md):
 * 1000 x 500 map pixels of 32768 units, each 8 x 8 RMB blocks. Pixel y
 * grows south: a pixel's world z is (499 - y) * 32768.
 *
 * Read once, the first time it is opened, and kept for the process
 * (mem_keep), about 2 MB: WOODS.WLD's height map (one byte a pixel),
 * CLIMATE.PAK unpacked (a climate id a pixel), and which location is on
 * each pixel, from every region's MAPTABLE (no pixel has two).
 */

#define WORLDMAP_W 1000
#define WORLDMAP_H 500
/* A map pixel's side in world units, and its RMB blocks on a side. */
#define WORLDMAP_PIXEL 32768
#define WORLDMAP_BLOCKS 8

/* 1 when the map is read (or already was). */
int worldmap_open(void);
/* WOODS.WLD's height, 0..255 (the sea is 1 or less); outside the map, the nearest edge's. */
int worldmap_height(int px, int py);
/*
 * A pixel's heights in thirds, from its WOODS.WLD record (the 5 x 5
 * block's inner 3 x 3), on the height map's scale: out[j * 3 + i] is
 * the third i east and j north of the pixel's south-west corner. Read
 * on demand and cached. 0 (and the pixel's height in all nine) when the
 * record does not read.
 */
int worldmap_detail(int px, int py, df_u8 *out);
/* CLIMATE.PAK's climate id (CLIMATE_*, 223..232); the sea's outside the map. */
int worldmap_climate(int px, int py);
/* 1 and the location's region and MAPPITEM index when one is on the pixel. */
int worldmap_location(int px, int py, int *region, int *index);
/*
 * The type of the location on a pixel, from its MAPTABLE record
 * (Daggerfall Unity's DFRegion.LocationTypes); -1 for none.
 */
enum {
    WORLDMAP_TOWN_CITY = 0,
    WORLDMAP_TOWN_HAMLET = 1,
    WORLDMAP_TOWN_VILLAGE = 2,
    WORLDMAP_HOME_FARMS = 3,
    WORLDMAP_DUNGEON_LABYRINTH = 4,
    WORLDMAP_RELIGION_TEMPLE = 5,
    WORLDMAP_TAVERN = 6,
    WORLDMAP_DUNGEON_KEEP = 7,
    WORLDMAP_HOME_WEALTHY = 8,
    WORLDMAP_RELIGION_CULT = 9,
    WORLDMAP_DUNGEON_RUIN = 10,
    WORLDMAP_HOME_POOR = 11,
    WORLDMAP_GRAVEYARD = 12,
    WORLDMAP_COVEN = 13,
    WORLDMAP_HOME_YOUR_SHIPS = 14
};
int worldmap_location_type(int px, int py);
/* The pixel a location (region, MAPPITEM index) is on. 0 when there is no such. */
int worldmap_pixel_of(int region, int index, int *px, int *py);
/* Locations read. */
int worldmap_locations(void);

#endif
