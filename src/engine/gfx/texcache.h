#ifndef DF_TEXCACHE_H
#define DF_TEXCACHE_H

struct Level;

/* Uploads every image in the level. The first room fits, so nothing is evicted. */

int texcache_upload(const struct Level *level);
void texcache_bind(int slot);
const char *texcache_error(void);

#endif
