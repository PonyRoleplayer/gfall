#include "engine/ui/shot.h"

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/gfx/glide.h"
#include "engine/platform/fs.h"
#include "engine/ui/font.h"

/*
 * One 640x480 565 grab, kept after the first shot. The BMP is written
 * a row at a time. The buffer's first row is the top of the glass, so
 * the file stores the last row first.
 */

#define SHOT_STRIDE (DF_VIEW_W * 2)
#define SHOT_NAME 64
#define NOTE_HOLD 700
#define NOTE_FADE 1200

static df_u8 *g_snap;
static char g_note[96];
static char g_status[96];
static DWORD g_note_at;
static int g_note_on;
static int g_f12_was;

static void put_dig(char *dst, int *n, int cap, int v, int digits)
{
    int div;
    int i;

    div = 1;
    for (i = 1; i < digits; i++) {
        div *= 10;
    }
    for (i = 0; i < digits; i++) {
        if (*n >= cap - 1) {
            return;
        }
        dst[(*n)++] = (char)('0' + (v / div) % 10);
        div /= 10;
    }
}

static int ends_bmp(const char *s)
{
    int n;

    n = 0;
    while (s[n]) {
        n++;
    }
    if (n < 4) {
        return 0;
    }
    s += n - 4;
    return (s[0] == '.' &&
            (s[1] == 'b' || s[1] == 'B') &&
            (s[2] == 'm' || s[2] == 'M') &&
            (s[3] == 'p' || s[3] == 'P'));
}

static int build_name(char *dst, int cap, const char *arg)
{
    int n;
    int i;

    n = 0;
    if (!arg) {
        return 0;
    }
    while (*arg == ' ') {
        arg++;
    }
    if (!arg[0] || arg[0] == '.') {
        return 0;
    }
    for (i = 0; arg[i]; i++) {
        char c;

        c = arg[i];
        if (c == ' ' && arg[i + 1] == 0) {
            break;
        }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
              c == ' ')) {
            return 0;
        }
        if (n >= cap - 5) {
            return 0;
        }
        dst[n++] = c;
    }
    while (n > 0 && dst[n - 1] == ' ') {
        n--;
    }
    if (n < 1) {
        return 0;
    }
    dst[n] = 0;
    if (!ends_bmp(dst)) {
        if (n + 4 >= cap) {
            return 0;
        }
        dst[n++] = '.';
        dst[n++] = 'b';
        dst[n++] = 'm';
        dst[n++] = 'p';
        dst[n] = 0;
    }
    return 1;
}

static void stamp_name(char *dst, int cap)
{
    SYSTEMTIME st;
    int n;
    const char *pre;
    const char *suf;
    int i;

    GetLocalTime(&st);
    n = 0;
    dst[0] = 0;
    pre = "screenshot_";
    for (i = 0; pre[i] && n < cap - 1; i++) {
        dst[n++] = pre[i];
    }
    put_dig(dst, &n, cap, (int)st.wYear, 4);
    if (n < cap - 1) {
        dst[n++] = '-';
    }
    put_dig(dst, &n, cap, (int)st.wMonth, 2);
    if (n < cap - 1) {
        dst[n++] = '-';
    }
    put_dig(dst, &n, cap, (int)st.wDay, 2);
    if (n < cap - 1) {
        dst[n++] = 'T';
    }
    put_dig(dst, &n, cap, (int)st.wHour, 2);
    put_dig(dst, &n, cap, (int)st.wMinute, 2);
    put_dig(dst, &n, cap, (int)st.wSecond, 2);
    suf = ".bmp";
    for (i = 0; suf[i] && n < cap - 1; i++) {
        dst[n++] = suf[i];
    }
    dst[n] = 0;
}

static void put_u16(df_u8 *p, int v)
{
    p[0] = (df_u8)(v & 255);
    p[1] = (df_u8)((v >> 8) & 255);
}

static void put_u32(df_u8 *p, df_u32 v)
{
    p[0] = (df_u8)(v & 255);
    p[1] = (df_u8)((v >> 8) & 255);
    p[2] = (df_u8)((v >> 16) & 255);
    p[3] = (df_u8)((v >> 24) & 255);
}

static void note_begin(const char *name)
{
    int n;
    int i;
    const char *tail;

    n = 0;
    g_note[0] = 0;
    tail = " taken...";
    for (i = 0; name[i] && n < (int)sizeof(g_note) - 12; i++) {
        g_note[n++] = name[i];
    }
    for (i = 0; tail[i] && n < (int)sizeof(g_note) - 1; i++) {
        g_note[n++] = tail[i];
    }
    g_note[n] = 0;
    g_note_at = GetTickCount();
    g_note_on = 1;
    df_copy(g_status, g_note, (int)sizeof(g_status));
}

