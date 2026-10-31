#include "game/dagger/formats/img.h"
#include "game/dagger/formats/palette.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"

/* See img.h. */

#define HEADER 12

/* BSS uses a frame count where IMG carries its compression and byte count.
 * See Daggerfall Unity's BssFile.ReadHeader/ReadImageData (MIT), and img.h. */
int img_bss_decode(const void *bytes, df_u32 size, ImgAnimation *out)
{
    const df_u8 *b;
    ImgAnimation a;
    df_u32 frame;

    if (!bytes || !out || size < 10u) {
        return 0;
    }
    b = (const df_u8 *)bytes;
    a.x = df_ri16(b);
    a.y = df_ri16(b + 2);
    a.width = df_ri16(b + 4);
    a.height = df_ri16(b + 6);
    a.frames = df_ri16(b + 8);
    if (a.width < 1 || a.width > 1024 || a.height < 1 || a.height > 1024 || a.frames < 1) {
        return 0;
    }
    frame = (df_u32)a.width * (df_u32)a.height;
    /* Divide the available bytes: multiplying an untrusted frame count could
     * wrap and accept a truncated file. Publish nothing until it all fits. */
    if ((df_u32)a.frames > (size - 10u) / frame) {
        return 0;
    }
    a.pixels = b + 10;
    *out = a;
    return 1;
}

int img_load_bss(const char *name, ImgAnimation *out)
{
    const df_u8 *b;
    df_u32 size;

    b = vfs_read(name, &size);
    return b && img_bss_decode(b, size, out);
}

int img_bss_frame(const ImgAnimation *anim, int index, ImgPicture *out)
{
    if (!anim || !out || index < 0 || index >= anim->frames) {
        return 0;
    }
    out->x = anim->x;
    out->y = anim->y;
    out->width = anim->width;
    out->height = anim->height;
    out->palette = 0;
    out->pixels = anim->pixels + (df_u32)index * (df_u32)anim->width * (df_u32)anim->height;
    return 1;
}

/* A headerless IMG's size by its byte count (DFU's ImgFile), or 0. */
static int headerless(df_u32 n, int *w, int *h)
{
    static const struct {
        df_u32 bytes;
        df_u16 w;
        df_u16 h;
    } sizes[] = {
        { 44, 22, 22 }, { 289, 17, 17 }, { 441, 49, 9 }, { 512, 32, 16 }, { 720, 9, 80 },
        { 990, 45, 22 }, { 1720, 43, 40 }, { 2140, 107, 20 }, { 2916, 81, 36 }, { 3200, 40, 80 },
        { 3938, 179, 22 }, { 4280, 107, 40 }, { 4508, 322, 14 }, { 20480, 320, 64 },
        { 26496, 184, 144 }, { 64000, 320, 200 }, { 64768, 320, 200 }, { 68800, 320, 215 },
        { 112128, 512, 219 }
    };
    int i;

    for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++) {
        if (sizes[i].bytes == n) {
            *w = sizes[i].w;
            *h = sizes[i].h;
            return 1;
        }
    }
    return 0;
}

int img_load(const char *name, ImgPicture *out)
{
    const df_u8 *b;
    df_u32 n;

    b = vfs_read(name, &n);
    if (!b) {
        log_print(LOG_WARN, "img: %s did not read", name);
        return 0;
    }
    out->palette = 0;
    if (headerless(n, &out->width, &out->height)) {
        out->x = 0;
        out->y = 0;
        out->pixels = b;
        /* A full screen and 768 bytes more: its own palette after the pixels. */
        if (n == (df_u32)out->width * (df_u32)out->height + 768u) {
            out->palette = b + (df_u32)out->width * (df_u32)out->height;
        }
        return 1;
    }
    if (n < HEADER) {
        return 0;
    }
    out->x = df_ri16(b);
    out->y = df_ri16(b + 2);
    out->width = df_ri16(b + 4);
    out->height = df_ri16(b + 6);
    if (df_ru16(b + 8) != 0) {
        log_print(LOG_WARN, "img: %s is compressed, not read yet", name);
        return 0;
    }
    if (out->width < 1 || out->height < 1 || (df_u32)out->width * (df_u32)out->height > n - HEADER) {
        log_print(LOG_WARN, "img: %s is shorter than its size", name);
        return 0;
    }
    out->pixels = b + HEADER;
    return 1;
}

int img_load_raw(const char *name, int width, int height, ImgPicture *out)
{
    const df_u8 *b;
    df_u32 n;

    b = vfs_read(name, &n);
    if (!b || n < (df_u32)width * (df_u32)height) {
        log_print(LOG_WARN, "img: %s did not read", name);
        return 0;
    }
    out->x = 0;
    out->y = 0;
    out->width = width;
    out->height = height;
    out->pixels = b;
    out->palette = 0;
    return 1;
}

int img_palette(const ImgPicture *pic, df_u8 *rgb768)
{
    int i;

    if (!pic->palette) {
        return img_art_palette(rgb768);
    }
    for (i = 0; i < 768; i++) {
        int v;

        v = (int)pic->palette[i] * 4;
        rgb768[i] = (df_u8)(v > 255 ? 255 : v);
    }
    return 1;
}

int img_art_palette(df_u8 *rgb768)
{
    const df_u8 *b;
    df_u32 n;
    Palette pal;
    int i;

    b = vfs_read("ART_PAL.COL", &n);
    if (!b || !palette_load(b, n, &pal)) {
        return 0;
    }
    for (i = 0; i < 256; i++) {
        rgb768[i * 3] = pal.r[i];
        rgb768[i * 3 + 1] = pal.g[i];
        rgb768[i * 3 + 2] = pal.b[i];
    }
    return 1;
}
