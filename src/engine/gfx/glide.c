/*
 * Glide 2 binding. Retail glide2x.dll exports plain stdcall names, so the
 * loader tries the plain name, the @N name, and the _name@N name.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#define FX_GLIDE_NO_FUNC_PROTO

#include <windows.h>
#include <glide.h>

#include "engine/gfx/glide.h"
#include "engine/core/mem.h"

typedef char gfall_two_tmus[(GLIDE_NUM_TMU == 2) ? 1 : -1];
typedef char gfall_vertex_is_glide2[
    (sizeof(GrVertex) == (9 + 3 * GLIDE_NUM_TMU) * sizeof(float)) ? 1 : -1];
typedef char gfall_view_is_640[(DF_VIEW_W == 640 && DF_VIEW_H == 480) ? 1 : -1];
typedef char gfall_fog_table_is_64[(GR_FOG_TABLE_SIZE == 64) ? 1 : -1];

/*
 * One entry per uploaded map. A place keeps a small copy of every
 * picture, then a second copy of the ones close to the camera.
 * The image table is TEX_SLOTS in texcache.c (384). This array has
 * to hold that table, the font, and the near copies.
 */
#define GPU_SLOTS 768

typedef void (FX_CALL *gr_init_fn)(void);
typedef void (FX_CALL *gr_shutdown_fn)(void);
typedef FxBool (FX_CALL *gr_query_fn)(GrHwConfiguration *hw);
typedef void (FX_CALL *gr_select_fn)(int which);
typedef FxBool (FX_CALL *gr_open_fn)(
    FxU32 hwnd, GrScreenResolution_t res, GrScreenRefresh_t refresh,
    GrColorFormat_t fmt, GrOriginLocation_t origin, int n_color, int n_aux);
typedef void (FX_CALL *gr_close_fn)(void);
typedef void (FX_CALL *gr_cull_fn)(GrCullMode_t mode);
typedef void (FX_CALL *gr_depth_mode_fn)(GrDepthBufferMode_t mode);
typedef void (FX_CALL *gr_depth_func_fn)(GrCmpFnc_t func);
typedef void (FX_CALL *gr_depth_mask_fn)(FxBool mask);
typedef void (FX_CALL *gr_color_fn)(
    GrCombineFunction_t function, GrCombineFactor_t factor,
    GrCombineLocal_t local, GrCombineOther_t other, FxBool invert);
typedef void (FX_CALL *gr_clear_fn)(GrColor_t color, GrAlpha_t alpha, FxU16 depth);
typedef void (FX_CALL *gr_swap_fn)(int interval);
typedef void (FX_CALL *gr_tri_fn)(const GrVertex *a, const GrVertex *b, const GrVertex *c);
typedef FxU32 (FX_CALL *gr_tex_calc_fn)(
    GrLOD_t lodmin, GrLOD_t lodmax, GrAspectRatio_t aspect, GrTextureFormat_t fmt);
typedef FxU32 (FX_CALL *gr_tex_mem_fn)(FxU32 even_odd, GrTexInfo *info);
typedef FxU32 (FX_CALL *gr_tex_min_fn)(GrChipID_t tmu);
typedef FxU32 (FX_CALL *gr_tex_max_fn)(GrChipID_t tmu);
typedef void (FX_CALL *gr_tex_source_fn)(
    GrChipID_t tmu, FxU32 start, FxU32 even_odd, GrTexInfo *info);
typedef void (FX_CALL *gr_tex_clamp_fn)(
    GrChipID_t tmu, GrTextureClampMode_t s, GrTextureClampMode_t t);
typedef void (FX_CALL *gr_tex_combine_fn)(
    GrChipID_t tmu,
    GrCombineFunction_t rgb_fn, GrCombineFactor_t rgb_fac,
    GrCombineFunction_t a_fn, GrCombineFactor_t a_fac,
    FxBool rgb_inv, FxBool a_inv);
typedef void (FX_CALL *gr_tex_filter_fn)(
    GrChipID_t tmu, GrTextureFilterMode_t minf, GrTextureFilterMode_t magf);
typedef void (FX_CALL *gr_tex_down_fn)(
    GrChipID_t tmu, FxU32 start, FxU32 even_odd, GrTexInfo *info);