static int write_bmp(const char *name)
{
    df_u8 hdr[54];
    static df_u8 row[DF_VIEW_W * 3];
    df_u32 pixels;
    int y;
    int x;

    if (!g_snap) {
        /* Kept for the process: not the temp heap a viewer draws inside. */
        g_snap = (df_u8 *)mem_keep((df_u32)SHOT_STRIDE * (df_u32)DF_VIEW_H);
    }
    if (!g_snap || !glide_lfb_read(g_snap, SHOT_STRIDE)) {
        return 0;
    }
    mem_zero(hdr, 54);
    pixels = (df_u32)DF_VIEW_W * (df_u32)DF_VIEW_H;
    hdr[0] = 'B';
    hdr[1] = 'M';
    put_u32(hdr + 2, 54u + pixels * 3u);
    put_u32(hdr + 10, 54u);
    put_u32(hdr + 14, 40u);
    put_u32(hdr + 18, (df_u32)DF_VIEW_W);
    put_u32(hdr + 22, (df_u32)DF_VIEW_H);
    put_u16(hdr + 26, 1);
    put_u16(hdr + 28, 24);
    put_u32(hdr + 34, pixels * 3u);
    if (!fs_create(name) || !fs_append(hdr, 54)) {
        fs_abort();
        return 0;
    }
    for (y = DF_VIEW_H - 1; y >= 0; y--) {
        const df_u8 *src;

        src = g_snap + y * SHOT_STRIDE;
        for (x = 0; x < DF_VIEW_W; x++) {
            df_u16 p;
            int r;
            int g;
            int b;

            p = (df_u16)src[x * 2] | ((df_u16)src[x * 2 + 1] << 8);
            r = (p >> 11) & 31;
            g = (p >> 5) & 63;
            b = p & 31;
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
            row[x * 3] = (df_u8)b;
            row[x * 3 + 1] = (df_u8)g;
            row[x * 3 + 2] = (df_u8)r;
        }
        if (!fs_append(row, (df_u32)(DF_VIEW_W * 3))) {
            fs_abort();
            return 0;
        }
    }
    fs_finish();
    return 1;
}

static int save_file(const char *name)
{
    if (!write_bmp(name)) {
        df_copy(g_status, "screenshot failed", (int)sizeof(g_status));
        return 0;
    }
    note_begin(name);
    return 1;
}

int shot_save(const char *arg)
{
    char name[SHOT_NAME];

    /* No name takes the F12 name, so "screenshot" alone is F12 from the console. */
    while (arg && *arg == ' ') {
        arg++;
    }
    if (!arg || !arg[0]) {
        stamp_name(name, SHOT_NAME);
        return save_file(name);
    }
    if (!build_name(name, SHOT_NAME, arg)) {
        df_copy(g_status, "screenshot name", (int)sizeof(g_status));
        return 0;
    }
    return save_file(name);
}

const char *shot_status(void)
{
    return g_status;
}

const char *shot_pump(void)
{
    int down;
    char name[SHOT_NAME];

    down = (GetAsyncKeyState(VK_F12) & 0x8000) ? 1 : 0;
    if (!down || g_f12_was) {
        g_f12_was = down;
        return 0;
    }
    g_f12_was = 1;
    stamp_name(name, SHOT_NAME);
    save_file(name);
    return g_status;
}

static void wash(float x, float y, float w, float h)
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

void shot_note(void)
{
    DWORD now;
    DWORD age;
    int alpha;
    int wide;
    int line;
    int x;
    int top;

    if (!g_note_on || !g_note[0]) {
        return;
    }
    now = GetTickCount();
    if (now < g_note_at) {
        g_note_on = 0;
        return;
    }
    age = now - g_note_at;
    if (age < NOTE_HOLD) {
        alpha = 255;
    } else if (age < NOTE_HOLD + NOTE_FADE) {
        alpha = (int)((NOTE_HOLD + NOTE_FADE - age) * 255u / NOTE_FADE);
        if (alpha < 1) {
            alpha = 1;
        }
    } else {
        g_note_on = 0;
        return;
    }
    wide = ui_string_width(g_note);
    line = ui_line_height();
    if (line < 1) {
        line = 8;
    }
    x = DF_VIEW_W - wide - 10;
    if (x < 4) {
        x = 4;
    }
    top = DF_VIEW_H - line - 6;
    /* Glyphs occupy [top - line, top]. wash y is the bottom of the plate. */
    glide_dim(1, alpha);
    wash((float)(x - 4), (float)(top - line - 3), (float)(wide + 8), (float)(line + 6));
    glide_dim(0, 0);
    ui_string_alpha(x, top, g_note, alpha);
}
