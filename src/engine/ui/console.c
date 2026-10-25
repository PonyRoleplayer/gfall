#include "engine/ui/console.h"

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/platform/fs.h"
#include "engine/platform/win32.h"
#include "engine/ui/build.h"
#include "engine/ui/cmd.h"
#include "engine/ui/font.h"
#include "engine/ui/hud.h"
#include "engine/ui/shot.h"
#include "engine/gfx/camera.h"
#include "engine/gfx/draw.h"
#include "engine/gfx/gfx.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/texcache.h"
#include "engine/scene/scene.h"

/*
 * The panel slides down from the top over a grabbed frame. The log it
 * shows is engine/core/log.c, the typed line and the command table are
 * engine/ui/cmd.c. This file is the Win32 and Glide side: keys, the
 * picture, files, and the commands that reach into the engine.
 *
 * Keys come as window messages, not polled: WM_CHAR carries the layout,
 * Shift, and the keyboard's own repeat, and a key pressed and released
 * between two slow frames is not lost.
 */

#define CON_H 240
#define CON_SLIDE_MS 200
/* win_pump only gives keys to a console some frame loop pumped this recently. */
#define CON_LIVE_MS 500
/* Scan code of the key left of 1, on US and ABNT boards alike. */
#define CON_SCAN 0x29
#define SHOT_STRIDE (DF_VIEW_W * 2)
#define EXEC_DEPTH 4
#define EXEC_BYTES 4096

static int g_h;
static int g_target;
static int g_esc_lock;
static DWORD g_anim;
static DWORD g_live;
static int g_live_ever;
static int g_ready;
/* Lines scrolled up from the newest. g_seen holds the view still as lines arrive. */
static int g_scroll;
static df_u32 g_seen;
static int g_shot_ok;
static int g_shot_tried;
static df_u8 *g_shot;
static const Camera *g_cam;
static Camera *g_player;
static HANDLE g_file = INVALID_HANDLE_VALUE;
static int g_exec_depth;

static void commands(void);
static void exec_file(const char *name, int quiet);

int console_open(void)
{
    return (g_h > 0 || g_target > 0) ? 1 : 0;
}

void console_bind(const Camera *cam)
{
    g_cam = cam;
}

void console_set_camera(Camera *cam)
{
    g_player = cam;
}

