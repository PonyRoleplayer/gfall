/*
 * SPINTRI: extruded "Shade Starlight" / "Now in 3D!" on a Voodoo 1.
 * The sign yaws on its own axis and orbits the middle of the screen.
 * Glide 2 has no geometry unit, so the CPU transforms every vertex.
 * Escape quits. Windows 98, Pentium MMX, no C runtime.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#define FX_GLIDE_NO_FUNC_PROTO

#include <windows.h>
#include <glide.h>

#define SCREEN_W 640.0f
#define SCREEN_H 480.0f
#define TWO_PI 6.28318530718f
#define DEGREE 0.01745329252f
#define TEXT_SCALE 0.060f
#define TEXT_THICK 0.70f
#define CAM_Z 5.0f
#define FOCAL 400.0f
#define ORBIT_R 1.00f
#define MAX_TRIS 4096

typedef char spintri_two_tmus[(GLIDE_NUM_TMU == 2) ? 1 : -1];
typedef char spintri_vertex_is_glide2[
    (sizeof(GrVertex) == (9 + 3 * GLIDE_NUM_TMU) * sizeof(float)) ? 1 : -1];

static int g_quit;

static float x87_sin(float x)
{
    float r;

    __asm__ __volatile__("fsin" : "=t"(r) : "0"(x));
    return r;
}

static float x87_cos(float x)
{
    float r;

    __asm__ __volatile__("fcos" : "=t"(r) : "0"(x));
    return r;
}

static float wrap_tau(float a)
{
    if (a > TWO_PI) {
        a = a - TWO_PI;
    }
    return a;
}

typedef struct {
    char ch;
    unsigned char row[7];
} Glyph;

/* 5x7, top row first. Bit 4 is the leftmost pixel. */
static const Glyph glyphs[] = {
    { 'S', { 0x0E, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x0E } },
    { 'h', { 0x10, 0x10, 0x10, 0x1C, 0x12, 0x12, 0x12 } },
    { 'a', { 0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F } },
    { 'd', { 0x01, 0x01, 0x01, 0x0F, 0x11, 0x11, 0x0F } },
    { 'e', { 0x00, 0x00, 0x0E, 0x11, 0x1E, 0x10, 0x0E } },
    { 't', { 0x04, 0x04, 0x1E, 0x04, 0x04, 0x04, 0x03 } },
    { 'r', { 0x00, 0x00, 0x16, 0x18, 0x10, 0x10, 0x10 } },
    { 'l', { 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0C } },
    { 'i', { 0x04, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'g', { 0x00, 0x00, 0x0E, 0x11, 0x0F, 0x01, 0x0E } },
    { 'N', { 0x11, 0x19, 0x15, 0x15, 0x13, 0x13, 0x11 } },
    { 'o', { 0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E } },
    { 'w', { 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0E } },
    { 'n', { 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11 } },
    { '3', { 0x0E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x0E } },
    { 'D', { 0x18, 0x14, 0x12, 0x12, 0x12, 0x14, 0x18 } },
    { '!', { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 } }
};

typedef struct {
    float x, y, z;
} V3;

typedef struct {
    V3 p[3];
    float nx, ny, nz;
    unsigned char line;
} Tri;

static Tri g_tris[MAX_TRIS];
static int g_ntris;

static const unsigned char *glyph_rows(char ch)
{
    int i;

    for (i = 0; i < (int)(sizeof(glyphs) / sizeof(glyphs[0])); i++) {
        if (glyphs[i].ch == ch) {
            return glyphs[i].row;
        }
    }
    return 0;
}

static int bit_on(const unsigned char *rows, int x, int y)
{
    if (!rows || x < 0 || x > 4 || y < 0 || y > 6) {
        return 0;
    }
    return (rows[y] >> (4 - x)) & 1;
}

static void add_tri(float x0, float y0, float z0,
                     float x1, float y1, float z1,
                     float x2, float y2, float z2,
                     float nx, float ny, float nz,
                     int line)
{
    Tri *t;

    if (g_ntris >= MAX_TRIS) {
        return;
    }
    t = &g_tris[g_ntris];
    g_ntris = g_ntris + 1;
    t->p[0].x = x0;
    t->p[0].y = y0;
    t->p[0].z = z0;
    t->p[1].x = x1;
    t->p[1].y = y1;
    t->p[1].z = z1;
    t->p[2].x = x2;
    t->p[2].y = y2;
    t->p[2].z = z2;
    t->nx = nx;
    t->ny = ny;
    t->nz = nz;
    t->line = (unsigned char)line;
}

static void add_quad(float x0, float y0, float z0,
                      float x1, float y1, float z1,
                      float x2, float y2, float z2,
                      float x3, float y3, float z3,
                      float nx, float ny, float nz,
                      int line)
{
    add_tri(x0, y0, z0, x1, y1, z1, x2, y2, z2, nx, ny, nz, line);
    add_tri(x0, y0, z0, x2, y2, z2, x3, y3, z3, nx, ny, nz, line);
}

static void add_cell(float x, float y, int line,
                      int open_l, int open_r, int open_u, int open_d)
{
    float x0;
    float x1;
    float y0;
    float y1;
    float z0;
    float z1;

    x0 = x;
    x1 = x + 1.0f;
    y0 = y;
    y1 = y + 1.0f;
    z0 = -TEXT_THICK;
    z1 = TEXT_THICK;

    add_quad(x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1, 0.0f, 0.0f, 1.0f, line);
    add_quad(x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0, 0.0f, 0.0f, -1.0f, line);
    if (open_l) {
        add_quad(x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0, -1.0f, 0.0f, 0.0f, line);
    }
    if (open_r) {
        add_quad(x1, y0, z1, x1, y0, z0, x1, y1, z0, x1, y1, z1, 1.0f, 0.0f, 0.0f, line);
    }
    if (open_u) {
        add_quad(x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, 0.0f, 1.0f, 0.0f, line);
    }
    if (open_d) {
        add_quad(x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, 0.0f, -1.0f, 0.0f, line);
    }
}

static void build_line(const char *text, float y_base, int line)
{
    int i;
    int bx;
    int by;
    float pen;

    pen = 0.0f;
    for (i = 0; text[i] != 0; i++) {
        const unsigned char *rows;
        int advance;

        if (text[i] == ' ') {
            pen = pen + 4.0f;
            continue;
        }
        rows = glyph_rows(text[i]);
        advance = 6;
        if (!rows) {
            pen = pen + advance;
            continue;
        }
        for (by = 0; by < 7; by++) {
            for (bx = 0; bx < 5; bx++) {
                float x;
                float y;

                if (!bit_on(rows, bx, by)) {
                    continue;
                }
                x = pen + (float)bx;
                y = y_base + (float)(6 - by);
                add_cell(
                    x,
                    y,
                    line,
                    !bit_on(rows, bx - 1, by),
                    !bit_on(rows, bx + 1, by),
                    !bit_on(rows, bx, by - 1),
                    !bit_on(rows, bx, by + 1));
            }
        }
        pen = pen + advance;
    }
}

static void center_mesh(void)
{
    float minx;
    float maxx;
    float miny;
    float maxy;
    float cx;
    float cy;
    int i;
    int k;

    if (g_ntris < 1) {
        return;
    }
    minx = g_tris[0].p[0].x;
    maxx = minx;
    miny = g_tris[0].p[0].y;
    maxy = miny;
    for (i = 0; i < g_ntris; i++) {
        for (k = 0; k < 3; k++) {
            float x;
            float y;

            x = g_tris[i].p[k].x;
            y = g_tris[i].p[k].y;
            if (x < minx) {
                minx = x;
            }
            if (x > maxx) {
                maxx = x;
            }
            if (y < miny) {
                miny = y;
            }
            if (y > maxy) {
                maxy = y;
            }
        }
    }
    cx = (minx + maxx) * 0.5f;
    cy = (miny + maxy) * 0.5f;
    for (i = 0; i < g_ntris; i++) {
        for (k = 0; k < 3; k++) {
            g_tris[i].p[k].x = g_tris[i].p[k].x - cx;
            g_tris[i].p[k].y = g_tris[i].p[k].y - cy;
        }
    }
}

static void build_mesh(void)
{
    g_ntris = 0;
    build_line("Shade Starlight", 10.0f, 0);
    build_line("Now in 3D!", 0.0f, 1);
    center_mesh();
}

typedef void (FX_CALL *gr_init_fn)(void);
typedef void (FX_CALL *gr_shutdown_fn)(void);
typedef FxBool (FX_CALL *gr_query_fn)(GrHwConfiguration *hw);
typedef void (FX_CALL *gr_select_fn)(int which);
typedef FxBool (FX_CALL *gr_open_fn)(
    FxU32 hwnd,
    GrScreenResolution_t res,
    GrScreenRefresh_t refresh,
    GrColorFormat_t fmt,
    GrOriginLocation_t origin,
    int n_color,
    int n_aux);
typedef void (FX_CALL *gr_close_fn)(void);
typedef void (FX_CALL *gr_cull_fn)(GrCullMode_t mode);
typedef void (FX_CALL *gr_depth_mode_fn)(GrDepthBufferMode_t mode);
typedef void (FX_CALL *gr_depth_mask_fn)(FxBool mask);
typedef void (FX_CALL *gr_depth_func_fn)(GrCmpFnc_t func);
typedef void (FX_CALL *gr_color_fn)(
    GrCombineFunction_t function,
    GrCombineFactor_t factor,
    GrCombineLocal_t local,
    GrCombineOther_t other,
    FxBool invert);
typedef void (FX_CALL *gr_clear_fn)(GrColor_t color, GrAlpha_t alpha, FxU16 depth);
typedef void (FX_CALL *gr_swap_fn)(int interval);
typedef void (FX_CALL *gr_tri_fn)(const GrVertex *a, const GrVertex *b, const GrVertex *c);

static struct {
    HMODULE dll;
    gr_init_fn init;
    gr_shutdown_fn shutdown;
    gr_query_fn query;
    gr_select_fn select;
    gr_open_fn open;
    gr_close_fn close;
    gr_cull_fn cull;
    gr_depth_mode_fn depth_mode;
    gr_depth_func_fn depth_func;
    gr_depth_mask_fn depth_mask;
    gr_color_fn color;
    gr_clear_fn clear;
    gr_swap_fn swap;
    gr_tri_fn triangle;
} glide;

static FARPROC find_export(HMODULE dll, const char *plain, const char *atn, const char *uscore)
{
    FARPROC sym;

    sym = GetProcAddress(dll, plain);
    if (sym) {
        return sym;
    }
    sym = GetProcAddress(dll, atn);
    if (sym) {
        return sym;
    }
    return GetProcAddress(dll, uscore);
}

static void fail(const char *text)
{
    MessageBoxA(0, text, "SPINTRI", MB_OK | MB_ICONERROR);
}

#define BIND(dst, ty, plain, atn, uscore) \
    do { \
        FARPROC sym; \
        sym = find_export(glide.dll, plain, atn, uscore); \
        if (!sym) { \
            fail("glide2x.dll has no " plain); \
            FreeLibrary(glide.dll); \
            glide.dll = 0; \
            return 0; \
        } \
        dst = (ty)sym; \
    } while (0)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
static int load_glide(void)
{
    glide.dll = LoadLibraryA("glide2x.dll");
    if (!glide.dll) {
        fail("Could not load glide2x.dll.");
        return 0;
    }

    BIND(glide.init, gr_init_fn, "grGlideInit", "grGlideInit@0", "_grGlideInit@0");
    BIND(glide.shutdown, gr_shutdown_fn, "grGlideShutdown", "grGlideShutdown@0", "_grGlideShutdown@0");
    BIND(glide.query, gr_query_fn, "grSstQueryHardware", "grSstQueryHardware@4", "_grSstQueryHardware@4");
    BIND(glide.select, gr_select_fn, "grSstSelect", "grSstSelect@4", "_grSstSelect@4");
    BIND(glide.open, gr_open_fn, "grSstWinOpen", "grSstWinOpen@28", "_grSstWinOpen@28");
    BIND(glide.close, gr_close_fn, "grSstWinClose", "grSstWinClose@0", "_grSstWinClose@0");
    BIND(glide.cull, gr_cull_fn, "grCullMode", "grCullMode@4", "_grCullMode@4");
    BIND(glide.depth_mode, gr_depth_mode_fn, "grDepthBufferMode", "grDepthBufferMode@4", "_grDepthBufferMode@4");
    BIND(glide.depth_func, gr_depth_func_fn, "grDepthBufferFunction", "grDepthBufferFunction@4", "_grDepthBufferFunction@4");
    BIND(glide.depth_mask, gr_depth_mask_fn, "grDepthMask", "grDepthMask@4", "_grDepthMask@4");
    BIND(glide.color, gr_color_fn, "grColorCombine", "grColorCombine@20", "_grColorCombine@20");
    BIND(glide.clear, gr_clear_fn, "grBufferClear", "grBufferClear@12", "_grBufferClear@12");
    BIND(glide.swap, gr_swap_fn, "grBufferSwap", "grBufferSwap@4", "_grBufferSwap@4");
    BIND(glide.triangle, gr_tri_fn, "grDrawTriangle", "grDrawTriangle@12", "_grDrawTriangle@12");
    return 1;
}
#pragma GCC diagnostic pop

static void fill_vert(GrVertex *v, float x, float y, float ooz, float r, float g, float b)
{
    v->x = x;
    v->y = y;
    v->z = 0.0f;
    v->r = r;
    v->g = g;
    v->b = b;
    v->ooz = ooz;
    v->a = 255.0f;
    v->oow = 1.0f;
    v->tmuvtx[0].sow = 0.0f;
    v->tmuvtx[0].tow = 0.0f;
    v->tmuvtx[0].oow = 1.0f;
    v->tmuvtx[1].sow = 0.0f;
    v->tmuvtx[1].tow = 0.0f;
    v->tmuvtx[1].oow = 1.0f;
}

static int project(float x, float y, float z, float cs, float sn, float ox, float oy,
                    float *sx, float *sy, float *ooz)
{
    float x1;
    float y1;
    float z1;
    float vz;

    x = x * TEXT_SCALE;
    y = y * TEXT_SCALE;
    z = z * TEXT_SCALE;
    x1 = x * cs + z * sn;
    y1 = y;
    z1 = -x * sn + z * cs;
    x1 = x1 + ox;
    y1 = y1 + oy;
    vz = CAM_Z - z1;
    if (vz < 0.25f) {
        return 0;
    }
    *sx = (SCREEN_W * 0.5f) + x1 * FOCAL / vz;
    *sy = (SCREEN_H * 0.5f) + y1 * FOCAL / vz;
    *ooz = 65535.0f / vz;
    return 1;
}

static void draw_frame(float spin, float orbit)
{
    float cs;
    float sn;
    float ox;
    float oy;
    int i;

    cs = x87_cos(spin);
    sn = x87_sin(spin);
    ox = x87_cos(orbit) * ORBIT_R;
    oy = x87_sin(orbit) * ORBIT_R;

    glide.clear(0, 0, GR_ZDEPTHVALUE_FARTHEST);
    for (i = 0; i < g_ntris; i++) {
        Tri *t;
        float nx;
        float ny;
        float nz;
        float ndot;
        float shade;
        float r;
        float g;
        float b;
        GrVertex vert[3];
        int k;
        int ok;

        t = &g_tris[i];
        nx = t->nx * cs + t->nz * sn;
        ny = t->ny;
        nz = -t->nx * sn + t->nz * cs;
        if (nz <= 0.02f) {
            continue;
        }
        ndot = nx * 0.24f + ny * 0.82f + nz * 0.53f;
        if (ndot < 0.0f) {
            ndot = 0.0f;
        }
        shade = 0.22f + 0.78f * ndot;
        if (t->line == 0) {
            r = 190.0f * shade;
            g = 220.0f * shade;
            b = 255.0f * shade;
        } else {
            r = 255.0f * shade;
            g = 210.0f * shade;
            b = 70.0f * shade;
        }
        ok = 1;
        for (k = 0; k < 3; k++) {
            float sx;
            float sy;
            float ooz;

            if (!project(t->p[k].x, t->p[k].y, t->p[k].z, cs, sn, ox, oy, &sx, &sy, &ooz)) {
                ok = 0;
                break;
            }
            fill_vert(&vert[k], sx, sy, ooz, r, g, b);
        }
        if (ok) {
            glide.triangle(&vert[0], &vert[1], &vert[2]);
        }
    }
    glide.swap(1);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        g_quit = 1;
        return 0;
    }
    if (msg == WM_CLOSE || msg == WM_DESTROY) {
        g_quit = 1;
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static HWND open_window(HINSTANCE inst)
{
    WNDCLASSA wc;
    HWND hwnd;

    wc.style = 0;
    wc.lpfnWndProc = wnd_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = inst;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = 0;
    wc.lpszMenuName = 0;
    wc.lpszClassName = "spintri";

    if (!RegisterClassA(&wc)) {
        fail("RegisterClass failed.");
        return 0;
    }

    hwnd = CreateWindowExA(
        0,
        "spintri",
        "spintri",
        WS_POPUP | WS_VISIBLE,
        0,
        0,
        640,
        480,
        0,
        0,
        inst,
        0);
    if (!hwnd) {
        fail("CreateWindow failed.");
        return 0;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    return hwnd;
}

static void pump(void)
{
    MSG msg;

    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_quit = 1;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
        g_quit = 1;
    }
}

static int win_main(HINSTANCE inst)
{
    GrHwConfiguration hw;
    HWND hwnd;
    int inited;
    int opened;
    float spin;
    float orbit;

    inited = 0;
    opened = 0;
    spin = 0.0f;
    orbit = 0.0f;
    g_quit = 0;

    hwnd = open_window(inst);
    if (!hwnd) {
        return 1;
    }
    pump();

    if (!load_glide()) {
        DestroyWindow(hwnd);
        return 1;
    }

    glide.init();
    inited = 1;

    if (!glide.query(&hw)) {
        fail("grSstQueryHardware found no Voodoo.");
        glide.shutdown();
        FreeLibrary(glide.dll);
        DestroyWindow(hwnd);
        return 1;
    }

    glide.select(0);
    if (!glide.open(
            (FxU32)hwnd,
            GR_RESOLUTION_640x480,
            GR_REFRESH_60Hz,
            GR_COLORFORMAT_ARGB,
            GR_ORIGIN_LOWER_LEFT,
            2,
            1)) {
        fail("grSstWinOpen failed. Wanted 640x480 at 60 Hz.");
        glide.shutdown();
        FreeLibrary(glide.dll);
        DestroyWindow(hwnd);
        return 1;
    }
    opened = 1;

    build_mesh();
    if (g_ntris < 1) {
        fail("Text mesh is empty.");
        glide.close();
        glide.shutdown();
        FreeLibrary(glide.dll);
        DestroyWindow(hwnd);
        return 1;
    }

    glide.cull(GR_CULL_DISABLE);
    glide.depth_mode(GR_DEPTHBUFFER_ZBUFFER);
    glide.depth_func(GR_CMP_GREATER);
    glide.depth_mask(FXTRUE);
    glide.color(
        GR_COMBINE_FUNCTION_LOCAL,
        GR_COMBINE_FACTOR_NONE,
        GR_COMBINE_LOCAL_ITERATED,
        GR_COMBINE_OTHER_NONE,
        FXFALSE);

    while (!g_quit) {
        pump();
        if (g_quit) {
            break;
        }
        draw_frame(spin, orbit);
        spin = wrap_tau(spin + 2.4f * DEGREE);
        orbit = wrap_tau(orbit + 1.15f * DEGREE);
    }

    if (opened) {
        glide.close();
    }
    if (inited) {
        glide.shutdown();
    }
    if (glide.dll) {
        FreeLibrary(glide.dll);
    }
    DestroyWindow(hwnd);
    return 0;
}

void WinMainCRTStartup(void)
{
    ExitProcess((UINT)win_main(GetModuleHandleA(0)));
}
