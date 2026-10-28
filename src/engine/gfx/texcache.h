#ifndef DF_TEXCACHE_H
#define DF_TEXCACHE_H

#include "engine/core/df.h"

struct Level;
struct Image;

/*
 * Each picture is uploaded once, at load: a mip chain when
 * glide_mipmap() is on, one level when it is off. A budget first drops
 * the top level of the largest pictures until the level fits the card
 * with room for the font. Nothing is sent during the frame. A picture
 * that still does not fit is left unbound. See docs/glide.md.
 */

int texcache_upload(const struct Level *level);
void texcache_bind(int slot);
const char *texcache_error(void);
/*
 * From the last upload. live pictures are on the card, using bytes.
 * unfit had pixels but no room (or the upload failed): they stay
 * unbound, so their faces draw with whatever texture was bound before.
 * total is the level's image count, including unreadable ones.
 */
void texcache_stats(int *live, int *unfit, int *total, df_u32 *bytes);
/* Pictures the budget put on the card below full size. */
int texcache_reduced(void);

/*
 * A streamed world's pictures (docs/streaming.md, The card): pictures
 * come and go as chunks do, each at the size its distance needs
 * (engine/gfx/texres.h). Begin takes the card's room past the fixed
 * uploads and the palette. Set builds slot's mip chain once, kept in
 * RAM, and puts its small copy on the card; drop frees both. The draw
 * notes each slot's nearest distance; service sends sharper copies
 * after the frame, budget bytes at most. texcache_bind binds whichever
 * copy a slot has.
 */
int texcache_stream_begin(const struct Level *level);
void texcache_stream_end(void);
int texcache_streamed(void);
int texcache_stream_set(int slot, const struct Image *img, int cutout, float density);
void texcache_stream_drop(int slot);
void texcache_note(int slot, float dist);
void texcache_stream_service(float focal, df_u32 budget);
/* RAM held by the streamed pictures' chains. */
df_u32 texcache_stream_ram(void);
/*
 * 1 while pictures go on the card by distance: a streamed world's, or,
 * in a place loaded whole, the ones only its entities wear. Those are
 * kept out of texcache_upload's budget, which leaves them room (up to
 * 512 KB); the draw notes them as a stream's, and service runs after
 * the frame (docs/memory.md).
 */
int texcache_resident(void);
/*
 * 1 when a place loaded whole was too large for the card at full size,
 * so every picture of it is on the card by distance, as a stream's.
 */
int texcache_resident_all(void);
/*
 * A place loaded whole: a picture read after its load (a creature's, as
 * it comes near: docs/entities.md) put on the card by distance, its
 * chain built now. Only while a residency session is open (texcache_
 * resident: a place with entities has one, its room kept for them).
 * 0 when it could not be.
 */
int texcache_resident_add(int slot, const struct Image *img, int cutout, float density);

#endif
