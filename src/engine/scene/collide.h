#ifndef ENG_COLLIDE_H
#define ENG_COLLIDE_H

#include "engine/scene/scene.h"

/*
 * Collision against a place's models. Read docs/collision.md before
 * editing.
 *
 * The body is an upright ellipsoid, radius rx across and ry tall (half
 * its height), swept along its motion: the method of Kasper Fauerby,
 * "Improved Collision detection and Response" (2003). Scaling the world
 * by 1 / radius turns the ellipsoid into a unit sphere, and a swept unit
 * sphere against a triangle is a plane test, then three vertices and
 * three edges as quadratics. It finds the first contact along the whole
 * motion, so no speed and no frame time lets the body pass through a
 * wall or a floor, and the motion left over slides along what it hit.
 *
 * Nothing is copied at load but a grid of model boxes. Each frame
 * collide_gather takes the triangles near the motion, from the few
 * models whose box it meets, into a small working set in ellipsoid
 * space; the sweeps run on that set. Triangles of every model count,
 * from both sides; flats (sprites) never do.
 *
 * Plain C with no Glide, so hostcheck runs it.
 */

/* Triangles a gather can hold; more are dropped, with a warning. */
#define COLLIDE_SET 1024

typedef struct CollideHit {
    /* 1 when anything was hit. */
    int hit;
    /*
     * The steepest-up world normal among the contacts, its y: above
     * about 0.7 the body stood on a floor; below about -0.7 it struck
     * a ceiling.
     */
    float floor_y;
    float ceiling_y;
} CollideHit;

/*
 * The model grid for a scene. Rebuilt by collide_use when the scene, its
 * model count, or the heap it lives on changes. 0 when out of memory.
 */
int collide_use(const Scene *scene);
/* The scene's lowest and highest point, once built. */
void collide_extent(float *miny, float *maxy);

/* The body's radii: across (x and z) and up (half its height). */
void collide_set_shape(float rx, float ry);

/*
 * Gather the triangles that meet a world box into the working set.
 * Every query below reads that set: gather once round everything a
 * frame's motion may touch. Returns how many it holds.
 */
int collide_gather(float minx, float miny, float minz,
                   float maxx, float maxy, float maxz);

/*
 * Move the body's centre (world) by v, sliding along what it meets.
 * hit may be 0. Returns how far it moved, as a fraction of |v|.
 */
float collide_slide(float *pos, float vx, float vy, float vz, CollideHit *hit);

/* 1 when the body at pos (world centre) cuts into a gathered triangle. */
int collide_overlap(const float *pos);

/* A floor's normal rises at least this steeply: 45 degrees. */
#define COLLIDE_FLOOR 0.7f

/*
 * The highest floor (a triangle whose normal rises at least COLLIDE_FLOOR)
 * under x, z between y_top and y_bottom, in the working set. 1 and its
 * height in *y when there is one.
 */
int collide_floor(float x, float z, float y_top, float y_bottom, float *y);

/* Triangles in the last gather, and whether it overflowed. */
void collide_stats(int *gathered, int *dropped);

#endif
