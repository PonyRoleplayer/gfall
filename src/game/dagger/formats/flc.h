#ifndef DF_FLC_H
#define DF_FLC_H

#include "engine/core/df.h"
#include "engine/platform/fs.h"

/*
 * The princes' *.FLC (AZURA.FLC .. VAERNIMA.FLC in ARENA2): Autodesk FLIC animations, 320 x 200, 8 bits, 14 frames
 * (FALL.EXE's flc_open, flc_next_frame, flc_read_frame and the decoders in pflc.c, xflc). Read docs/video.md.
 *
 * A 128-byte header (size, the magic AF12h, the frame count, the size, the depth, flags, the speed in milliseconds
 * a frame, the offsets of the first and second frames at 80 and 84), then an optional prefix chunk (F100h) and the
 * frames. A frame is a 16-byte header (size, F1FAh, the chunk count) and chunks, each a size (its header included)
 * and a type: 4 a palette (packets of a skip, a count and that many RGB triples, 8 bits: the files' values go to
 * 255), 11 the same at six bits, 13 black, 16 the pixels raw, 15 BRUN (the first frame), 12 LC, 7 SS2 (every later
 * frame, word-oriented deltas); any other (18, the thumbnail) is skipped by its size. The first frame is the whole
 * picture and the file's last frame is a delta back to it, so a loop starts again at the second frame
 * (flc_rewind): the second round plays 14 frames from there, as flc_play_with_text does.
 *
 * Time: FALL.EXE counts the BIOS's 18.2 Hz ticks, speed / 55 + 1 of them a frame (214 ms is 4 ticks, 220 ms).
 * Each chunk is read from the file at its place into one scratch buffer, so a 700 KB file costs the 64000-byte
 * picture and the buffer. Host-tested over every file (check_dagger.c).
 */

#define FLC_MAGIC 0xAF12u
#define FLC_HEADER_BYTES 128
#define FLC_FRAME_MAGIC 0xF1FAu
#define FLC_PAL_BYTES 768
/* The BIOS tick the speed is counted in (milliseconds; FALL.EXE divides by 55 and adds one). */
#define FLC_TICK_MS 55
/* The largest chunk read: the files' largest frame is 54714 bytes. */
#define FLC_CHUNK_MAX 131072
#define FLC_MAX_SIDE 640

typedef struct Flc {
    FsFile *file;
    df_u32 size;
    df_u32 next;            /* file offset of the next frame */
    df_u32 first, second;   /* the offsets of the first and second frames */
    int frames, width, height;
    int ticks;              /* BIOS ticks (FLC_TICK_MS each) a frame lasts */
    int frames_done;
    int palette_changed;    /* a palette chunk was read by the last flc_next */
    df_u8 *image;           /* width * height pixels, the frame as it stands */
    df_u8 *chunk;           /* the scratch buffer one chunk is read into */
    df_u8 palette[FLC_PAL_BYTES];
} Flc;

/* Opens one by its name under the game's root (ARENA2\AZURA.FLC); 0 when missing or not a FLIC of 8 bits. */
int flc_open(Flc *f, const char *rel);
void flc_close(Flc *f);
/* Reads the next frame into f->image; 0 at the end of the file (nothing changed). */
int flc_next(Flc *f);
/* Back to the second frame, where a loop goes on (FALL.EXE's loop_offset). */
void flc_rewind(Flc *f);
/* A frame's length in milliseconds. */
int flc_frame_ms(const Flc *f);

#endif
