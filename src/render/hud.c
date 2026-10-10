#include "render/hud.h"
#include "render/glide.h"

/*
 * 8x8 glyphs, row 0 at the top, bit 7 on the left. The right column and
 * the bottom row stay clear so two letters do not touch.
 */

#define SCALE 2
#define CELL 8
#define GAP 4
#define MARGIN 8

static const struct {
    char ch;
    df_u8 row[CELL];
} g_font[] = {
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { '-', { 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00 } },
    { '0', { 0x70, 0x88, 0x98, 0xa8, 0xc8, 0x88, 0x70, 0x00 } },
    { '1', { 0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00 } },
    { '2', { 0x70, 0x88, 0x08, 0x30, 0x40, 0x80, 0xf8, 0x00 } },
    { '3', { 0x70, 0x88, 0x08, 0x30, 0x08, 0x88, 0x70, 0x00 } },
    { '4', { 0x10, 0x30, 0x50, 0x90, 0xf8, 0x10, 0x10, 0x00 } },
    { '5', { 0xf8, 0x80, 0xf0, 0x08, 0x08, 0x88, 0x70, 0x00 } },
    { '6', { 0x30, 0x40, 0x80, 0xf0, 0x88, 0x88, 0x70, 0x00 } },
    { '7', { 0xf8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40, 0x00 } },
    { '8', { 0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70, 0x00 } },
    { '9', { 0x70, 0x88, 0x88, 0x78, 0x08, 0x10, 0x60, 0x00 } },
    { 'a', { 0x30, 0x48, 0x08, 0x38, 0x48, 0x48, 0x38, 0x00 } },
    { 'c', { 0x38, 0x40, 0x80, 0x80, 0x80, 0x40, 0x38, 0x00 } },
    { 'h', { 0x80, 0x80, 0x80, 0xf0, 0x88, 0x88, 0x88, 0x00 } },
    { 'i', { 0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00 } },
    { 'p', { 0xf0, 0x88, 0x88, 0xf0, 0x80, 0x80, 0x80, 0x00 } },
    { 't', { 0xf8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x10, 0x00 } },
    { 'w', { 0x88, 0x88, 0x88, 0xa8, 0xa8, 0x50, 0x50, 0x00 } },
    { 'x', { 0x88, 0x50, 0x20, 0x20, 0x50, 0x88, 0x00, 0x00 } },
    { 'y', { 0x88, 0x88, 0x50, 0x20, 0x20, 0x40, 0x80, 0x00 } },
    { 'z', { 0xf8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xf8, 0x00 } },
};

static const df_u8 *glyph(char ch)
{
    int i;

    for (i = 0; i < (int)(sizeof(g_font) / sizeof(g_font[0])); i++) {
        if (g_font[i].ch == ch) {
            return g_font[i].row;
        }
    }
    return g_font[0].row;
}

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

static int text_width(const char *s)
{
    int n;

    n = 0;
    while (s[n]) {
        n++;
    }
    return n * CELL * SCALE;
}

static void text(int x, int top, const char *s)
{
    int i;

    i = 0;
    while (s[i]) {
        const df_u8 *row;
        int ry;

        row = glyph(s[i]);
        for (ry = 0; ry < CELL; ry++) {
            int bit;
            int run;
            int col;

            bit = row[ry];
            col = 0;
            while (col < CELL) {
                if ((bit & (0x80 >> col)) == 0) {
                    col++;
                    continue;
                }
                run = 0;
                while (col + run < CELL && (bit & (0x80 >> (col + run))) != 0) {
                    run++;
                }
                rect((float)(x + col * SCALE),
                     (float)(top - (ry + 1) * SCALE),
                     (float)(run * SCALE),
                     (float)SCALE,
                     255.0f, 236.0f, 180.0f);
                col += run;
            }
        }
        x += CELL * SCALE;
        i++;
    }
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

    wide = text_width(pos);
    if (text_width(look) > wide) {
        wide = text_width(look);
    }
    line = CELL * SCALE;
    top = DF_VIEW_H - MARGIN;
    glide_overlay(1);
    rect(4.0f,
         (float)(top - line - GAP - line - 4),
         (float)(wide + MARGIN),
         (float)(line + GAP + line + 8),
         0.0f, 0.0f, 0.0f);
    text(MARGIN, top, pos);
    text(MARGIN, top - line - GAP, look);
    glide_overlay(0);
}
