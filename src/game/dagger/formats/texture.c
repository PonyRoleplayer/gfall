#include "game/dagger/formats/texture.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "engine/gfx/mip.h"

#define COMP_IMAGE_RLE 0x0108
#define COMP_RECORD_RLE 0x1108
#define ROW_RLE 0x8000
#define SOLID_SIZE 32
#define FILE_CACHE 512

static df_u8 *g_file[FILE_CACHE];
static df_u32 g_file_size[FILE_CACHE];
/*
 * The cap (texture_cache_limit): files then live in g_heap, each read
 * stamped, the oldest freed when the total passes the cap.
 */
static df_u32 g_limit;
static df_u32 g_total;
static df_u32 g_stamp;
static df_u32 g_used_at[FILE_CACHE];
/* 1 for a file read under the cap, into g_heap: only those can be freed one by one. */
static df_u8 g_in_heap[FILE_CACHE];
static MemHeap *g_heap;

void texture_filename(int file, char *dst, int n)
{
    char tmp[13];

    tmp[0] = 'T';
    tmp[1] = 'E';
    tmp[2] = 'X';
    tmp[3] = 'T';
    tmp[4] = 'U';
    tmp[5] = 'R';
    tmp[6] = 'E';
    tmp[7] = '.';
    tmp[8] = (char)('0' + (file / 100) % 10);
    tmp[9] = (char)('0' + (file / 10) % 10);
    tmp[10] = (char)('0' + file % 10);
    tmp[11] = 0;
    df_copy(dst, tmp, n);
}

