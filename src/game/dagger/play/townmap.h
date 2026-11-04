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
/* A left-button drag: the map follows the pointer, classic's pixels. A held arrow button acts again and again. */
void townmap_drag(int dx, int dy);
int townmap_repeats(int x, int y);
int townmap_on_map(int x, int y);
/* Where the player's arrow goes, in the engine's screen pixels from the lower left, and its way, clockwise from up. 0 off the map. */
int townmap_marker(float *sx, float *sy, float *angle);
/* The wheel: notches above 0 zoom in. */
void townmap_wheel(int notches);
int townmap_key(int vk);

/*
 * A building someone gave the way to in talk, marked on the town's map with
 * a ring and named on the map's line, until another is pointed out (DFU's
 * talk discovers the building and shows it on the automap). Not saved.
 */
void townmap_mark(int region, int index, float x, float z, int type);
/* 1 and where, in the town's map pixels, when a building in this town is marked. */
int townmap_marked(int region, int index, int *mx, int *my);

/* What a test reads: the map pixels a screen pixel is (negative: screen pixels a map pixel is, halved); the cell at the view's middle; the line. */
int townmap_scale(void);
void townmap_centre(int *x, int *y);
const char *townmap_line(void);
/* The colour a map byte is drawn in, in this view mode; 0 for none. */
int townmap_colour(int byte, int mode);

#endif
