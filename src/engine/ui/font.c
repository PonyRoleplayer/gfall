#include "engine/ui/font.h"

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/mem.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/texcache.h"

/*
 * The built-in face is 8x8, row 0 at the top, bit 7 on the left. It is
 * drawn 1:1. The right column and the bottom row stay clear.
 */

#define FALL_CELL 8

static const df_u8 *g_pixels;
static df_u8 g_width[FONT_MAX];
static int g_side;
static int g_cell;
static int g_height;
static int g_nwidths;
static int g_first;
static int g_gap;
static int g_uploaded;
static int g_system;
static int g_sys_tried;
static df_u32 g_addr;

/*
 * OEM fixed font, the face Windows stocks for a DOS box. Cells are
 * 16 so an 8 by 12 glyph sits in the corner of a 256 square. The
 * atlas treats s,t as pixels on a 256-wide sheet, which is also
 * Glide's 0..256 range. Index 0 is clear. Index 1 is white.
 */
#define SYS_SIDE 256
#define SYS_CELL 16
#define SYS_N 256
#define SYS_INK 1

static df_u8 g_sys_pixels[SYS_SIDE * SYS_SIDE];
static df_u8 g_sys_width[SYS_N];
static df_u8 g_sys_r[256];
static df_u8 g_sys_g[256];
static df_u8 g_sys_b[256];
static int g_sys_height;

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

