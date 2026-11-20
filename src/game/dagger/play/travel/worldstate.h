#ifndef GFALL_WORLDSTATE_H
#define GFALL_WORLDSTATE_H
/* Saved place identity and explored geometry, independent of builder pointers.
 * World/back positions are whole engine units; stream origins are chunks. */
#include "engine/scene/automap.h"
#define KNOWN_BYTES 2048
#define WORLD_WIRE_BASE (96 + 8 + KNOWN_BYTES + 4)
enum { WHERE_DUNGEON, WHERE_WORLD, WHERE_INSIDE };
typedef struct PlaceWhere {
    int kind, located, region, index, block, record;
    int has_back, back_x, back_z, back_yaw, origin_x, origin_z;
} PlaceWhere;
/* The player's ship (transport_toggle_ship, inven.c 1582..1603): where they stood when they boarded (the place and the feet, the way they faced), valid while they are aboard. */
typedef struct ShipBack { int valid; PlaceWhere where; float x, y, z; int yaw, pitch; } ShipBack;
/* The bytes a save holds of it from schema 22: the flag, the place's twelve ints, the position and the two angles. */
#define SHIP_WIRE_BYTES (4 + 48 + 12 + 8)

typedef struct WorldState {
    PlaceWhere where, recall_where;
    int px, py;
    ShipBack ship;
    df_u8 known[KNOWN_BYTES];
    /* How many automap rows the player has seen (the rows live in the engine's automap and are streamed to and from a save). */
    int map_count;
} WorldState;
WorldState *world_where(void);
int world_valid(const WorldState *world);
int world_map_row_valid(const AutomapRow *row);
void world_capture(void);
void world_restore(void);
#endif
