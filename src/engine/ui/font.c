#include "engine/ui/font.h"

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/texcache.h"

/*
 * FONT0000.FNT: uint16 cell width, uint16 cell height, then 240 records
 * of uint16 offset and uint16 glyph width. Each glyph is height rows of
 * a little-endian uint16. Bit 15 is the leftmost pixel. Index 0 is '!'.
 *
 * The built-in face is 8x8, row 0 at the top, bit 7 on the left. It is
 * drawn 1:1. The right column and the bottom row stay clear.
 */

#define FNT_NAME "FONT0000.FNT"
#define BMP_NAME "font0000.bmp"
#define FONT_N 240
#define FONT_FIRST 33
#define FONT_SIDE 256
#define FONT_CELL 16
#define FONT_INK 224
#define FONT_SPACE 5
#define BMP_OFF (14 + 40 + 256 * 4)

#define FALL_CELL 8

static const char *g_why = "FONT0000.FNT is missing";
static df_u8 *g_pixels;
static df_u8 g_width[FONT_N];
static int g_height;
static int g_ready;
static int g_generated;
static int g_tried;
static int g_uploaded;
static df_u32 g_addr;

static const struct {
    char ch;
    df_u8 row[FALL_CELL];
} g_fallback[] = {
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { '-', { 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00 } },
    { ':', { 0x00, 0x20, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00 } },
    { '0', { 0x70, 0x88, 0x98, 0xa8, 0xc8, 0x88, 0x70, 0x00 } },
    { '1', { 0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00 } },
    { '2', { 0x70, 0x88, 0x08, 0x30, 0x40, 0x80, 0xf8, 0x00 } },
    { '3', { 0x70, 0x88, 0x08, 0x30, 0x08, 0x88, 0x70, 0x00 } },
    { '4', { 0x10, 0x30, 0x50, 0x90, 0xf8, 0x10, 0x10, 0x00 } },
    { '5', { 0xf8, 0x80, 0xf0, 0x08, 0x08, 0x88, 0x70, 0x00 } },
    { '6', { 0x30, 0x40, 0x80, 0xf0, 0x88, 0x88, 0x70, 0x00 } },
    { '7', { 0xf8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40, 0x00 } },
    { '8', { 0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70, 0x00 } },
    { '9', { 0x70, 0x88, 0x88, 0x78, 0x08, 0x10, 0x60, 0x00 } },
    { 'a', { 0x30, 0x48, 0x08, 0x38, 0x48, 0x48, 0x38, 0x00 } },
    { 'b', { 0x80, 0x80, 0x80, 0xf0, 0x88, 0x88, 0xf0, 0x00 } },
    { 'c', { 0x38, 0x40, 0x80, 0x80, 0x80, 0x40, 0x38, 0x00 } },
    { 'd', { 0x08, 0x08, 0x08, 0x78, 0x88, 0x88, 0x78, 0x00 } },
    { 'e', { 0x70, 0x88, 0x88, 0xf8, 0x80, 0x88, 0x70, 0x00 } },
    { 'f', { 0x18, 0x20, 0x70, 0x20, 0x20, 0x20, 0x20, 0x00 } },
    { 'g', { 0x70, 0x88, 0x88, 0x78, 0x08, 0x88, 0x70, 0x00 } },
    { 'h', { 0x80, 0x80, 0x80, 0xf0, 0x88, 0x88, 0x88, 0x00 } },
    { 'i', { 0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00 } },
    { 'k', { 0x80, 0x90, 0xa0, 0xc0, 0xa0, 0x90, 0x88, 0x00 } },
    { 'l', { 0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70, 0x00 } },
    { 'm', { 0x88, 0xd8, 0xa8, 0xa8, 0x88, 0x88, 0x88, 0x00 } },
    { 'n', { 0x80, 0x80, 0xf0, 0x88, 0x88, 0x88, 0x88, 0x00 } },
    { 'o', { 0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00 } },
    { 'p', { 0xf0, 0x88, 0x88, 0xf0, 0x80, 0x80, 0x80, 0x00 } },
    { 'r', { 0xf0, 0x88, 0x88, 0xf0, 0xa0, 0x90, 0x88, 0x00 } },
    { 's', { 0x70, 0x88, 0x80, 0x70, 0x08, 0x88, 0x70, 0x00 } },
    { 't', { 0xf8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x10, 0x00 } },
    { 'u', { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00 } },
    { 'w', { 0x88, 0x88, 0x88, 0xa8, 0xa8, 0x50, 0x50, 0x00 } },
    { 'x', { 0x88, 0x50, 0x20, 0x20, 0x50, 0x88, 0x00, 0x00 } },
    { 'y', { 0x88, 0x88, 0x50, 0x20, 0x20, 0x40, 0x80, 0x00 } },
    { 'z', { 0xf8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xf8, 0x00 } },
};

