#include "engine/gfx/gfx.h"
#include "engine/ui/console.h"
#include "engine/core/log.h"
#include "engine/core/prof.h"
#include "engine/scene/light.h"
#include "engine/scene/clock.h"
#include "engine/scene/weather.h"
#include "engine/gfx/sky.h"
#include "engine/gfx/draw.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/texcache.h"
#include "engine/ui/font.h"
#include "engine/ui/pic.h"

static const char *g_why = "graphics failed";

/* The screen's pictures on the card (pic.c, docs/pictures.md). */
static df_u32 pic_bytes(int side, int cutout)
{
    df_u32 bytes;

    if (!glide_texture_bytes(side, cutout, 0, &bytes)) {
        return (df_u32)side * (df_u32)side * (cutout ? 2u : 1u);
    }
    return bytes;
}

static int pic_upload(const df_u8 *texels, int side, int cutout, df_u32 addr, df_u32 *handle)
{
    return glide_upload_chain(texels, side, cutout, 0, 0, addr, handle);
}

static df_u32 g_pic_tint;
static int g_pic_alpha;

static void pic_begin_draw(int cutout, int alpha)
{
    g_pic_tint = glide_tinted();
    g_pic_alpha = alpha;
    glide_ui(1);
    glide_tint(0xffffffu);
    if (cutout) {
        glide_sprite_alpha(1);
    }
    if (alpha < 255) {
        glide_opacity(alpha, cutout);
    }
}

static void pic_quad(df_u32 handle, float x0, float y0, float x1, float y1,
                     float s0, float t0, float s1, float t1)
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
    v[0].x = x0;
    v[0].y = y0;
    v[0].sow = s0;
    v[0].tow = t0;
    v[1].x = x1;
    v[1].y = y0;
    v[1].sow = s1;
    v[1].tow = t0;
    v[2].x = x1;
    v[2].y = y1;
    v[2].sow = s1;
    v[2].tow = t1;
    v[3].x = x0;
    v[3].y = y1;
    v[3].sow = s0;
    v[3].tow = t1;
    glide_bind(handle);
    glide_polygon(v, 4);
}

static void pic_end_draw(void)
{
    if (g_pic_alpha < 255) {
        glide_opacity(-1, 0);
    }
    glide_sprite_alpha(0);
    glide_tint(g_pic_tint);
    glide_ui(0);
}

static const PicOps g_pic_ops = {
    pic_bytes, pic_upload, glide_release,
    pic_begin_draw, glide_pic_palette, pic_quad, pic_end_draw
};

/*
 * The pictures' room, after the fixed pictures and before the place's,
 * so the place's budget sees what is left; every picture goes back in.
 */
static void pictures_prepare(void)
{
    df_u32 bytes;
    df_u32 lo;

    bytes = (df_u32)pic_room() * 1024u;
    if (bytes && glide_tex_take(bytes, &lo)) {
        pic_begin(lo, lo + bytes, &g_pic_ops);
        return;
    }
    if (bytes) {
        log_print(LOG_WARN, "pictures: no %u KB room left on the card", (unsigned)(bytes / 1024u));
    }
    pic_begin(0, 0, &g_pic_ops);
}
/* A level is on the card, and mipmap changed: send it again next frame. */
static int g_prepared;
static int g_reload;
/* The prepared level, for a light rebuild. The caller keeps it alive. */
static const Level *g_level;

int gfx_startup(void *hwnd)
{
    if (!glide_open(hwnd)) {
        g_why = glide_error();
        return 0;
    }
    return 1;
}

void gfx_shutdown(void)
{
    g_prepared = 0;
    g_level = 0;
    g_reload = 0;
    sky_reset();
    draw_set_extra(0);
    draw_set_under(0);
    pic_end();
    glide_close();
}

const char *gfx_error(void)
{
    return g_why;
}

/*
 * The last place's load on the card side, in cycles (gfx_load_times):
 * its pictures sent or put on residency, its light decals cut, and the
 * prime before its first frame.
 */
static df_u32 g_load_card;
static df_u32 g_load_lights;
static df_u32 g_load_prime;