typedef void (FX_CALL *gr_tex_table_fn)(GrChipID_t tmu, GrTexTable_t type, void *data);
typedef void (FX_CALL *gr_tex_mip_fn)(GrChipID_t tmu, GrMipMapMode_t mode, FxBool blend);
typedef void (FX_CALL *gr_fog_mode_fn)(GrFogMode_t mode);
typedef void (FX_CALL *gr_fog_color_fn)(GrColor_t color);
typedef void (FX_CALL *gr_fog_table_fn)(const GrFog_t *table);
typedef void (FX_CALL *gr_alpha_fn)(
    GrCombineFunction_t function, GrCombineFactor_t factor,
    GrCombineLocal_t local, GrCombineOther_t other, FxBool invert);
typedef void (FX_CALL *gr_alpha_test_fn)(GrCmpFnc_t function);
typedef void (FX_CALL *gr_alpha_ref_fn)(GrAlpha_t value);
typedef void (FX_CALL *gr_blend_fn)(
    GrAlphaBlendFnc_t rgb_sf, GrAlphaBlendFnc_t rgb_df,
    GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df);
typedef void (FX_CALL *gr_line_fn)(const GrVertex *a, const GrVertex *b);
typedef FxBool (FX_CALL *gr_lfb_read_fn)(
    GrBuffer_t src, FxU32 x, FxU32 y, FxU32 w, FxU32 h, FxU32 stride, void *dst);
typedef FxBool (FX_CALL *gr_lfb_write_fn)(
    GrBuffer_t dst, FxU32 x, FxU32 y, GrLfbSrcFmt_t fmt,
    FxU32 w, FxU32 h, FxI32 stride, void *src);

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
    gr_tex_calc_fn tex_calc;
    gr_tex_mem_fn tex_mem;
    gr_tex_min_fn tex_min;
    gr_tex_max_fn tex_max;
    gr_tex_source_fn tex_source;
    gr_tex_clamp_fn tex_clamp;
    gr_tex_combine_fn tex_combine;
    gr_tex_filter_fn tex_filter;
    gr_tex_down_fn tex_down;
    gr_tex_table_fn tex_table;
    gr_tex_mip_fn tex_mip;
    gr_fog_mode_fn fog_mode;
    gr_fog_color_fn fog_color;
    gr_fog_table_fn fog_table;
    gr_alpha_fn alpha;
    gr_alpha_test_fn alpha_test;
    gr_alpha_ref_fn alpha_ref;
    gr_blend_fn blend;
    gr_line_fn line;
    gr_lfb_read_fn lfb_read;
    gr_lfb_write_fn lfb_write;
} glide;

static int g_inited;
static int g_opened;
static int g_fog = 1;
static float g_fog_near = DF_FOG_NEAR;
static float g_fog_far = DF_FOG_FAR;
static const char *g_why = "glide failed";
static GrColor_t g_clear_color;
static float g_alpha = 255.0f;
static df_u32 g_addr;
static df_u32 g_addr_max;
static int g_ntex;
static df_u32 g_mark_addr;
static int g_mark_ntex;

static struct {
    df_u32 addr;
    GrTexInfo info;
    int live;
} g_tex[GPU_SLOTS];

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