const char *font_error(void)
{
    return g_why;
}

int font_ready(void)
{
    return g_ready;
}

int font_generated(void)
{
    return g_generated;
}

static int exe_file(char *dst, int n, const char *leaf)
{
    char exe[260];
    DWORD len;
    int slash;
    int i;
    int k;

    len = GetModuleFileNameA(0, exe, 260);
    if (len == 0 || len >= 259) {
        return 0;
    }
    exe[len] = 0;
    slash = -1;
    for (i = 0; exe[i]; i++) {
        if (exe[i] == '\\' || exe[i] == '/') {
            slash = i;
        }
    }
    if (slash < 1) {
        return 0;
    }
    k = 0;
    while (leaf[k]) {
        k++;
    }
    if (slash + 1 + k + 1 > n) {
        return 0;
    }
    for (i = 0; i <= slash; i++) {
        dst[i] = exe[i];
    }
    for (i = 0; i < k; i++) {
        dst[slash + 1 + i] = leaf[i];
    }
    dst[slash + 1 + k] = 0;
    return 1;
}

static void put_u16(df_u8 *p, unsigned v)
{
    p[0] = (df_u8)(v & 0xff);
    p[1] = (df_u8)((v >> 8) & 0xff);
}

static void put_u32(df_u8 *p, unsigned v)
{
    p[0] = (df_u8)(v & 0xff);
    p[1] = (df_u8)((v >> 8) & 0xff);
    p[2] = (df_u8)((v >> 16) & 0xff);
    p[3] = (df_u8)((v >> 24) & 0xff);
}

static int decode_fnt(const df_u8 *fnt, df_u32 size)
{
    int height;
    int i;

    if (!fnt || size < 4u + 240u * 4u) {
        g_why = "FONT0000.FNT is too small";
        return 0;
    }
    height = (int)df_ru16(fnt + 2);
    if (height < 1 || height > FONT_CELL) {
        g_why = "FONT0000.FNT height is unusable";
        return 0;
    }
    g_pixels = (df_u8 *)mem_alloc(FONT_SIDE * FONT_SIDE);
    if (!g_pixels) {
        g_why = "out of memory for the font image";
        return 0;
    }
    g_height = height;
    for (i = 0; i < FONT_N; i++) {
        int rec;
        int off;
        int w;
        int col;
        int row;
        int y;
        int x;

        rec = 4 + i * 4;
        off = (int)df_ru16(fnt + rec);
        w = (int)df_ru16(fnt + rec + 2);
        if (w > FONT_CELL) {
            w = FONT_CELL;
        }
        g_width[i] = (df_u8)w;
        if (w < 1) {
            continue;
        }
        if ((df_u32)off + (df_u32)height * 2u > size) {
            g_width[i] = 0;
            continue;
        }
        col = i & 15;
        row = i >> 4;
        for (y = 0; y < height; y++) {
            df_u16 bits;

            bits = df_ru16(fnt + off + y * 2);
            for (x = 0; x < w; x++) {
                if (bits & (df_u16)(1u << (15 - x))) {
                    g_pixels[(row * FONT_CELL + y) * FONT_SIDE
                        + col * FONT_CELL + x] = FONT_INK;
                }
            }
        }
    }
    return 1;
}

