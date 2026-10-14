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

typedef struct Mesh {
    df_u32 id;
    Tri *tris;
    int ntris;
} Mesh;

#endif
