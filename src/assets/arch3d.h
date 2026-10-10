#ifndef DF_ARCH3D_H
#define DF_ARCH3D_H

#include "df.h"

/*
 * One ARCH3D model as a triangle list in block units (file point / 256).
 * u and v are texel units (file uv / 16) on the original image, not 0..1.
 * slot is filled later with the world's texture index. 0xffff means none.
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

Mesh *arch3d_load(const df_u8 *rec, df_u32 size, df_u32 id);
int arch3d_uv_failures(void);

#endif
