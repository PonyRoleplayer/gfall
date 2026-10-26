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
#include "engine/gfx/mip.h"
#include "engine/core/log.h"
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
typedef void (FX_CALL *gr_planar_fn)(int nverts, const GrVertex vlist[]);
typedef void (FX_CALL *gr_constant_fn)(GrColor_t value);
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
    gr_planar_fn planar;
    gr_constant_fn constant;
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
/*
 * The sky's horizon colour, while an outdoor place draws its sky: the
 * clear and the fog take it instead of the viewer's background, so the
 * far town fades into the bottom of the sky. See docs/daylight.md.
 */
static GrColor_t g_sky_color;
static int g_sky_color_on;
/* The level's palette, kept so the sky pass can put it back. */
static FxU32 g_pal[256];
static int g_have_pal;
/*
 * Alpha state. Models take a constant alpha, not the iterated one, so
 * the triangle setup never computes or sends alpha gradients for them.
 * g_cut is on for sprites and text: alpha comes from the texture and
 * is tested. g_fade is a blend opacity 0..255, or -1 for no blending;
 * it lives in the constant colour too, never in the vertices.
 */
static int g_cut;
/*
 * The constant colour's RGB: the light the scene is drawn in, 0xRRGGBB.
 * White draws textures as they are. Anything else multiplies them,
 * without a gradient: see color_textured and docs/lighting.md.
 */
static GrColor_t g_tint = 0xffffffu;
/* 1 while color_flat is the colour path, so a tint change leaves it alone. */
static int g_flat_mode;
/* The decal pass is on: a tint change keeps its combine. */
static int g_decal_mode;
static int g_fade = -1;
static int g_fade_tex;
/*
 * Depth. W-buffering stores the 1/w every textured vertex already
 * carries, as a 16-bit float, so precision follows distance. It also
 * drops ooz from each triangle's parameters. Z-buffering is linear in
 * 1/z: with camera_project's near-scaled ooz a depth step is z^2 /
 * (65535 * DF_NEAR), 4 units at 1000 and 256 at 8192. See docs/glide.md.
 */
static int g_wbuf = 1;
static GrCmpFnc_t g_depth_cmp = GR_CMP_LESS;
static FxU16 g_depth_far = GR_WDEPTHVALUE_FARTHEST;
static int g_planar_on = 1;
/* Mip chains on uploads and GR_MIPMAP_NEAREST. texcache reads it. */
static int g_mip = 1;
/* Handle of the texture on TMU0, so a repeat bind is free. -1 none. */
static int g_bound = -1;
static df_u32 g_addr;
static df_u32 g_addr_max;
/*
 * texcap: a debug ceiling on TMU0, in bytes from grTexMinAddress, so a
 * card with a 4 MB TMU can show what a 2 MB Voodoo 1 gets. 0 is the
 * card's own grTexMaxAddress. See docs/memory.md.
 */
static df_u32 g_tex_cap;
static df_u32 g_addr_card;
static int g_ntex;
static df_u32 g_mark_addr;
static int g_mark_ntex;
/*
 * grBufferSwap(1) waits for the vertical retrace. At 60 Hz a frame that
 * takes 17 ms then shows at 30 fps, so a counter that averages swaps
 * reads values like 35 that the scene never ran at. 0 swaps at once,
 * tearing, and the counter then reads the real rate.
 */
static int g_vsync = 1;
/* Which export spelling find_export matched last: 0 plain, 1 @N, 2 _name@N. */
static int g_form;
/* What grSstQueryHardware said, for glide_report. */
static GrHwConfiguration g_hw;
static int g_have_hw;

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
        g_form = 0;
        return sym;
    }
    sym = GetProcAddress(dll, atn);
    if (sym) {
        g_form = 1;
        return sym;
    }
    g_form = 2;
    return GetProcAddress(dll, uscore);
}

