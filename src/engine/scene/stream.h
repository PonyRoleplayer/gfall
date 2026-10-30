#ifndef ENG_STREAM_H
#define ENG_STREAM_H

#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"
#include "engine/scene/entity.h"

/*
 * A world loaded around the camera, chunk by chunk. Read
 * docs/streaming.md before editing.
 *
 * The world is a grid of STREAM_CHUNK squares, named by whole numbers
 * (cx east, cz north). The chunks within the radius of the camera's
 * chunk are kept loaded, nearest first; one is dropped only when it is
 * STREAM_KEEP rings out past the radius, so a camera on a border does
 * not load and drop the same chunks over and over.
 *
 * Each loaded chunk sits in a slot: a fixed range of every array of the
 * scene (models and their bounds, sprites, lights, emitters, usables,
 * movers) and two spans, set up once by stream_open. The first span is
 * the chunk's ground, near or far: within STREAM_NEAR rings of the
 * camera's chunk it is the ground's near models (tiles), beyond it the
 * far ones (one quad and its baked picture); the second is the rest of
 * the chunk, its sprites with it (Scene.sprite_spans). Empty, a slot's
 * spans are 0 long, its models have no mesh and negative boxes, its
 * sprites no size, its lights no radius, its emitters no clip, its
 * usables no reach: every pass over the scene skips them. Indices never
 * move.
 *
 * The scene is drawn in a local frame whose origin is a chunk's corner
 * (stream_origin). When the camera strays more than STREAM_REBASE chunks
 * from it, the origin moves to the camera's chunk, and the camera and
 * everything loaded move back by that many whole chunks (and the
 * source's shift, for what it keeps of its own: light decals): an exact
 * shift, so a float keeps its precision anywhere on a world of 32
 * million units.
 *
 * When a chunk's fill is done, its movers' and entities' kept state is
 * put back (keep_restore, docs/interact.md) before it is in the scene;
 * before it is dropped it is saved (keep_save), in the place's frame.
 * stream_keep_now saves the loaded ones too, for a save game.
 *
 * What a chunk holds is the game's: its fill writes the slot's arrays,
 * in the chunk's own frame (0..STREAM_CHUNK east and north of its
 * corner), the ground's near models first, then its far ones, then the
 * rest; a usable's or a mover's model and a usable's sprite are counted
 * within the chunk. It may take several calls, a step at a time, under
 * the frame's budget. The chunk's own pointer (StreamOut.data) comes
 * back to the source's drop when the chunk goes.
 *
 * Plain C with no Glide, so hostcheck runs it.
 */

#define STREAM_CHUNK 4096
/* Chunks a side round the camera's, kept loaded: 1 is 3 x 3, 4 is 9 x 9. */
#define STREAM_RADIUS_MAX 4
/* Dropped past this many rings beyond the radius. */
#define STREAM_KEEP 1
/* Ground within this many rings of the camera's chunk draws near (its tiles). */
#define STREAM_NEAR 1
/* The origin moves when the camera is further than this from it, in chunks (two map pixels). */
#define STREAM_REBASE 16

typedef struct StreamCaps {
    int models;
    int sprites;
    int lights;
    int emitters;
    int usables;
    int movers;
    /*
     * 0 (the world's): the stream leaves Scene.entities and Scene.water
     * to the game (the townspeople's own array). Above 0, the stream owns
     * them, a range a slot as for the rest: a dungeon's creatures and
     * water, block by block.
     */
    int entities;
    int water;
} StreamCaps;

typedef struct StreamOut {
    /* The slot's arrays to write, cap of each long (StreamSource.caps). */
    Instance *models;
    Flat *sprites;
    Light *lights;
    Emitter *emitters;
    Usable *usables;
    Mover *movers;
    Entity *entities;
    Water *water;
    /*
     * What is written so far; the fill keeps them between its steps. Not
     * "near" and "far": windows.h defines both as empty macros.
     */
    int near_n;
    int far_n;
    int models_n;
    int sprites_n;
    int lights_n;
    int emitters_n;
    int usables_n;
    int movers_n;
    int entities_n;
    int water_n;
    /* 0 on the first call for a chunk; the fill keeps it between its steps. */
    int step;
    /* The chunk's own state, the source's; handed back to its drop. */
    void *data;
} StreamOut;

typedef struct StreamSource {
    /* 1 when the chunk is done, 0 when there is more to do on a later call. */
    int (*fill)(void *user, int cx, int cz, StreamOut *out);
    /* The chunk goes (dropped, or abandoned half filled): its data. May be 0. */
    void (*drop)(void *user, int cx, int cz, void *data);
    /* The origin moved: what the source keeps in world units moves by -dx, -dz. May be 0. */
    void (*shift)(void *user, float dx, float dz);
    /* A chunk finished (it is in the scene) or went: index is its slot. May be 0. */
    void (*changed)(void *user, int slot, int loaded);
    void *user;
    StreamCaps caps;
} StreamSource;

