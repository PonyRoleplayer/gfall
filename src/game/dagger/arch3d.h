#ifndef DF_ARCH3D_H
#define DF_ARCH3D_H

#include "engine/assets/mesh.h"

/*
 * ARCH3D.BSA record -> engine mesh.
 * Points in the file are divided by 256 into world units.
 * File uv is divided by 16 into texels on the original image.
 * A face's point references are byte offsets into the point table:
 * 12 bytes a point in v2.6 and v2.7, 4 in v2.5.
 */

typedef struct ArchLoad {
    df_u32 id;
    Mesh *mesh;
} ArchLoad;

Mesh *arch3d_load(const df_u8 *rec, df_u32 size, df_u32 id);
int arch3d_uv_failures(void);
/* Faces skipped because a point reference fell outside the point table. */
int arch3d_bad_refs(void);

#endif
