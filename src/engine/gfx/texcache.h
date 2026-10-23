#ifndef DF_TEXCACHE_H
#define DF_TEXCACHE_H

#include "engine/core/df.h"

struct Level;

/*
 * Each picture is uploaded once, at full size, one level.
 * A 2MB Voodoo stores even and odd mip levels on one address, so a
 * chain overwrites the picture and the font. Nothing is sent again
 * during the frame. A picture that does not fit is left unbound.
 */

int texcache_upload(const struct Level *level);
void texcache_bind(int slot);
/* The next bind runs even if that slot was the last one selected. */
void texcache_forget(void);
const char *texcache_error(void);
/*
 * From the last upload. live pictures are on the card, using bytes.
 * unfit had pixels but no room (or the upload failed): they stay
 * unbound, so their faces draw with whatever texture was bound before.
 * total is the level's image count, including unreadable ones.
 */
void texcache_stats(int *live, int *unfit, int *total, df_u32 *bytes);

#endif
