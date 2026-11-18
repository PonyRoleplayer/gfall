#ifndef DF_VID_H
#define DF_VID_H

#include "engine/core/df.h"
#include "engine/platform/fs.h"

/*
 * ANIM####.VID and DAG2.VID: Daggerfall's movies (FALL.EXE's xn_vid_play, which the decomp documents in
 * xvid.h). A 15-byte header (the magic "VID\0", a version byte, the frame count, the width and height,
 * a delay added to every frame and some flags), then chunks, each a type byte:
 *   0 a raw frame, 1 a delta, 4 a delta from a start row, 3 an RLE frame: a word delay in 60 Hz
 *     ticks, then the pixels (8 bits, under the movie's palette);
 *   2 a palette: 256 RGB triples, six bits each (the VGA DAC's);
 *   7Ch an audio header (a word delay in ticks, a rate byte: the rate is 1000000 / (256 - byte)), whose
 *     first audio chunk follows; 7Dh audio data: a word length and that many unsigned 8-bit mono samples.
 * A frame is followed at once by the audio chunks that go with it. A delta command byte is 80h + n to
 * skip n pixels, n (under 80h) to copy the next n bytes, 0 to end the frame; from version 2 a byte follows
 * the last row. An RLE command is 80h + n then a colour to fill n pixels, or n to copy n bytes; the
 * commands run across row ends and the frame ends with its last row.
 *
 * The movie is read through a window of the file, so an 18 MB movie costs one buffer and one frame
 * (64000 bytes), and decoding is a copy: it costs nothing on the 233. Host-tested over every movie
 * (check_dagger.c). Read docs/video.md.
 */

#define VID_MAGIC "VID"
#define VID_HEADER_BYTES 15
#define VID_TICK_HZ 60
/* The read window: the largest audio chunk is a word, plus its length word. */
#define VID_WINDOW 131072
/* Header flag bit 0: the original leaves the file open at the end (ours always closes). */
#define VID_PAL_BYTES 768

/* Called with each audio chunk (unsigned 8-bit mono at rate Hz) as it is read; data lives until the next call. */
typedef void (*VidAudio)(void *ctx, const df_u8 *data, int bytes, int rate);

typedef struct Vid {
    FsFile *file;
    df_u32 size;
    df_u32 base;            /* file offset of window[0] */
    df_u32 have;            /* bytes in the window */
    df_u32 at;              /* cursor in the window */
    df_u8 *window;
    df_u8 *frame;           /* width * height pixels */
    int version, frames, width, height, delay, flags;
    int frames_done;
    /* Of the last vid_next: the frame's own delay plus the header's, in ticks; a palette read since the previous frame
     * (the 6-bit triples widened to 8 bits in palette[]); the audio's rate byte and its delay in ticks. */
    int ticks;
    int palette_changed;
    df_u8 palette[VID_PAL_BYTES];
    int audio_delay, audio_rate;
    VidAudio audio;
    void *audio_ctx;
} Vid;

/* Opens a movie by its name under the game's root (ARENA2\ANIM0012.VID); 0 when missing or not a VID. */
int vid_open(Vid *v, const char *rel);
void vid_close(Vid *v);
void vid_set_audio(Vid *v, VidAudio fn, void *ctx);
/* Reads and decodes up to the next frame; 1 when v->frame holds it, 0 at the end of the movie. */
int vid_next(Vid *v);

#endif
