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

/*
 * World box of one instance. Filled once the placement is final.
 * The center is cx,cy,cz. ex,ey,ez are the half-size.
 */

typedef struct Bound {
    float cx, cy, cz;
    float ex, ey, ez;
} Bound;

/*
 * One loaded block. first is an index into models[] and bounds[].
 * The box is the union of those models, so a block the camera cannot
 * see is skipped before any of its instances are read.
 */

typedef struct Span {
    int first;
    int count;
    float cx, cy, cz;
    float ex, ey, ez;
} Span;

typedef struct Scene {
    const Instance *models;
    const Bound *bounds;
    int nmodels;
    const Span *spans;
    int nspans;
    const Flat *sprites;
    int nsprites;
} Scene;

void instance_set_rotation(Instance *in, float rx, float ry, float rz);
void instance_point(const Instance *in, float lx, float ly, float lz,
                    float *x, float *y, float *z);

/* Local box from the triangles. A second call with the same count is free. */
void mesh_measure(Mesh *mesh);

/* World box. 0 when the mesh has no triangles. */
int instance_world_box(const Instance *in, Bound *out);

/*
 * 1 when the plane faces the camera, or the normal is degenerate.
 * nx,ny,nz and lx,ly,lz are in mesh space. The normal uses the same
 * winding as the triangle. A camera on the front side sees dot < 0
 * after both are transformed into view space.
 */
int instance_faces(const Instance *in, const Camera *cam, const Basis *basis,
                   float nx, float ny, float nz,
                   float lx, float ly, float lz);

/*
 * How far a frame can see. clip_far is the card's far clip, in world
 * units. fog nonzero means distance fog is on, so the visible limit is
 * the solid fog distance: clip_far * (DF_FOG_FAR / DF_FAR), the same
 * fraction gfx uses, and never closer than the fog start. fog zero
 * uses the clip itself. Defaults are fog on and DF_FAR, matching a
 * fresh device. gfx calls this when either value changes.
 */
void scene_set_limits(float clip_far, int fog);
/* 1 keeps the frustum. 0 lists every entity, as a null camera does. */
void scene_set_cull(int on);
int scene_cull(void);

/*
 * Visibility. Writes storage indices into out, in storage order, and
 * returns how many. A null camera lists every entity, so a test can
 * ignore the frustum. scene_set_cull(0) does the same with a camera.
 * With culling on, a block span is tested first, then
 * each model's baked box. A model or sprite is dropped when its box
 * lies wholly outside the view: behind the near plane, past the fog or
 * the far clip, or past a screen edge. A close vertical face then hides
 * a box only when that box sits inside that face's screen rectangle
 * and is farther than the face. A floor, a ceiling, and the gap
 * between a roof and a side wall stay. scene_set_cull(0) skips both.
 * Without baked bounds, the box is
 * measured from the mesh and a model with no triangles is omitted.
 * A count above cap is truncated, so pass a cap of at least nmodels
 * or nsprites.
 */

int scene_visible_models(const Scene *scene, const Camera *cam, int *out, int cap);
int scene_visible_sprites(const Scene *scene, const Camera *cam, int *out, int cap);

/* Draw the rectangles the wall test used last frame. Default on. */
void scene_set_cover_draw(int on);
int scene_cover_draw(void);
int scene_cover_count(void);
void scene_cover_at(int index, float *x0, float *y0, float *x1, float *y1);
/* Joined walls. Faces join only when their rectangles meet. */
int scene_facade_count(void);
void scene_facade_at(int index, float *x0, float *y0, float *x1, float *y1);

#endif
