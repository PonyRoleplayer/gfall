#ifndef DF_TEXCACHE_H
#define DF_TEXCACHE_H

struct World;

/* Uploads every image the world lists. The first room fits, so nothing is evicted. */

int texcache_upload(const struct World *world);
void texcache_bind(int slot);
const char *texcache_error(void);

#endif
