#ifndef GFALL_TRAVEL_H
#define GFALL_TRAVEL_H
/* Original travel rules and commands. App adapters supply map data/placement;
 * no GFALL player, save, or gameplay state crosses this boundary. */
#include "game/dagger/play/worldstate.h"
#include "engine/scene/scene.h"
typedef struct LocationInfo {
    char name[33];
    int region, index, px, py, type, known;
} LocationInfo;
typedef struct TravelHooks {
    int (*count)(void);
    int (*location)(int ordinal, LocationInfo *out);
    int (*go)(int region, int index);
    int (*asked)(void);
    const char *(*passage)(const Usable *usable);
    void (*here)(PlaceWhere *out, int *px, int *py);
    int (*climate)(int px, int py);
    int (*return_to)(const PlaceWhere *where, float x, float eye_y, float z, int yaw, int pitch);
} TravelHooks;
void travel_install(const TravelHooks *hooks);
void travel_commands(void);
int play_travel_asked(void);
int travel_passage(const Usable *usable);
int travel_location(int ordinal, LocationInfo *out);
int travel_count(void);
int travel_known(int ordinal);
void travel_discover(int ordinal);
void travel_capture(void);
int travel_climate(int px, int py);
int travel_go(int region, int index);
/* The Mages Guild's teleportation (guild_teleport, the map opened in mode 100): to a place on the map, at once, from anywhere, for nothing. */
int teleport(int region, int index);
int travel_at(const PlaceWhere *where);
int travel_return(const PlaceWhere *where, float x, float eye_y, float z, int yaw, int pitch);
/* A real journey uses original time/cost/options; goto changes place only. */
typedef struct JourneyPlan { int minutes, sea_pixels, hours, cost; } JourneyPlan;
void journey_plan(int sx, int sy, int ex, int ey, int reckless, int camp,
                     int ship, int transport, JourneyPlan *out);
int journey_transport(void);
int journey_warning(void);
int journey_begin(int region, int index, int reckless, int camp, int ship);
#endif
