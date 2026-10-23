#include "engine/ui/console.h"

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/ui/shot.h"
#include "engine/ui/build.h"
#include "engine/gfx/camera.h"
#include "engine/gfx/draw.h"
#include "engine/gfx/gfx.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/texcache.h"
#include "engine/scene/scene.h"
#include "engine/ui/font.h"

/*
 * The panel slides down from the top over a grabbed frame. The log is
 * a ring: past CON_LINES, the oldest line is dropped. The grab is one
 * 640x480 565 picture, allocated the first time the console opens.
 */

#define CON_LINES 48
#define CON_COLS 96
#define CON_H 240
#define CON_TYPE 64
#define CON_SLIDE_MS 200
#define SHOT_STRIDE (DF_VIEW_W * 2)

static char g_log[CON_LINES][CON_COLS];
static int g_next;
static int g_count;
static char g_type[CON_TYPE];
static int g_tlen;
static unsigned char g_was[256];
static int g_h;
static int g_target;
static int g_esc_lock;
static int g_toggle_vk;
static int g_toggle_known;
static DWORD g_anim;
static int g_shot_ok;
static int g_shot_tried;
static df_u8 *g_shot;
static const Camera *g_cam;

static int key_down(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
}

static int edge(int vk)
{
    int down;
    int was;
    int i;

    i = vk & 255;
    down = key_down(vk);
    was = g_was[i];
    g_was[i] = (unsigned char)down;
    return down && !was;
}

static int toggle_vk(void)
{
    if (!g_toggle_known) {
        /* Scan code 0x29 is the key to the left of 1. */
        g_toggle_vk = (int)MapVirtualKeyA(0x29, 1);
        if (g_toggle_vk == 0) {
            g_toggle_vk = 0xC0;
        }
        g_toggle_known = 1;
    }
    return g_toggle_vk;
}

int console_open(void)
{
    return (g_h > 0 || g_target > 0) ? 1 : 0;
}

void console_bind(const Camera *cam)
{
    g_cam = cam;
}

void console_log(const char *s)
{
    if (!s) {
        s = "";
    }
    df_copy(g_log[g_next], s, CON_COLS);
    g_next++;
    if (g_next >= CON_LINES) {
        g_next = 0;
    }
    if (g_count < CON_LINES) {
        g_count++;
    }
}

void console_release(void)
{
    g_shot_ok = 0;
    g_shot_tried = 0;
}

int console_take_escape(void)
{
    if (g_esc_lock) {
        return 1;
    }
    if (!console_open()) {
        return 0;
    }
    g_esc_lock = 1;
    g_target = 0;
    return 1;
}

static void say(const char *s)
{
    console_log(s);
}

static void say_int(const char *name, int v)
{
    char buf[CON_COLS];
    char tmp[12];
    int n;
    int i;
    int neg;
    unsigned int u;

    n = 0;
    buf[0] = 0;
    while (name[n] && n < CON_COLS - 1) {
        buf[n] = name[n];
        n++;
    }
    if (n < CON_COLS - 1) {
        buf[n++] = ' ';
    }
    neg = v < 0;
    u = neg ? (unsigned int)(-v) : (unsigned int)v;
    i = 0;
    do {
        tmp[i++] = (char)('0' + (int)(u % 10u));
        u /= 10u;
    } while (u && i < (int)sizeof(tmp));
    if (neg && n < CON_COLS - 1) {
        buf[n++] = '-';
    }
    while (i > 0 && n < CON_COLS - 1) {
        buf[n++] = tmp[--i];
    }
    buf[n] = 0;
    console_log(buf);
}

static int parse_int(const char *s, int *out)
{
    int i;
    int sign;
    int v;

    if (!s || !s[0]) {
        return 0;
    }
    i = 0;
    sign = 1;
    if (s[0] == '-') {
        sign = -1;
        i++;
    }
    if (s[i] < '0' || s[i] > '9') {
        return 0;
    }
    v = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        if (v > 200000) {
            return 0;
        }
        v = v * 10 + (s[i] - '0');
        i++;
    }
    if (s[i]) {
        return 0;
    }
    *out = v * sign;
    return 1;
}

