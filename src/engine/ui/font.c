#include "engine/ui/font.h"

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/mem.h"
#include "engine/gfx/glide.h"

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
/* Upload handle of the sheet, and the colour of its ink as 0xRRGGBB. */
static df_u32 g_handle;
static df_u32 g_ink = 0xffffffu;

/*
 * 8x8 Terminal, the EGA OEM strike. The stock OEM_FIXED_FONT on a VGA
 * machine is the 8x12 face, so this asks for height 8 by name. Cells
 * are 16 so the glyph sits in the corner of a 256 square. The atlas
 * treats s,t as pixels on a 256-wide sheet, which is also Glide's
 * 0..256 range. Index 0 is clear. Index 1 is white.
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

/* Positive height is the cell. Width 0 lets the strike pick its own. */
#define TERM_CHARSET 255
#define TERM_RASTER 6
#define TERM_FIXED ((1) | (3 << 4))
#define TERM_WEIGHT 400

typedef void *(WINAPI *create_font_fn)(
    int, int, int, int, int,
    unsigned, unsigned, unsigned, unsigned,
    unsigned, unsigned, unsigned, unsigned,
    const char *);
typedef int (WINAPI *add_font_fn)(const char *);
typedef void *(WINAPI *sel_fn)(HDC, void *);
typedef int (WINAPI *tm_fn)(HDC, TEXTMETRIC *);
typedef int (WINAPI *del_fn)(void *);

static void *create_terminal(create_font_fn create, int width)
{
    if (!create) {
        return 0;
    }
    return create(8, width, 0, 0, TERM_WEIGHT,
                  0, 0, 0, TERM_CHARSET,
                  TERM_RASTER, 0, 0, TERM_FIXED, "Terminal");
}

static int path_join(char *dst, int cap, const char *a, const char *b)
{
    int n;
    int i;

    n = 0;
    while (a[n]) {
        n++;
    }
    i = 0;
    while (b[i]) {
        i++;
    }
    if (n + i + 1 > cap) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        dst[i] = a[i];
    }
    n = 0;
    while (b[n]) {
        dst[i] = b[n];
        i++;
        n++;
    }
    dst[i] = 0;
    return 1;
}

/* EGAOEM.FON is the 8x8 Terminal. The VGA stock face does not include it. */
static void add_ega(add_font_fn add)
{
    char dir[260];
    char path[260];
    unsigned n;
    int i;
    static const char *tail[] = {
        "\\FONTS\\EGAOEM.FON",
        "\\SYSTEM\\EGAOEM.FON",
        "\\EGAOEM.FON"
    };

    if (!add) {
        return;
    }
    n = GetWindowsDirectoryA(dir, 240);
    if (n == 0 || n >= 240) {
        return;
    }
    for (i = 0; i < 3; i++) {
        if (path_join(path, (int)sizeof(path), dir, tail[i])) {
            add(path);
        }
    }
}

static int metrics_8x8(tm_fn metrics, HDC dc)
{
    TEXTMETRIC tm;

    if (!metrics || !metrics(dc, &tm)) {
        return 0;
    }
    return (int)tm.tmAveCharWidth == 8
        && (int)tm.tmHeight == 8
        && (int)tm.tmMaxCharWidth <= SYS_CELL;
}

/* Selects the 8x8 strike, then deselects it. The caller owns the font. */
static void *font_8x8(create_font_fn create, add_font_fn add,
                      sel_fn select, tm_fn metrics, del_fn kill, HDC dc)
{
    int pass;
    void *font;
    void *old;

    if (!dc) {
        return 0;
    }
    for (pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            add_ega(add);
        }
        font = create_terminal(create, 8);
        if (!font) {
            font = create_terminal(create, 0);
        }
        if (!font) {
            continue;
        }
        old = select(dc, font);
        if (old && metrics_8x8(metrics, dc)) {
            select(dc, old);
            return font;
        }
        if (old) {
            select(dc, old);
        }
        kill(font);
    }
    return 0;
}

/* 8x8 Terminal. A missing gdi32, or no 8x8 strike, leaves the built-in face. */
static int raster_oem(void)
{
    typedef HDC (WINAPI *cdc_fn)(HDC);
    typedef int (WINAPI *deldc_fn)(HDC);
    typedef unsigned (WINAPI *color_fn)(HDC, unsigned);
    typedef int (WINAPI *mode_fn)(HDC, int);
    typedef int (WINAPI *text_fn)(HDC, int, int, const char *, int);
    typedef void *(WINAPI *dib_fn)(HDC, const BITMAPINFO *, unsigned, void **, void *, unsigned);
    HMODULE gdi;
    create_font_fn create;
    add_font_fn addfont;
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
    create = (create_font_fn)(void *)GetProcAddress(gdi, "CreateFontA");
    addfont = (add_font_fn)(void *)GetProcAddress(gdi, "AddFontResourceA");
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
    if (!create || !make_dc || !free_dc || !select || !metrics
        || !set_text || !set_bk || !set_mode || !text_out
        || !make_dib || !free_obj) {
        return 0;
    }
    screen = GetDC(0);
    if (screen) {
        dc = make_dc(screen);
    }
    font = font_8x8(create, addfont, select, metrics, free_obj, dc);
    if (dc && font) {
        oldfont = select(dc, font);
        if (oldfont && metrics(dc, &tm)) {
            cw = (int)tm.tmAveCharWidth;
            ch = (int)tm.tmHeight;
            if (cw == 8 && ch == 8 && (int)tm.tmMaxCharWidth <= SYS_CELL) {
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
    if (font) {
        free_obj(font);
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
    /*
     * The glyphs are ink or nothing: one byte a texel, ALPHA_INTENSITY_44,
     * and the ink's colour is the tint the text is drawn with. As ARGB
     * 1555 the 256 x 256 sheet was 128 KB of the card; now 64 KB.
     */
    {
        int i;
        int n;

        n = g_side * g_side;
        for (i = 0; i < n && !g_pixels[i]; i++) {
        }
        g_ink = 0xffffffu;
        if (i < n) {
            df_u8 c;

            c = g_pixels[i];
            g_ink = ((df_u32)pr[c] << 16) | ((df_u32)pg[c] << 8) | (df_u32)pb[c];
        }
    }
    if (!glide_upload_ai44(g_pixels, g_side, &g_handle)) {
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
    /*
     * One planar quad, not two triangles: Glide sets up the texture
     * gradients once. LEVELFALL's panels are some 700 glyphs, more
     * triangles than a corridor's whole scene.
     */
    glide_polygon(v, 4);
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
    ui_string_alpha(x, top, s, 255);
}

void ui_string_alpha(int x, int top, const char *s, int alpha)
{
    if (!s || alpha < 1) {
        return;
    }
    if (alpha > 255) {
        alpha = 255;
    }
    if (g_uploaded && g_height > 0) {
        df_u32 was;

        glide_ui(1);
        glide_sprite_alpha(1);
        /* The sheet is white; the ink's colour is the tint. */
        was = glide_tinted();
        glide_tint(g_ink);
        if (alpha < 255) {
            glide_opacity(alpha, 1);
        }
        glide_bind(g_handle);
        atlas_text(x, top, s);
        if (alpha < 255) {
            glide_opacity(-1, 0);
        }
        glide_tint(was);
        glide_sprite_alpha(0);
        glide_ui(0);
        return;
    }
    glide_overlay(1);
    if (alpha < 255) {
        glide_opacity(alpha, 0);
    }
    fallback_text(x, top, s);
    if (alpha < 255) {
        glide_opacity(-1, 0);
    }
    glide_overlay(0);
}