static int pot(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

/* Stock OEM face. A missing gdi32, or a cell that will not fit, leaves the 8x8. */
static int raster_oem(void)
{
    typedef void *(WINAPI *stock_fn)(int);
    typedef HDC (WINAPI *cdc_fn)(HDC);
    typedef int (WINAPI *deldc_fn)(HDC);
    typedef void *(WINAPI *sel_fn)(HDC, void *);
    typedef int (WINAPI *tm_fn)(HDC, TEXTMETRIC *);
    typedef unsigned (WINAPI *color_fn)(HDC, unsigned);
    typedef int (WINAPI *mode_fn)(HDC, int);
    typedef int (WINAPI *text_fn)(HDC, int, int, const char *, int);
    typedef void *(WINAPI *dib_fn)(HDC, const BITMAPINFO *, unsigned, void **, void *, unsigned);
    typedef int (WINAPI *del_fn)(void *);
    HMODULE gdi;
    stock_fn stock;
    cdc_fn make_dc;
    deldc_fn free_dc;
    sel_fn select;
    tm_fn metrics;
    color_fn set_text;
    color_fn set_bk;
    mode_fn set_mode;
    text_fn text_out;
    dib_fn make_dib;
    del_fn free_obj;
    HDC screen;
    HDC dc;
    void *font;
    void *oldfont;
    void *dib;
    void *oldbmp;
    void *bits;
    TEXTMETRIC tm;
    BITMAPINFO info;
    int cw;
    int ch;
    int stride;
    int ok;
    int i;
    int y;
    unsigned char *px;

    screen = 0;
    dc = 0;
    oldfont = 0;
    dib = 0;
    oldbmp = 0;
    bits = 0;
    ok = 0;
    cw = 0;
    ch = 0;
    stride = 0;
    gdi = GetModuleHandleA("gdi32.dll");
    if (!gdi) {
        gdi = LoadLibraryA("gdi32.dll");
    }
    if (!gdi) {
        return 0;
    }
    stock = (stock_fn)(void *)GetProcAddress(gdi, "GetStockObject");
    make_dc = (cdc_fn)(void *)GetProcAddress(gdi, "CreateCompatibleDC");
    free_dc = (deldc_fn)(void *)GetProcAddress(gdi, "DeleteDC");
    select = (sel_fn)(void *)GetProcAddress(gdi, "SelectObject");
    metrics = (tm_fn)(void *)GetProcAddress(gdi, "GetTextMetricsA");
    set_text = (color_fn)(void *)GetProcAddress(gdi, "SetTextColor");
    set_bk = (color_fn)(void *)GetProcAddress(gdi, "SetBkColor");
    set_mode = (mode_fn)(void *)GetProcAddress(gdi, "SetBkMode");
    text_out = (text_fn)(void *)GetProcAddress(gdi, "TextOutA");
    make_dib = (dib_fn)(void *)GetProcAddress(gdi, "CreateDIBSection");
    free_obj = (del_fn)(void *)GetProcAddress(gdi, "DeleteObject");
    if (!stock || !make_dc || !free_dc || !select || !metrics
        || !set_text || !set_bk || !set_mode || !text_out
        || !make_dib || !free_obj) {
        return 0;
    }
    font = stock(OEM_FIXED_FONT);
    screen = GetDC(0);
    if (font && screen) {
        dc = make_dc(screen);
    }
    if (dc) {
        oldfont = select(dc, font);
        if (oldfont && metrics(dc, &tm)) {
            cw = (int)tm.tmAveCharWidth;
            ch = (int)tm.tmHeight;
            if (cw >= 1 && cw <= SYS_CELL && ch >= 1 && ch <= SYS_CELL
                && (int)tm.tmMaxCharWidth <= SYS_CELL) {
                stride = (cw * 3 + 3) & ~3;
                mem_zero(&info, (df_u32)sizeof(info));
                info.bmiHeader.biSize = (unsigned)sizeof(info.bmiHeader);
                info.bmiHeader.biWidth = cw;
                info.bmiHeader.biHeight = -ch;
                info.bmiHeader.biPlanes = 1;
                info.bmiHeader.biBitCount = 24;
                dib = make_dib(dc, &info, 0, &bits, 0, 0);
            }
        }
    }
    if (dib && bits) {
        oldbmp = select(dc, dib);
    }
    if (oldbmp) {
        set_text(dc, 0x00ffffffu);
        set_bk(dc, 0);
        set_mode(dc, 2);
        mem_zero(g_sys_pixels, (df_u32)sizeof(g_sys_pixels));
        for (i = 0; i < SYS_N; i++) {
            char letter;
            int col;
            int row;
            int x;

            letter = (char)i;
            g_sys_width[i] = (df_u8)cw;
            px = (unsigned char *)bits;
            for (y = 0; y < ch; y++) {
                int b;

                for (b = 0; b < stride; b++) {
                    px[y * stride + b] = 0;
                }
            }
            if (!text_out(dc, 0, 0, &letter, 1)) {
                continue;
            }
            col = i & 15;
            row = i >> 4;
            for (y = 0; y < ch; y++) {
                for (x = 0; x < cw; x++) {
                    unsigned char *p;

                    p = px + y * stride + x * 3;
                    if (p[0] || p[1] || p[2]) {
                        g_sys_pixels[(row * SYS_CELL + y) * SYS_SIDE
                            + col * SYS_CELL + x] = SYS_INK;
                    }
                }
            }
        }
        g_sys_height = ch;
        ok = 1;
    }
    if (dc && oldbmp) {
        select(dc, oldbmp);
    }
    if (dib) {
        free_obj(dib);
    }
    if (dc && oldfont) {
        select(dc, oldfont);
    }
    if (dc) {
        free_dc(dc);
    }
    if (screen) {
        ReleaseDC(0, screen);
    }
    return ok;
}

static int install_system(void)
{
    FontSheet sheet;

    if (g_sys_tried) {
        return 0;
    }
    g_sys_tried = 1;
    if (!raster_oem()) {
        return 0;
    }
    sheet.pixels = g_sys_pixels;
    sheet.side = SYS_SIDE;
    sheet.cell = SYS_CELL;
    sheet.height = g_sys_height;
    sheet.widths = g_sys_width;
    sheet.nwidths = SYS_N;
    sheet.first = 0;
    sheet.gap = g_sys_width[0];
    if (!font_set(&sheet)) {
        return 0;
    }
    g_system = 1;
    return 1;
}

int font_set(const FontSheet *sheet)
{
    int i;
    int cols;

    g_uploaded = 0;
    g_system = 0;
    g_pixels = 0;
    g_side = 0;
    g_cell = 0;
    g_height = 0;
    g_nwidths = 0;
    g_first = 0;
    g_gap = 0;
    if (!sheet) {
        return 1;
    }
    if (!sheet->pixels || !sheet->widths
        || sheet->nwidths < 1 || sheet->nwidths > FONT_MAX
        || !pot(sheet->side) || sheet->cell < 1
        || sheet->side % sheet->cell != 0
        || sheet->height < 1 || sheet->height > sheet->cell
        || sheet->first < 0 || sheet->gap < 0) {
        return 0;
    }
    cols = sheet->side / sheet->cell;
    if (cols < 1 || sheet->nwidths > cols * cols) {
        return 0;
    }
    for (i = 0; i < sheet->nwidths; i++) {
        g_width[i] = sheet->widths[i];
    }
    g_pixels = sheet->pixels;
    g_side = sheet->side;
    g_cell = sheet->cell;
    g_height = sheet->height;
    g_nwidths = sheet->nwidths;
    g_first = sheet->first;
    g_gap = sheet->gap;
    return 1;
}

void font_upload(const df_u8 *r, const df_u8 *g, const df_u8 *b)
{
    const df_u8 *pr;
    const df_u8 *pg;
    const df_u8 *pb;

    g_uploaded = 0;
    if (!g_pixels) {
        install_system();
    }
    if (!g_pixels || g_side < 1 || !r || !g || !b) {
        return;
    }
    pr = r;
    pg = g;
    pb = b;
    if (g_system) {
        /* Bright OEM foreground. The level palette is the game's ink. */
        g_sys_r[SYS_INK] = 255;
        g_sys_g[SYS_INK] = 255;
        g_sys_b[SYS_INK] = 255;
        pr = g_sys_r;
        pg = g_sys_g;
        pb = g_sys_b;
    }
    if (!glide_upload(g_pixels, g_side, g_side, 1, pr, pg, pb, &g_addr)) {
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
    int cols;

    cols = g_side / g_cell;
    while (*s) {
        int c;
        int i;
        int w;
        int col;
        int row;

        c = (unsigned char)*s++;
        if (c < g_first || c >= g_first + g_nwidths) {
            x += g_gap;
            continue;
        }
        i = c - g_first;
        w = g_width[i];
        if (w < 1) {
            x += g_gap;
            continue;
        }
        col = i % cols;
        row = i / cols;
        glyph_quad((float)x, (float)(top - g_height),
                   (float)w, (float)g_height,
                   (float)(col * g_cell),
                   (float)(row * g_cell));
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
        if (c < g_first || c >= g_first + g_nwidths) {
            x += g_gap;
            continue;
        }
        w = g_width[c - g_first];
        if (w < 1) {
            x += g_gap;
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
