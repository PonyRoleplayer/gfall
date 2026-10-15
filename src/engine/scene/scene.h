#ifndef ENG_SCENE_H
#define ENG_SCENE_H

#include "engine/assets/mesh.h"
#include "engine/gfx/camera.h"

/*
 * A placed mesh. m[] takes a mesh point into the world:
 *   wx = m[0]*lx + m[1]*ly + m[2]*lz + x
 *   wy = m[3]*lx + m[4]*ly + m[5]*lz + y
 *   wz = m[6]*lx + m[7]*ly + m[8]*lz + z
 * The matrix is right-hand X, then Y, then Z. A positive Y angle takes
 * local +X toward -Z. World +Y is up: a floor is one constant Y.
 */

typedef struct Instance {
    Mesh *mesh;
    float x, y, z;
    float m[9];
} Instance;

/*
 * A camera-facing quad, centered on x,y,z. w and h are the full size in
 * world units. slot is 0xffff until a loader assigns a texture.
 * file and image are loader keys. The rasterizer uses slot.
 */

typedef struct Flat {
    float x, y, z;
    float w, h;
    df_u16 file;
    df_u16 image;
    df_u16 slot;
} Flat;

/*
 * What a frame can see. models[] is drawn first, then sprites[].
 * Storage order is submission order.
 */

typedef struct Scene {
    const Instance *models;
    int nmodels;
    const Flat *sprites;
    int nsprites;
} Scene;

void instance_set_rotation(Instance *in, float rx, float ry, float rz);
/* RDB placement. Negates the file angles and turns Y, then X, then Z. */
void instance_set_dungeon_rotation(Instance *in, float rx, float ry, float rz);
void instance_point(const Instance *in, float lx, float ly, float lz,
                    float *x, float *y, float *z);

/*
 * Visibility. Writes storage indices into out and returns how many.
 * Nothing is culled yet: every entity is visible, in the order it was
 * stored. cam is part of the call so a later room can test the frustum
 * here without changing the draw loop. A count above cap is truncated,
 * so pass a cap of at least nmodels or nsprites.
 */

int scene_visible_models(const Scene *scene, const Camera *cam, int *out, int cap);
int scene_visible_sprites(const Scene *scene, const Camera *cam, int *out, int cap);

#endif