void console_log(const char *s)
{
    log_text(LOG_PRINT, s);
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

void console_pump(void)
{
    const char *shot;

    g_live = GetTickCount();
    g_live_ever = 1;
    if (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
        g_esc_lock = 0;
    }
    /* The first pump is the first frame: the device is open by now. */
    if (!g_ready) {
        g_ready = 1;
        commands();
        exec_file("autoexec.cfg", 1);
    }
    shot = shot_pump();
    if (shot) {
        log_text(LOG_PRINT, shot);
    }
}

/* --- keys -------------------------------------------------------------- */

static int live(void)
{
    return g_live_ever && GetTickCount() - g_live < CON_LIVE_MS;
}

static void toggle(void)
{
    if (g_target > 0) {
        g_target = 0;
        return;
    }
    g_target = CON_H;
    g_scroll = 0;
}

static int page(void)
{
    int fit;

    fit = (CON_H - 8) / (ui_line_height() + 2) - 1;
    return fit > 2 ? fit / 2 : 1;
}

static void scroll_by(int n)
{
    g_scroll += n;
    if (g_scroll > log_count() - 1) {
        g_scroll = log_count() - 1;
    }
    if (g_scroll < 0) {
        g_scroll = 0;
    }
}

/* An editing key, already known to arrive while the line takes keys. */
static int edit_key(unsigned int vk)
{
    int ctrl;

    ctrl = (GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0;
    switch (vk) {
    case VK_ESCAPE:
        g_target = 0;
        g_esc_lock = 1;
        return 1;
    case VK_RETURN:
        g_scroll = 0;
        cmd_line_key(CMD_KEY_ENTER);
        return 1;
    case VK_BACK: cmd_line_key(CMD_KEY_BACK); return 1;
    case VK_DELETE: cmd_line_key(CMD_KEY_DELETE); return 1;
    case VK_TAB: cmd_line_key(CMD_KEY_TAB); return 1;
    case VK_LEFT: cmd_line_key(CMD_KEY_LEFT); return 1;
    case VK_RIGHT: cmd_line_key(CMD_KEY_RIGHT); return 1;
    case VK_UP: cmd_line_key(CMD_KEY_UP); return 1;
    case VK_DOWN: cmd_line_key(CMD_KEY_DOWN); return 1;
    case VK_PRIOR: scroll_by(page()); return 1;
    case VK_NEXT: scroll_by(-page()); return 1;
    case VK_HOME:
        if (ctrl) {
            scroll_by(log_count());
        } else {
            cmd_line_key(CMD_KEY_HOME);
        }
        return 1;
    case VK_END:
        if (ctrl) {
            g_scroll = 0;
        } else {
            cmd_line_key(CMD_KEY_END);
        }
        return 1;
    default:
        break;
    }
    if (ctrl && vk == 'U') {
        cmd_line_key(CMD_KEY_KILL);
        return 1;
    }
    if (ctrl && vk == 'L') {
        log_clear();
        g_scroll = 0;
        return 1;
    }
    return 0;
}

int console_message(unsigned int msg, unsigned int wp, long lp)
{
    int scan;

    if (msg != WM_KEYDOWN && msg != WM_CHAR && msg != WM_DEADCHAR) {
        return CONSOLE_PASS;
    }
    if (!live()) {
        return CONSOLE_PASS;
    }
    scan = (int)((lp >> 16) & 0xff);
    if (msg == WM_KEYDOWN && scan == CON_SCAN) {
        /* Bit 30 is the key's previous state: a held key only toggles once.
           Kept untranslated, so no character or dead key reaches the line. */
        if (!(lp & 0x40000000L)) {
            toggle();
        }
        return CONSOLE_KEEP;
    }
    if (g_target == 0) {
        return CONSOLE_PASS;
    }
    if (msg == WM_KEYDOWN) {
        return edit_key(wp) ? CONSOLE_KEEP : CONSOLE_TRANSLATE;
    }
    if (msg == WM_CHAR && scan != CON_SCAN) {
        cmd_line_char((int)(wp & 0xff));
    }
    return CONSOLE_KEEP;
}

/* --- files ------------------------------------------------------------- */

static int single_name(const char *name)
{
    int i;

    if (!name || !name[0]) {
        return 0;
    }
    for (i = 0; name[i]; i++) {
        if (name[i] == '/' || name[i] == '\\' || name[i] == ':') {
            return 0;
        }
    }
    return 1;
}

/*
 * Runs a file of commands beside the program, as if typed, without the
 * echo. Each level of nesting has its own buffer, so a file may exec
 * another up to EXEC_DEPTH deep.
 */
static void exec_file(const char *name, int quiet)
{
    static char text[EXEC_DEPTH][EXEC_BYTES];
    char path[260];
    HANDLE f;
    DWORD got;
    char *buf;
    int ran;

    if (!single_name(name) || !fs_beside(path, (int)sizeof(path), name)) {
        log_print(LOG_PRINT, "exec: %s is not a file name beside the program", name);
        return;
    }
    if (g_exec_depth >= EXEC_DEPTH) {
        log_print(LOG_WARN, "exec: %s nests too deep", name);
        return;
    }
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, 0);
    if (f == INVALID_HANDLE_VALUE) {
        if (quiet) {
            log_print(LOG_DEBUG, "exec: no %s", name);
        } else {
            log_print(LOG_PRINT, "exec: no %s beside the program", name);
        }
        return;
    }
    buf = text[g_exec_depth];
    got = 0;
    if (!ReadFile(f, buf, EXEC_BYTES - 1, &got, 0)) {
        got = 0;
    }
    CloseHandle(f);
    buf[got] = 0;
    if (got == EXEC_BYTES - 1) {
        log_print(LOG_WARN, "exec: %s is cut at %d bytes", name, EXEC_BYTES - 1);
    }
    g_exec_depth++;
    ran = cmd_run(buf);
    g_exec_depth--;
    log_print(LOG_INFO, "exec: %s ran %d commands", name, ran);
}

static void file_write(const char *s)
{
    DWORD wrote;
    int n;

    if (g_file == INVALID_HANDLE_VALUE) {
        return;
    }
    n = 0;
    while (s[n]) {
        n++;
    }
    WriteFile(g_file, s, (DWORD)n, &wrote, 0);
    WriteFile(g_file, "\r\n", 2, &wrote, 0);
}

static int logfile_on(void)
{
    return g_file != INVALID_HANDLE_VALUE;
}

/*
 * console.log beside the program. Opening it replaces the old file and
 * writes the lines already in the log first, so the Glide start-up is
 * in it even when autoexec.cfg turns it on. No flush per line: the
 * system keeps written data when the program dies, and a flush is a
 * disk wait on Windows 98.
 */
static void logfile_set(int on)
{
    char path[260];
    int i;

    if (!on) {
        if (g_file != INVALID_HANDLE_VALUE) {
            log_set_sink(0);
            CloseHandle(g_file);
            g_file = INVALID_HANDLE_VALUE;
        }
        return;
    }
    if (g_file != INVALID_HANDLE_VALUE) {
        return;
    }
    if (!fs_beside(path, (int)sizeof(path), "console.log")) {
        log_text(LOG_ERROR, "logfile: no path beside the program");
        return;
    }
    g_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, 0);
    if (g_file == INVALID_HANDLE_VALUE) {
        log_print(LOG_ERROR, "logfile: cannot write %s", path);
        return;
    }
    file_write(build_text());
    for (i = log_count() - 1; i >= 0; i--) {
        file_write(log_get(i));
    }
    log_set_sink(file_write);
}