/*
 * Set scene up for a stream round chunk (cx, cz), which becomes the
 * origin, on the current heap: its arrays are the slots'. radius
 * 1..STREAM_RADIUS_MAX. 0 when out of memory. The world's: chunks of
 * STREAM_CHUNK, slots for STREAM_RADIUS_MAX.
 */
int stream_open(Scene *scene, const StreamSource *src, int radius, int cx, int cz);
/*
 * The same with chunks of chunk units a side (a dungeon's 2048: one
 * block a chunk), and slots for radius alone: (2 (radius + STREAM_KEEP)
 * + 1)^2, 25 at radius 1, so a dungeon block's large caps are not kept
 * 121 times. stream_set_radius cannot go past it. docs/streaming.md,
 * Dungeons.
 */
int stream_open_sized(Scene *scene, const StreamSource *src, int radius, int cx, int cz, int chunk);
/* The stream's chunk side in units. */
int stream_chunk_size(void);
/* The slots this stream has (STREAM_SLOTS at most): a slot's ranges are this many end to end. */
int stream_slots(void);
/* Every chunk goes (the source's drop for each), and the stream stops. */
void stream_close(void);
int stream_active(void);
/* Every chunk goes, to be loaded again by the updates to come, nearest first (the season turned). */
void stream_reload(void);
/*
 * Every loaded chunk's changes into the keep table now, as if it were
 * being dropped, and nothing is dropped: a chunk writes its changes only
 * when it goes, so a save game calls this before it writes the table.
 * Nothing with no stream open.
 */
void stream_keep_now(void);
/* Chunks within rings of the camera's (at most the radius) not loaded yet, as of the last update. */
int stream_missing(int rings);
/* Chunks a side round the camera's (1..STREAM_RADIUS_MAX); takes effect on the next update. */
void stream_set_radius(int radius);
int stream_radius(void);

/*
 * Once a frame, before the frame draws: move the origin if the camera
 * strayed (the camera moves with it), drop what is out of range, choose
 * each chunk's ground, and load what is missing, nearest first, for at
 * most budget_ms of the cycle counter (one step when it is not
 * calibrated yet).
 */
void stream_update(Camera *cam, float budget_ms);
/*
 * Times the camera's own chunk was not loaded when it got there (it ran
 * past the budget's filling) and was loaded at once, a wait; since open.
 */
int stream_outran(void);

/* The chunk under a camera, in world chunks. */
void stream_chunk_at(const Camera *cam, int *cx, int *cz);
/* The origin chunk: local (0, 0) is its south-west corner. */
void stream_origin(int *cx, int *cz);
/* A point's world position in units, from a local one: x east, z north. */
void stream_world(float x, float z, double *wx, double *wz);
/* And back: a world position in the local frame. */
void stream_local(double wx, double wz, float *x, float *z);
/* The source's data (StreamOut.data) of chunk (cx, cz) when it is loaded, else 0. */
void *stream_chunk_data(int cx, int cz);
/* The slots: how many, the slot a scene index of each kind is in, the chunk a slot holds (0 if empty). */
#define STREAM_SLOTS ((2 * (STREAM_RADIUS_MAX + STREAM_KEEP) + 1) * (2 * (STREAM_RADIUS_MAX + STREAM_KEEP) + 1))
int stream_slot_chunk(int slot, int *cx, int *cz);
/* Slot's ranges in the scene's models and lights: first and count, the ground's near and far, and the rest. */
void stream_slot_models(int slot, int *first, int *near_n, int *far_n, int *rest);
void stream_slot_lights(int slot, int *first, int *count);
/*
 * A slot's entities, writable (the game's motors), when the stream owns
 * them (StreamCaps.entities): the first, and in *first its index in the
 * scene's and in *n how many; 0 when it does not or the slot is empty.
 */
Entity *stream_slot_entities(int slot, int *first, int *n);
/*
 * The longest single fill call since the last ask, in ms of the cycle
 * counter, and its step (StreamOut.step before the call); then forgets
 * them. The budget is checked between calls, so this is what a frame
 * can overrun it by.
 */
void stream_worst(float *ms, int *step);
/* Chunks loaded, waiting, being filled now (1 or 0), and origin moves so far. */
void stream_stats(int *loaded, int *waiting, int *filling, int *shifts);

#endif