#define BIND(dst, ty, plain, atn, uscore) \
    do { \
        FARPROC sym; \
        sym = find_export(glide.dll, plain, atn, uscore); \
        if (!sym) { \
            g_why = "glide2x.dll has no " plain; \
            log_text(LOG_ERROR, g_why); \
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
        log_text(LOG_ERROR, g_why);
        return 0;
    }
    BIND(glide.init, gr_init_fn, "grGlideInit", "grGlideInit@0", "_grGlideInit@0");
    {
        static const char *form_name[3] = { "plain", "@N", "_name@N" };
        char path[260];
        DWORD len;

        len = GetModuleFileNameA(glide.dll, path, (DWORD)sizeof(path));
        if (len == 0 || len >= sizeof(path)) {
            df_copy(path, "glide2x.dll", (int)sizeof(path));
        }
        log_print(LOG_INFO, "glide: %s, %s exports", path, form_name[g_form]);
    }
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
    BIND(glide.constant, gr_constant_fn, "grConstantColorValue", "grConstantColorValue@4",
         "_grConstantColorValue@4");
    /* Optional. Without it a polygon goes as a fan of grDrawTriangle. */
    glide.planar = (gr_planar_fn)(void *)find_export(
        glide.dll, "grDrawPlanarPolygonVertexList", "grDrawPlanarPolygonVertexList@8",
        "_grDrawPlanarPolygonVertexList@8");
    /* Optional. A console can freeze the picture without them. */
    glide.lfb_read = (gr_lfb_read_fn)(void *)find_export(
        glide.dll, "grLfbReadRegion", "grLfbReadRegion@28", "_grLfbReadRegion@28");
    glide.lfb_write = (gr_lfb_write_fn)(void *)find_export(
        glide.dll, "grLfbWriteRegion", "grLfbWriteRegion@32", "_grLfbWriteRegion@32");
    return 1;
}
#pragma GCC diagnostic pop

/*
 * x and y are snapped to the 1/16 pixel the SST-1 rasterizes at, as the
 * Glide 2 guide asks. Unsnapped, two triangles that share an edge reach
 * the card a fraction apart, and its edge walker drops pixels along the
 * seam: sparkles in every wall joint. Snapped, a shared edge is shared
 * exactly and the card's fill rule leaves no gap. The occlusion seam
 * pass in scene.c relies on that, and snaps its outlines the same way.
 */
static void fill_vertex(GrVertex *v, const DrawVert *s)
{
    v->x = df_snap16(s->x);
    v->y = df_snap16(s->y);
    v->z = 0.0f;
    v->r = s->r;
    v->g = s->g;
    v->b = s->b;
    v->ooz = s->ooz;
    /* Unused: alpha is a constant or the texture's. See g_cut. */
    v->a = 255.0f;
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
static void depth_apply(void);
static void alpha_apply(void);

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
        log_text(LOG_ERROR, g_why);
        glide_close();
        return 0;
    }
    g_hw = hw;
    g_have_hw = 1;
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
        log_text(LOG_ERROR, g_why);
        glide_close();
        return 0;
    }
    g_opened = 1;

    /* Winding is still unproven, so the depth buffer holds the room. */
    glide.cull(GR_CULL_DISABLE);
    g_bound = -1;
    depth_apply();
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
     * Nearest level, picked per pixel by the card: no blend between
     * levels, so it runs at full fill rate. A texture of one level (the
     * font, or every picture with mipmap off) is clamped to that level.
     * See docs/glide.md for the scramble that once kept this off.
     */
    glide_set_mipmap(g_mip);

    /*
     * Table entry i is eye-space w = 2^(3 + i/4) / (8 - (i % 4)).
     * That is the Glide 2 fog-table spacing. Density is 0 at g_fog_near
     * and solid at g_fog_far. The colour matches the clear.
     */
    fog_upload();
    g_cut = 0;
    g_fade = -1;
    alpha_apply();

    g_addr = glide.tex_min(GR_TMU0);
    g_addr_card = glide.tex_max(GR_TMU0);
    glide_set_tex_cap(g_tex_cap / 1024u);
    glide_report(LOG_INFO);
    return 1;
}

void glide_set_tex_cap(df_u32 kb)
{
    df_u32 lo;

    g_tex_cap = kb * 1024u;
    if (!g_opened) {
        return;
    }
    lo = glide.tex_min(GR_TMU0);
    g_addr_max = g_addr_card;
    if (g_tex_cap > 0u && lo + g_tex_cap < g_addr_card) {
        g_addr_max = lo + g_tex_cap;
    }
}

