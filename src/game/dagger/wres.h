#ifndef DF_WRES_H
#define DF_WRES_H

#include "engine/assets/level.h"
#include "game/dagger/rdb.h"

/*
 * A streamed world's pictures and meshes (docs/streaming.md, Memory
 * that can be freed). One set at a time, for the stream's session.
 *
 * Pictures. A picture is its TEXTURE archive, its record and its use (a
 * wall tiles, a sprite is padded or a sheet of frames: the same record
 * used both ways is two pictures). Its slot is fixed for the session,
 * since meshes keep slot numbers in their triangles; the level's images,
 * cutout and glass arrays are indexed by it. A chunk acquires the
 * pictures it draws and releases them when it goes. A picture's pixels
 * (and its mip chain, texcache_stream_set) live while any chunk holds it
 * and for as long after as the RAM cap allows: wres_trim frees the least
 * recently released first.
 *
 * Meshes. Read once for the session per climate variant (a model's
 * faces name the climate's pictures: swap_climate), in a heap of their
 * own; their faces are given picture slots when first read.
 */

enum {
    WRES_WALL,
    WRES_SPRITE
};

#define WRES_PICTURES 4096
/* RAM for pictures nothing holds, and for meshes, before wres_trim and wres_full act. */
#define WRES_PICTURE_RAM (8u * 1024u * 1024u)
#define WRES_MESH_RAM (10u * 1024u * 1024u)

/* Start a session: pictures through pal; level's picture arrays become the session's. */
int wres_begin(const Palette *pal, Level *level);
void wres_end(void);

/* The slot of a picture, registered if new (nothing is loaded). -1 when the table is full. */
int wres_slot(int file, int record, int use);
/* Hold slot's picture: its pixels and its card copies are made if they are not. 1 when it is ready. */
int wres_acquire(int slot);
/*
 * The same in two halves, so a chunk reads one picture a call (a
 * picture is a file read, a stretch and a mip chain: a frame's budget on
 * the 233): wres_hold counts the chunk in; wres_load reads the pixels
 * if they are not, returning 1 when it did that work, 0 when there was
 * none to do (in RAM already, failed before, or made by the game).
 */
void wres_hold(int slot);
int wres_load(int slot);
void wres_release(int slot);
const Image *wres_image(int slot);
/* The archive and record slot reads. 0 for a free or game-made slot. */
int wres_picture(int slot, int *file, int *record);
/*
 * A picture the game made, a far chunk's ground: a slot of its own,
 * given back by wres_forget, and its card copies made (density texels
 * a world unit). The image stays the caller's and must outlive the slot.
 */
int wres_dynamic(Image *img, float density);
void wres_forget(int slot);
/* Free unheld pictures, the least recently released first, until their RAM is under cap. */
void wres_trim(df_u32 cap);

/* The mesh cache for a climate family (TEX_*) and season (winter 1), for rmb_load. */
ArchEnt *wres_meshes(int family, int winter, int **count);
/* The meshes' heap, for arch_cache_heap while a block is read. */
struct MemHeap *wres_mesh_heap(void);
/* Give a mesh's faces their pictures in family and season, once (its slot keys mark it done). */
void wres_paint(Mesh *mesh, int family, int winter);
/* 1 when the meshes' RAM has passed WRES_MESH_RAM: the stream should start again. */
int wres_full(void);
/* Every mesh goes, its heap with it; read again as chunks ask. Only with no chunk holding one. */
void wres_meshes_reset(void);

/* Slots registered, pictures in RAM, their bytes, pictures held; meshes and their bytes. */
void wres_stats(int *slots, int *in_ram, df_u32 *ram, int *held, int *meshes, df_u32 *mesh_ram);

#endif
