#include "game/dagger/fnt.h"

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/ui/font.h"

/*
 * FONT0000.FNT: uint16 cell width, uint16 cell height, then 240 records
 * of uint16 offset and uint16 glyph width. Each glyph is height rows of
 * a little-endian uint16. Bit 15 is the leftmost pixel. Index 0 is '!'.
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

static const char *g_why = "FONT0000.FNT is missing";
static df_u8 *g_pixels;
static df_u8 g_width[FONT_N];
static int g_height;
static int g_ready;
static int g_generated;
static int g_tried;

const char *fnt_error(void)
{
    return g_why;
}

int fnt_ready(void)
{
    return g_ready;
}

int fnt_generated(void)
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

static int install_sheet(void)
{
    FontSheet sheet;

    sheet.pixels = g_pixels;
    sheet.side = FONT_SIDE;
    sheet.cell = FONT_CELL;
    sheet.height = g_height;
    sheet.widths = g_width;
    sheet.nwidths = FONT_N;
    sheet.first = FONT_FIRST;
    sheet.gap = FONT_SPACE;
    if (!font_set(&sheet)) {
        g_why = "font sheet was rejected";
        return 0;
    }
    return 1;
}

int fnt_install(void)
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
    if (!decode_fnt(fnt, size) || !install_sheet()) {
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