df_u32 glide_tex_cap(void)
{
    return g_tex_cap / 1024u;
}

df_u32 glide_tex_card(void)
{
    if (!g_opened) {
        return 0;
    }
    return (g_addr_card - glide.tex_min(GR_TMU0)) / 1024u;
}

static const char *board_name(GrSstType type)
{
    switch (type) {
    case GR_SSTTYPE_VOODOO: return "Voodoo Graphics";
    case GR_SSTTYPE_SST96: return "Voodoo Rush";
    case GR_SSTTYPE_AT3D: return "AT3D";
    case GR_SSTTYPE_Voodoo2: return "Voodoo2";
    default: return "unknown board";
    }
}

void glide_report(int level)
{
    if (!log_wants(level)) {
        return;
    }
    if (!g_have_hw) {
        log_text(level, "glide: not open yet");
        return;
    }
    if (g_hw.SSTs[0].type == GR_SSTTYPE_VOODOO || g_hw.SSTs[0].type == GR_SSTTYPE_Voodoo2) {
        const GrVoodooConfig_t *v;

        v = &g_hw.SSTs[0].sstBoard.VoodooConfig;
        log_print(level, "board: %s, 1 of %d, frame buffer %d MB, FBI rev %d%s",
                  board_name(g_hw.SSTs[0].type), g_hw.num_sst, v->fbRam, v->fbiRev,
                  v->sliDetect ? ", SLI" : "");
        log_print(level, "texture: %d TMU, TMU0 %d MB rev %d",
                  v->nTexelfx, v->tmuConfig[0].tmuRam, v->tmuConfig[0].tmuRev);
    } else if (g_hw.SSTs[0].type == GR_SSTTYPE_SST96) {
        const GrSst96Config_t *r;

        r = &g_hw.SSTs[0].sstBoard.SST96Config;
        log_print(level, "board: %s, frame buffer %d MB, TMU %d MB",
                  board_name(g_hw.SSTs[0].type), r->fbRam, r->tmuConfig.tmuRam);
    } else {
        log_print(level, "board: %s", board_name(g_hw.SSTs[0].type));
    }
    if (!g_opened) {
        log_text(level, "mode: closed");
        return;
    }
    log_print(level, "mode: 640x480 60 Hz, 2 colour buffers, %s, vsync %s, %s",
              g_wbuf ? "W-buffer" : "Z-buffer", g_vsync ? "on" : "off",
              glide.planar ? (g_planar_on ? "planar polygons" : "planar polygons off")
                           : "no planar polygons in this DLL");
    log_print(level, "textures: %u KB of %u KB left on TMU0; frame grab %s",
              (unsigned int)((g_addr_max > g_addr ? g_addr_max - g_addr : 0u) / 1024u),
              (unsigned int)((g_addr_max - glide.tex_min(GR_TMU0)) / 1024u),
              glide.lfb_read && glide.lfb_write ? "yes" : "no, the console shows black");
}

static void depth_apply(void)
{
    g_depth_cmp = g_wbuf ? GR_CMP_LESS : GR_CMP_GREATER;
    g_depth_far = g_wbuf ? GR_WDEPTHVALUE_FARTHEST : GR_ZDEPTHVALUE_FARTHEST;
    if (!g_opened) {
        return;
    }
    glide.depth_mode(g_wbuf ? GR_DEPTHBUFFER_WBUFFER : GR_DEPTHBUFFER_ZBUFFER);
    glide.depth_func(g_depth_cmp);
}

void glide_set_wbuffer(int on)
{
    g_wbuf = on ? 1 : 0;
    depth_apply();
}

int glide_wbuffer(void)
{
    return g_wbuf;
}

void glide_set_planar(int on)
{
    g_planar_on = on ? 1 : 0;
}

int glide_planar(void)
{
    return g_planar_on && glide.planar;
}

void glide_set_vsync(int on)
{
    g_vsync = on ? 1 : 0;
}

