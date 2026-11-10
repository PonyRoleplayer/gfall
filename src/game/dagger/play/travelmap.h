#ifndef DF_TRAVELMAP_H
#define DF_TRAVELMAP_H

#include "game/dagger/play/journey.h"
#include "game/dagger/play/player.h"

/*
 * Classic's travel map in memory: the world (TRAV0I00.IMG), a click on a
 * province for its own map (FMAP0I##.IMG), a click on a place the player knows
 * of to travel there, and the journey's popup (TRAV0I04.IMG) to choose how.
 * Read docs/game.md (The wider world) before editing.
 *
 * The maps are under FMAP_PAL.COL; the popup and the scroll are under
 * ART_PAL.COL. One screen is one picture under one palette, so what is drawn in
 * the art palette is mapped to the map's nearest colours where it lies.
 *
 * Plain C, drawing nothing and reaching no card: hostcheck drives it by clicks.
 */

enum {
    TRAVELMAP_STAY,
    TRAVELMAP_LEAVE,
    /* The journey's countdown ended: travelmap_chosen says where and how. */
    TRAVELMAP_GO
};

/* Opened with the player standing on pixel (px, py) of region. 0 when a picture is missing. */
int travelmap_open(Player *p, int px, int py, int region);
/* Presentation-only entry for another game adapter: its own money, map
 * discovery and journey rules. The ordinary GFALL entry keeps its defaults. */
typedef struct TravelMapRules {
    unsigned (*gold)(void);
    int (*known)(int ordinal);
    void (*plan)(int sx, int sy, int ex, int ey, int reckless, int inns, int ship, Journey *out);
    void (*question)(const char *place, char *out, int capacity);
    /* Optional pre-journey confirmation, such as an illness warning. */
    const char *(*warning)(void);
} TravelMapRules;
int travelmap_open_rules(const TravelMapRules *rules, int px, int py, int region);
void travelmap_options(int *reckless, int *camp, int *ship);
void travelmap_close(void);
void travelmap_compose(void);
const unsigned char *travelmap_pixels(void);
/* The map's palette, 768 bytes. */
void travelmap_palette(unsigned char *rgb);
int travelmap_changed(void);
/* The pointer at x, y: what is under it is named. */
void travelmap_point(int x, int y);
int travelmap_click(int x, int y);
int travelmap_key(int vk);
/* Each frame: the countdown of a journey begun. */
int travelmap_step(float dt);
/* Where the journey goes, its sums, and whether cautiously. */
void travelmap_chosen(int *region, int *index, Journey *journey, int *cautious);

/* What a test reads: the province shown (-1 the world), the place chosen (-1 none), what the top line says. */
int travelmap_region(void);
/* How many places the last Find listed (FOUND_MOST at most). */
int travelmap_found(void);
int travelmap_place(void);
const char *travelmap_line(void);
/* A region's map's place on the world: its pixel at the panel's top left, and the panel pixels a map pixel is. 0 for a region with no map. */
int travelmap_region_origin(int region, int part, int *x, int *y, int *scale);

#endif
