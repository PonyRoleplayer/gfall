#include "game/dagger/formats/flc.h"
#include "engine/core/mem.h"

/* See flc.h. */

#define HDR_MAGIC 4
#define HDR_FRAMES 6
#define HDR_WIDTH 8
#define HDR_HEIGHT 10
#define HDR_DEPTH 12
#define HDR_SPEED 16
#define HDR_FIRST 80
#define HDR_SECOND 84
#define FRAME_HEADER_BYTES 16
#define FRAME_CHUNKS 6
#define CHUNK_HEADER_BYTES 6
#define COLOUR_BITS 8
#define CHUNK_COLOUR_256 4
#define CHUNK_COLOUR_64 11
#define CHUNK_BLACK 13
#define CHUNK_COPY 16
#define CHUNK_BRUN 15
#define CHUNK_LC 12
#define CHUNK_SS2 7
/* A six-bit colour widened to eight bits: shifted up, its top bits repeated below. */
#define SIX_TO_EIGHT_SHIFT 2
#define SIX_TO_EIGHT_REPEAT 4
/* SS2's line word: its top two bits say what it is. */
#define SS2_KIND_MASK 0xC000
#define SS2_SKIP_LINES 0xC000
#define SS2_LAST_BYTE 0x8000
/* The palette's colours when a packet's count byte is 0. */
#define PAL_COLOURS 256
#define PAL_TRIPLE 3

static unsigned rd16(const df_u8 *p) { return (unsigned)(p[0] | p[1] << 8); }
static df_u32 rd32(const df_u8 *p) { return (df_u32)p[0] | (df_u32)p[1] << 8 | (df_u32)p[2] << 16 | (df_u32)p[3] << 24; }

int flc_open(Flc *f, const char *rel)
{
    df_u8 head[FLC_HEADER_BYTES];
    static df_u8 *chunk, *image;
    static df_u32 image_cap;

    mem_zero(f, sizeof(*f));
    f->file = fs_open_media(rel);
    if (!f->file) { return 0; }
    f->size = fs_size(f->file);
    if (f->size < FLC_HEADER_BYTES || !fs_read_at(f->file, 0, head, FLC_HEADER_BYTES) || rd16(head + HDR_MAGIC) != FLC_MAGIC || rd16(head + HDR_DEPTH) != COLOUR_BITS) {
        fs_close(f->file); f->file = 0; return 0;
    }
    f->frames = (int)rd16(head + HDR_FRAMES); f->width = (int)rd16(head + HDR_WIDTH); f->height = (int)rd16(head + HDR_HEIGHT);
    f->ticks = (int)rd16(head + HDR_SPEED) / FLC_TICK_MS + 1;
    f->first = rd32(head + HDR_FIRST); f->second = rd32(head + HDR_SECOND);
    if (f->width < 1 || f->height < 1 || f->width > FLC_MAX_SIDE || f->height > FLC_MAX_SIDE || f->first >= f->size) { fs_close(f->file); f->file = 0; return 0; }
    /* The picture and the scratch are kept for the process and reused (a temp heap must not own them). */
    if (!chunk) { chunk = (df_u8 *)mem_keep(FLC_CHUNK_MAX); }
    if ((df_u32)(f->width * f->height) > image_cap) { image = (df_u8 *)mem_keep((df_u32)(f->width * f->height)); image_cap = image ? (df_u32)(f->width * f->height) : 0; }
    f->chunk = chunk; f->image = image;
    if (!f->chunk || !f->image) { flc_close(f); return 0; }
    mem_zero(f->image, (df_u32)(f->width * f->height));
    f->next = f->first;
    return 1;
}

void flc_close(Flc *f)
{
    if (f->file) { fs_close(f->file); f->file = 0; }
    f->image = 0; f->chunk = 0;
}

void flc_rewind(Flc *f) { f->next = f->second; f->frames_done = 0; }
int flc_frame_ms(const Flc *f) { return f->ticks * FLC_TICK_MS; }

/* A palette chunk: packets of (skip, count, count RGB triples); the colour index runs on across packets. */
static void read_palette(Flc *f, const df_u8 *p, df_u32 n, int six_bit)
{
    unsigned packets, i;
    int index = 0, count, c;
    df_u32 at = 2;

    if (n < 2) { return; }
    packets = rd16(p);
    for (i = 0; i < packets && at + 2 <= n; i++) {
        index += p[at];
        count = p[at + 1] ? p[at + 1] : PAL_COLOURS;
        at += 2;
        for (c = 0; c < count * PAL_TRIPLE && at < n; c++, at++) {
            int at_pal = index * PAL_TRIPLE + c;
            if (at_pal < FLC_PAL_BYTES) { f->palette[at_pal] = six_bit ? (df_u8)(p[at] << SIX_TO_EIGHT_SHIFT | p[at] >> SIX_TO_EIGHT_REPEAT) : p[at]; }
        }
        index += count;
    }
    f->palette_changed = 1;
}

/* BRUN: each row a (never used) packet count byte, then packets to the row's width: a positive count repeats the next byte, a negative one copies -count bytes. */
static void decode_brun(Flc *f, const df_u8 *p, df_u32 n)
{
    int y, x, count;
    df_u32 at = 0;

    for (y = 0; y < f->height && at < n; y++) {
        at++;
        x = 0;
        while (x < f->width && at < n) {
            count = (signed char)p[at++];
            if (count > 0) {
                if (at >= n) { return; }
                if (x + count > f->width) { count = f->width - x; }
                mem_fill(f->image + y * f->width + x, p[at++], (df_u32)count);
                x += count;
            } else if (count < 0) {
                count = -count;
                if (at + (df_u32)count > n) { count = (int)(n - at); }
                if (x + count > f->width) { count = f->width - x; }
                mem_copy(f->image + y * f->width + x, p + at, (df_u32)count);
                at += (df_u32)count; x += count;
            }
        }
    }
}

