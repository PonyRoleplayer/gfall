#ifndef DF_TEXCACHE_H
#define DF_TEXCACHE_H

#include "engine/core/df.h"

struct Level;

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

#endif
