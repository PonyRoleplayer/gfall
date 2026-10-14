#include "engine/gfx/gfx.h"
#include "engine/gfx/draw.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/texcache.h"

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
    return 1;
}

void gfx_frame(const Camera *cam, const Level *level)
{
    draw_frame(cam, level);
}
