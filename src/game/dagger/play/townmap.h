#ifndef DF_TOWNMAP_H
#define DF_TOWNMAP_H

#include "game/dagger/play/player.h"

/*
 * Classic's automap of a town (AMAP00I0.IMG, TOWN00I0.IMG's caption) in memory:
 * each block's own 64 x 64 map, north up, its buildings coloured by kind, the
 * player an arrow. Read docs/game.md (The wider world, Automaps) before editing.
 *
 * Plain C, drawing nothing and reaching no card: hostcheck drives it by clicks.
 */

enum {
    TOWNMAP_STAY,
    TOWNMAP_LEAVE,
    /* The travel map's key (V): the travel map instead. */
    TOWNMAP_TRAVEL
};

/*
 * Opened on the town at region and index, the player at (x, z) in its frame
 * (travel_town_here), facing yaw (Camera.yaw). 0 when a picture or the town
 * does not read.
 */
int townmap_open(int region, int index, float x, float z, float yaw);
void townmap_close(void);
void townmap_compose(void);
const unsigned char *townmap_pixels(void);
int townmap_changed(void);
void townmap_point(int x, int y);
int townmap_click(int x, int y);
/* The wheel: notches above 0 zoom in. */
void townmap_wheel(int notches);
int townmap_key(int vk);

/* What a test reads: the map pixels a screen pixel is (negative: screen pixels a map pixel is, halved); the cell at the view's middle; the line. */
int townmap_scale(void);
void townmap_centre(int *x, int *y);
const char *townmap_line(void);
/* The colour a map byte is drawn in, in this view mode; 0 for none. */
int townmap_colour(int byte, int mode);

#endif
