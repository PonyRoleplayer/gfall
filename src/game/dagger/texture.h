#ifndef DF_TEXTURE_H
#define DF_TEXTURE_H

#include "engine/assets/texture.h"

/*
 * Daggerfall TEXTURE.### record, frame 0. TEXTURE.000 and TEXTURE.001
 * are solid colour sets, 32x32. The result is an engine Image.
 */

void texture_filename(int file, char *dst, int n);
Image *texture_load(int file, int image);
/* Records in TEXTURE.file, or 0 when the file is missing. */
int texture_record_count(int file);
/*
 * Drops the cached TEXTURE.### pointers. Does not free them.
 * Call this just before destroying the heap those bytes came from.
 */
void texture_cache_clear(void);

/* What the "texture" loader writes. file and image pick the record. */
typedef struct TexLoad {
    int file;
    int image;
    Image *out;
} TexLoad;

#endif
