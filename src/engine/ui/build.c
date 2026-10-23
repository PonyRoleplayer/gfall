#include "engine/ui/build.h"

#include "engine/core/df.h"
#include "engine/gfx/glide.h"
#include "engine/ui/font.h"

/*
 * BUILD_STAMP is the local time of the link, YYYY-MM-DDTHH:MM:SS.
 * A compile that does not pass it falls back to the compiler's own
 * date and time, which is still unique to that translation.
 */
#ifndef BUILD_STAMP
#define BUILD_STAMP __DATE__ " " __TIME__
#endif

static const char g_text[] = "build " BUILD_STAMP;
static int g_draw = 1;

const char *build_text(void)
{
    return g_text;
}

void build_set_draw(int on)
{
    g_draw = on ? 1 : 0;
}

int build_draw(void)
{
    return g_draw;
}

static void plate(float x, float y, float w, float h)
{
    DrawVert v[4];
    int i;

    for (i = 0; i < 4; i++) {
        v[i].oow = 1.0f;
        v[i].ooz = 1.0f;
        v[i].sow = 0.0f;
        v[i].tow = 0.0f;
        v[i].r = 0.0f;
        v[i].g = 0.0f;
        v[i].b = 0.0f;
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

void build_legend(void)
{
    int wide;
    int line;
    int x;
    int top;

    if (!g_draw) {
        return;
    }
    wide = ui_string_width(g_text);
    line = ui_line_height();
    if (line < 1) {
        line = 8;
    }
    x = 8;
    top = line + 8;
    glide_dim(1, 180);
    plate((float)(x - 4), (float)(top - line - 3), (float)(wide + 8), (float)(line + 6));
    glide_dim(0, 0);
    ui_string(x, top, g_text);
}
