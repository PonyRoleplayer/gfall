#ifndef DF_ARCH3D_H
#define DF_ARCH3D_H

#include "engine/assets/mesh.h"

/*
 * ARCH3D.BSA record -> engine mesh.
 * Points in the file are divided by 256 into world units.
 * File uv is divided by 16 into texels on the original image.
 */

typedef struct ArchLoad {
    df_u32 id;
    Mesh *mesh;
} ArchLoad;

Mesh *arch3d_load(const df_u8 *rec, df_u32 size, df_u32 id);
int arch3d_uv_failures(void);

#endif