/* --- the picture ------------------------------------------------------- */

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
        /* Kept for the process: not the temp heap a viewer draws inside. */
        g_shot = (df_u8 *)mem_keep(bytes);
    }
    if (!g_shot) {
        return 0;
    }
    return glide_lfb_read(g_shot, SHOT_STRIDE);
}

/*
 * The prompt line. A line wider than the glass shows the part that
 * ends at the cursor. The cursor is an underscore drawn over the text
 * at the cursor's pixel, which also works for a proportional face.
 */
static void draw_prompt(int y)
{
    char shown[CMD_LINE + 4];
    const char *text;
    int cur;
    int from;
    int n;
    int i;
    int x;

    text = cmd_line_text();
    cur = cmd_line_cursor();
    from = 0;
    for (;;) {
        n = 0;
        shown[n++] = '>';
        shown[n++] = ' ';
        for (i = from; i < cur; i++) {
            shown[n++] = text[i];
        }
        shown[n] = 0;
        if (from >= cur || ui_string_width(shown) < DF_VIEW_W - 32) {
            break;
        }
        from++;
    }
    x = 8 + ui_string_width(shown);
    for (i = cur; text[i]; i++) {
        shown[n++] = text[i];
    }
    shown[n] = 0;
    ui_string(8, y, shown);
    if (((GetTickCount() / 400) & 1) == 0) {
        ui_string(x, y, "_");
    }
}

static void draw_panel(void)
{
    int line_h;
    int fit;
    int row;
    int base;
    int first;
    df_u32 serial;

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
    /* Hold a scrolled view on the same lines while new ones arrive. */
    serial = log_serial();
    if (g_scroll > 0) {
        scroll_by((int)(serial - g_seen));
    }
    g_seen = serial;
    base = DF_VIEW_H - g_h + line_h + 2;
    draw_prompt(base);
    /* Newest at the bottom, just above the prompt, older lines upward. */
    fit = (g_h - line_h - 8) / line_h;
    first = g_scroll;
    row = 0;
    if (g_scroll > 0 && fit > 1) {
        ui_string(8, base + line_h, "^   ^   ^   ^   PgDn, Ctrl+End");
        row = 1;
    }
    for (; row < fit; row++) {
        int index;

        index = first + row - (g_scroll > 0 ? 1 : 0);
        if (index >= log_count()) {
            break;
        }
        ui_string(8, base + line_h * (row + 1), log_get(index));
    }
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
    draw_panel();
    shot_note();
    build_legend();
    glide_swap();
}

/* --- engine commands --------------------------------------------------- */

static int deg_of(float rad)
{
    return df_iround(rad * (360.0f / DF_TAU));
}

static int get_fov(void) { return df_iround(camera_fov()); }
static void set_fov(int v) { camera_set_fov((float)v); }
static int get_far(void) { return df_iround(gfx_far()); }
static void set_far(int v) { gfx_set_far((float)v); }

