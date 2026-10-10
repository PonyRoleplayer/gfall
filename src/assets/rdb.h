#ifndef DF_RDB_H
#define DF_RDB_H

#include "assets/arch3d.h"

/*
 * One placed model. m[] takes a mesh point into the block:
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
 * One flat. The quad is centered on x,y,z. w and h are the full size in
 * world units, filled in after the texture's scale is known. slot is
 * 0xffff until the world assigns a texture.
 * file is the TEXTURE archive and image is the record.
 */
typedef struct Flat {
    float x, y, z;
    float w, h;
    df_u16 file;
    df_u16 image;
    df_u16 slot;
} Flat;

typedef struct Block {
    Instance *inst;
    int ninst;
    int ntris;
    Flat *flat;
    int nflat;
    int neditor;
} Block;

struct Bsa;

Block *rdb_load(const df_u8 *rdb, df_u32 size, struct Bsa *arch);
void instance_set_rotation(Instance *in, float rx, float ry, float rz);
void instance_point(const Instance *in, float lx, float ly, float lz,
                    float *x, float *y, float *z);

#endif