int glide_vsync(void)
{
    return g_vsync;
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

static GrColor_t clear_color(void)
{
    return g_sky_color_on ? g_sky_color : g_clear_color;
}

void glide_background(int r, int g, int b)
{
    g_clear_color = pack_rgb(r, g, b);
    if (g_opened && glide.fog_color) {
        glide.fog_color(clear_color());
    }
}

void glide_sky_color(int on, int r, int g, int b)
{
    GrColor_t was;

    was = clear_color();
    g_sky_color_on = on ? 1 : 0;
    if (on) {
        g_sky_color = pack_rgb(r, g, b);
    }
    if (g_opened && glide.fog_color && clear_color() != was) {
        glide.fog_color(clear_color());
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
    glide.fog_color(clear_color());
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
    glide.clear(clear_color(), 0, g_depth_far);
}

void glide_swap(void)
{
    glide.swap(g_vsync ? 1 : 0);
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

    if (!g_opened) {
        return;
    }
    fill_vertex(&va, a);
    fill_vertex(&vb, b);
    fill_vertex(&vc, c);
    glide.triangle(&va, &vb, &vc);
}

/*
 * A convex planar polygon: one model face, a clipped triangle, or a
 * sprite. Each corner is converted once, not once per triangle that
 * shares it. grDrawPlanarPolygonVertexList computes the s/w, t/w, 1/w
 * gradients once, from its first three vertices, and sends only start
 * values for the rest: those parameters are exactly linear on the
 * glass across a plane. So the fan starts at the corner whose triangle
 * with its two successors has the largest area: snapping each corner
 * to 1/16 pixel moves the gradients least there.
 */
void glide_polygon(const DrawVert *v, int n)
{
    GrVertex g[GLIDE_POLY_MAX];
    GrVertex fan[GLIDE_POLY_MAX];
    float best;
    int start;
    int i;

    if (!g_opened || n < 3 || n > GLIDE_POLY_MAX) {
        return;
    }
    for (i = 0; i < n; i++) {
        fill_vertex(&g[i], &v[i]);
    }
    if (n == 3) {
        glide.triangle(&g[0], &g[1], &g[2]);
        return;
    }
    if (!g_planar_on || !glide.planar) {
        for (i = 1; i < n - 1; i++) {
            glide.triangle(&g[0], &g[i], &g[i + 1]);
        }
        return;
    }
    best = -1.0f;
    start = 0;
    for (i = 0; i < n; i++) {
        const GrVertex *a;
        const GrVertex *b;
        const GrVertex *c;
        float area;

        a = &g[i];
        b = &g[(i + 1) % n];
        c = &g[(i + 2) % n];
        area = (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
        if (area < 0.0f) {
            area = -area;
        }
        if (area > best) {
            best = area;
            start = i;
        }
    }
    for (i = 0; i < n; i++) {
        fan[i] = g[(start + i) % n];
    }
    glide.planar(n, fan);
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
        g_pal[i] = pal[i];
    }
    g_have_pal = 1;
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
 *
 * Every upload is built by mip_build into this buffer: the levels the
 * card gets, largest first. Download copies it onto the card.
 */
static df_u8 g_levels[MIP_MAX_BYTES];

/*
 * side is the first level's. chain runs the levels down to 1 x 1,
 * otherwise the texture is that one level.
 */
static int shape_bytes(int side, int cutout, int chain, GrTexInfo *info, df_u32 *bytes_out)
{
    GrLOD_t lod;
    GrAspectRatio_t aspect;
    FxU32 bytes;

    if (!lod_of(side, &lod) || !aspect_of(side, side, &aspect)) {
        return 0;
    }
    info->largeLod = lod;
    info->smallLod = chain ? GR_LOD_1 : lod;
    info->aspectRatio = aspect;
    info->format = cutout ? GR_TEXFMT_ARGB_1555 : GR_TEXFMT_P_8;
    info->data = &g_levels[0];
    if (!glide.tex_calc) {
        return 0;
    }
    /* lodmin is the smallest level, lodmax the largest. */
    bytes = glide.tex_calc(info->smallLod, info->largeLod, info->aspectRatio, info->format);
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

int glide_texture_bytes(int side, int cutout, int chain, df_u32 *bytes)
{
    GrTexInfo info;

    if (!bytes) {
        return 0;
    }
    return shape_bytes(side, cutout, chain, &info, bytes);
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
    /* A rewound handle will name a new picture after the next upload. */
    if (g_bound >= g_mark_ntex) {
        g_bound = -1;
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

void glide_tex_reset(void)
{
    int i;

    for (i = 0; i < g_ntex; i++) {
        g_tex[i].live = 0;
    }
    g_ntex = 0;
    g_mark_ntex = 0;
    g_mark_addr = 0;
    g_bound = -1;
    if (g_opened) {
        g_addr = glide.tex_min(GR_TMU0);
    }
}

void glide_set_mipmap(int on)
{
    g_mip = on ? 1 : 0;
    if (g_opened) {
        glide.tex_mip(GR_TMU0, g_mip ? GR_MIPMAP_NEAREST : GR_MIPMAP_DISABLE, FXFALSE);
    }
}

int glide_mipmap(void)
{
    return g_mip;
}

df_u32 glide_tex_free(void)
{
    if (!g_opened || g_addr_max <= g_addr) {
        return 0;
    }
    return g_addr_max - g_addr;
}

/*
 * One square picture of one level, in format, from data: what the
 * falloff, the sky's tiles, the dome and the font send. Not budgeted
 * by texcache; upload before texcache_upload so the budget sees the
 * room it leaves. data must stay valid until the call returns; Glide
 * copies it to the card.
 */
static int upload_one(GrTextureFormat_t format, const void *data, int side, df_u32 *handle)
{
    df_u32 size;
    df_u32 at;
    GrLOD_t lod;
    GrAspectRatio_t aspect;
    GrTexInfo info;

    if (!g_opened || !data || !handle || g_ntex >= GPU_SLOTS
        || !lod_of(side, &lod) || !aspect_of(side, side, &aspect)) {
        return 0;
    }
    info.largeLod = lod;
    info.smallLod = lod;
    info.aspectRatio = aspect;
    info.format = format;
    info.data = (void *)data;
    size = glide.tex_calc(lod, lod, aspect, format);
    if (glide.tex_mem) {
        FxU32 need;

        need = glide.tex_mem(GR_MIPMAPLEVELMASK_BOTH, &info);
        if (need > size) {
            size = need;
        }
    }
    size = (size + 7u) & ~7u;
    at = (g_addr + 7u) & ~7u;
    if (at + size > ((at & ~0x1fffffu) + 0x200000u)) {
        at = (at & ~0x1fffffu) + 0x200000u;
    }
    if (at + size > g_addr_max) {
        g_why = "texture memory is full";
        return 0;
    }
    glide.tex_down(GR_TMU0, at, GR_MIPMAPLEVELMASK_BOTH, &info);
    g_tex[g_ntex].addr = at;
    g_tex[g_ntex].info = info;
    g_tex[g_ntex].info.data = 0;
    g_tex[g_ntex].live = 1;
    *handle = (df_u32)g_ntex;
    g_addr = at + size;
    g_ntex++;
    return 1;
}

/* RGB 565: the light decals' falloff and the sky's dome. */
int glide_upload_565(const df_u16 *pixels, int side, df_u32 *handle)
{
    int i;

    if (!pixels || side < 1 || side > 256) {
        return 0;
    }
    /* Glide reads the texels little-endian, as df_u16 already is on x86. */
    for (i = 0; i < side * side; i++) {
        g_levels[i * 2] = (df_u8)(pixels[i] & 0xff);
        g_levels[i * 2 + 1] = (df_u8)(pixels[i] >> 8);
    }
    return upload_one(GR_TEXFMT_RGB_565, g_levels, side, handle);
}

/*
 * P_8, read through whatever palette is on TMU0 when it is drawn: the
 * sky's tiles, drawn under their own palette (glide_sky).
 */
int glide_upload_p8(const df_u8 *pixels, int side, df_u32 *handle)
{
    return upload_one(GR_TEXFMT_P_8, pixels, side, handle);
}

/*
 * ALPHA_INTENSITY_44, one byte a texel: a texel of ink is white and
 * opaque (0xff), anything else clear (0x00). The font sheet: its glyphs
 * are ink or nothing, and the ink's colour comes from the constant
 * colour (glide_tint) when the text is drawn. Half the bytes of the
 * ARGB 1555 it was sent as.
 */
int glide_upload_ai44(const df_u8 *ink, int side, df_u32 *handle)
{
    int i;

    if (!ink || side < 1 || side > 256) {
        return 0;
    }
    for (i = 0; i < side * side; i++) {
        g_levels[i] = ink[i] ? 0xffu : 0x00u;
    }
    return upload_one(GR_TEXFMT_ALPHA_INTENSITY_44, g_levels, side, handle);
}

int glide_replace(df_u32 handle, const df_u8 *pixels)
{
    GrTexInfo info;
    int i;

    i = (int)handle;
    if (!g_opened || !pixels || i < 0 || i >= g_ntex || !g_tex[i].live) {
        return 0;
    }
    info = g_tex[i].info;
    info.data = (void *)pixels;
    glide.tex_down(GR_TMU0, g_tex[i].addr, GR_MIPMAPLEVELMASK_BOTH, &info);
    return 1;
}

int glide_upload(const df_u8 *pixels, int side, int cutout, int drop, int chain,
                 const df_u8 *pr, const df_u8 *pg, const df_u8 *pb, df_u32 *handle)
{
    df_u32 size;
    df_u32 at;
    df_u32 boundary;
    GrTexInfo info;

    if (!pixels || !handle || g_ntex >= GPU_SLOTS || drop < 0 || (side >> drop) < 1) {
        g_why = "texture upload failed";
        return 0;
    }
    if (!shape_bytes(side >> drop, cutout, chain, &info, &size)) {
        g_why = "texture size is not a Voodoo shape";
        return 0;
    }
    if (!mip_build(pixels, side, cutout, drop, chain, pr, pg, pb, g_levels)) {
        g_why = "texture upload failed";
        return 0;
    }
    info.data = (void *)g_levels;
    /*
     * startAddress is a byte offset between grTexMinAddress and
     * grTexMaxAddress. grTexTextureMemRequired returns the bytes to
     * add for the next free address, and that address stays 8-byte
     * aligned. Shrinking the byte count to 8-byte units packs eight
     * maps into one slot, so a wall reads the font. A download that
     * crosses a 2MB boundary blanks the frame, so a map that would
     * land on the boundary starts after it. MIPMAPLEVELMASK_BOTH puts
     * the whole chain at that one address, largest level first.
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
    /* The record index, so glide_bind is a lookup, not a search. */
    *handle = (df_u32)g_ntex;
    g_addr = at + size;
    g_ntex++;
    return 1;
}

/* Sprites and text keep a texel only when its filtered alpha passes half. */
#define CUT_REF 127

/*
 * One place sets the alpha combine, the test, the constant, and the
 * blend, from g_cut and g_fade, so no mode can leave another's alpha
 * behind. Every path uses the constant or the texture alpha: never
 * GR_COMBINE_OTHER_ITERATED, which would make every triangle carry
 * alpha gradients.
 */
static void alpha_apply(void)
{
    int a;

    if (!g_opened) {
        return;
    }
    a = g_fade >= 0 ? g_fade : 255;
    glide.constant(((GrColor_t)a << 24) | g_tint);
    if (g_cut) {
        if (g_fade >= 0 && g_fade_tex) {
            /* Texture alpha times the opacity: a fading glyph. */
            glide.alpha(
                GR_COMBINE_FUNCTION_SCALE_OTHER,
                GR_COMBINE_FACTOR_LOCAL,
                GR_COMBINE_LOCAL_CONSTANT,
                GR_COMBINE_OTHER_TEXTURE,
                FXFALSE);
        } else {
            glide.alpha(
                GR_COMBINE_FUNCTION_SCALE_OTHER,
                GR_COMBINE_FACTOR_ONE,
                GR_COMBINE_LOCAL_NONE,
                GR_COMBINE_OTHER_TEXTURE,
                FXFALSE);
        }
        glide.alpha_test(GR_CMP_GREATER);
        /* A fade scales alpha down, so the half test would cut it short. */
        glide.alpha_ref(g_fade >= 0 ? 0 : CUT_REF);
    } else {
        glide.alpha(
            GR_COMBINE_FUNCTION_LOCAL,
            GR_COMBINE_FACTOR_NONE,
            GR_COMBINE_LOCAL_CONSTANT,
            GR_COMBINE_OTHER_NONE,
            FXFALSE);
        glide.alpha_test(GR_CMP_ALWAYS);
        glide.alpha_ref(0);
    }
    glide_blend(g_fade >= 0);
}

void glide_sprite_alpha(int on)
{
    g_cut = on ? 1 : 0;
    alpha_apply();
}

/* The scene's colour path: texture times one, TMU0 passes its texel. */
static void color_textured(void)
{
    g_flat_mode = 0;
    /*
     * Texture times the constant colour when the scene is lit: the
     * constant is a register, so the triangles still carry no colour.
     * White keeps the plain texture path, exact and unchanged.
     */
    glide.color(
        GR_COMBINE_FUNCTION_SCALE_OTHER,
        g_tint == 0xffffffu ? GR_COMBINE_FACTOR_ONE : GR_COMBINE_FACTOR_LOCAL,
        GR_COMBINE_LOCAL_CONSTANT,
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

/* Vertex colour only. TMU0 outputs zero, so no texel is fetched for it. */
static void color_flat(void)
{
    g_flat_mode = 1;
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
}

/* on draws over the frame: no depth test, no depth write. */
static void depth_over(int on)
{
    glide.depth_func(on ? GR_CMP_ALWAYS : g_depth_cmp);
    glide.depth_mask(on ? FXFALSE : FXTRUE);
}

void glide_tint(df_u32 rgb)
{
    GrColor_t was;

    rgb &= 0xffffffu;
    if (!g_opened || rgb == g_tint) {
        g_tint = rgb;
        return;
    }
    was = g_tint;
    g_tint = rgb;
    /*
     * Only the constant carries the tint; the alpha state does not
     * change. Outdoors this runs once a face, so it is one register
     * write, not alpha_apply's five calls.
     */
    glide.constant(((GrColor_t)(g_fade >= 0 ? g_fade : 255) << 24) | g_tint);
    /* Only a switch to or from white changes the combine. */
    if (!g_flat_mode && !g_decal_mode && ((was == 0xffffffu) != (rgb == 0xffffffu))) {
        color_textured();
    }
}

df_u32 glide_tinted(void)
{
    return g_tint;
}

/*
 * The light decal pass. The decal's colour is the light it adds, 0 at
 * its edge: blended DST_COLOR, ONE the pixel becomes dst + dst * src,
 * so the edge leaves the wall exactly as it was and the centre at most
 * doubles it. The texture is scaled by the tint, so street lamps can
 * fade in at dusk; white (indoors) is the texture as it is. No depth
 * write; the decal is pulled toward the eye so the depth test passes on
 * its own face. No fog: under a multiply, black fog already on the
 * wall stays right. Clamped addressing, so the falloff picture does not
 * wrap at its border.
 */
void glide_decal(int on)
{
    if (!g_opened) {
        return;
    }
    g_decal_mode = on ? 1 : 0;
    if (on) {
        glide.color(
            GR_COMBINE_FUNCTION_SCALE_OTHER,
            GR_COMBINE_FACTOR_LOCAL,
            GR_COMBINE_LOCAL_CONSTANT,
            GR_COMBINE_OTHER_TEXTURE,
            FXFALSE);
        glide.blend(GR_BLEND_DST_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE);
        glide.depth_mask(FXFALSE);
        glide.fog_mode(GR_FOG_DISABLE);
        glide.tex_clamp(GR_TMU0, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
        return;
    }
    glide_blend(g_fade >= 0);
    glide.depth_mask(FXTRUE);
    glide.fog_mode(g_fog ? GR_FOG_WITH_TABLE : GR_FOG_DISABLE);
    glide.tex_clamp(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
    color_textured();
}

/*
 * The sky pass, first in the frame: the sky's palette on TMU0, no depth
 * test or write, no fog, clamped addressing, the texture as it is. Off
 * puts back the level's palette and the scene's state. Two palette
 * downloads a frame, 256 entries each.
 */
void glide_sky(int on, const df_u8 *rgb)
{
    FxU32 pal[256];
    int i;

    if (!g_opened) {
        return;
    }
    if (on) {
        if (!rgb) {
            return;
        }
        for (i = 0; i < 256; i++) {
            pal[i] = ((FxU32)rgb[i * 3] << 16) | ((FxU32)rgb[i * 3 + 1] << 8)
                | (FxU32)rgb[i * 3 + 2];
        }
        glide.tex_table(GR_TMU0, GR_TEXTABLE_PALETTE, pal);
        glide_tint(0xffffffu);
        depth_over(1);
        glide.fog_mode(GR_FOG_DISABLE);
        glide.tex_clamp(GR_TMU0, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
        return;
    }
    if (g_have_pal) {
        glide.tex_table(GR_TMU0, GR_TEXTABLE_PALETTE, g_pal);
    }
    depth_over(0);
    glide.fog_mode(g_fog ? GR_FOG_WITH_TABLE : GR_FOG_DISABLE);
    glide.tex_clamp(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
}

void glide_glow(int on, df_u32 rgb)
{
    FxU32 pal[256];
    int i;

    if (!g_opened) {
        return;
    }
    if (on) {
        for (i = 0; i < 256; i++) {
            pal[i] = 0;
        }
        pal[MIP_GLASS] = (FxU32)(rgb & 0xffffffu);
        glide.tex_table(GR_TMU0, GR_TEXTABLE_PALETTE, pal);
        glide.color(
            GR_COMBINE_FUNCTION_SCALE_OTHER,
            GR_COMBINE_FACTOR_LOCAL,
            GR_COMBINE_LOCAL_CONSTANT,
            GR_COMBINE_OTHER_TEXTURE,
            FXFALSE);
        glide.blend(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE);
        glide.depth_mask(FXFALSE);
        glide.depth_func(g_wbuf ? GR_CMP_LEQUAL : GR_CMP_GEQUAL);
        glide.fog_mode(GR_FOG_DISABLE);
        return;
    }
    if (g_have_pal) {
        glide.tex_table(GR_TMU0, GR_TEXTABLE_PALETTE, g_pal);
    }
    glide_blend(g_fade >= 0);
    glide.depth_mask(FXTRUE);
    glide.depth_func(g_depth_cmp);
    glide.fog_mode(g_fog ? GR_FOG_WITH_TABLE : GR_FOG_DISABLE);
    color_textured();
}

void glide_flat(int on)
{
    if (!g_opened) {
        return;
    }
    if (on) {
        color_flat();
        return;
    }
    color_textured();
}

void glide_ui(int on)
{
    if (!g_opened) {
        return;
    }
    depth_over(on);
    color_textured();
    if (on) {
        glide.tex_filter(GR_TMU0, GR_TEXTUREFILTER_POINT_SAMPLED, GR_TEXTUREFILTER_POINT_SAMPLED);
        return;
    }
    glide.tex_filter(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
}

void glide_opacity(int alpha, int texture)
{
    if (alpha < 0) {
        g_fade = -1;
        alpha_apply();
        return;
    }
    if (alpha > 255) {
        alpha = 255;
    }
    g_fade = alpha;
    g_fade_tex = texture ? 1 : 0;
    alpha_apply();
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
        glide_overlay(1);
        glide_opacity(alpha, 0);
        return;
    }
    glide_opacity(-1, 0);
    glide_overlay(0);
}

void glide_plate(int on)
{
    /* 191/255 is about 75% opacity. */
    glide_dim(on, 191);
}

void glide_overlay(int on)
{
    if (!g_opened) {
        return;
    }
    depth_over(on);
    if (on) {
        color_flat();
        return;
    }
    color_textured();
}

/*
 * handle is what glide_upload returned: an index, so this is a lookup.
 * A bind of the texture already on TMU0 sends nothing, which is why
 * texcache keeps no cache of its own and text needs no forget.
 */
void glide_bind(df_u32 handle)
{
    int i;

    i = (int)handle;
    if (!g_opened || i < 0 || i >= g_ntex || !g_tex[i].live || i == g_bound) {
        return;
    }
    glide.tex_source(GR_TMU0, g_tex[i].addr, GR_MIPMAPLEVELMASK_BOTH, &g_tex[i].info);
    g_bound = i;
}
