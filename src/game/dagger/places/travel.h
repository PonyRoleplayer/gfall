#ifndef DF_TRAVEL_H
#define DF_TRAVEL_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"
#include "engine/scene/interact.h"
#include "engine/scene/walker.h"

/*
 * Going from place to place (docs/game.md, Places and travel). One place is
 * open at a time, on a temp heap of its own: a dungeon streamed by block
 * (dunstream.c), the world streamed round the camera (wild.c), or a building's
 * inside loaded whole (world.c). Every move between them is one passage
 * (doors.h), asked for by the use key and carried out by the program between
 * two frames, under its "Loading...":
 *
 *   travel_use          the use key met a passage: is there somewhere to go?
 *   travel_asked        1 from then until the place has changed
 *   travel_leave        the old place's state kept, its heap gone
 *   travel_open         the new place on a new heap; the program's gfx_prepare
 *   travel_land         where to stand, what is round it loaded
 *
 * The way back out of a door is remembered as a world point: the step outside
 * the door taken, with the way it faces. Going in from the stream keeps it, so
 * going out lands at the same door; a save keeps it too.
 *
 * Plain C, no Glide: hostcheck walks out of the hold and back. The far clip
 * and the budget are the caller's, as gfx and the frame's sums are not built
 * for the host. LEVELFALL shares the landing sums.
 */

/* The kinds of place. */
enum {
    TRAVEL_DUNGEON,
    TRAVEL_WORLD,
    TRAVEL_INSIDE
};

/* A place, as a save or a door names it. */
typedef struct TravelWhere {
    int kind;
    /* A dungeon's numbers (dungeon_load_at), or a town's for an inside. located 0: Privateer's Hold, by its name. */
    int located;
    int region;
    int index;
    /* An inside: the building's block in the town, and its record there. */
    int block;
    int record;
    /* The step outside the door that led here, in world units, and the way it faces; has_back 0: none yet. */
    int has_back;
    double back_x;
    double back_z;
    float back_yaw;
} TravelWhere;

/* Where to stand: the eye, and the way it looks. */
typedef struct TravelPose {
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
} TravelPose;

/* Out of a door three body radii, into a house a radius and 0.4 m: Daggerfall Unity's. */
#define TRAVEL_LAND_OUT (3.0f * WALK_RADIUS)
#define TRAVEL_LAND_INTO (WALK_RADIUS + 16.0f)

/* The next place is the new game's: Privateer's Hold, at its start. */
void travel_start_game(void);
/* The next place is a save's: where, the feet as saved and the stream's origin then (chunks), the way it looked. */
void travel_from_save(const TravelWhere *where, const float *feet, const int *origin, float yaw, float pitch);
/*
 * The use key met passage u. 0 when it leads somewhere, travel_asked now 1;
 * otherwise what to tell the player.
 */
const char *travel_use(const Usable *u);
int travel_asked(void);
/* 1 when the next place is the world: its far clip is the program's to set before travel_open. */
int travel_next_is_world(void);
/* What the place left is kept (keep.h), the sound stopped, its pictures and its heap gone. Safe with none open. */
void travel_leave(void);
/*
 * The next place, on a new temp heap, over level, for gfx_prepare; radius as
 * travel_radius, for a stream of the world. 0 when it cannot be read and there
 * is no way back to the street either, with travel_error saying why. A building
 * that does not read puts the player back outside its door, and travel_said says so.
 */
int travel_open(Level *level, int radius);
const char *travel_error(void);
/* Something to say on arriving, once; 0 for nothing. */
const char *travel_said(void);
/*
 * After gfx_prepare. Where to stand in the place just opened: a body with its
 * eye this far above its feet. What is round it is loaded, sharp for the first
 * frame, and cam is put there.
 */
void travel_land(const Level *level, Camera *cam, float eye, TravelPose *out);
/*
 * Once a frame of play: what is round cam in and out, within budget_ms, and
 * the people walking, dt seconds. *shift_x and *shift_z are what a stream's
 * origin move took off everything in the scene and off cam: a body the
 * caller keeps is moved with them.
 */
void travel_update(Camera *cam, int radius, float budget_ms, float dt, float *shift_x, float *shift_z);
/* 1 when the world's season turned or its meshes filled up: travel_reload under a hold, then a gfx_prime. */
int travel_must_reload(void);
void travel_reload(Camera *cam);
/* What the place now open has changed, into the keep table: before a save is written. */
void travel_keep_now(void);
/* The place now open, and its stream's origin in chunks (0, 0 for one with no stream). */
void travel_here(TravelWhere *where, int *origin);
/* The radius of chunks a far clip needs: 4096 a ring, at least one. */
int travel_radius(float far);
/* The nearest usable passage of what (DOOR_*) to x, z in the scene; 0 when none is loaded. */
const Usable *travel_nearest_passage(const Scene *scene, int what, float x, float z);
/*
 * A door of the streamed world in its town's own frame, where a house is placed
 * against it (world_load_inside): into *in, with the town's numbers. 0 for a
 * door in no town.
 */
int travel_door_in_town(const Usable *door, Usable *in, int *region, int *index);

#endif
