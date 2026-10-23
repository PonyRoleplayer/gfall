#ifndef DF_TEXCACHE_H
#define DF_TEXCACHE_H

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

#endif