static int parse_switch(const char *s, int *out)
{
    if (!s || !s[0]) {
        return 0;
    }
    if (df_eq(s, "1") || df_eq(s, "on")) {
        *out = 1;
        return 1;
    }
    if (df_eq(s, "0") || df_eq(s, "off")) {
        *out = 0;
        return 1;
    }
    return 0;
}

static void cmd_screenshot(const char *arg)
{
    if (!shot_save(arg)) {
        say(shot_status());
        return;
    }
    say(shot_status());
}

static void cmd_help(void)
{
    say("fov 45..120");
    say("fog on|off");
    say("cull on|off");
    say("occlude on|off");
    say("covers on|off");
    say("version on|off");
    say("far units");
    say("faces on|off");
    say("stats");
    say("pos");
    say("screenshot name");
    say("clear");
    say("help");
}

static void cmd_fov(const char *arg)
{
    int v;

    if (!arg[0]) {
        say_int("fov", (int)camera_fov());
        return;
    }
    if (!parse_int(arg, &v)) {
        say("fov 45..120");
        return;
    }
    camera_set_fov((float)v);
    say_int("fov", (int)camera_fov());
}

static void cmd_fog(const char *arg)
{
    int v;

    if (!arg[0]) {
        say_int("fog", gfx_fog());
        return;
    }
    if (!parse_switch(arg, &v)) {
        say("fog on|off");
        return;
    }
    gfx_set_fog(v);
    say_int("fog", gfx_fog());
}

/* Off keeps the frustum and skips the wall test, to compare a frame. */
static void cmd_occlude(const char *arg)
{
    int v;

    if (!arg[0]) {
        say_int("occlude", scene_occlude());
        return;
    }
    if (!parse_switch(arg, &v)) {
        say("occlude on|off");
        return;
    }
    scene_set_occlude(v);
    say_int("occlude", scene_occlude());
}

static void cmd_covers(const char *arg)
{
    int v;

    if (!arg[0]) {
        say_int("covers", scene_cover_draw());
        return;
    }
    if (!parse_switch(arg, &v)) {
        say("covers on|off");
        return;
    }
    scene_set_cover_draw(v);
    say_int("covers", scene_cover_draw());
}

static void cmd_version(const char *arg)
{
    int v;

    if (!arg[0]) {
        say(build_text());
        say_int("version", build_draw());
        return;
    }
    if (!parse_switch(arg, &v)) {
        say("version on|off");
        return;
    }
    build_set_draw(v);
    say(build_text());
    say_int("version", build_draw());
}

static void cmd_cull(const char *arg)
{
    int v;

    if (!arg[0]) {
        say_int("cull", scene_cull());
        return;
    }
    if (!parse_switch(arg, &v)) {
        say("cull on|off");
        return;
    }
    scene_set_cull(v);
    say_int("cull", scene_cull());
}

static void cmd_far(const char *arg)
{
    int v;

    if (!arg[0]) {
        say_int("far", (int)gfx_far());
        return;
    }
    if (!parse_int(arg, &v) || v < 1) {
        say("far units");
        return;
    }
    gfx_set_far((float)v);
    say_int("far", (int)gfx_far());
}

static void cmd_faces(const char *arg)
{
    int v;

    if (!arg[0]) {
        say_int("faces", draw_faces());
        return;
    }
    if (!parse_switch(arg, &v)) {
        say("faces on|off");
        return;
    }
    draw_set_faces(v);
    say_int("faces", draw_faces());
}

