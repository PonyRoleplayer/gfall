#ifndef DF_TERRAIN_H
#define DF_TERRAIN_H

#include "engine/assets/mesh.h"
#include "engine/scene/scene.h"

/*
 * The land's height and its ground tiles, for the streamed world. Read
 * docs/streaming.md (The land) before editing.
 *
 * Heights are Daggerfall Unity's recipe in a form of our own: a smooth
 * function of the world position, so any two chunks agree where they
 * meet. In "scaled" units (DFU's): 8 times WOODS.WLD's height map, its
 * byte on each map pixel's south-east corner, Catmull-Rom between; plus
 * 4 times its finer heights, a pixel's 3 x 3, each at the middle of its
 * third, Catmull-Rom between; plus a little noise; never under the sea
 * floor (27.2). A world y is TERRAIN_UNIT units a scaled unit above it:
 * the sea is y 0. Round a town (a TerrainFlat) the land eases to the
 * town's level over TERRAIN_BLEND, and is that level under it.
 *
 * A chunk's ground is a 5 x 5 grid of heights 1024 apart and its 16 x
 * 16 tiles: water, dirt, grass or stone at each tile corner (by height
 * and noise, as DFU's), then the archive's transition tile for each
 * tile from its four corners (DFU's marching table). Plain C, no Glide:
 * hostcheck runs it. Load time only: C casts are allowed.
 */

/* World units a scaled unit: DFU's terrain scale, 1.5 m, at 40 units a metre. */
#define TERRAIN_UNIT 60.0f
#define TERRAIN_OCEAN 27.2f
#define TERRAIN_BEACH 40.0f
#define TERRAIN_CELL 1024
#define TERRAIN_CELLS 4
#define TERRAIN_TILES 16
#define TERRAIN_TILE 256
/* Round a town its ground eases back to the land's over this many units. */
#define TERRAIN_BLEND 4096.0

enum {
    TERRAIN_WATER,
    TERRAIN_DIRT,
    TERRAIN_GRASS,
    TERRAIN_STONE
};

/* A town's rectangle in world units, x east, z north, and the level it is flat at (scaled). */
typedef struct TerrainFlat {
    double x0, z0, x1, z1;
    float s;
} TerrainFlat;

typedef struct TerrainChunk {
    int cx;
    int cz;
    /* World y at the grid, [z][x], z north from the chunk's south edge, 1024 apart. */
    float y[TERRAIN_CELLS + 1][TERRAIN_CELLS + 1];
    /* The tiles as an RMB block has them: tiles[row * 16 + col], row 0 north; record | 0x40 turn | 0x80 flip. */
    df_u8 tiles[TERRAIN_TILES * TERRAIN_TILES];
    /* Bit z * 4 + x: the 1024 cell lies under a town, whose own ground is drawn there. */
    unsigned int covered;
} TerrainChunk;

/* The land's scaled height at a world point, no town in it. */
float terrain_natural(double wx, double wz);
/* The same with the towns' flats eased in. */
float terrain_scaled(double wx, double wz, const TerrainFlat *flat, int nflat);
float terrain_y(float scaled);
/* A town's level: the mean of the land's heights over its rectangle (9 x 9 of them). */
float terrain_level(double x0, double z0, double x1, double z1);

/* Chunk (cx, cz)'s heights and tiles, the flats that reach it eased in. */
void terrain_chunk(TerrainChunk *t, int cx, int cz, const TerrainFlat *flat, int nflat);
/* World y at a point of the chunk, its own frame (0..4096), bilinear in its cell. */
float terrain_height_in(const TerrainChunk *t, float lx, float lz);
/* How steep the chunk is at a point: rise over run, the larger of the two axes'. */
float terrain_slope_in(const TerrainChunk *t, float lx, float lz);

/*
 * The near ground: the cells not under a town as quads of their tiles,
 * as few as the tiles allow (a cell whose 16 tiles are one is one quad,
 * its tile repeated; else its halves, else its tiles), each corner's
 * height bilinear in its cell so no edge cracks against a neighbour.
 * Triangles grouped by picture. slot_of gives a tile record's picture
 * slot. One face a triangle (a quad is not flat). On the current heap;
 * 0 when every cell is a town's.
 */
Mesh *terrain_near_mesh(const TerrainChunk *t, int (*slot_of)(int record, void *user), void *user);
/*
 * The far ground: 2 x 2 quads 2048 apart under one picture of the chunk
 * (slot, side texels across 4096), and a skirt hung under each edge, so
 * a finer neighbour's edge never shows a gap. 0 when every cell is a town's.
 */
Mesh *terrain_far_mesh(const TerrainChunk *t, int slot, int side);

#endif
