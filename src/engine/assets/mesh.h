#ifndef ENG_MESH_H
#define ENG_MESH_H

#include "engine/core/df.h"

/*
 * Triangle list in world units. u and v are texels on the source image,
 * not 0..1. tex_file and tex_image are keys for the loader that owns the
 * art. slot is the level texture index. 0xffff means the face is skipped.
 * id is a loader tag. The rasterizer does not read it.
 */

typedef struct Tri {
    float x[3];
    float y[3];
    float z[3];
    float u[3];
    float v[3];
    df_u16 tex_file;
    df_u16 tex_image;
    df_u16 slot;
} Tri;

/*
 * One ARCH3D plane after it has been fanned into triangles.
 * The normal is local, in the same winding as triangle first.
 * first is an index into tris. ntris is how many follow.
 */

typedef struct Face {
    float nx, ny, nz;
    int first;
    int ntris;
    /*
     * draw.c's note, set on the first draw: 0 not checked yet, 1 the
     * triangles are an exact fan of one polygon (drawn as a polygon),
     * 2 they are not (drawn triangle by triangle). A loader leaves 0.
     */
    int fan;
} Face;

typedef struct Mesh {
    df_u32 id;
    Tri *tris;
    int ntris;
    Face *faces;
    int nfaces;
    int face_cap;
    /*
     * Unique texture slots, filled after paint assigns them.
     * The near-set scan reads this instead of every triangle.
     */
    df_u16 *slot_keys;
    int nslot_keys;
    int have_keys;
    /*
     * Local box. bound_n is the triangle count that produced it.
     * 0 means not measured yet. The center is cx,cy,cz.
     * ex,ey,ez are the half-size.
     */
    float cx, cy, cz;
    float ex, ey, ez;
    int bound_n;
} Mesh;

#endif