static void cmd_stats(void)
{
    int models;
    int sprites;
    int kept;
    int dropped;

    say_int("fov", (int)camera_fov());
    say_int("fog", gfx_fog());
    say_int("far", (int)gfx_far());
    say_int("cull", scene_cull());
    say_int("faces", draw_faces());
    draw_stats(&models, &sprites, &kept, &dropped);
    say_int("models", models);
    say_int("sprites", sprites);
    say_int("faces", kept);
    say_int("hidden", dropped);
    say_int("occlude", scene_occlude());
    scene_occ_stats(&kept, &dropped);
    say_int("occluders", kept);
    say_int("occluded", dropped);
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

static void cmd_pos(void)
{
    if (!g_cam) {
        say("no camera");
        return;
    }
    say_int("x", (int)g_cam->x);
    say_int("y", (int)g_cam->y);
    say_int("z", (int)g_cam->z);
    say_int("yaw", yaw_deg(g_cam->yaw));
    say_int("pitch", (int)(g_cam->pitch * (360.0f / DF_TAU)));
}

static void cmd_clear(void)
{
    g_count = 0;
    g_next = 0;
}

static void run_line(char *line)
{
    char *cmd;
    char *arg;
    char *p;

    while (*line == ' ') {
        line++;
    }
    if (!*line) {
        return;
    }
    cmd = line;
    while (*line && *line != ' ') {
        line++;
    }
    if (*line) {
        *line = 0;
        line++;
        while (*line == ' ') {
            line++;
        }
    }
    arg = line;
    for (p = cmd; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = (char)(*p - 'A' + 'a');
        }
    }
    if (df_eq(cmd, "help")) {
        cmd_help();
    } else if (df_eq(cmd, "fov")) {
        cmd_fov(arg);
    } else if (df_eq(cmd, "fog")) {
        cmd_fog(arg);
    } else if (df_eq(cmd, "cull")) {
        cmd_cull(arg);
    } else if (df_eq(cmd, "occlude")) {
        cmd_occlude(arg);
    } else if (df_eq(cmd, "covers")) {
        cmd_covers(arg);
    } else if (df_eq(cmd, "version")) {
        cmd_version(arg);
    } else if (df_eq(cmd, "far")) {
        cmd_far(arg);
    } else if (df_eq(cmd, "faces")) {
        cmd_faces(arg);
    } else if (df_eq(cmd, "stats")) {
        cmd_stats();
    } else if (df_eq(cmd, "pos")) {
        cmd_pos();
    } else if (df_eq(cmd, "screenshot")) {
        cmd_screenshot(arg);
    } else if (df_eq(cmd, "clear")) {
        cmd_clear();
    } else {
        say("unknown");
    }
}

static void type_keys_idle(void);

static void type_char(char ch)
{
    if (g_tlen >= CON_TYPE - 1) {
        return;
    }
    g_type[g_tlen++] = ch;
    g_type[g_tlen] = 0;
}

static void type_keys(void)
{
    static const char letters[] = "abcdefghijklmnopqrstuvwxyz";
    int i;

    for (i = 0; letters[i]; i++) {
        if (edge('A' + i)) {
            type_char(letters[i]);
        }
    }
    for (i = 0; i < 10; i++) {
        if (edge('0' + i)) {
            type_char((char)('0' + i));
        }
    }
    if (edge(VK_SPACE)) {
        type_char(' ');
    }
    if (edge(VK_OEM_MINUS) | edge(VK_SUBTRACT)) {
        type_char('-');
    }
    if (edge(VK_BACK)) {
        if (g_tlen > 0) {
            g_tlen--;
            g_type[g_tlen] = 0;
        }
    }
    if (edge(VK_RETURN)) {
        run_line(g_type);
        g_tlen = 0;
        g_type[0] = 0;
    }
}

void console_pump(void)
{
    int was_open;
    int vk;

    if (!key_down(VK_ESCAPE)) {
        g_esc_lock = 0;
    }
    vk = toggle_vk();
    was_open = console_open();
    if (edge(vk)) {
        if (g_target > 0) {
            g_target = 0;
        } else {
            g_target = CON_H;
        }
    }
    {
        const char *shot;

        shot = shot_pump();
        if (shot) {
            say(shot);
        }
    }
    /* Letters are tracked every frame so a held key is not typed on open. */
    if (was_open && g_target > 0) {
        type_keys();
        return;
    }
    type_keys_idle();
}