static int next_pot(int n)
{
    int p;

    if (n < 1) {
        n = 1;
    }
    if (n > 256) {
        n = 256;
    }
    p = 1;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

static int legal_size(int *w, int *h)
{
    int pw;
    int ph;

    pw = next_pot(*w);
    ph = next_pot(*h);
    while (pw * 8 < ph && pw < 256) {
        pw <<= 1;
    }
    while (ph * 8 < pw && ph < 256) {
        ph <<= 1;
    }
    if (pw < 1 || ph < 1 || pw > 256 || ph > 256) {
        return 0;
    }
    if (pw >= ph) {
        if (ph == 0 || (pw / ph) > 8 || (pw % ph) != 0) {
            return 0;
        }
    } else if ((ph / pw) > 8 || (ph % pw) != 0) {
        return 0;
    }
    *w = pw;
    *h = ph;
    return 1;
}

/*
 * Repeat the short side until the map is square. The first pot buffer
 * stays in the arena. One copy of the picture is span_w by span_h
 * texels inside the square.
 */
static void square_pot(Image *img)
{
    int side;
    int src_w;
    int src_h;
    int y;
    df_u8 *sq;

    src_w = img->pot_w;
    src_h = img->pot_h;
    img->span_w = src_w;
    img->span_h = src_h;
    if (src_w == src_h) {
        return;
    }
    side = (src_w > src_h) ? src_w : src_h;
    sq = (df_u8 *)mem_alloc((df_u32)side * (df_u32)side);
    if (!sq) {
        img->pot = 0;
        return;
    }
    for (y = 0; y < side; y++) {
        int x;
        int sy;

        sy = y & (src_h - 1);
        for (x = 0; x < side; x++) {
            int sx;

            sx = x & (src_w - 1);
            sq[y * side + x] = img->pot[sy * src_w + sx];
        }
    }
    img->pot = sq;
    img->pot_w = side;
    img->pot_h = side;
}

static void scale_pot(Image *img)
{
    int y;
    int x;

    img->pot = (df_u8 *)mem_alloc((df_u32)img->pot_w * (df_u32)img->pot_h);
    if (!img->pot) {
        return;
    }
    for (y = 0; y < img->pot_h; y++) {
        int sy;

        sy = y * img->height / img->pot_h;
        for (x = 0; x < img->pot_w; x++) {
            int sx;

            sx = x * img->width / img->pot_w;
            img->pot[y * img->pot_w + x] = img->index[sy * img->width + sx];
        }
    }
    square_pot(img);
}

static Image *image_new(int w, int h)
{
    Image *img;
    int pw;
    int ph;

    if (w < 1 || h < 1 || w > 256 || h > 256) {
        return 0;
    }
    pw = w;
    ph = h;
    if (!legal_size(&pw, &ph)) {
        return 0;
    }
    img = (Image *)mem_alloc(sizeof(Image));
    if (!img) {
        return 0;
    }
    img->width = w;
    img->height = h;
    img->pot_w = pw;
    img->pot_h = ph;
    img->index = (df_u8 *)mem_alloc((df_u32)w * (df_u32)h);
    if (!img->index) {
        return 0;
    }
    return img;
}

static Image *solid_image(int file, int image)
{
    Image *img;
    df_u8 colour;
    df_u32 n;
    df_u32 i;

    if (image < 0 || image > 255) {
        return 0;
    }
    img = image_new(SOLID_SIZE, SOLID_SIZE);
    if (!img) {
        return 0;
    }
    colour = (file == 0) ? (df_u8)image : (df_u8)(128 + image);
    n = (df_u32)SOLID_SIZE * (df_u32)SOLID_SIZE;
    for (i = 0; i < n; i++) {
        img->index[i] = colour;
    }
    scale_pot(img);
    if (!img->pot) {
        return 0;
    }
    return img;
}

/* A file now read whole: counted in, and past the cap the files read longest ago go, never this one. */
static void keep_file(int file)
{
    g_total += g_file_size[file];
    while (g_limit && g_heap && g_total > g_limit) {
        int old;
        int i;

        old = -1;
        for (i = 0; i < FILE_CACHE; i++) {
            if (g_file[i] && g_in_heap[i] && i != file
                && (old < 0 || g_used_at[i] < g_used_at[old])) {
                old = i;
            }
        }
        if (old < 0) {
            break;
        }
        mem_heap_free(g_heap, g_file[old]);
        g_total -= g_file_size[old];
        g_file[old] = 0;
        g_file_size[old] = 0;
        g_in_heap[old] = 0;
    }
}

/*
 * The file being read a piece a call (texture_file_step), one at a time,
 * into g_heap: its number (-1 none), its bytes so far, its handle.
 */
static int g_part_file = -1;
static df_u8 *g_part;
static df_u32 g_part_size;
static df_u32 g_part_at;
static VfsFile *g_part_f;
/* 1 when the pieces go into g_heap (under the cap); else the current heap, which cannot free them one by one. */
static int g_part_in_heap;

static void part_drop(void)
{
    if (g_part_f) {
        vfs_close(g_part_f);
    }
    if (g_part && g_heap && g_part_in_heap) {
        mem_heap_free(g_heap, g_part);
    }
    g_part_f = 0;
    g_part = 0;
    g_part_file = -1;
}

int texture_file_step(int file)
{
    char name[16];
    df_u32 n;

    /* Cached, or no such file: nothing to do. */
    if (file <= 1 || file >= FILE_CACHE || g_file[file]) {
        return 1;
    }
    if (g_part_file != file) {
        MemHeap *was;

        part_drop();
        texture_filename(file, name, (int)sizeof(name));
        g_part_f = vfs_open(name);
        if (!g_part_f) {
            return 1;
        }
        g_part_size = vfs_size(g_part_f);
        /* Under the cap, into the cache's own heap; a place loaded whole, into its heap, as vfs_read would. */
        g_part_in_heap = g_limit != 0;
        if (g_part_in_heap && !g_heap) {
            g_heap = mem_heap_new();
        }
        was = g_part_in_heap ? mem_heap_use(g_heap) : 0;
        g_part = g_part_size ? (df_u8 *)mem_alloc(g_part_size) : 0;
        if (g_part_in_heap) {
            mem_heap_use(was);
        }
        if (!g_part) {
            part_drop();
            return 1;
        }
        g_part_file = file;
        g_part_at = 0;
    }
    n = g_part_size - g_part_at;
    n = n > TEXTURE_FILE_PIECE ? TEXTURE_FILE_PIECE : n;
    if (!vfs_read_at(g_part_f, g_part_at, g_part + g_part_at, n)) {
        part_drop();
        return 1;
    }
    g_part_at += n;
    if (g_part_at < g_part_size) {
        return 0;
    }
    /* Whole: it is the cache's now. */
    vfs_close(g_part_f);
    g_part_f = 0;
    g_file[file] = g_part;
    g_file_size[file] = g_part_size;
    g_in_heap[file] = (df_u8)g_part_in_heap;
    g_used_at[file] = ++g_stamp;
    g_part = 0;
    g_part_file = -1;
    keep_file(file);
    /* A piece was read this call: the decode is the next call's, not this one's too. */
    return 0;
}

static const df_u8 *file_bytes(int file, df_u32 *size)
{
    char name[16];

    if (file < 0 || file >= FILE_CACHE) {
        return 0;
    }
    if (!g_file[file]) {
        if (g_part_file == file) {
            /* Wanted whole now: the pieces read so far are dropped, it is read at once. */
            part_drop();
        }
        texture_filename(file, name, (int)sizeof(name));
        if (g_limit) {
            MemHeap *was;

            if (!g_heap) {
                g_heap = mem_heap_new();
            }
            was = mem_heap_use(g_heap);
            g_file[file] = vfs_read(name, &g_file_size[file]);
            mem_heap_use(was);
            g_in_heap[file] = 1;
        } else {
            g_file[file] = vfs_read(name, &g_file_size[file]);
        }
        if (!g_file[file]) {
            return 0;
        }
        keep_file(file);
    }
    g_used_at[file] = ++g_stamp;
    if (size) {
        *size = g_file_size[file];
    }
    return g_file[file];
}

static int read_uncompressed(const df_u8 *file, df_u32 file_size, df_u32 pos, int w, int h, df_u8 *dst)
{
    int y;
    df_u32 src;
    df_u32 di;

    src = pos;
    di = 0;
    for (y = 0; y < h; y++) {
        if (src + (df_u32)w > file_size) {
            return 0;
        }
        mem_copy(dst + di, file + src, (df_u32)w);
        di += (df_u32)w;
        src += 256;
    }
    return 1;
}

/*
 * A multi-frame record that is not row-RLE: a table of one 32-bit offset
 * per frame, each to a frame of its own width and height, rows of zero
 * runs and raw runs. frame picks one.
 */
static int read_frames(const df_u8 *file, df_u32 file_size, df_u32 data, int frames, int frame,
                       Image **out)
{
    df_u32 off0;
    int cx;
    int cy;
    df_u32 src;
    df_u32 di;
    int y;
    Image *img;

    if (frames < 2 || frames > 256 || frame < 0 || frame >= frames) {
        return 0;
    }
    if (data + (df_u32)frames * 4 + 4 > file_size) {
        return 0;
    }
    off0 = df_ru32(file + data + (df_u32)frame * 4);
    if (data + off0 + 4 > file_size) {
        return 0;
    }
    cx = df_ri16(file + data + off0);
    cy = df_ri16(file + data + off0 + 2);
    img = image_new(cx, cy);
    if (!img) {
        return 0;
    }
    src = data + off0 + 4;
    di = 0;
    for (y = 0; y < cy; y++) {
        int x;

        x = 0;
        while (x < cx) {
            int run;
            int zero_n;
            int raw_n;
            int i;

            if (src + 2 > file_size) {
                return 0;
            }
            zero_n = file[src++];
            run = x + zero_n;
            if (run > cx) {
                return 0;
            }
            for (; x < run; x++) {
                img->index[di++] = 0;
            }
            raw_n = file[src++];
            run = x + raw_n;
            if (run > cx || src + (df_u32)raw_n > file_size) {
                return 0;
            }
            for (i = 0; i < raw_n; i++) {
                img->index[di++] = file[src++];
                x++;
            }
        }
    }
    if (di != (df_u32)cx * (df_u32)cy) {
        return 0;
    }
    scale_pot(img);
    if (!img->pot) {
        return 0;
    }
    *out = img;
    return 1;
}

static int read_rle(const df_u8 *file, df_u32 file_size, df_u32 rec_pos, df_u32 data,
                     int w, int h, int frame, Image **out)
{
    df_u32 table;
    Image *img;
    df_u32 di;
    int row;

    if (frame < 0 || h < 1 || w < 1) {
        return 0;
    }
    table = data + (df_u32)h * (df_u32)frame * 4;
    if (table + (df_u32)h * 4 > file_size) {
        return 0;
    }
    img = image_new(w, h);
    if (!img) {
        return 0;
    }
    di = 0;
    for (row = 0; row < h; row++) {
        df_i32 row_off;
        df_u16 enc;
        df_u32 pos;

        row_off = df_ri16(file + table + (df_u32)row * 4);
        enc = df_ru16(file + table + (df_u32)row * 4 + 2);
        if (row_off < 0) {
            return 0;
        }
        pos = rec_pos + (df_u32)row_off;
        if (enc == ROW_RLE) {
            df_u32 row_w;
            df_u32 row_pos;

            if (pos + 2 > file_size) {
                return 0;
            }
            row_w = df_ru16(file + pos);
            pos += 2;
            row_pos = 0;
            while (row_pos < row_w) {
                int probe;

                if (pos + 2 > file_size) {
                    return 0;
                }
                probe = df_ri16(file + pos);
                pos += 2;
                if (probe < 0) {
                    int n;
                    df_u8 pixel;
                    int i;

                    n = -probe;
                    if (pos >= file_size) {
                        return 0;
                    }
                    pixel = file[pos++];
                    for (i = 0; i < n; i++) {
                        if (di >= (df_u32)w * (df_u32)h) {
                            return 0;
                        }
                        img->index[di++] = pixel;
                        row_pos++;
                    }
                } else if (probe > 0) {
                    int i;

                    if (pos + (df_u32)probe > file_size) {
                        return 0;
                    }
                    for (i = 0; i < probe; i++) {
                        if (di >= (df_u32)w * (df_u32)h) {
                            return 0;
                        }
                        img->index[di++] = file[pos++];
                        row_pos++;
                    }
                } else {
                    return 0;
                }
            }
        } else {
            if (pos + (df_u32)w > file_size || di + (df_u32)w > (df_u32)w * (df_u32)h) {
                return 0;
            }
            mem_copy(img->index + di, file + pos, (df_u32)w);
            di += (df_u32)w;
        }
    }
    scale_pot(img);
    if (!img->pot) {
        return 0;
    }
    *out = img;
    return 1;
}

/*
 * Each file's record count once known (0 none, else count + 1): the
 * climate swaps ask it of every face a mesh paints, and reading the
 * whole file for its first two bytes put a file read in a mesh's step.
 */
static short g_count[FILE_CACHE];

int texture_record_count(int file)
{
    const df_u8 *bytes;
    df_u8 head[2];
    df_u32 size;
    int count;

    if (file <= 1 || file >= FILE_CACHE) {
        return 0;
    }
    if (g_count[file]) {
        return g_count[file] - 1;
    }
    if (g_file[file]) {
        bytes = file_bytes(file, &size);
        if (!bytes || size < 2) {
            return 0;
        }
        count = df_ri16(bytes);
    } else {
        char name[16];
        VfsFile *f;

        texture_filename(file, name, (int)sizeof(name));
        f = vfs_open(name);
        count = 0;
        /* A file not there, or too short, is remembered as none: the swaps ask of missing ones often. */
        if (f && vfs_size(f) >= 2 && vfs_read_at(f, 0, head, 2)) {
            count = df_ri16(head);
        }
        vfs_close(f);
    }
    if (count < 0 || count > 1024) {
        count = 0;
    }
    g_count[file] = (short)(count + 1);
    return count;
}

/* The record's header: its size, compression, data offset, frames and scale. */
typedef struct RecHead {
    df_i32 pos;
    int w;
    int h;
    int comp;
    df_u32 data;
    int frames;
    int xs;
    int ys;
} RecHead;

static const df_u8 *record_head(int file, int image, df_u32 *size, RecHead *r)
{
    const df_u8 *bytes;
    df_i32 count;
    df_u32 header;

    if (file <= 1) {
        return 0;
    }
    bytes = file_bytes(file, size);
    if (!bytes || *size < 26) {
        return 0;
    }
    count = df_ri16(bytes);
    if (image < 0 || image >= count || count > 1024) {
        return 0;
    }
    header = 26 + (df_u32)image * 20;
    if (header + 20 > *size) {
        return 0;
    }
    r->pos = df_ri32(bytes + header + 2);
    if (r->pos < 0 || (df_u32)r->pos + 28 > *size) {
        return 0;
    }
    r->w = df_ri16(bytes + r->pos + 4);
    r->h = df_ri16(bytes + r->pos + 6);
    r->comp = df_ri16(bytes + r->pos + 8);
    r->data = df_ru32(bytes + r->pos + 14);
    r->frames = df_ri16(bytes + r->pos + 20);
    r->xs = df_ri16(bytes + r->pos + 24);
    r->ys = df_ri16(bytes + r->pos + 26);
    if (r->frames < 1 || (df_u32)r->pos + r->data > *size) {
        return 0;
    }
    return bytes;
}

int texture_offset(int file, int image, int *x, int *y)
{
    const df_u8 *bytes;
    df_u32 size;
    RecHead r;

    bytes = record_head(file, image, &size, &r);
    if (!bytes) {
        return 0;
    }
    *x = df_ri16(bytes + r.pos);
    *y = df_ri16(bytes + r.pos + 2);
    return 1;
}

int texture_cached(int file)
{
    return file >= 0 && file < FILE_CACHE && g_file[file] != 0;
}

void texture_forget(int file)
{
    if (file < 0 || file >= FILE_CACHE || !g_file[file] || g_in_heap[file]) {
        return;
    }
    g_total -= g_file_size[file];
    g_file[file] = 0;
    g_file_size[file] = 0;
}

int texture_frame_count(int file, int image)
{
    df_u32 size;
    RecHead r;

    if (!record_head(file, image, &size, &r)) {
        return 1;
    }
    return r.frames;
}

Image *texture_load(int file, int image)
{
    return texture_load_frame(file, image, 0);
}

Image *texture_load_frame(int file, int image, int frame)
{
    const df_u8 *bytes;
    df_u32 size;
    RecHead r;
    Image *img;

    if (file < 0) {
        return 0;
    }
    if (file == 0 || file == 1) {
        return frame == 0 ? solid_image(file, image) : 0;
    }
    bytes = record_head(file, image, &size, &r);
    if (!bytes || frame < 0 || frame >= r.frames) {
        return 0;
    }
    if (r.comp == COMP_IMAGE_RLE || r.comp == COMP_RECORD_RLE) {
        /* Row-RLE: each frame is h more row entries in the same table. */
        if (!read_rle(bytes, size, (df_u32)r.pos, (df_u32)r.pos + r.data, r.w, r.h, frame, &img)) {
            return 0;
        }
    } else if (r.frames == 1) {
        img = image_new(r.w, r.h);
        if (!img) {
            return 0;
        }
        if (!read_uncompressed(bytes, size, (df_u32)r.pos + r.data, r.w, r.h, img->index)) {
            return 0;
        }
        scale_pot(img);
        if (!img->pot) {
            return 0;
        }
    } else if (!read_frames(bytes, size, (df_u32)r.pos + r.data, r.frames, frame, &img)) {
        return 0;
    }
    img->xscale = r.xs;
    img->yscale = r.ys;
    return img;
}

/*
 * A sprite: the picture at its own size in the top left of the square,
 * the rest index 0, which is transparent on a cutout. Nothing is
 * stretched, and the texels around the picture are transparent at
 * every mip level, so filtering at an edge reads nothing but the
 * sprite and clear texels. The square's side does not change.
 */
void texture_pad(Image *img)
{
    int side;
    int x;
    int y;

    if (!img || !img->pot || !img->index || img->pot_w != img->pot_h) {
        return;
    }
    side = img->pot_w;
    if (img->width > side || img->height > side) {
        return;
    }
    for (y = 0; y < side; y++) {
        for (x = 0; x < side; x++) {
            img->pot[y * side + x] = (x < img->width && y < img->height)
                ? img->index[y * img->width + x] : 0;
        }
    }
    img->span_w = img->width;
    img->span_h = img->height;
}

/*
 * A wall whose side is not a power of two: scale_pot repeated texels
 * to stretch it, so some columns and rows came out twice as wide. This
 * redoes that stretch bilinearly, wrapping at the edges because a wall
 * repeats, and maps each blended colour to the nearest palette entry.
 * A texel that lands exactly on a source texel keeps its index. The
 * square still holds whole copies, so span and the face UVs stay.
 * Load time only: 8-bit fixed point, no float casts.
 */
/*
 * One axis of the stretch: for each texel of the square, its source
 * texel, the next (wrapping), and the weight of the next, 0..255.
 * Worked once a column or a row, not a texel: two divisions and three
 * remainders a texel were half the stretch's time on the 233.
 */
static void smooth_axis(int side, int span, int size, short *t0, short *t1, short *w)
{
    int i;

    for (i = 0; i < side; i++) {
        int f;

        /* Source position of this texel's centre, in 1/256 texel. */
        f = (((i % span) * 2 + 1) * size * 128) / span - 128;
        if (f < 0) {
            f += size * 256;
        }
        t0[i] = (short)((f >> 8) % size);
        t1[i] = (short)((t0[i] + 1) % size);
        w[i] = (short)(f & 255);
    }
}

void texture_smooth(Image *img, const df_u8 *pr, const df_u8 *pg, const df_u8 *pb)
{
    short x0[256];
    short x1[256];
    short wxs[256];
    short y0s[256];
    short y1s[256];
    short wys[256];
    int side;
    int tw;
    int th;
    int x;
    int y;

    if (!img || !img->pot || !img->index || !pr || !pg || !pb || img->pot_w != img->pot_h
        || img->pot_w > 256) {
        return;
    }
    side = img->pot_w;
    tw = img->span_w;
    th = img->span_h;
    if (tw < 1 || th < 1 || (tw == img->width && th == img->height)) {
        return;
    }
    mip_palette(pr, pg, pb);
    smooth_axis(side, tw, img->width, x0, x1, wxs);
    smooth_axis(side, th, img->height, y0s, y1s, wys);
    for (y = 0; y < side; y++) {
        const df_u8 *row0;
        const df_u8 *row1;
        df_u8 *out;
        int wy;

        row0 = img->index + y0s[y] * img->width;
        row1 = img->index + y1s[y] * img->width;
        wy = wys[y];
        out = img->pot + y * side;
        for (x = 0; x < side; x++) {
            int wx;
            int k0;
            int k1;
            int k2;
            int k3;
            int top;
            int bot;
            int r;
            int g;
            int b;

            wx = wxs[x];
            k0 = row0[x0[x]];
            if (wx == 0 && wy == 0) {
                out[x] = (df_u8)k0;
                continue;
            }
            k1 = row0[x1[x]];
            k2 = row1[x0[x]];
            k3 = row1[x1[x]];
            /*
             * Mostly a window's glass stays glass: the night pass lights
             * that index alone. Its weight only when a corner is glass.
             */
            if (k0 == MIP_GLASS || k1 == MIP_GLASS || k2 == MIP_GLASS || k3 == MIP_GLASS) {
                int glass;

                glass = (k0 == MIP_GLASS ? (256 - wx) * (256 - wy) : 0)
                    + (k1 == MIP_GLASS ? wx * (256 - wy) : 0)
                    + (k2 == MIP_GLASS ? (256 - wx) * wy : 0)
                    + (k3 == MIP_GLASS ? wx * wy : 0);
                if (glass >= 32768) {
                    out[x] = MIP_GLASS;
                    continue;
                }
            }
            /*
             * The four weights' sum, (256 - wx)(256 - wy) a + wx(256 - wy) b
             * + ..., as two lerps across and one down, 256 a + w (b - a):
             * the same integer in three multiplies a channel, not four
             * (and the weights' own four): imul is 10 cycles on a Pentium.
             */
            top = 256 * pr[k0] + wx * (pr[k1] - pr[k0]);
            bot = 256 * pr[k2] + wx * (pr[k3] - pr[k2]);
            r = 256 * top + wy * (bot - top);
            top = 256 * pg[k0] + wx * (pg[k1] - pg[k0]);
            bot = 256 * pg[k2] + wx * (pg[k3] - pg[k2]);
            g = 256 * top + wy * (bot - top);
            top = 256 * pb[k0] + wx * (pb[k1] - pb[k0]);
            bot = 256 * pb[k2] + wx * (pb[k3] - pb[k2]);
            b = 256 * top + wy * (bot - top);
            out[x] = mip_nearest((r + 32768) >> 16, (g + 32768) >> 16, (b + 32768) >> 16);
        }
    }
}

void texture_cache_clear(void)
{
    int i;

    for (i = 0; i < FILE_CACHE; i++) {
        g_file[i] = 0;
        g_file_size[i] = 0;
        g_used_at[i] = 0;
        g_in_heap[i] = 0;
    }
    g_total = 0;
    if (g_part_f) {
        vfs_close(g_part_f);
    }
    g_part_f = 0;
    g_part = 0;
    g_part_file = -1;
    if (g_heap) {
        mem_heap_delete(g_heap);
        g_heap = 0;
    }
}

void texture_cache_limit(df_u32 bytes)
{
    g_limit = bytes;
}

df_u32 texture_cache_bytes(void)
{
    return g_total;
}

int texture_extent(int pixels, int scale)
{
    int change;

    if (pixels < 1) {
        return 0;
    }
    change = (int)((float)pixels * ((float)scale / 256.0f));
    return pixels + change;
}

static int pot_up(int v)
{
    int p;

    p = 1;
    while (p < v) {
        p <<= 1;
    }
    return p;
}

int texture_sheet(Image *img, int file, int image)
{
    Image *frame[TEXTURE_SHEET_FRAMES];
    df_u8 *pot;
    int frames;
    int fw;
    int fh;
    int side;
    int cols;
    int k;

    if (!img) {
        return 0;
    }
    frames = texture_frame_count(file, image);
    if (frames < 2 || frames > TEXTURE_SHEET_FRAMES) {
        return 0;
    }
    fw = 0;
    fh = 0;
    for (k = 0; k < frames; k++) {
        frame[k] = k == 0 ? img : texture_load_frame(file, image, k);
        if (!frame[k]) {
            return 0;
        }
        fw = frame[k]->width > fw ? frame[k]->width : fw;
        fh = frame[k]->height > fh ? frame[k]->height : fh;
    }
    /* The smallest square whose grid of cells holds every frame. */
    for (side = pot_up(fw > fh ? fw : fh); side <= 256; side <<= 1) {
        int rows;

        cols = (side + TEXTURE_SHEET_GAP) / (fw + TEXTURE_SHEET_GAP);
        rows = (side + TEXTURE_SHEET_GAP) / (fh + TEXTURE_SHEET_GAP);
        if (cols * rows >= frames) {
            break;
        }
    }
    if (side > 256) {
        return 0;
    }
    pot = (df_u8 *)mem_alloc((df_u32)side * (df_u32)side);
    if (!pot) {
        return 0;
    }
    for (k = 0; k < frames; k++) {
        int x0;
        int y0;
        int y;

        x0 = (k % cols) * (fw + TEXTURE_SHEET_GAP);
        y0 = (k / cols) * (fh + TEXTURE_SHEET_GAP);
        for (y = 0; y < frame[k]->height; y++) {
            mem_copy(pot + (y0 + y) * side + x0, frame[k]->index + y * frame[k]->width,
                     (df_u32)frame[k]->width);
        }
    }
    img->pot = pot;
    img->pot_w = side;
    img->pot_h = side;
    img->span_w = img->width;
    img->span_h = img->height;
    img->frames = frames;
    img->frame_cols = cols;
    img->frame_dx = fw + TEXTURE_SHEET_GAP;
    img->frame_dy = fh + TEXTURE_SHEET_GAP;
    return 1;
}
