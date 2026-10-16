#ifndef DF_TEXCACHE_H
#define DF_TEXCACHE_H

struct Level;
struct Camera;

/*
 * Every image is uploaded at 16 by 16. texcache_detail sends the real
 * square for models and sprites near the camera, into the rest of the
 * 2MB. A far face keeps the small copy even when a near face shares
 * the record.
 */

int texcache_upload(const struct Level *level);
void texcache_detail(const struct Camera *cam, const struct Level *level);
int texcache_near_model(int index);
int texcache_near_flat(int index);
void texcache_bind(int slot, int sharp);
/* The next bind runs even if that slot was the last one selected. */
void texcache_forget(void);
const char *texcache_error(void);

#endif