int gfx_prepare(const Level *level)
{
    g_load_card = 0;
    g_load_lights = 0;
    g_load_prime = 0;
    if (level && level->streamed) {
        Weather w;

        /* The fixed pictures, then the rest of the card to the stream's residency. */
        glide_tex_reset();
        draw_prepare();
        weather_state(&w);
        sky_prepare(level->sky, clock_minute(), w.sky);
        draw_light_upload(1);
        if (level->palette) {
            font_upload(level->palette->r, level->palette->g, level->palette->b);
        }
        pictures_prepare();
        if (!texcache_stream_begin(level)) {
            g_why = texcache_error();
            return 0;
        }
        g_level = level;
        g_prepared = 1;
        return 1;
    }
    if (level) {
        log_print(LOG_INFO, "level: %d models in %d blocks, %d sprites, %d pictures",
                  level->scene.nmodels, level->scene.nspans,
                  level->scene.nsprites, level->nimages);
    }
    /* Start the card from empty, so a second prepare replaces the first. */
    glide_tex_reset();
    draw_prepare();
    /*
     * The sky and the light falloff first, so the picture budget sees
     * the room left. Without room for them the place draws as before.
     */
    {
        Weather w;

        weather_state(&w);
        sky_prepare(level ? level->sky : 0, clock_minute(), w.sky);
    }
    draw_light_upload(level && level->scene.nlights > 0);
    /*
     * The font before the level's pictures too: the budget then sees the
     * exact room, instead of keeping back a guess for it.
     */
    if (level && level->palette) {
        font_upload(level->palette->r, level->palette->g, level->palette->b);
    }
    pictures_prepare();
    g_load_card = prof_now();
    if (!texcache_upload(level)) {
        g_why = texcache_error();
        return 0;
    }
    g_load_lights = prof_now();
    g_load_card = g_load_lights - g_load_card;
    glide_mark();
    light_build(&level->scene);
    g_load_lights = prof_now() - g_load_lights;
    g_level = level;
    g_prepared = 1;
    return 1;
}

void gfx_load_times(df_u32 *card, df_u32 *lights, df_u32 *prime)
{
    *card = g_load_card;
    *lights = g_load_lights;
    *prime = g_load_prime;
}

void gfx_set_light_range(int scale)
{
    light_set_range(scale);
    if (g_prepared && g_level) {
        light_build(&g_level->scene);
    }
}

void gfx_set_light_shadow(int on)
{
    light_set_shadow(on);
    if (g_prepared && g_level) {
        light_build(&g_level->scene);
    }
}

void gfx_hold(const char *text)
{
    draw_hold(text);
}

void gfx_fade_in(float secs)
{
    draw_fade_in(secs);
}

void gfx_prime(const Level *level, const Camera *cam)
{
    g_load_prime = prof_now();
    draw_prime(level, cam);
    g_load_prime = prof_now() - g_load_prime;
}

int gfx_paused(void)
{
    return console_open() || draw_pause();
}

void gfx_set_pause(int on)
{
    draw_set_pause(on);
}

void gfx_set_use_key(int on)
{
    draw_set_use_key(on);
}

void gfx_frame(const Camera *cam, const Level *level)
{
    /* A streamed world's pictures are the game's to add: a mipmap or texcap change waits for a place. */
    if (g_reload && level && level->streamed) {
        g_reload = 0;
        log_text(LOG_WARN, "textures: a streamed world keeps its card as it is");
    }
    if (g_reload && level) {
        g_reload = 0;
        if (!gfx_prepare(level)) {
            log_print(LOG_ERROR, "the level did not go back on the card: %s",
                      gfx_error());
        }
    }
    draw_frame(cam, level);
}

void gfx_set_mipmap(int on)
{
    on = on ? 1 : 0;
    if (on == glide_mipmap()) {
        return;
    }
    glide_set_mipmap(on);
    g_reload = g_prepared;
}

void gfx_set_texcap(int kb)
{
    if (kb < 0) {
        kb = 0;
    }
    /* Under half a megabyte not even the font and the sky fit. */
    if (kb > 0 && kb < 512) {
        kb = 512;
    }
    if ((df_u32)kb == glide_tex_cap()) {
        return;
    }
    glide_set_tex_cap((df_u32)kb);
    g_reload = g_prepared;
}

int gfx_texcap(void)
{
    return (int)glide_tex_cap();
}

int gfx_mipmap(void)
{
    return glide_mipmap();
}

void gfx_set_extra(void (*fn)(void))
{
    draw_set_extra(fn);
}

void gfx_set_under(void (*fn)(void))
{
    draw_set_under(fn);
}

void gfx_set_fog(int on)
{
    glide_fog(on);
    scene_set_limits(draw_far(), on);
}

int gfx_fog(void)
{
    return glide_fog_on();
}

void gfx_set_far(float far)
{
    float fog_far;

    draw_set_far(far);
    /* Keep fog solid a little before the clip, same ratio as 1100/1400. */
    fog_far = df_fog_far(draw_far());
    glide_fog_range(DF_FOG_NEAR, fog_far);
    scene_set_limits(draw_far(), gfx_fog());
}

float gfx_far(void)
{
    return draw_far();
}
