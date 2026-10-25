#ifndef DF_TEXTURE_H
#define DF_TEXTURE_H

#include "engine/assets/texture.h"

/*
 * Daggerfall TEXTURE.### record, frame 0. TEXTURE.000 and TEXTURE.001
 * are solid colour sets, 32x32. The result is an engine Image.
 */

void texture_filename(int file, char *dst, int n);
Image *texture_load(int file, int image);
/*
 * Frame frame of the record, 0 up to texture_frame_count. A flame, a
 * waterfall, a walking townsman: Daggerfall's animated flats are records
 * of several frames.
 */
Image *texture_load_frame(int file, int image, int frame);
/* Frames in the record; 1 for a still picture, or when it cannot be read. */
int texture_frame_count(int file, int image);
/* Records in TEXTURE.file, or 0 when the file is missing. */
int texture_record_count(int file);
/*
 * Drops the cached TEXTURE.### pointers. Does not free them.
 * Call this just before destroying the heap those bytes came from.
 */
void texture_cache_clear(void);

/*
 * A loaded image's pot is the picture stretched to powers of two by
 * repeating texels, then repeated until square, so it can tile a wall.
 * texture_pad makes it a sprite instead: the picture at its own size,
 * transparent around it, span_w and span_h its width and height.
 * texture_smooth redoes a wall's stretch bilinearly against the
 * palette, for an image whose sides are not powers of two. The world
 * calls them once the palette is loaded and it knows each image's use.
 */
void texture_pad(Image *img);
void texture_smooth(Image *img, const df_u8 *pr, const df_u8 *pg, const df_u8 *pb);

/* What the "texture" loader writes. file and image pick the record. */
typedef struct TexLoad {
    int file;
    int image;
    int frame;
    Image *out;
} TexLoad;

#endif
