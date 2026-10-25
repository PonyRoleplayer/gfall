#include "engine/gfx/gfx.h"
#include "engine/core/log.h"
#include "engine/scene/light.h"
#include "engine/scene/clock.h"
#include "engine/gfx/sky.h"
#include "engine/gfx/draw.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/texcache.h"
#include "engine/ui/font.h"

static const char *g_why = "graphics failed";
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
    glide_close();
}

const char *gfx_error(void)
{
    return g_why;
}

int gfx_prepare(const Level *level)
{
    if (level) {
        log_print(LOG_INFO, "level: %d models in %d blocks, %d sprites, %d pictures",
                  level->scene.nmodels, level->scene.nspans,
                  level->scene.nsprites, level->nimages);
    }
    /* Start the card from empty, so a second prepare replaces the first. */
    glide_tex_reset();
    /*
     * The sky and the light falloff first, so the picture budget sees
     * the room left. Without room for them the place draws as before.
     */
    sky_prepare(level ? level->sky : 0, clock_minute());
    draw_light_upload(level && level->scene.nlights > 0);
    if (!texcache_upload(level)) {
        g_why = texcache_error();
        return 0;
    }
    if (level->palette) {
        font_upload(level->palette->r, level->palette->g, level->palette->b);
    }
    glide_mark();
    light_build(&level->scene);
    g_level = level;
    g_prepared = 1;
    return 1;
}

void gfx_set_light_range(int scale)
{
    light_set_range(scale);
    if (g_prepared && g_level) {
        light_build(&g_level->scene);
    }
}

void gfx_frame(const Camera *cam, const Level *level)
{
    if (g_reload && level) {
        g_reload = 0;
        if (!gfx_prepare(level)) {
            log_print(LOG_ERROR, "mipmap: the level did not go back on the card: %s",
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
