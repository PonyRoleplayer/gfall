#ifndef OGFALL_WORLDSTATE_H
#define OGFALL_WORLDSTATE_H
/* Saved place identity and explored geometry, independent of builder pointers.
 * World/back positions are whole engine units; stream origins are chunks. */
#include "engine/scene/automap.h"
#define OG_KNOWN_BYTES 2048
#define OG_MAP_ROWS 8192
#define OG_WORLD_WIRE_BASE (96 + 8 + OG_KNOWN_BYTES + 4)
enum { OG_PLACE_DUNGEON, OG_PLACE_WORLD, OG_PLACE_INSIDE };
typedef struct OgWhere {
    int kind, located, region, index, block, record;
    int has_back, back_x, back_z, back_yaw, origin_x, origin_z;
} OgWhere;
typedef struct OgWorldState {
    OgWhere where, recall_where;
    int px, py;
    df_u8 known[OG_KNOWN_BYTES];
    int map_count;
    AutomapRow map[OG_MAP_ROWS];
} OgWorldState;
OgWorldState *og_world_state(void);
int og_world_valid(const OgWorldState *world);
void og_world_capture(void);
void og_world_restore(void);
#endif
