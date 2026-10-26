#ifndef ENG_TEXRES_H
#define ENG_TEXRES_H

#include "engine/core/df.h"

/*
 * Which pictures are on the card, and how sharp, for a streamed world.
 * Read docs/streaming.md (The card) before editing. Plain C: the card
 * is reached through ops, so hostcheck runs it with a fake one.
 *
 * The card's room from base to end is a buddy allocator: blocks of a
 * power of two from TEXRES_MIN bytes, split to fit and joined when both
 * halves are free, each 2 MB half of the card an arena of its own so no
 * download crosses the edge.
 *
 * A picture added gets its smallest copy at once (its side halved down
 * to TEXRES_TINY or less), kept while it is added: it is never drawn
 * untextured. Each frame the draw notes the nearest distance a picture
 * was drawn at; the level it needs is the one at which a texel covers a
 * pixel or less there (density texels a world unit, focal pixels a unit
 * at distance 1). texres_service then sends sharper copies where they
 * are needed most (the most levels short, then the nearest), under a
 * byte budget; short of room it frees the sharp copies drawn longest
 * ago, and still short, it settles for a level down.
 */

#define TEXRES_SLOTS 4096
#define TEXRES_MIN 256u
/* The smallest copy every added picture keeps: 8 x 8. */
#define TEXRES_TINY 8

typedef struct TexResOps {
    /* The card's bytes for a picture of side, from its top level down, as uploaded. */
    df_u32 (*bytes)(int side, int cutout);
    /* Send slot's picture from level drop to addr; its handle. 0 when it failed. */
    int (*upload)(int slot, int drop, df_u32 addr, df_u32 *handle);
    void (*release)(df_u32 handle);
} TexResOps;

/* The card's room for streamed pictures, base to end, and how to reach it. Starts empty. */
void texres_begin(df_u32 base, df_u32 end, const TexResOps *ops);
void texres_end(void);
/*
 * Add picture slot: side (a power of two, 1..256), cutout, density
 * texels a world unit. Its smallest copy goes on the card now. 0 when
 * even that has no room.
 */
int texres_add(int slot, int side, int cutout, float density);
/* Remove slot: both its copies leave the card. */
void texres_remove(int slot);
/* The draw: slot was drawn this frame at dist world units from the eye. */
void texres_note(int slot, float dist);
/*
 * After the frame: sharper copies where needed, budget bytes at most;
 * focal is the camera's pixels a unit at distance 1. Ends the frame.
 */
void texres_service(float focal, df_u32 budget);
/* The handle to bind for slot (its sharp copy if it has one); 0 when it is not added. */
int texres_handle(int slot, df_u32 *handle);
/* Slot's level on the card: 0 full size, more halved; -1 when not added. */
int texres_drop(int slot);
/* Pictures added, sharp copies, bytes in use, bytes free, uploads and evictions so far. */
void texres_stats(int *added, int *sharp, df_u32 *used, df_u32 *free_bytes,
                  int *uploads, int *evictions);

#endif