static int write_bmp(const char *path)
{
    df_u8 *file;
    df_u32 bytes;
    int y;
    int x;
    HANDLE out;
    DWORD wrote;
    int ok;

    bytes = BMP_OFF + FONT_SIDE * FONT_SIDE + FONT_N;
    file = (df_u8 *)mem_alloc(bytes);
    if (!file) {
        g_why = "out of memory for the font image";
        return 0;
    }
    file[0] = 'B';
    file[1] = 'M';
    put_u32(file + 2, bytes);
    put_u32(file + 10, BMP_OFF);
    put_u32(file + 14, 40);
    put_u32(file + 18, FONT_SIDE);
    put_u32(file + 22, FONT_SIDE);
    put_u16(file + 26, 1);
    put_u16(file + 28, 8);
    put_u32(file + 34, FONT_SIDE * FONT_SIDE);
    put_u32(file + 42, (unsigned)g_height);
    put_u32(file + 46, 256);
    /* Index 0 is black. Index 1 is the letter, so Paint shows the sheet. */
    file[BMP_OFF - 256 * 4 + 4 + 0] = 199;
    file[BMP_OFF - 256 * 4 + 4 + 1] = 255;
    file[BMP_OFF - 256 * 4 + 4 + 2] = 254;
    for (y = 0; y < FONT_SIDE; y++) {
        df_u8 *dst;

        dst = file + BMP_OFF + (FONT_SIDE - 1 - y) * FONT_SIDE;
        for (x = 0; x < FONT_SIDE; x++) {
            dst[x] = g_pixels[y * FONT_SIDE + x] ? 1 : 0;
        }
    }
    for (x = 0; x < FONT_N; x++) {
        file[BMP_OFF + FONT_SIDE * FONT_SIDE + x] = g_width[x];
    }
    out = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (out == INVALID_HANDLE_VALUE) {
        g_why = "could not write font0000.bmp";
        return 0;
    }
    ok = WriteFile(out, file, bytes, &wrote, 0) && wrote == bytes;
    CloseHandle(out);
    if (!ok) {
        g_why = "could not write font0000.bmp";
        return 0;
    }
    return 1;
}

int font_ensure(void)
{
    char path[260];
    df_u8 *fnt;
    df_u32 size;

    if (g_tried) {
        return g_ready;
    }
    g_tried = 1;
    g_ready = 0;
    g_generated = 0;
    g_pixels = 0;
    g_height = 0;
    fnt = vfs_read(FNT_NAME, &size);
    if (!fnt) {
        g_why = "FONT0000.FNT is missing";
        return 0;
    }
    if (!decode_fnt(fnt, size)) {
        return 0;
    }
    g_ready = 1;
    g_why = 0;
    if (!exe_file(path, (int)sizeof(path), BMP_NAME)) {
        return 1;
    }
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        return 1;
    }
    if (write_bmp(path)) {
        g_generated = 1;
        g_why = 0;
    }
    return 1;
}

void font_upload(const df_u8 *r, const df_u8 *g, const df_u8 *b)
{
    g_uploaded = 0;
    font_ensure();
    if (!g_ready || !g_pixels || !r || !g || !b) {
        return;
    }
    if (!glide_upload(g_pixels, FONT_SIDE, FONT_SIDE, 1, r, g, b, &g_addr)) {
        return;
    }
    g_uploaded = 1;
}

static void flat_rect(float x, float y, float w, float h, float r, float g, float b)
{
    DrawVert v[4];
    int i;

    for (i = 0; i < 4; i++) {
        v[i].oow = 1.0f;
        v[i].ooz = 1.0f;
        v[i].sow = 0.0f;
        v[i].tow = 0.0f;
        v[i].r = r;
        v[i].g = g;
        v[i].b = b;
    }
    v[0].x = x;
    v[0].y = y + h;
    v[1].x = x + w;
    v[1].y = y + h;
    v[2].x = x + w;
    v[2].y = y;
    v[3].x = x;
    v[3].y = y;
    glide_triangle(&v[0], &v[1], &v[2]);
    glide_triangle(&v[0], &v[2], &v[3]);
}

