#include "engine/ui/hud.h"

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/gfx/glide.h"
#include "engine/ui/font.h"

#define GAP 2
#define MARGIN 8

static void rect(float x, float y, float w, float h, float r, float g, float b)
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

static int fmt_int(char *dst, int cap, int v)
{
    char tmp[12];
    int n;
    int i;
    int neg;
    unsigned int u;

    if (cap < 2) {
        return 0;
    }
    neg = v < 0;
    u = neg ? (unsigned int)(-v) : (unsigned int)v;
    n = 0;
    do {
        tmp[n++] = (char)('0' + (int)(u % 10u));
        u /= 10u;
    } while (u && n < (int)sizeof(tmp));
    i = 0;
    if (neg && i < cap - 1) {
        dst[i++] = '-';
    }
    while (n > 0 && i < cap - 1) {
        dst[i++] = tmp[--n];
    }
    dst[i] = 0;
    return i;
}

static void add_str(char *dst, int *n, int cap, const char *s)
{
    int i;

    i = 0;
    while (s[i] && *n < cap - 1) {
        dst[*n] = s[i];
        *n += 1;
        i++;
    }
    dst[*n] = 0;
}

static void add_int(char *dst, int *n, int cap, int v)
{
    char num[16];

    fmt_int(num, (int)sizeof(num), v);
    add_str(dst, n, cap, num);
}

static int yaw_deg(float yaw)
{
    int d;

    d = (int)(yaw * (360.0f / DF_TAU));
    d %= 360;
    if (d < 0) {
        d += 360;
    }
    return d;
}

static int pitch_deg(float pitch)
{
    return (int)(pitch * (360.0f / DF_TAU));
}

static int g_fps_on = 1;
static int g_fps_ready;
static int g_fps;
static int g_fps_frames;
static DWORD g_fps_tick;
static int g_fps_key;

static void fps_sample(void)
{
    DWORD now;
    DWORD dt;
    int down;

    down = (GetAsyncKeyState(VK_F3) & 0x8000) ? 1 : 0;
    if (down && !g_fps_key) {
        g_fps_on = g_fps_on ? 0 : 1;
    }
    g_fps_key = down;
    now = GetTickCount();
    if (g_fps_tick == 0) {
        g_fps_tick = now;
        return;
    }
    g_fps_frames++;
    dt = now - g_fps_tick;
    if (dt >= 500) {
        g_fps = (int)(((unsigned int)g_fps_frames * 1000u) / dt);
        g_fps_frames = 0;
        g_fps_tick = now;
        g_fps_ready = 1;
    }
}

static void draw_fps(void)
{
    char fps[24];
    int n;
    int wide;
    int line;
    int top;

    if (!g_fps_on || !g_fps_ready) {
        return;
    }
    n = 0;
    fps[0] = 0;
    add_str(fps, &n, (int)sizeof(fps), "fps ");
    add_int(fps, &n, (int)sizeof(fps), g_fps);
    wide = ui_string_width(fps);
    line = ui_line_height();
    top = DF_VIEW_H - MARGIN;
    glide_plate(1);
    rect((float)(DF_VIEW_W - wide - 16),
         (float)(top - line - 4),
         (float)(wide + 12),
         (float)(line + 8),
         0.0f, 0.0f, 0.0f);
    glide_plate(0);
    ui_string(DF_VIEW_W - wide - 10, top, fps);
}

void hud_draw(const Camera *cam)
{
    char pos[64];
    char look[64];
    int n;
    int wide;
    int top;
    int line;

    if (!cam) {
        return;
    }
    fps_sample();
    n = 0;
    pos[0] = 0;
    add_str(pos, &n, (int)sizeof(pos), "x ");
    add_int(pos, &n, (int)sizeof(pos), (int)cam->x);
    add_str(pos, &n, (int)sizeof(pos), " y ");
    add_int(pos, &n, (int)sizeof(pos), (int)cam->y);
    add_str(pos, &n, (int)sizeof(pos), " z ");
    add_int(pos, &n, (int)sizeof(pos), (int)cam->z);

    n = 0;
    look[0] = 0;
    add_str(look, &n, (int)sizeof(look), "yaw ");
    add_int(look, &n, (int)sizeof(look), yaw_deg(cam->yaw));
    add_str(look, &n, (int)sizeof(look), " pitch ");
    add_int(look, &n, (int)sizeof(look), pitch_deg(cam->pitch));

    wide = ui_string_width(pos);
    if (ui_string_width(look) > wide) {
        wide = ui_string_width(look);
    }
    line = ui_line_height();
    top = DF_VIEW_H - MARGIN;
    glide_plate(1);
    rect(4.0f,
         (float)(top - line - GAP - line - 4),
         (float)(wide + MARGIN),
         (float)(line + GAP + line + 8),
         0.0f, 0.0f, 0.0f);
    glide_plate(0);
    ui_string(MARGIN, top, pos);
    ui_string(MARGIN, top - line - GAP, look);
    draw_fps();
}