/* LC: a first row and a row count, then per row a packet count, and packets of a skip, a count and bytes (positive) or one byte repeated -count times (negative). */
static void decode_lc(Flc *f, const df_u8 *p, df_u32 n)
{
    int y, rows, packets, x, count;
    df_u32 at = 4;

    if (n < 4) { return; }
    y = (int)rd16(p); rows = (int)rd16(p + 2);
    for (; rows > 0 && y < f->height && at < n; rows--, y++) {
        packets = p[at++];
        x = 0;
        while (packets-- > 0 && at + 2 <= n) {
            x += p[at++];
            count = (signed char)p[at++];
            if (count >= 0) {
                if (at + (df_u32)count > n) { count = (int)(n - at); }
                if (x + count > f->width) { count = f->width - x; }
                if (count > 0) { mem_copy(f->image + y * f->width + x, p + at, (df_u32)count); }
                at += (df_u32)count; x += count;
            } else {
                count = -count;
                if (at >= n) { return; }
                if (x + count > f->width) { count = f->width - x; }
                if (count > 0) { mem_fill(f->image + y * f->width + x, p[at], (df_u32)count); }
                at++; x += count;
            }
        }
    }
}

/* SS2: a row count, then words: bit 15 and 14 set skip -word rows, 15 alone puts its low byte at the row's last pixel (the row goes on), else the word is a packet count, each packet a skip byte, a signed word count and the words (positive) or one word repeated (negative). */
static void decode_ss2(Flc *f, const df_u8 *p, df_u32 n)
{
    int y = 0, done = 0, lines, packets, x, count, i;
    unsigned w;
    df_u32 at = 2;

    if (n < 2) { return; }
    lines = (int)rd16(p);
    while (y < f->height && done < lines && at + 2 <= n) {
        w = rd16(p + at); at += 2;
        if ((w & SS2_KIND_MASK) == SS2_SKIP_LINES) {
            y += (int)(0x10000u - w);
        } else if ((w & SS2_KIND_MASK) == SS2_LAST_BYTE) {
            f->image[f->width * (y + 1) - 1] = (df_u8)(w & 0xFF);
        } else if (w != 0) {
            packets = (int)w; x = 0;
            for (i = 0; i < packets && at + 2 <= n; i++) {
                x += p[at++];
                count = (signed char)p[at++];
                if (count >= 0) {
                    int bytes = count * 2;
                    if (at + (df_u32)bytes > n) { bytes = (int)(n - at); }
                    if (x + bytes > f->width) { bytes = f->width - x; }
                    if (bytes > 0) { mem_copy(f->image + y * f->width + x, p + at, (df_u32)bytes); }
                    at += (df_u32)count * 2; x += count * 2;
                } else {
                    int j;
                    count = -count;
                    if (at + 2 > n) { return; }
                    for (j = 0; j < count; j++, x += 2) {
                        if (x + 1 < f->width) { f->image[y * f->width + x] = p[at]; f->image[y * f->width + x + 1] = p[at + 1]; }
                    }
                    at += 2;
                }
            }
            y++; done++;
        } else {
            y++; done++;
        }
    }
}

int flc_next(Flc *f)
{
    df_u8 head[FRAME_HEADER_BYTES];
    df_u32 size, at;
    unsigned chunks, i;

    if (!f->file) { return 0; }
    f->palette_changed = 0;
    for (;;) {
        if (f->next + FRAME_HEADER_BYTES > f->size || !fs_read_at(f->file, f->next, head, FRAME_HEADER_BYTES)) { return 0; }
        size = rd32(head);
        if (size < FRAME_HEADER_BYTES) { return 0; }
        if (rd16(head + 4) == FLC_FRAME_MAGIC) { break; }
        f->next += size;                  /* a prefix chunk and the like: not a frame */
    }
    chunks = rd16(head + FRAME_CHUNKS);
    at = f->next + FRAME_HEADER_BYTES;
    f->next += size;
    for (i = 0; i < chunks && at + CHUNK_HEADER_BYTES <= f->next; i++) {
        df_u8 ch[CHUNK_HEADER_BYTES];
        df_u32 csize, body;
        unsigned type;

        if (!fs_read_at(f->file, at, ch, CHUNK_HEADER_BYTES)) { break; }
        csize = rd32(ch); type = rd16(ch + 4);
        if (csize < CHUNK_HEADER_BYTES || at + csize > f->size) { break; }
        body = csize - CHUNK_HEADER_BYTES;
        at += csize;
        if (body > FLC_CHUNK_MAX || type == 0) { continue; }
        switch (type) {
        case CHUNK_BLACK: mem_zero(f->image, (df_u32)(f->width * f->height)); continue;
        case CHUNK_COLOUR_256: case CHUNK_COLOUR_64: case CHUNK_COPY: case CHUNK_BRUN: case CHUNK_LC: case CHUNK_SS2: break;
        default: continue;
        }
        if (!fs_read_at(f->file, at - body, f->chunk, body)) { continue; }
        switch (type) {
        case CHUNK_COLOUR_256: read_palette(f, f->chunk, body, 0); break;
        case CHUNK_COLOUR_64: read_palette(f, f->chunk, body, 1); break;
        case CHUNK_COPY: mem_copy(f->image, f->chunk, body < (df_u32)(f->width * f->height) ? body : (df_u32)(f->width * f->height)); break;
        case CHUNK_BRUN: decode_brun(f, f->chunk, body); break;
        case CHUNK_LC: decode_lc(f, f->chunk, body); break;
        case CHUNK_SS2: decode_ss2(f, f->chunk, body); break;
        }
    }
    f->frames_done++;
    return 1;
}
