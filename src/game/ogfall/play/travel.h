#ifndef OGFALL_TRAVEL_H
#define OGFALL_TRAVEL_H
/* Original travel rules and commands. App adapters supply map data/placement;
 * no GFALL player, save, or gameplay state crosses this boundary. */
#include "game/ogfall/play/worldstate.h"
#include "engine/scene/scene.h"
typedef struct OgLocation {
    char name[33];
    int region, index, px, py, type, known;
} OgLocation;
typedef struct OgTravelHooks {
    int (*count)(void);
    int (*location)(int ordinal, OgLocation *out);
    int (*go)(int region, int index);
    int (*asked)(void);
    const char *(*passage)(const Usable *usable);
    void (*here)(OgWhere *out, int *px, int *py);
    int (*climate)(int px, int py);
    int (*return_to)(const OgWhere *where, float x, float eye_y, float z, int yaw, int pitch);
} OgTravelHooks;
void og_travel_install(const OgTravelHooks *hooks);
void og_travel_commands(void);
int og_travel_asked(void);
int og_travel_passage(const Usable *usable);
int og_travel_location(int ordinal, OgLocation *out);
int og_travel_count(void);
int og_travel_known(int ordinal);
void og_travel_discover(int ordinal);
void og_travel_capture(void);
int og_travel_climate(int px, int py);
int og_travel_go(int region, int index);
int og_travel_at(const OgWhere *where);
int og_travel_return(const OgWhere *where, float x, float eye_y, float z, int yaw, int pitch);
/* A real journey uses original time/cost/options; goto changes place only. */
typedef struct OgJourney { int minutes, sea_pixels, hours, cost; } OgJourney;
void og_journey_plan(int sx, int sy, int ex, int ey, int reckless, int camp,
                     int ship, int transport, OgJourney *out);
int og_journey_transport(void);
int og_journey_warning(void);
int og_journey_begin(int region, int index, int reckless, int camp, int ship);
#endif
