#ifndef ENG_INTERACT_H
#define ENG_INTERACT_H

#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

/*
 * Using things, and the models that move. Read docs/interact.md before
 * editing.
 *
 * The player uses what the middle of the view rests on: a ray from the
 * eye along the camera's forward, as Daggerfall Unity casts it. The ray
 * stops at the first model triangle it meets, so nothing is used
 * through a wall; a usable's box nearer than that, within the usable's
 * own reach, is what it took. A door that is a face of a house (a
 * passage) has a box a few units thick about that face, so the ray
 * enters it before the wall behind.
 *
 * Using a USE_MOVER starts its mover (refused while it is locked, with
 * a word on the screen). Every other kind goes to the game's handler,
 * which knows what a passage leads to or who a person is. Movers then
 * ease on, once a frame, writing their model's pose; a chain starts
 * together, and nothing in a chain starts again while any of it moves.
 *
 * The cast tests every model box against the ray, then the triangles
 * of the few it crosses: a use costs one such pass, and the aim the
 * debug view shows one a frame, only while that view is on.
 *
 * Plain C with no Glide, so hostcheck runs it.
 */

/* What the ray met. usable is -1 when it met none within reach. */
typedef struct UseHit {
    int usable;
    /* From the eye to where it struck: the usable's box, or the wall. */
    float dist;
    /* The model that stopped it, -1 for none within the ray's length. */
    int model;
    float wall;
} UseHit;

/*
 * The first usable along the ray from x, y, z toward dx, dy, dz (unit
 * length) within len, that no triangle comes before. 1 when one was met.
 */
int interact_cast(const Scene *scene, float x, float y, float z,
                  float dx, float dy, float dz, float len, UseHit *hit);
/* The cast from the camera's eye along its forward, as far as the longest reach. */
int interact_aim(const Scene *scene, const Camera *cam, UseHit *hit);

/*
 * Use usable i: a mover starts (a locked one says so instead); the rest
 * go to the handler. 1 when something happened.
 */
int interact_use(const Scene *scene, int i);
/*
 * The game's part of a use: called for every kind but USE_MOVER, with
 * the scene's usable and its index. Returns 1 when it did something.
 * 0 clears it; with none, a use of those says what it is and no more.
 */
typedef int (*UseHandler)(const Scene *scene, const Usable *u, int index);
void interact_set_handler(UseHandler fn);

/*
 * Start mover i toward the end it is not at, and its chain, sounding
 * each from its model. 0 while any of it moves, or when it has nowhere
 * to go (a once mover at its end).
 */
int mover_start(const Scene *scene, int i);
/* 1 while mover i, or one it starts, is moving. */
int mover_busy(const Scene *scene, int i);
/* Move every mover on by secs and write its model's pose. */
void mover_step(const Scene *scene, float secs);
/*
 * A teleport a use or a chain reached since the last call (Mover.teleport):
 * 1 and where the player's eye goes, once; 0 for none. draw_frame takes
 * it for the console's camera, and the walker starts again from there.
 */
int interact_teleport(float *x, float *y, float *z, float *yaw);
/*
 * The world box mover i's model sweeps from start to end, for the cull
 * and the collision grid, which read Scene.bounds: the game writes it
 * there once, before the first frame.
 */
int mover_sweep(const Mover *m, Bound *out);
/* Record each mover's start pose from its model. Once, when the place is built. */
void mover_init(Mover *movers, int n);

/*
 * One frame, from draw_frame: when used is 1 (the use key went down),
 * use what the camera aims at; then move the movers on by secs. Takes
 * the aim again for the debug view while it is on.
 */
void interact_frame(const Scene *scene, const Camera *cam, float secs, int used);
/* The usable last aimed at by the view, -1 for none. */
int interact_aimed(void);
/* The scene of the last frame, for the console; 0 when that place is gone. */
const Scene *interact_scene(void);

/*
 * A line at the middle of the screen ("This door is locked."), for
 * INTERACT_SAY seconds, the last half fading. The text is copied.
 */
#define INTERACT_SAY 2.5f
void interact_say(const char *text);
/* The line now and its alpha 0..255; 0 when there is none. */
const char *interact_saying(int *alpha);

/* 0 lets a locked mover be used as if open. Default 1. */
void interact_set_locks(int on);
int interact_locks(void);
/*
 * The debug picture (useview): each usable within this many world units
 * of the camera, its box and its word, the aimed one bright. 0 (default)
 * is off.
 */
#define INTERACT_VIEW_MAX 4096
/* 1 to INTERACT_VIEW_SMALL - 1 turns it on at INTERACT_VIEW_ON. */
#define INTERACT_VIEW_SMALL 64
#define INTERACT_VIEW_ON 512
void interact_set_view(int units);
int interact_view(void);
/*
 * The debug picture of triggers (trigview): within this many world
 * units, every usable that starts a mover, every passage, and every
 * mover only a chain starts; how each is set off (by hand, walked
 * into: its touch volume, red while the body is in it), what it does
 * (a teleport with a line to its target, a door and its lock, a turn,
 * a slide, a relay; the mover it starts next), and the game's own
 * numbers for it (Usable.what, .tag). 0 (default) is off; 1 to 63 is 512.
 */
void interact_set_trigger_view(int units);
int interact_trigger_view(void);
/* The touch usable the body is in now (Usable.touch), -1 for none. */
int interact_touching(void);

#endif
