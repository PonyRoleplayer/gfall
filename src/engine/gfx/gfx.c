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
