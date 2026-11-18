#include "game/dagger/formats/vid.h"
#include "engine/core/mem.h"

/* See vid.h. */

#define CHUNK_RAW 0
#define CHUNK_DELTA 1
#define CHUNK_PALETTE 2
#define CHUNK_RLE 3
#define CHUNK_DELTA_FROM 4
#define CHUNK_AUDIO_HEAD 0x7C
#define CHUNK_AUDIO 0x7D
#define CMD_RUN 0x80
#define CMD_COUNT 0x7F
/* A delta or RLE command never copies more than its count plus a colour: keep this much in the window. */
#define COMMAND_MARGIN 0x81
#define PAL_SIX_BITS_SHIFT 2
#define AUDIO_RATE_BASE 256
#define AUDIO_RATE_NUM 1000000

static void refill(Vid *v)
{
    df_u32 n, want;

    /* Slide the unread bytes to the front and read on. */
    n = v->have - v->at;
    if (v->at) { mem_copy(v->window, v->window + v->at, n); } /* forward and downward: safe where the ranges meet */
    v->base += v->at; v->at = 0; v->have = n;
    want = VID_WINDOW - n;
    if (v->base + n + want > v->size) { want = v->size - v->base - n; }
    if (want && fs_read_at(v->file, v->base + n, v->window + n, want)) { v->have += want; }
}

/* Makes n bytes readable at the cursor (fewer at the end of the file; reads past it give zeros). */
static void need(Vid *v, df_u32 n)
{
    if (v->have - v->at < n) { refill(v); }
}

static int byte_at(Vid *v) { need(v, 1); return v->at < v->have ? v->window[v->at++] : 0; }
static int word_at(Vid *v) { int lo; lo = byte_at(v); return lo | byte_at(v) << 8; }

/* The end of a row of the frame: 1 when it was the last. */
typedef struct Rows { df_u8 *dst; int w, left, row_w; } Rows;
static int row_done(Rows *r)
{
    if (--r->left <= 0) { return 1; }
    r->w = r->row_w;
    return 0;
}

static void decode_delta(Vid *v, df_u8 *dst, int rows, int w)
{
    Rows r;
    int b, n, m;

    r.dst = dst; r.w = w; r.row_w = w; r.left = rows;
    for (;;) {
        need(v, COMMAND_MARGIN);
        if (v->at >= v->have) { return; }
        b = v->window[v->at++];
        if (b == 0) { return; }
        n = b & CMD_COUNT;
        if (b & CMD_RUN) {
            if (n == 0) { continue; }
            while (n >= r.w) {
                r.dst += r.w; n -= r.w;
                if (row_done(&r)) { if (v->version >= 2) { v->at++; } return; }
            }
            r.dst += n; r.w -= n;
        } else {
            while (n >= r.w) {
                m = r.w;
                mem_copy(r.dst, v->window + v->at, (df_u32)m);
                r.dst += m; v->at += (df_u32)m;
                if (row_done(&r)) { if (v->version >= 2) { v->at++; } return; }
                n -= m;
                if (n == 0) { break; }
            }
            if (n != 0 && n < r.w) { mem_copy(r.dst, v->window + v->at, (df_u32)n); r.dst += n; v->at += (df_u32)n; r.w -= n; }
        }
    }
}

static void decode_rle(Vid *v, df_u8 *dst, int rows, int w)
{
    Rows r;
    int b, n, m, colour;

    r.dst = dst; r.w = w; r.row_w = w; r.left = rows;
    for (;;) {
        need(v, COMMAND_MARGIN);
        if (v->at >= v->have) { return; }
        b = v->window[v->at++];
        n = b & CMD_COUNT;
        if (n == 0) { continue; }
        if (b & CMD_RUN) {
            colour = v->window[v->at++];
            while (n >= r.w) {
                m = r.w;
                mem_fill(r.dst, colour, (df_u32)m); r.dst += m;
                if (row_done(&r)) { return; }
                n -= m;
                if (n == 0) { break; }
            }
            if (n != 0 && n < r.w) { mem_fill(r.dst, colour, (df_u32)n); r.dst += n; r.w -= n; }
        } else {
            while (n >= r.w) {
                m = r.w;
                mem_copy(r.dst, v->window + v->at, (df_u32)m);
                r.dst += m; v->at += (df_u32)m;
                if (row_done(&r)) { return; }
                n -= m;
                if (n == 0) { break; }
            }
            if (n != 0 && n < r.w) { mem_copy(r.dst, v->window + v->at, (df_u32)n); r.dst += n; v->at += (df_u32)n; r.w -= n; }
        }
    }
}

static void read_audio(Vid *v)
{
    int size;

    size = word_at(v);
    need(v, (df_u32)size);
    if (v->have - v->at < (df_u32)size) { size = (int)(v->have - v->at); }
    if (v->audio && v->audio_rate > 0) { v->audio(v->audio_ctx, v->window + v->at, size, v->audio_rate); }
    v->at += (df_u32)size;
}

