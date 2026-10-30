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
/*
 * Where a record says it sits on classic's 320 x 200 screen (its header's
 * first two words). The paper doll's pieces are placed by it. 0 when the
 * record cannot be read.
 */
int texture_offset(int file, int image, int *x, int *y);
/* Records in TEXTURE.file, or 0 when the file is missing. */
int texture_record_count(int file);
/*
 * Drops the cached TEXTURE.### pointers. Does not free them.
 * Call this just before destroying the heap those bytes came from.
 */
void texture_cache_clear(void);
/*
 * For a screen that reads a few files into a heap of its own and then
 * deletes the heap. texture_cached says whether a file is cached before
 * the screen reads it; texture_forget drops the cache's pointer to one it
 * read (the heap frees the bytes). A file under the cap (below) is the
 * cache's own and stays.
 */
int texture_cached(int file);
void texture_forget(int file);
/*
 * Keep at most bytes of whole TEXTURE files, the least recently read
 * freed first, in texture.c's own heap (docs/streaming.md: a streamed
 * world reads files for as long as it runs). 0, the default, keeps
 * every file read until texture_cache_clear, as a place loaded whole
 * does. Takes effect from the next file read.
 */
void texture_cache_limit(df_u32 bytes);
df_u32 texture_cache_bytes(void);
/*
 * The next TEXTURE_FILE_PIECE bytes of TEXTURE.file read into the
 * cache, 0; 1, reading nothing, when the file is whole there already
 * (or cannot be read). Under the cap (a streamed world) into the cache's
 * own heap; else into the current one, a place's. One file at a time;
 * asking for another drops the first's pieces. A step reads a file a
 * piece a call, never 80 KB to 440 KB of the 233's disk in one frame
 * (docs/streaming.md; a place's creatures, docs/entities.md).
 */
#define TEXTURE_FILE_PIECE 32768u
int texture_file_step(int file);
/*
 * An animated record's frames laid into img (its frame 0, loaded) as
 * one padded sheet: side by side, TEXTURE_SHEET_GAP texels apart, in the
 * smallest power-of-two square that holds them (docs/sprites.md). 1
 * when img is now a sheet; 0 for a still record, or one whose frames do
 * not all load or do not fit 256.
 */
#define TEXTURE_SHEET_GAP 2
#define TEXTURE_SHEET_FRAMES 64
int texture_sheet(Image *img, int file, int image);
/* A flat's size in world units from its picture's pixels and scale. */
int texture_extent(int pixels, int scale);

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