static void c_quit(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    log_text(LOG_PRINT, "quit");
    win_quit();
}

static void c_version(int argc, char **argv)
{
    int v;

    if (argc > 1) {
        if (!cmd_parse_switch(argv[1], &v)) {
            log_text(LOG_PRINT, "usage: version [on|off]");
            return;
        }
        build_set_draw(v);
    }
    log_print(LOG_PRINT, "%s, drawn %s", build_text(), build_draw() ? "on" : "off");
}

static void c_stats(int argc, char **argv)
{
    int models;
    int sprites;
    int kept;
    int dropped;
    int occ_faces;
    int occ_hidden;
    int frames;
    float avg;
    float worst;

    (void)argc;
    (void)argv;
    frames = draw_frame_time(&avg, &worst);
    if (frames > 0) {
        log_print(LOG_PRINT, "frame %.1f ms mean, %.1f ms worst, %d frames, vsync %s",
                  (double)avg, (double)worst, frames, glide_vsync() ? "on" : "off");
    } else {
        log_text(LOG_PRINT, "frame: no whole second of frames yet");
    }
    draw_stats(&models, &sprites, &kept, &dropped);
    log_print(LOG_PRINT, "drawn %d models, %d sprites, %d faces; %d faces turned away",
              models, sprites, kept, dropped);
    scene_occ_stats(&occ_faces, &occ_hidden);
    log_print(LOG_PRINT, "cull %s, occlude %s: %d occluders hid %d boxes",
              scene_cull() ? "on" : "off", scene_occlude() ? "on" : "off",
              occ_faces, occ_hidden);
    log_print(LOG_PRINT, "fov %d, far %d, fog %s, faces %s, covers %s",
              get_fov(), get_far(), gfx_fog() ? "on" : "off",
              draw_faces() ? "on" : "off", scene_cover_draw() ? "on" : "off");
}

static void c_pos(int argc, char **argv)
{
    const Camera *cam;

    (void)argc;
    (void)argv;
    cam = g_cam ? g_cam : g_player;
    if (!cam) {
        log_text(LOG_PRINT, "no camera");
        return;
    }
    log_print(LOG_PRINT, "x %d y %d z %d yaw %d pitch %d",
              df_iround(cam->x), df_iround(cam->y), df_iround(cam->z),
              ((deg_of(cam->yaw) % 360) + 360) % 360, deg_of(cam->pitch));
}

static void c_setpos(int argc, char **argv)
{
    int v[5];
    int i;
    float yaw;
    float pitch;

    if (!g_player) {
        log_text(LOG_PRINT, "setpos: this program has no camera to move");
        return;
    }
    if (argc < 4 || argc > 6) {
        log_text(LOG_PRINT, "usage: setpos x y z [yaw [pitch]]");
        return;
    }
    for (i = 1; i < argc; i++) {
        if (!cmd_parse_int(argv[i], &v[i - 1])) {
            log_text(LOG_PRINT, "usage: setpos x y z [yaw [pitch]], whole numbers");
            return;
        }
    }
    yaw = argc > 4 ? (float)v[3] * (DF_TAU / 360.0f) : g_player->yaw;
    pitch = argc > 5 ? (float)v[4] * (DF_TAU / 360.0f) : g_player->pitch;
    camera_set(g_player, (float)v[0], (float)v[1], (float)v[2], yaw, pitch);
    c_pos(1, argv);
}

static void c_screenshot(int argc, char **argv)
{
    shot_save(argc > 1 ? argv[1] : "");
    log_text(LOG_PRINT, shot_status());
}

static void c_exec(int argc, char **argv)
{
    char name[64];
    int i;
    int dot;

    if (argc < 2) {
        log_text(LOG_PRINT, "usage: exec file, beside the program; .cfg is added");
        return;
    }
    df_copy(name, argv[1], (int)sizeof(name) - 4);
    dot = 0;
    for (i = 0; name[i]; i++) {
        if (name[i] == '.') {
            dot = 1;
        }
    }
    if (!dot) {
        name[i++] = '.';
        name[i++] = 'c';
        name[i++] = 'f';
        name[i++] = 'g';
        name[i] = 0;
    }
    exec_file(name, 0);
}

