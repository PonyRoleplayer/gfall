#include "game/dagger/texture.h"
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

static const df_u8 *file_bytes(int file, df_u32 *size)
{
    char name[16];

    if (file < 0 || file >= FILE_CACHE) {
        return 0;
    }
    if (!g_file[file]) {
        texture_filename(file, name, (int)sizeof(name));
        g_file[file] = vfs_read(name, &g_file_size[file]);
        if (!g_file[file]) {
            return 0;
        }
    }
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

static int read_frames(const df_u8 *file, df_u32 file_size, df_u32 data, int frames, Image **out)
{
    df_u32 off0;
    int cx;
    int cy;
    df_u32 src;
    df_u32 di;
    int y;
    Image *img;

    if (frames < 2 || frames > 256) {
        return 0;
    }
    if (data + (df_u32)frames * 4 + 4 > file_size) {
        return 0;
    }
    off0 = df_ru32(file + data);
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

int texture_record_count(int file)
{
    const df_u8 *bytes;
    df_u32 size;
    int count;

    if (file <= 1) {
        return 0;
    }
    bytes = file_bytes(file, &size);
    if (!bytes || size < 2) {
        return 0;
    }
    count = df_ri16(bytes);
    if (count < 0 || count > 1024) {
        return 0;
    }
    return count;
}

Image *texture_load(int file, int image)
{
    const df_u8 *bytes;
    df_u32 size;
    df_i32 count;
    df_u32 header;
    df_i32 rec_pos;
    int w;
    int h;
    int comp;
    int frames;
    int xs;
    int ys;
    df_u32 data_off;
    Image *img;

    if (file < 0) {
        return 0;
    }
    if (file == 0 || file == 1) {
        return solid_image(file, image);
    }
    bytes = file_bytes(file, &size);
    if (!bytes || size < 26) {
        return 0;
    }
    count = df_ri16(bytes);
    if (image < 0 || image >= count || count > 1024) {
        return 0;
    }
    header = 26 + (df_u32)image * 20;
    if (header + 20 > size) {
        return 0;
    }
    rec_pos = df_ri32(bytes + header + 2);
    if (rec_pos < 0 || (df_u32)rec_pos + 28 > size) {
        return 0;
    }
    w = df_ri16(bytes + rec_pos + 4);
    h = df_ri16(bytes + rec_pos + 6);
    comp = df_ri16(bytes + rec_pos + 8);
    data_off = df_ru32(bytes + rec_pos + 14);
    frames = df_ri16(bytes + rec_pos + 20);
    xs = df_ri16(bytes + rec_pos + 24);
    ys = df_ri16(bytes + rec_pos + 26);
    if (frames < 1) {
        return 0;
    }
    if ((df_u32)rec_pos + data_off > size) {
        return 0;
    }
    if (comp == COMP_IMAGE_RLE || comp == COMP_RECORD_RLE) {
        if (!read_rle(bytes, size, (df_u32)rec_pos, (df_u32)rec_pos + data_off, w, h, 0, &img)) {
            return 0;
        }
        img->xscale = xs;
        img->yscale = ys;
        return img;
    }
    if (frames == 1) {
        img = image_new(w, h);
        if (!img) {
            return 0;
        }
        if (!read_uncompressed(bytes, size, (df_u32)rec_pos + data_off, w, h, img->index)) {
            return 0;
        }
        scale_pot(img);
        if (!img->pot) {
            return 0;
        }
        img->xscale = xs;
        img->yscale = ys;
        return img;
    }
    if (!read_frames(bytes, size, (df_u32)rec_pos + data_off, frames, &img)) {
        return 0;
    }
    img->xscale = xs;
    img->yscale = ys;
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
void texture_smooth(Image *img, const df_u8 *pr, const df_u8 *pg, const df_u8 *pb)
{
    int side;
    int tw;
    int th;
    int x;
    int y;

    if (!img || !img->pot || !img->index || !pr || !pg || !pb || img->pot_w != img->pot_h) {
        return;
    }
    side = img->pot_w;
    tw = img->span_w;
    th = img->span_h;
    if (tw < 1 || th < 1 || (tw == img->width && th == img->height)) {
        return;
    }
    mip_palette(pr, pg, pb);
    for (y = 0; y < side; y++) {
        int fy;
        int y0;
        int y1;
        int wy;

        /* Source position of this texel's centre, in 1/256 texel. */
        fy = (((y % th) * 2 + 1) * img->height * 128) / th - 128;
        if (fy < 0) {
            fy += img->height * 256;
        }
        y0 = (fy >> 8) % img->height;
        y1 = (y0 + 1) % img->height;
        wy = fy & 255;
        for (x = 0; x < side; x++) {
            int fx;
            int x0;
            int x1;
            int wx;
            const df_u8 *row0;
            const df_u8 *row1;
            int k[4];
            int w4[4];
            int r;
            int g;
            int b;
            int i;

            fx = (((x % tw) * 2 + 1) * img->width * 128) / tw - 128;
            if (fx < 0) {
                fx += img->width * 256;
            }
            x0 = (fx >> 8) % img->width;
            x1 = (x0 + 1) % img->width;
            wx = fx & 255;
            row0 = img->index + y0 * img->width;
            row1 = img->index + y1 * img->width;
            if (wx == 0 && wy == 0) {
                img->pot[y * side + x] = row0[x0];
                continue;
            }
            k[0] = row0[x0];
            k[1] = row0[x1];
            k[2] = row1[x0];
            k[3] = row1[x1];
            w4[0] = (256 - wx) * (256 - wy);
            w4[1] = wx * (256 - wy);
            w4[2] = (256 - wx) * wy;
            w4[3] = wx * wy;
            r = 0;
            g = 0;
            b = 0;
            for (i = 0; i < 4; i++) {
                r += pr[k[i]] * w4[i];
                g += pg[k[i]] * w4[i];
                b += pb[k[i]] * w4[i];
            }
            img->pot[y * side + x] = mip_nearest((r + 32768) >> 16, (g + 32768) >> 16,
                                                 (b + 32768) >> 16);
        }
    }
}

void texture_cache_clear(void)
{
    int i;

    for (i = 0; i < FILE_CACHE; i++) {
        g_file[i] = 0;
        g_file_size[i] = 0;
    }
}