static void read_palette(Vid *v)
{
    int i;

    need(v, VID_PAL_BYTES);
    for (i = 0; i < VID_PAL_BYTES; i++) {
        /* The DAC's six-bit values, widened by repeating the top bits. */
        int c;
        c = v->at < v->have ? v->window[v->at++] & 63 : 0;
        v->palette[i] = (df_u8)(c << PAL_SIX_BITS_SHIFT | c >> 4);
    }
    v->palette_changed = 1;
}

int vid_open(Vid *v, const char *rel)
{
    df_u8 head[VID_HEADER_BYTES];

    mem_zero(v, sizeof(*v));
    v->file = fs_open_media(rel);
    if (!v->file) { return 0; }
    v->size = fs_size(v->file);
    if (v->size < VID_HEADER_BYTES || !fs_read_at(v->file, 0, head, VID_HEADER_BYTES) || head[0] != 'V' || head[1] != 'I' || head[2] != 'D') {
        fs_close(v->file); v->file = 0; return 0;
    }
    v->version = head[4]; v->frames = head[5] | head[6] << 8; v->width = head[7] | head[8] << 8; v->height = head[9] | head[10] << 8;
    v->delay = head[11] | head[12] << 8; v->flags = head[13] | head[14] << 8;
    if (v->width < 1 || v->height < 1 || v->width > 1024 || v->height > 1024) { fs_close(v->file); v->file = 0; return 0; }
    /* The window and the frame are kept for the process and reused by the next movie (a temp heap must not own them). */
    {
        static df_u8 *window, *frame;
        static df_u32 frame_cap;
        df_u32 need_frame;

        need_frame = (df_u32)(v->width * v->height);
        if (!window) { window = (df_u8 *)mem_keep(VID_WINDOW); }
        if (need_frame > frame_cap) { frame = (df_u8 *)mem_keep(need_frame); frame_cap = frame ? need_frame : 0; }
        v->window = window; v->frame = frame;
    }
    if (!v->window || !v->frame) { vid_close(v); return 0; }
    mem_zero(v->frame, (df_u32)(v->width * v->height));
    v->base = VID_HEADER_BYTES; v->have = 0; v->at = 0;
    return 1;
}

void vid_close(Vid *v)
{
    if (v->file) { fs_close(v->file); v->file = 0; }
    v->window = 0; v->frame = 0;
}

void vid_set_audio(Vid *v, VidAudio fn, void *ctx) { v->audio = fn; v->audio_ctx = ctx; }

int vid_next(Vid *v)
{
    int type, w, start, rows, delay;

    if (!v->file || v->frames_done >= v->frames) { return 0; }
    v->palette_changed = 0;
    w = v->width;
    for (;;) {
        need(v, 1);
        if (v->at >= v->have) { return 0; }
        type = v->window[v->at++];
        rows = v->height;
        switch (type) {
        case CHUNK_PALETTE: read_palette(v); continue;
        case CHUNK_AUDIO_HEAD:
            v->audio_delay = word_at(v);
            v->audio_rate = AUDIO_RATE_NUM / (AUDIO_RATE_BASE - byte_at(v));
            read_audio(v);
            continue;
        case CHUNK_AUDIO: read_audio(v); continue;
        case CHUNK_RAW:
            delay = word_at(v);
            for (; rows > 0; rows--) {
                need(v, (df_u32)w);
                mem_copy(v->frame + (v->height - rows) * w, v->window + v->at, (df_u32)w); v->at += (df_u32)w;
            }
            break;
        case CHUNK_DELTA: delay = word_at(v); decode_delta(v, v->frame, rows, w); break;
        case CHUNK_DELTA_FROM:
            delay = word_at(v); start = word_at(v);
            if (start >= rows) { start = rows - 1; }
            decode_delta(v, v->frame + start * w, rows - start, w);
            break;
        case CHUNK_RLE: delay = word_at(v); decode_rle(v, v->frame, rows, w); break;
        default: v->frames_done = v->frames; return 0;
        }
        v->ticks = delay + v->delay;
        v->frames_done++;
        /* The audio that goes with the frame follows at once. */
        for (;;) {
            need(v, 1);
            if (v->at >= v->have) { break; }
            type = v->window[v->at];
            if (type != CHUNK_AUDIO_HEAD && type != CHUNK_AUDIO) { break; }
            v->at++;
            if (type == CHUNK_AUDIO_HEAD) { v->audio_delay = word_at(v); v->audio_rate = AUDIO_RATE_NUM / (AUDIO_RATE_BASE - byte_at(v)); }
            read_audio(v);
        }
        return 1;
    }
}