static void c_glinfo(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    glide_report(LOG_PRINT);
}

static void c_texmem(int argc, char **argv)
{
    int live_n;
    int unfit;
    int total;
    df_u32 bytes;

    (void)argc;
    (void)argv;
    texcache_stats(&live_n, &unfit, &total, &bytes);
    log_print(LOG_PRINT, "textures: %d of %d on the card, %u KB; %d did not fit",
              live_n, total, (unsigned int)(bytes / 1024u), unfit);
    log_print(LOG_PRINT, "mip chains %s; %d at half size or less to fit",
              gfx_mipmap() ? "on" : "off", texcache_reduced());
    log_print(LOG_PRINT, "TMU0: %u KB free after them",
              (unsigned int)(glide_tex_free() / 1024u));
}

static void c_mem(int argc, char **argv)
{
    MEMORYSTATUS ms;
    df_u32 main_bytes;
    df_u32 temp_bytes;

    (void)argc;
    (void)argv;
    mem_stats(&main_bytes, &temp_bytes);
    log_print(LOG_PRINT, "heap: %u KB main, %u KB temp",
              (unsigned int)(main_bytes / 1024u), (unsigned int)(temp_bytes / 1024u));
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    log_print(LOG_PRINT, "system: %u KB of %u KB physical free, %d%% in use",
              (unsigned int)(ms.dwAvailPhys / 1024u), (unsigned int)(ms.dwTotalPhys / 1024u),
              (int)ms.dwMemoryLoad);
}

static void commands(void)
{
    cmd_add("quit", c_quit, "", "leave the program");
    cmd_add("qqq", c_quit, "", "same as quit");
    cmd_add("exit", c_quit, "", "same as quit");
    cmd_add("version", c_version, "[on|off]", "the compile stamp, and whether it is drawn");
    cmd_add("stats", c_stats, "", "frame time and what the last world frame drew");
    cmd_add("pos", c_pos, "", "camera position, yaw and pitch in degrees");
    cmd_add("setpos", c_setpos, "x y z [yaw [pitch]]", "move the camera there");
    cmd_add("screenshot", c_screenshot, "[name]", "save the picture as a BMP beside the program");
    cmd_add("exec", c_exec, "file", "run the commands in a file beside the program");
    cmd_add("glinfo", c_glinfo, "", "the glide2x.dll, board, TMU, and mode in use");
    cmd_add("texmem", c_texmem, "", "pictures on the card and the room left");
    cmd_add("mem", c_mem, "", "engine heaps and the machine's free memory");
    cmd_var("fov", get_fov, set_fov, 45, 120, "horizontal field of view in degrees");
    cmd_var("far", get_far, set_far, 16, (int)DF_FAR_MAX, "far clip in world units");
    cmd_var("fog", gfx_fog, gfx_set_fog, 0, 1, "distance fog");
    cmd_var("cull", scene_cull, scene_set_cull, 0, 1, "drop models and sprites out of view");
    cmd_var("occlude", scene_occlude, scene_set_occlude, 0, 1,
            "drop models and sprites hidden behind nearer walls");
    cmd_var("covers", scene_cover_draw, scene_set_cover_draw, 0, 1,
            "wash the occlusion cells of the last frame green");
    cmd_var("faces", draw_faces, draw_set_faces, 0, 1, "drop ARCH3D faces that point away");
    cmd_var("vsync", glide_vsync, glide_set_vsync, 0, 1,
            "wait for the retrace; off shows the real frame rate, with tearing");
    cmd_var("wbuffer", glide_wbuffer, glide_set_wbuffer, 0, 1,
            "depth from 1/w, fine at any range; off is the 16-bit Z-buffer");
    cmd_var("planar", glide_planar, glide_set_planar, 0, 1,
            "send each face as one planar polygon; off sends triangles");
    cmd_var("mipmap", gfx_mipmap, gfx_set_mipmap, 0, 1,
            "mip chains on the card; a change sends the level again");
    cmd_var("hud", hud_shown, hud_set, 0, 1, "position and fps corners");
    cmd_var("fps", hud_fps, hud_set_fps, 0, 1, "the fps corner of the hud; F3 flips it");
    cmd_var("logfile", logfile_on, logfile_set, 0, 1,
            "copy the log to console.log beside the program");
}
