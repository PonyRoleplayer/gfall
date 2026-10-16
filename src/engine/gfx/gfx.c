#include "engine/gfx/gfx.h"
#include "engine/gfx/draw.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/texcache.h"
#include "engine/ui/font.h"

static const char *g_why = "graphics failed";

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
    if (!texcache_upload(level)) {
        g_why = texcache_error();
        return 0;
    }
    if (level->palette) {
        font_upload(level->palette->r, level->palette->g, level->palette->b);
    }
    /* Detail maps upload after this. Rewind must not cover the font. */
    glide_mark();
    return 1;
}

void gfx_frame(const Camera *cam, const Level *level)
{
    draw_frame(cam, level);
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
    fog_far = draw_far() * (DF_FOG_FAR / DF_FAR);
    glide_fog_range(DF_FOG_NEAR, fog_far);
    scene_set_limits(draw_far(), gfx_fog());
}

float gfx_far(void)
{
    return draw_far();
}