static const df_u8 *fallback_glyph(char ch)
{
    int i;

    if (ch >= 'A' && ch <= 'Z') {
        ch = (char)(ch - 'A' + 'a');
    }
    for (i = 0; i < (int)(sizeof(g_fallback) / sizeof(g_fallback[0])); i++) {
        if (g_fallback[i].ch == ch) {
            return g_fallback[i].row;
        }
    }
    return g_fallback[0].row;
}

static void fallback_text(int x, int top, const char *s)
{
    int i;

    i = 0;
    while (s[i]) {
        const df_u8 *row;
        int ry;

        row = fallback_glyph(s[i]);
        for (ry = 0; ry < FALL_CELL; ry++) {
            int bit;
            int run;
            int col;

            bit = row[ry];
            col = 0;
            while (col < FALL_CELL) {
                if ((bit & (0x80 >> col)) == 0) {
                    col++;
                    continue;
                }
                run = 0;
                while (col + run < FALL_CELL && (bit & (0x80 >> (col + run))) != 0) {
                    run++;
                }
                flat_rect((float)(x + col),
                          (float)(top - (ry + 1)),
                          (float)run,
                          1.0f,
                          255.0f, 236.0f, 180.0f);
                col += run;
            }
        }
        x += FALL_CELL;
        i++;
    }
}

static void glyph_quad(float x, float y, float w, float h, float s, float t)
{
    DrawVert v[4];
    int i;

    for (i = 0; i < 4; i++) {
        v[i].oow = 1.0f;
        v[i].ooz = 1.0f;
        v[i].r = 255.0f;
        v[i].g = 255.0f;
        v[i].b = 255.0f;
    }
    /* y is the bottom. t is the top row of the glyph in the sheet. */
    v[0].x = x;
    v[0].y = y + h;
    v[0].sow = s;
    v[0].tow = t;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].sow = s + w;
    v[1].tow = t;
    v[2].x = x + w;
    v[2].y = y;
    v[2].sow = s + w;
    v[2].tow = t + h;
    v[3].x = x;
    v[3].y = y;
    v[3].sow = s;
    v[3].tow = t + h;
    glide_triangle(&v[0], &v[1], &v[2]);
    glide_triangle(&v[0], &v[2], &v[3]);
}

static void atlas_text(int x, int top, const char *s)
{
    while (*s) {
        int c;
        int i;
        int w;
        int col;
        int row;

        c = (unsigned char)*s++;
        if (c < FONT_FIRST || c >= FONT_FIRST + FONT_N) {
            x += FONT_SPACE;
            continue;
        }
        i = c - FONT_FIRST;
        w = g_width[i];
        if (w < 1) {
            x += FONT_SPACE;
            continue;
        }
        col = i & 15;
        row = i >> 4;
        glyph_quad((float)x, (float)(top - g_height),
                   (float)w, (float)g_height,
                   (float)(col * FONT_CELL),
                   (float)(row * FONT_CELL));
        x += w + 1;
    }
}

int ui_line_height(void)
{
    if (g_uploaded && g_height > 0) {
        return g_height;
    }
    return FALL_CELL;
}

int ui_string_width(const char *s)
{
    int x;

    if (!s) {
        return 0;
    }
    if (!g_uploaded) {
        x = 0;
        while (s[x]) {
            x++;
        }
        return x * FALL_CELL;
    }
    x = 0;
    while (*s) {
        int c;
        int w;

        c = (unsigned char)*s++;
        if (c < FONT_FIRST || c >= FONT_FIRST + FONT_N) {
            x += FONT_SPACE;
            continue;
        }
        w = g_width[c - FONT_FIRST];
        if (w < 1) {
            x += FONT_SPACE;
            continue;
        }
        x += w + 1;
    }
    return x;
}

void ui_string(int x, int top, const char *s)
{
    if (!s) {
        return;
    }
    if (g_uploaded && g_height > 0) {
        glide_ui(1);
        glide_sprite_alpha(1);
        glide_bind(g_addr);
        atlas_text(x, top, s);
        glide_sprite_alpha(0);
        glide_ui(0);
        texcache_forget();
        return;
    }
    glide_overlay(1);
    fallback_text(x, top, s);
    glide_overlay(0);
}