#define BIND(dst, ty, plain, atn, uscore) \
    do { \
        FARPROC sym; \
        sym = find_export(glide.dll, plain, atn, uscore); \
        if (!sym) { \
            g_why = "glide2x.dll has no " plain; \
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
        g_why = "Could not load glide2x.dll.";
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
    BIND(glide.tex_calc, gr_tex_calc_fn, "grTexCalcMemRequired", "grTexCalcMemRequired@16", "_grTexCalcMemRequired@16");
    /*
     * Optional. This one counts the even/odd mip slot the TMU actually
     * reserves. CalcMemRequired is only the host image.
     */
    glide.tex_mem = (gr_tex_mem_fn)(void *)find_export(
        glide.dll,
        "grTexTextureMemRequired",
        "grTexTextureMemRequired@8",
        "_grTexTextureMemRequired@8");
    BIND(glide.tex_min, gr_tex_min_fn, "grTexMinAddress", "grTexMinAddress@4", "_grTexMinAddress@4");
    BIND(glide.tex_max, gr_tex_max_fn, "grTexMaxAddress", "grTexMaxAddress@4", "_grTexMaxAddress@4");
    BIND(glide.tex_source, gr_tex_source_fn, "grTexSource", "grTexSource@16", "_grTexSource@16");
    BIND(glide.tex_clamp, gr_tex_clamp_fn, "grTexClampMode", "grTexClampMode@12", "_grTexClampMode@12");
    BIND(glide.tex_combine, gr_tex_combine_fn, "grTexCombine", "grTexCombine@28", "_grTexCombine@28");
    BIND(glide.tex_filter, gr_tex_filter_fn, "grTexFilterMode", "grTexFilterMode@12", "_grTexFilterMode@12");
    BIND(glide.tex_down, gr_tex_down_fn, "grTexDownloadMipMap", "grTexDownloadMipMap@16", "_grTexDownloadMipMap@16");
    BIND(glide.tex_table, gr_tex_table_fn, "grTexDownloadTable", "grTexDownloadTable@12", "_grTexDownloadTable@12");
    BIND(glide.tex_mip, gr_tex_mip_fn, "grTexMipMapMode", "grTexMipMapMode@12", "_grTexMipMapMode@12");
    BIND(glide.fog_mode, gr_fog_mode_fn, "grFogMode", "grFogMode@4", "_grFogMode@4");
    BIND(glide.fog_color, gr_fog_color_fn, "grFogColorValue", "grFogColorValue@4", "_grFogColorValue@4");
    BIND(glide.fog_table, gr_fog_table_fn, "grFogTable", "grFogTable@4", "_grFogTable@4");
    BIND(glide.alpha, gr_alpha_fn, "grAlphaCombine", "grAlphaCombine@20", "_grAlphaCombine@20");
    BIND(glide.alpha_test, gr_alpha_test_fn, "grAlphaTestFunction", "grAlphaTestFunction@4", "_grAlphaTestFunction@4");
    BIND(glide.alpha_ref, gr_alpha_ref_fn, "grAlphaTestReferenceValue", "grAlphaTestReferenceValue@4", "_grAlphaTestReferenceValue@4");
    BIND(glide.blend, gr_blend_fn, "grAlphaBlendFunction", "grAlphaBlendFunction@16", "_grAlphaBlendFunction@16");
    BIND(glide.line, gr_line_fn, "grDrawLine", "grDrawLine@8", "_grDrawLine@8");
    /* Optional. A console can freeze the picture without them. */
    glide.lfb_read = (gr_lfb_read_fn)(void *)find_export(
        glide.dll, "grLfbReadRegion", "grLfbReadRegion@28", "_grLfbReadRegion@28");
    glide.lfb_write = (gr_lfb_write_fn)(void *)find_export(
        glide.dll, "grLfbWriteRegion", "grLfbWriteRegion@32", "_grLfbWriteRegion@32");
    return 1;
}
#pragma GCC diagnostic pop

static void fill_vertex(GrVertex *v, const DrawVert *s)
{
    v->x = s->x;
    v->y = s->y;
    v->z = 0.0f;
    v->r = s->r;
    v->g = s->g;
    v->b = s->b;
    v->ooz = s->ooz;
    v->a = g_alpha;
    v->oow = s->oow;
    v->tmuvtx[0].sow = s->sow;
    v->tmuvtx[0].tow = s->tow;
    v->tmuvtx[0].oow = s->oow;
    v->tmuvtx[1].sow = 0.0f;
    v->tmuvtx[1].tow = 0.0f;
    v->tmuvtx[1].oow = s->oow;
}

static int lod_of(int n, GrLOD_t *lod)
{
    switch (n) {
    case 256: *lod = GR_LOD_256; return 1;
    case 128: *lod = GR_LOD_128; return 1;
    case 64: *lod = GR_LOD_64; return 1;
    case 32: *lod = GR_LOD_32; return 1;
    case 16: *lod = GR_LOD_16; return 1;
    case 8: *lod = GR_LOD_8; return 1;
    case 4: *lod = GR_LOD_4; return 1;
    case 2: *lod = GR_LOD_2; return 1;
    case 1: *lod = GR_LOD_1; return 1;
    default: return 0;
    }
}

static int aspect_of(int w, int h, GrAspectRatio_t *aspect)
{
    int ratio;

    if (w == h) {
        *aspect = GR_ASPECT_1x1;
        return 1;
    }
    if (w > h) {
        if (h == 0 || (w % h) != 0) {
            return 0;
        }
        ratio = w / h;
        if (ratio == 2) { *aspect = GR_ASPECT_2x1; return 1; }
        if (ratio == 4) { *aspect = GR_ASPECT_4x1; return 1; }
        if (ratio == 8) { *aspect = GR_ASPECT_8x1; return 1; }
        return 0;
    }
    if (w == 0 || (h % w) != 0) {
        return 0;
    }
    ratio = h / w;
    if (ratio == 2) { *aspect = GR_ASPECT_1x2; return 1; }
    if (ratio == 4) { *aspect = GR_ASPECT_1x4; return 1; }
    if (ratio == 8) { *aspect = GR_ASPECT_1x8; return 1; }
    return 0;
}

const char *glide_error(void)
{
    return g_why;
}

static void fog_upload(void);

int glide_open(void *hwnd)
{
    GrHwConfiguration hw;

    g_why = "glide failed";
    g_ntex = 0;
    g_mark_ntex = 0;
    g_mark_addr = 0;
    if (!load_glide()) {
        return 0;
    }
    glide.init();
    g_inited = 1;
    if (!glide.query(&hw)) {
        g_why = "grSstQueryHardware found no Voodoo.";
        glide_close();
        return 0;
    }
    glide.select(0);
    if (!glide.open(
            (FxU32)(UINT_PTR)hwnd,
            GR_RESOLUTION_640x480,
            GR_REFRESH_60Hz,
            GR_COLORFORMAT_ARGB,
            GR_ORIGIN_LOWER_LEFT,
            2,
            1)) {
        g_why = "grSstWinOpen failed. Wanted 640x480 at 60 Hz.";
        glide_close();
        return 0;
    }
    g_opened = 1;

    /* Winding is still unproven, so the depth buffer holds the room. */
    glide.cull(GR_CULL_DISABLE);
    glide.depth_mode(GR_DEPTHBUFFER_ZBUFFER);
    glide.depth_func(GR_CMP_GREATER);
    glide.depth_mask(FXTRUE);
    glide.color(
        GR_COMBINE_FUNCTION_SCALE_OTHER,
        GR_COMBINE_FACTOR_ONE,
        GR_COMBINE_LOCAL_NONE,
        GR_COMBINE_OTHER_TEXTURE,
        FXFALSE);
    glide.tex_combine(
        GR_TMU0,
        GR_COMBINE_FUNCTION_LOCAL,
        GR_COMBINE_FACTOR_NONE,
        GR_COMBINE_FUNCTION_LOCAL,
        GR_COMBINE_FACTOR_NONE,
        FXFALSE,
        FXFALSE);
    glide.tex_clamp(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
    /* Nearest sampling turns a 64-wide brick course into vertical
       streaks once the wall is edge-on. Bilinear keeps the courses. */
    glide.tex_filter(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
    /*
     * One level only. On a 2MB TMU even and odd mip levels share an
     * address in two banks, and the odd levels overwrite the picture.
     * That scrambled the font sheet and the ground.
     */
    glide.tex_mip(GR_TMU0, GR_MIPMAP_DISABLE, FXFALSE);

    /*
     * Table entry i is eye-space w = 2^(3 + i/4) / (8 - (i % 4)).
     * That is the Glide 2 fog-table spacing. Density is 0 at g_fog_near
     * and solid at g_fog_far. The colour matches the clear.
     */
    fog_upload();
    glide_sprite_alpha(0);

    g_addr = glide.tex_min(GR_TMU0);
    g_addr_max = glide.tex_max(GR_TMU0);
    return 1;
}

void glide_close(void)
{
    if (g_opened && glide.close) {
        glide.close();
    }
    g_opened = 0;
    if (g_inited && glide.shutdown) {
        glide.shutdown();
    }
    g_inited = 0;
    if (glide.dll) {
        FreeLibrary(glide.dll);
        glide.dll = 0;
    }
}

static int clamp_byte(int v)
{
    if (v < 0) {
        return 0;
    }
    if (v > 255) {
        return 255;
    }
    return v;
}

static GrColor_t pack_rgb(int r, int g, int b)
{
    /* grSstWinOpen was asked for GR_COLORFORMAT_ARGB. */
    return ((GrColor_t)clamp_byte(r) << 16)
        | ((GrColor_t)clamp_byte(g) << 8)
        | (GrColor_t)clamp_byte(b);
}

void glide_background(int r, int g, int b)
{
    g_clear_color = pack_rgb(r, g, b);
    if (g_opened && glide.fog_color) {
        glide.fog_color(g_clear_color);
    }
}

static void fog_upload(void)
{
    GrFog_t table[GR_FOG_TABLE_SIZE];
    int i;
    float span;

    if (!g_opened || !glide.fog_table || !glide.fog_mode) {
        return;
    }
    span = g_fog_far - g_fog_near;
    if (span < 1.0f) {
        span = 1.0f;
    }
    for (i = 0; i < GR_FOG_TABLE_SIZE; i++) {
        int q;
        float w;
        float density;

        q = i >> 2;
        w = (float)(8u << q) / (float)(8 - (i & 3));
        density = (w - g_fog_near) / span;
        if (density < 0.0f) {
            density = 0.0f;
        }
        if (density > 1.0f) {
            density = 1.0f;
        }
        table[i] = (GrFog_t)(density * 255.0f);
    }
    glide.fog_color(g_clear_color);
    glide.fog_table(table);
    glide.fog_mode(g_fog ? GR_FOG_WITH_TABLE : GR_FOG_DISABLE);
}

void glide_fog(int on)
{
    g_fog = on ? 1 : 0;
    if (g_opened && glide.fog_mode) {
        glide.fog_mode(g_fog ? GR_FOG_WITH_TABLE : GR_FOG_DISABLE);
    }
}

void glide_fog_range(float near_z, float far_z)
{
    if (near_z < 1.0f) {
        near_z = 1.0f;
    }
    if (far_z < near_z + 1.0f) {
        far_z = near_z + 1.0f;
    }
    g_fog_near = near_z;
    g_fog_far = far_z;
    fog_upload();
}

int glide_fog_on(void)
{
    return g_fog;
}

void glide_clear(void)
{
    if (!g_opened) {
        return;
    }
    glide.clear(g_clear_color, 0, GR_ZDEPTHVALUE_FARTHEST);
}

void glide_swap(void)
{
    glide.swap(1);
}

int glide_lfb_read(void *dst, int stride_bytes)
{
    if (!g_opened || !dst || !glide.lfb_read || stride_bytes < 2) {
        return 0;
    }
    return glide.lfb_read(
        GR_BUFFER_FRONTBUFFER, 0, 0,
        (FxU32)DF_VIEW_W, (FxU32)DF_VIEW_H,
        (FxU32)stride_bytes, dst) ? 1 : 0;
}

int glide_lfb_write(const void *src, int stride_bytes)
{
    if (!g_opened || !src || !glide.lfb_write || stride_bytes < 2) {
        return 0;
    }
    return glide.lfb_write(
        GR_BUFFER_BACKBUFFER, 0, 0, GR_LFB_SRC_FMT_565,
        (FxU32)DF_VIEW_W, (FxU32)DF_VIEW_H,
        (FxI32)stride_bytes, (void *)src) ? 1 : 0;
}

void glide_triangle(const DrawVert *a, const DrawVert *b, const DrawVert *c)
{
    GrVertex va, vb, vc;

    fill_vertex(&va, a);
    fill_vertex(&vb, b);
    fill_vertex(&vc, c);
    glide.triangle(&va, &vb, &vc);
}

void glide_line(const DrawVert *a, const DrawVert *b)
{
    GrVertex va, vb;

    if (!g_opened) {
        return;
    }
    fill_vertex(&va, a);
    fill_vertex(&vb, b);
    glide.line(&va, &vb);
}

static void glide_blend(int on)
{
    if (!g_opened) {
        return;
    }
    if (on) {
        glide.blend(
            GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA,
            GR_BLEND_ZERO, GR_BLEND_ZERO);
        return;
    }
    glide.blend(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
}

int glide_palette(const df_u8 *r, const df_u8 *g, const df_u8 *b)
{
    FxU32 pal[256];
    int i;

    if (!g_opened) {
        g_why = "palette before open";
        return 0;
    }
    /*
     * grTexDownloadTable keeps the low 24 bits and writes them straight
     * into the palette. That is 8-bit RGB, red in bits 16..23.
     * A 565 pack lands in the blue and green bytes only, so the room
     * comes out cyan.
     */
    for (i = 0; i < 256; i++) {
        pal[i] = ((FxU32)r[i] << 16) | ((FxU32)g[i] << 8) | (FxU32)b[i];
    }
    glide.tex_table(GR_TMU0, GR_TEXTABLE_PALETTE, pal);
    return 1;
}

/*
 * SST-1 has no GR_TEXFMT_AP_88. Those bytes were sampled as ordinary
 * texels, which shredded the picture. Index 0 is the margin the sprite
 * decoder writes for a zero run (one frame, not the whole animation).
 * ARGB1555 is a Voodoo 1 format: the high bit is alpha, so a black
 * opaque pixel stays. A colour key of black would punch that pixel too,
 * because index 255 is also RGB 0. Walls stay P_8.
 */
/* Reused for every cutout upload. Download copies it onto the card. */
static df_u8 g_expand[256 * 256 * 2];

static df_u8 *expand_argb1555(const df_u8 *pixels, int n,
                              const df_u8 *pr, const df_u8 *pg, const df_u8 *pb)
{
    df_u8 *buf;
    int i;

    if (!pr || !pg || !pb || n < 1 || n > 256 * 256) {
        return 0;
    }
    buf = g_expand;
    for (i = 0; i < n; i++) {
        df_u8 index;
        df_u16 pix;

        index = pixels[i];
        if (index == 0) {
            pix = 0;
        } else {
            pix = (df_u16)(0x8000u
                | ((df_u16)(pr[index] >> 3) << 10)
                | ((df_u16)(pg[index] >> 3) << 5)
                | (df_u16)(pb[index] >> 3));
        }
        buf[i * 2] = (df_u8)(pix & 0xff);
        buf[i * 2 + 1] = (df_u8)(pix >> 8);
    }
    return buf;
}

static int shape_bytes(int pot_w, int pot_h, int cutout, GrTexInfo *info, df_u32 *bytes_out)
{
    GrLOD_t lod;
    GrAspectRatio_t aspect;
    int major;
    FxU32 bytes;

    major = (pot_w > pot_h) ? pot_w : pot_h;
    if (!lod_of(major, &lod) || !aspect_of(pot_w, pot_h, &aspect)) {
        return 0;
    }
    info->largeLod = lod;
    info->smallLod = lod;
    info->aspectRatio = aspect;
    info->format = cutout ? GR_TEXFMT_ARGB_1555 : GR_TEXFMT_P_8;
    info->data = &g_expand[0];
    if (!glide.tex_calc) {
        return 0;
    }
    bytes = glide.tex_calc(lod, lod, info->aspectRatio, info->format);
    if (glide.tex_mem) {
        FxU32 need;

        need = glide.tex_mem(GR_MIPMAPLEVELMASK_BOTH, info);
        if (need > bytes) {
            bytes = need;
        }
    }
    if (bytes == 0) {
        return 0;
    }
    *bytes_out = ((df_u32)bytes + 7u) & ~7u;
    return 1;
}

int glide_texture_bytes(int pot_w, int pot_h, int cutout, df_u32 *bytes)
{
    GrTexInfo info;

    if (!bytes) {
        return 0;
    }
    return shape_bytes(pot_w, pot_h, cutout, &info, bytes);
}

void glide_mark(void)
{
    g_mark_addr = g_addr;
    g_mark_ntex = g_ntex;
}

void glide_rewind(void)
{
    int i;

    for (i = g_mark_ntex; i < g_ntex; i++) {
        g_tex[i].live = 0;
    }
    g_ntex = g_mark_ntex;
    g_addr = g_mark_addr;
}

df_u32 glide_detail_room(void)
{
    df_u32 room;

    if (g_addr_max <= g_mark_addr) {
        return 0;
    }
    room = g_addr_max - g_mark_addr;
    /* A download that would cross a 2MB edge is pushed past it. Leave a gap. */
    if (room > 65536u) {
        return room - 65536u;
    }
    return 0;
}

df_u32 glide_tex_free(void)
{
    if (!g_opened || g_addr_max <= g_addr) {
        return 0;
    }
    return g_addr_max - g_addr;
}

int glide_upload(const df_u8 *pixels, int pot_w, int pot_h, int cutout,
                 const df_u8 *pr, const df_u8 *pg, const df_u8 *pb, df_u32 *addr)
{
    int n;
    df_u32 size;
    df_u32 at;
    df_u32 boundary;
    const df_u8 *src;
    GrTexInfo info;

    if (!pixels || !addr || g_ntex >= GPU_SLOTS) {
        g_why = "texture upload failed";
        return 0;
    }
    if (!shape_bytes(pot_w, pot_h, cutout, &info, &size)) {
        g_why = "texture size is not a Voodoo shape";
        return 0;
    }
    src = pixels;
    if (cutout) {
        n = pot_w * pot_h;
        src = expand_argb1555(pixels, n, pr, pg, pb);
        if (!src) {
            g_why = "texture upload failed";
            return 0;
        }
    }
    info.data = (void *)src;
    /*
     * startAddress is a byte offset between grTexMinAddress and
     * grTexMaxAddress. grTexTextureMemRequired returns the bytes to
     * add for the next free address, and that address stays 8-byte
     * aligned. Shrinking the byte count to 8-byte units packs eight
     * maps into one slot, so a wall reads the font. A download that
     * crosses a 2MB boundary blanks the frame, so a map that would
     * land on the boundary starts after it.
     */
    at = (g_addr + 7u) & ~7u;
    boundary = (at & ~0x1fffffu) + 0x200000u;
    if (at + size > boundary) {
        at = boundary;
    }
    if (at < g_addr || at + size < at || at + size > g_addr_max) {
        g_why = "texture memory is full";
        return 0;
    }
    glide.tex_down(GR_TMU0, at, GR_MIPMAPLEVELMASK_BOTH, &info);
    g_tex[g_ntex].addr = at;
    g_tex[g_ntex].info = info;
    g_tex[g_ntex].live = 1;
    *addr = at;
    g_addr = at + size;
    g_ntex++;
    return 1;
}

void glide_sprite_alpha(int on)
{
    if (!g_opened) {
        return;
    }
    if (on) {
        glide.alpha(
            GR_COMBINE_FUNCTION_SCALE_OTHER,
            GR_COMBINE_FACTOR_ONE,
            GR_COMBINE_LOCAL_NONE,
            GR_COMBINE_OTHER_TEXTURE,
            FXFALSE);
        glide.alpha_test(GR_CMP_GREATER);
        glide.alpha_ref(0);
        return;
    }
    glide.alpha(
        GR_COMBINE_FUNCTION_SCALE_OTHER,
        GR_COMBINE_FACTOR_ONE,
        GR_COMBINE_LOCAL_NONE,
        GR_COMBINE_OTHER_ITERATED,
        FXFALSE);
    glide.alpha_test(GR_CMP_ALWAYS);
    glide.alpha_ref(0);
}

void glide_flat(int on)
{
    if (!g_opened) {
        return;
    }
    if (on) {
        glide.color(
            GR_COMBINE_FUNCTION_LOCAL,
            GR_COMBINE_FACTOR_NONE,
            GR_COMBINE_LOCAL_ITERATED,
            GR_COMBINE_OTHER_NONE,
            FXFALSE);
        glide.tex_combine(
            GR_TMU0,
            GR_COMBINE_FUNCTION_ZERO,
            GR_COMBINE_FACTOR_NONE,
            GR_COMBINE_FUNCTION_ZERO,
            GR_COMBINE_FACTOR_NONE,
            FXFALSE,
            FXFALSE);
        return;
    }
    glide.color(
        GR_COMBINE_FUNCTION_SCALE_OTHER,
        GR_COMBINE_FACTOR_ONE,
        GR_COMBINE_LOCAL_NONE,
        GR_COMBINE_OTHER_TEXTURE,
        FXFALSE);
    glide.tex_combine(
        GR_TMU0,
        GR_COMBINE_FUNCTION_LOCAL,
        GR_COMBINE_FACTOR_NONE,
        GR_COMBINE_FUNCTION_LOCAL,
        GR_COMBINE_FACTOR_NONE,
        FXFALSE,
        FXFALSE);
}

void glide_ui(int on)
{
    if (!g_opened) {
        return;
    }
    if (on) {
        glide.depth_func(GR_CMP_ALWAYS);
        glide.depth_mask(FXFALSE);
        glide.color(
            GR_COMBINE_FUNCTION_SCALE_OTHER,
            GR_COMBINE_FACTOR_ONE,
            GR_COMBINE_LOCAL_NONE,
            GR_COMBINE_OTHER_TEXTURE,
            FXFALSE);
        glide.tex_combine(
            GR_TMU0,
            GR_COMBINE_FUNCTION_LOCAL,
            GR_COMBINE_FACTOR_NONE,
            GR_COMBINE_FUNCTION_LOCAL,
            GR_COMBINE_FACTOR_NONE,
            FXFALSE,
            FXFALSE);
        glide.tex_filter(GR_TMU0, GR_TEXTUREFILTER_POINT_SAMPLED, GR_TEXTUREFILTER_POINT_SAMPLED);
        return;
    }
    glide.depth_func(GR_CMP_GREATER);
    glide.depth_mask(FXTRUE);
    glide.color(
        GR_COMBINE_FUNCTION_SCALE_OTHER,
        GR_COMBINE_FACTOR_ONE,
        GR_COMBINE_LOCAL_NONE,
        GR_COMBINE_OTHER_TEXTURE,
        FXFALSE);
    glide.tex_combine(
        GR_TMU0,
        GR_COMBINE_FUNCTION_LOCAL,
        GR_COMBINE_FACTOR_NONE,
        GR_COMBINE_FUNCTION_LOCAL,
        GR_COMBINE_FACTOR_NONE,
        FXFALSE,
        FXFALSE);
    glide.tex_filter(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
}

void glide_opacity(int alpha, int texture)
{
    if (!g_opened) {
        return;
    }
    if (alpha < 0) {
        g_alpha = 255.0f;
        glide_blend(0);
        return;
    }
    if (alpha > 255) {
        alpha = 255;
    }
    g_alpha = (float)alpha;
    glide_blend(1);
    if (texture) {
        glide.alpha(
            GR_COMBINE_FUNCTION_SCALE_OTHER,
            GR_COMBINE_FACTOR_LOCAL,
            GR_COMBINE_LOCAL_ITERATED,
            GR_COMBINE_OTHER_TEXTURE,
            FXFALSE);
        return;
    }
    glide.alpha(
        GR_COMBINE_FUNCTION_SCALE_OTHER,
        GR_COMBINE_FACTOR_ONE,
        GR_COMBINE_LOCAL_NONE,
        GR_COMBINE_OTHER_ITERATED,
        FXFALSE);
}

void glide_dim(int on, int alpha)
{
    if (!g_opened) {
        return;
    }
    if (on) {
        if (alpha < 0) {
            alpha = 0;
        }
        if (alpha > 255) {
            alpha = 255;
        }
        glide_overlay(1);
        glide_blend(1);
        g_alpha = (float)alpha;
        return;
    }
    g_alpha = 255.0f;
    glide_blend(0);
    glide_overlay(0);
}

void glide_plate(int on)
{
    if (!g_opened) {
        return;
    }
    if (on) {
        glide_overlay(1);
        glide_blend(1);
        /* 191/255 is about 75% opacity. */
        g_alpha = 191.0f;
        return;
    }
    g_alpha = 255.0f;
    glide_blend(0);
    glide_overlay(0);
}

void glide_overlay(int on)
{
    if (!g_opened) {
        return;
    }
    if (on) {
        glide.depth_func(GR_CMP_ALWAYS);
        glide.depth_mask(FXFALSE);
        glide.color(
            GR_COMBINE_FUNCTION_LOCAL,
            GR_COMBINE_FACTOR_NONE,
            GR_COMBINE_LOCAL_ITERATED,
            GR_COMBINE_OTHER_NONE,
            FXFALSE);
        glide.tex_combine(
            GR_TMU0,
            GR_COMBINE_FUNCTION_ZERO,
            GR_COMBINE_FACTOR_NONE,
            GR_COMBINE_FUNCTION_ZERO,
            GR_COMBINE_FACTOR_NONE,
            FXFALSE,
            FXFALSE);
        return;
    }
    glide.depth_func(GR_CMP_GREATER);
    glide.depth_mask(FXTRUE);
    glide.color(
        GR_COMBINE_FUNCTION_SCALE_OTHER,
        GR_COMBINE_FACTOR_ONE,
        GR_COMBINE_LOCAL_NONE,
        GR_COMBINE_OTHER_TEXTURE,
        FXFALSE);
    glide.tex_combine(
        GR_TMU0,
        GR_COMBINE_FUNCTION_LOCAL,
        GR_COMBINE_FACTOR_NONE,
        GR_COMBINE_FUNCTION_LOCAL,
        GR_COMBINE_FACTOR_NONE,
        FXFALSE,
        FXFALSE);
}

void glide_bind(df_u32 addr)
{
    int i;

    for (i = 0; i < g_ntex; i++) {
        if (g_tex[i].live && g_tex[i].addr == addr) {
            glide.tex_source(GR_TMU0, addr, GR_MIPMAPLEVELMASK_BOTH, &g_tex[i].info);
            return;
        }
    }
}
