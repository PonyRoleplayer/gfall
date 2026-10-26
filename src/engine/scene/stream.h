#ifndef ENG_STREAM_H
#define ENG_STREAM_H

#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

/*
 * A world loaded around the camera, chunk by chunk. Read
 * docs/streaming.md before editing.
 *
 * The world is a grid of STREAM_CHUNK squares, named by whole numbers
 * (cx east, cz north). The chunks within the radius of the camera's
 * chunk are kept loaded, nearest first; one is dropped only when it is
 * two rings out past the radius, so a camera on a border does not load
 * and drop the same chunks over and over.
 *
 * Each loaded chunk sits in a slot: a fixed range of the scene's models,
 * bounds and one span, set up once by stream_open. An empty slot's span
 * has a count of 0, so the cull skips it, and its models' boxes are
 * negative, so the collision grid does too. Indices never move.
 *
 * The scene is drawn in a local frame whose origin is a chunk's corner
 * (stream_origin). When the camera strays more than STREAM_REBASE chunks
 * from it, the origin moves to the camera's chunk and the camera and
 * everything loaded move back by that many whole chunks: an exact shift,
 * so a float keeps its precision anywhere on a world of 32 million units.
 *
 * What a chunk holds is the game's: its fill is called with the chunk
 * and the slot's models to write, in the chunk's own frame (0..
 * STREAM_CHUNK east and north of its corner). It may take several calls:
 * the stream calls it again, a step at a time, under the frame's budget.
 *
 * Plain C with no Glide, so hostcheck runs it.
 */

#define STREAM_CHUNK 4096
/* Chunks a side round the camera's, kept loaded: 1 is 3 x 3, 4 is 9 x 9. */
#define STREAM_RADIUS_MAX 4
/* Dropped past this many rings beyond the radius. */
#define STREAM_KEEP 1
/* The origin moves when the camera is further than this from it, in chunks (two map pixels). */
#define STREAM_REBASE 16

typedef struct StreamOut {
    /* The slot's models to write, and how many there is room for. */
    Instance *models;
    int cap;
    /* How many are written so far; the fill keeps it. */
    int count;
    /* 0 on the first call for a chunk; the fill keeps it between its steps. */
    int step;
} StreamOut;

/* 1 when the chunk is done, 0 when there is more to do on a later call. */
typedef int (*StreamFill)(void *user, int cx, int cz, StreamOut *out);

typedef struct StreamSource {
    StreamFill fill;
    void *user;
    /* Models a chunk may hold: every slot has room for this many. */
    int models;
} StreamSource;

/*
 * Set scene up for a stream round chunk (cx, cz), which becomes the
 * origin, on the current heap: its models, bounds and spans are the
 * slots'. radius 1..STREAM_RADIUS_MAX. 0 when out of memory.
 */
int stream_open(Scene *scene, const StreamSource *src, int radius, int cx, int cz);
void stream_close(void);
int stream_active(void);
/* Chunks a side round the camera's (1..STREAM_RADIUS_MAX); takes effect on the next update. */
void stream_set_radius(int radius);
int stream_radius(void);

/*
 * Once a frame, before the frame draws: move the origin if the camera
 * strayed (the camera moves with it), drop what is out of range, and
 * load what is missing, nearest first, for at most budget_ms of the
 * cycle counter (one step when it is not calibrated yet).
 */
void stream_update(Camera *cam, float budget_ms);

/* The chunk under a camera, in world chunks. */
void stream_chunk_at(const Camera *cam, int *cx, int *cz);
/* The origin chunk: local (0, 0) is its south-west corner. */
void stream_origin(int *cx, int *cz);
/* A point's world position in units, from a local one: x east, z north. */
void stream_world(float x, float z, double *wx, double *wz);
/* Chunks loaded, waiting, being filled now (1 or 0), and origin moves so far. */
void stream_stats(int *loaded, int *waiting, int *filling, int *shifts);

#endif
