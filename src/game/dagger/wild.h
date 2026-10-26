#ifndef DF_WILD_H
#define DF_WILD_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"

/*
 * Daggerfall's world, streamed round the camera (docs/streaming.md),
 * flat. Every chunk (an RMB block's square, 4096 units) is its map
 * pixel's ground: the climate's grass, the sea's water off the coast;
 * and, in a location's pixel, the block whose corner falls in it: its
 * ground, buildings, flats, lamps, sounds and doors. Far chunks draw a
 * few quads of baked ground instead. The sky and the weather's odds are
 * the camera's pixel's climate's. One stream at a time, on the current
 * heap.
 */

/*
 * Open the stream on map pixel (px, py), radius chunks round the
 * camera's (engine/scene/stream.h), and fill level for gfx_prepare and
 * gfx_frame. The camera's start: the pixel's middle, at eye height. 0
 * when the world map or the pictures do not read.
 */
int wild_open(int px, int py, int radius, Level *level, float *x, float *y, float *z);
/* The same, the camera's start at world point (wx, wz), in units east and north (a door's step). */
int wild_open_at(double wx, double wz, int radius, Level *level, float *x, float *y, float *z);
void wild_close(void);
/*
 * Updates until every chunk within the radius is in and its lamps cut:
 * before a first frame, under "Loading...", so the walk does not start
 * with seconds of loading a few milliseconds a frame.
 */
void wild_fill(Camera *cam);
/*
 * The town whose pixel holds world point (wx, wz): its region and index
 * in the list, and its own frame's origin in world units (the corner
 * of its block 0, 0: exterior_load_at's frame). 0 for the wilderness.
 */
int wild_location_at(double wx, double wz, int *region, int *index, double *ox, double *oz);
/* Once a frame, before gfx_frame: load and drop chunks round cam, within budget_ms. */
void wild_update(Camera *cam, float budget_ms);
/*
 * 1 when the calendar's season is no longer the stream's: the pictures
 * change. wild_reseason loads the chunks again in the new season's, the
 * whole radius before it returns (a hitch: hold the frame first).
 */
int wild_season_turned(void);
void wild_reseason(Camera *cam);
/* The ground's y at world point (wx, wz): the land, or a town's level. Needs the stream open. */
float wild_ground(double wx, double wz);
/* The map pixel under the camera. */
void wild_here(const Camera *cam, int *px, int *py);

#endif