/*
 * type_keys always calls edge(), which records the key. When the line
 * is closed those edges must still be consumed or the first open frame
 * inserts whatever was held. The idle path throws the character away.
 */
static void type_keys_idle(void)
{
    int i;

    for (i = 0; i < 26; i++) {
        edge('A' + i);
    }
    for (i = 0; i < 10; i++) {
        edge('0' + i);
    }
    edge(VK_SPACE);
    edge(VK_OEM_MINUS);
    edge(VK_SUBTRACT);
    edge(VK_BACK);
    edge(VK_RETURN);
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

static void slide(void)
{
    DWORD now;
    DWORD dt;
    int step;

    now = GetTickCount();
    if (g_anim == 0) {
        g_anim = now;
    }
    dt = now - g_anim;
    g_anim = now;
    if (dt > 50) {
        dt = 50;
    }
    step = (int)(dt * (DWORD)CON_H / CON_SLIDE_MS);
    if (step < 1 && g_h != g_target) {
        step = 1;
    }
    if (g_h < g_target) {
        g_h += step;
        if (g_h > g_target) {
            g_h = g_target;
        }
    } else if (g_h > g_target) {
        g_h -= step;
        if (g_h < g_target) {
            g_h = g_target;
        }
    }
}

static int grab_frame(void)
{
    df_u32 bytes;

    if (!g_shot) {
        bytes = (df_u32)SHOT_STRIDE * (df_u32)DF_VIEW_H;
        g_shot = (df_u8 *)mem_alloc(bytes);
    }
    if (!g_shot) {
        return 0;
    }
    return glide_lfb_read(g_shot, SHOT_STRIDE);
}

static const char *log_line(int age)
{
    int i;

    /* age 0 is the oldest kept line. */
    i = g_next - g_count + age;
    while (i < 0) {
        i += CON_LINES;
    }
    while (i >= CON_LINES) {
        i -= CON_LINES;
    }
    return g_log[i];
}

static void draw_panel(void)
{
    int line_h;
    int fit;
    int i;
    int top;
    int y;
    char prompt[CON_TYPE + 4];
    int n;

    if (g_h < 4) {
        return;
    }
    glide_dim(1, 88);
    wash(0.0f, (float)(DF_VIEW_H - g_h), (float)DF_VIEW_W, (float)g_h);
    glide_dim(0, 0);
    line_h = ui_line_height() + 2;
    if (g_h < line_h + 4) {
        return;
    }
    fit = (g_h - line_h - 8) / line_h;
    if (fit > g_count) {
        fit = g_count;
    }
    if (fit < 0) {
        fit = 0;
    }
    top = DF_VIEW_H - 4;
    for (i = g_count - fit; i < g_count; i++) {
        ui_string(8, top, log_line(i));
        top -= line_h;
    }
    n = 0;
    prompt[n++] = '>';
    prompt[n++] = ' ';
    for (i = 0; i < g_tlen && n < CON_TYPE + 2; i++) {
        prompt[n++] = g_type[i];
    }
    if (((GetTickCount() / 400) & 1) == 0 && n < CON_TYPE + 3) {
        prompt[n++] = '_';
    }
    prompt[n] = 0;
    y = DF_VIEW_H - g_h + line_h + 2;
    ui_string(8, y, prompt);
}

void console_present(const Camera *cam)
{
    g_cam = cam;
    slide();
    if (!g_shot_tried) {
        g_shot_tried = 1;
        g_shot_ok = grab_frame();
    }
    if (g_shot_ok && g_shot) {
        if (!glide_lfb_write(g_shot, SHOT_STRIDE)) {
            glide_clear();
        }
    } else {
        glide_clear();
    }
    texcache_forget();
    draw_panel();
    shot_note();
    build_legend();
    glide_swap();
}
