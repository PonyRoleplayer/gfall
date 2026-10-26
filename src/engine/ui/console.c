#include "engine/ui/console.h"

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/core/prof.h"
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
#include "engine/scene/light.h"
#include "engine/scene/clock.h"
#include "engine/scene/daylight.h"
#include "engine/scene/weather.h"
#include "engine/scene/walker.h"
#include "engine/scene/collide.h"
#include "engine/scene/interact.h"
#include "engine/gfx/sky.h"
#include "engine/assets/loader.h"
#include "engine/sound/ambience.h"
#include "engine/sound/audio.h"
#include "engine/sound/mixer.h"
#include "engine/sound/music.h"
#include "engine/sound/sound.h"

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

Camera *console_camera(void)
{
    return g_player;
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

static void print_view(void);

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
    if (light_count() > 0) {
        int lit;
        int decals;

        draw_light_stats(&lit, &decals);
        log_print(LOG_PRINT, "lights %s: %d of %d lit something on screen, %d decals drawn",
                  light_on() ? "on" : "off", lit, light_count(), decals);
    }
    if (light_outdoor()) {
        int h;
        int m;

        clock_hm(&h, &m);
        log_print(LOG_PRINT, "outdoors %d%d:%d%d, timescale %d, sky %s, daylight %s, lamps %d of 255",
                  h / 10, h % 10, m / 10, m % 10, clock_scale(), sky_on() ? "on" : "off",
                  daylight_on() ? "on" : "off", light_lamps());
        print_view();
    }
    log_print(LOG_PRINT, "fov %d, far %d, fog %s, faces %s, covers %s",
              get_fov(), get_far(), gfx_fog() ? "on" : "off",
              draw_faces() ? "on" : "off", scene_cover_draw() ? "on" : "off");
}

/* "14", "14:30" or "7:05": minutes since midnight, or -1. */
static int parse_hm(const char *s)
{
    int h;
    int m;
    int digits;

    h = 0;
    digits = 0;
    while (*s >= '0' && *s <= '9' && digits < 2) {
        h = h * 10 + (*s++ - '0');
        digits++;
    }
    if (digits == 0 || h > 23) {
        return -1;
    }
    m = 0;
    if (*s == ':') {
        s++;
        digits = 0;
        while (*s >= '0' && *s <= '9' && digits < 2) {
            m = m * 10 + (*s++ - '0');
            digits++;
        }
        if (digits != 2 || m > 59) {
            return -1;
        }
    }
    return *s ? -1 : h * 60 + m;
}

static void c_time(int argc, char **argv)
{
    int h;
    int m;

    if (argc > 2) {
        log_text(LOG_PRINT, "usage: time [hh:mm]");
        return;
    }
    if (argc == 2) {
        m = parse_hm(argv[1]);
        if (m < 0) {
            log_text(LOG_PRINT, "usage: time [hh:mm], 00:00 to 23:59");
            return;
        }
        clock_set_minute((float)m);
    }
    clock_hm(&h, &m);
    log_print(LOG_PRINT, "time %d%d:%d%d, %d game seconds a second",
              h / 10, h % 10, m / 10, m % 10, clock_scale());
}

/* Where the frame's time goes: each stage's mean and worst frame, in milliseconds. */
static void c_prof(int argc, char **argv)
{
    int i;

    (void)argv;
    if (argc != 1) {
        log_text(LOG_PRINT, "usage: prof");
        return;
    }
    if (prof_mhz() == 0 || prof_frames() == 0) {
        log_text(LOG_PRINT, "prof: not measured yet; close the console and let a few frames draw");
        return;
    }
    log_print(LOG_PRINT, "prof: %d MHz, %d frames; ms mean, worst", prof_mhz(), prof_frames());
    for (i = 0; i < PROF_STAGES; i++) {
        int mean;
        int worst;

        mean = df_iround(prof_ms(i) * 10.0f);
        worst = df_iround(prof_peak_ms(i) * 10.0f);
        log_print(LOG_PRINT, "  %s %d.%d, %d.%d", prof_name(i), mean / 10, mean % 10, worst / 10, worst % 10);
    }
}

/* "1st", "2nd", "23rd", for the day of a month counted from 1. */
static const char *ordinal(int n)
{
    if (n % 100 >= 11 && n % 100 <= 13) {
        return "th";
    }
    return n % 10 == 1 ? "st" : (n % 10 == 2 ? "nd" : (n % 10 == 3 ? "rd" : "th"));
}

static void print_date(void)
{
    int y;
    int m;
    int d;

    clock_date(&y, &m, &d);
    log_print(LOG_PRINT, "date %s, %d%s of %s, 3E %d, %s", clock_weekday_name(d), d + 1,
              ordinal(d + 1), clock_month_name(m), y, clock_season_name(clock_season()));
}

/* The calendar: date, or date day month [year], from 1; a new season shows at the next load. */
static void c_date(int argc, char **argv)
{
    int y;
    int m;
    int d;

    clock_date(&y, &m, &d);
    if (argc == 3 || argc == 4) {
        if (!cmd_parse_int(argv[1], &d) || !cmd_parse_int(argv[2], &m)
            || (argc == 4 && !cmd_parse_int(argv[3], &y))
            || d < 1 || d > CLOCK_MONTH_DAYS || m < 1 || m > CLOCK_MONTHS || y < 0 || y > 9999) {
            log_text(LOG_PRINT, "usage: date [day 1..30 month 1..12 [year]]");
            return;
        }
        clock_set_date(y, m - 1, d - 1);
    } else if (argc != 1) {
        log_text(LOG_PRINT, "usage: date [day 1..30 month 1..12 [year]]");
        return;
    }
    print_date();
}

/* season [fall|spring|summer|winter]: the first day of that season, this year. */
static void c_season(int argc, char **argv)
{
    int i;

    if (argc == 2) {
        for (i = 0; i < 4 && !df_eq(argv[1], clock_season_name(i)); i++) {
        }
        if (i == 4) {
            log_text(LOG_PRINT, "usage: season [fall|spring|summer|winter]");
            return;
        }
        clock_set_season(i);
    } else if (argc != 1) {
        log_text(LOG_PRINT, "usage: season [fall|spring|summer|winter]");
        return;
    }
    print_date();
}

/* The far clip and fog the last frame drew, and who set them. */
static void print_view(void)
{
    float far_now;
    float fog_near;
    float fog_far;

    draw_view(&far_now, &fog_near, &fog_far);
    log_print(LOG_PRINT, "view: far %d, fog %d to %d, %s",
              df_iround(far_now), df_iround(fog_near), df_iround(fog_far),
              weather_sim() ? "the weather's (weathersim on, far is the ceiling)" : "the far setting's");
}

static void c_weather(int argc, char **argv)
{
    Weather w;
    int kind;
    int v;

    if (argc > 2) {
        log_text(LOG_PRINT, "usage: weather [sunny|cloudy|overcast|fog|rain|thunder|snow|auto|on|off]");
        return;
    }
    if (argc == 2) {
        kind = weather_by_name(argv[1]);
        if (kind < WEATHER_KINDS) {
            weather_force(kind);
        } else if (argv[1][0] == 'a') {
            weather_force(-1);
        } else if (cmd_parse_switch(argv[1], &v)) {
            weather_set_on(v);
        } else {
            log_text(LOG_PRINT, "usage: weather [sunny|cloudy|overcast|fog|rain|thunder|snow|auto|on|off]");
            return;
        }
    }
    weather_state(&w);
    log_print(LOG_PRINT, "weather %s%s, %d%% of the way from %s; %s",
              weather_name(w.kind), weather_on() ? "" : " (off: drawn clear)",
              df_iround(w.blend * 100.0f), weather_name(w.from),
              weather_forced() >= 0 ? "held there" : "following the odds");
    if (weather_forced() < 0) {
        log_print(LOG_PRINT, "next roll in %d game minutes", df_iround(w.next));
    }
    log_print(LOG_PRINT, "cloud %d%%, fog solid at %d, rain %d%%, snow %d%%, storm %d%%",
              df_iround(w.cloud * 100.0f), w.fog >= WEATHER_CLEAR ? 0 : df_iround(w.fog),
              df_iround(w.rain * 100.0f), df_iround(w.snow * 100.0f), df_iround(w.storm * 100.0f));
    print_view();
}

static void c_stuck(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!walker_on()) {
        log_text(LOG_PRINT, "stuck: only with collision on (tcl 1)");
        return;
    }
    walker_unstick();
    log_text(LOG_PRINT, "stuck: moving to the nearest free place");
}

/* Sound: see docs/sound.md. */
static void c_sndinfo(int argc, char **argv)
{
    int heard;
    int loops;
    int stolen;
    int peak;
    int events;

    (void)argc;
    (void)argv;
    ambience_stats(&heard, &loops);
    music_stats(&stolen, &peak, &events);
    log_print(LOG_PRINT, "sound: %s, %d Hz, ring %d ms, lead %d ms, mixer %s",
              audio_mode_name(audio_mode()), sound_rate(), audio_buffer_ms(), audio_lead_ms(),
              mix_mmx() ? "MMX" : "C");
    log_print(LOG_PRINT, "sound: %d of %d voices, %d emitters in reach, %d loops held",
              sound_busy(), SOUND_VOICES, heard, loops);
    log_print(LOG_PRINT, "music: \"%s\"%s on \"%s\", %d events, %d notes at most, %d cut by the %d-voice limit",
              music_name(), music_playing() ? "" : " (stopped)", music_device_name(), events, peak,
              stolen, music_voices());
}

static void c_musdev(int argc, char **argv)
{
    int n;
    int i;

    if (argc >= 2) {
        if (df_eq(argv[1], "auto")) {
            music_set_device(-1);
        } else if (cmd_parse_int(argv[1], &i)) {
            music_set_device(i);
        } else {
            log_text(LOG_PRINT, "musdev: a number, or auto");
            return;
        }
    }
    n = music_devices();
    for (i = 0; i < n; i++) {
        log_print(LOG_PRINT, "musdev %d: %s", i, music_device_at(i));
    }
    log_print(LOG_PRINT, "musdev: playing on \"%s\" (%s)", music_device_name(),
              music_device() < 0 ? "chosen" : "set");
}

static void c_song(int argc, char **argv)
{
    Song song;

    if (argc < 2) {
        log_print(LOG_PRINT, "song: \"%s\"%s", music_name(),
                  music_name()[0] && !music_playing() ? ", not playing (music off, or no device)" : "");
        return;
    }
    if (df_eq(argv[1], "stop")) {
        music_stop();
        log_text(LOG_PRINT, "song: stopped");
        return;
    }
    mem_zero(&song, sizeof(song));
    if (!loader_call("song", argv[1], 0, 0, &song)) {
        log_print(LOG_PRINT, "song: the game has no song %s", argv[1]);
        return;
    }
    music_stop();
    if (!music_play(&song, argv[1])) {
        log_print(LOG_PRINT, "song: %s did not start", argv[1]);
        return;
    }
    log_print(LOG_PRINT, "song: %s, %d events", argv[1], song.count);
}

static void c_play(int argc, char **argv)
{
    const SoundClip *clip;

    if (argc < 2) {
        log_text(LOG_PRINT, "play: which sound (a number)");
        return;
    }
    clip = 0;
    if (!loader_call("sound", argv[1], 0, 0, &clip) || !clip) {
        log_print(LOG_PRINT, "play: the game has no sound %s", argv[1]);
        return;
    }
    if (!sound_play(clip, 1.0f, 0.0f, 0)) {
        log_text(LOG_PRINT, "play: no voice (sound off?)");
        return;
    }
    log_print(LOG_PRINT, "play: %s, %d ms", argv[1], (int)(clip->frames * 1000u / clip->rate));
}

static const char *use_kind(int kind)
{
    switch (kind) {
    case USE_MOVER:
        return "moves";
    case USE_PASSAGE:
        return "passage";
    case USE_TALK:
        return "person";
    default:
        return "other";
    }
}

/* Where a usable is: its model's box, its sprite, or its own centre. */
static void use_spot(const Scene *sc, const Usable *u, float *x, float *y, float *z)
{
    Bound b;

    *x = u->cx;
    *y = u->cy;
    *z = u->cz;
    if (u->model >= 0 && u->model < sc->nmodels
        && instance_world_box(&sc->models[u->model], &b)) {
        *x = b.cx;
        *y = b.cy;
        *z = b.cz;
    } else if (u->sprite >= 0 && u->sprite < sc->nsprites) {
        *x = sc->sprites[u->sprite].x;
        *y = sc->sprites[u->sprite].y;
        *z = sc->sprites[u->sprite].z;
    }
}

/* What can be used here: how many of each word, then the nearest, nearest first. */
static void c_uselist(int argc, char **argv)
{
    enum { KINDS = 16, SHOW_MAX = 64 };
    const char *kind[KINDS];
    int count[KINDS];
    int pick[SHOW_MAX];
    float dist2[SHOW_MAX];
    const Scene *sc;
    const Camera *cam;
    int nkind;
    int npick;
    int show;
    int i;
    int j;

    show = 10;
    if (argc >= 2 && (!cmd_parse_int(argv[1], &show) || show < 1)) {
        log_text(LOG_PRINT, "uselist: how many, 1..64");
        return;
    }
    if (show > SHOW_MAX) {
        show = SHOW_MAX;
    }
    cam = g_cam ? g_cam : g_player;
    sc = interact_scene();
    if (!cam || !sc || sc->nusables < 1) {
        log_text(LOG_PRINT, "uselist: nothing to use in this place");
        return;
    }
    nkind = 0;
    for (i = 0; i < sc->nusables; i++) {
        const char *word;

        word = sc->usables[i].label ? sc->usables[i].label : "?";
        for (j = 0; j < nkind && !df_eq(kind[j], word); j++) {
        }
        if (j == nkind && nkind < KINDS) {
            kind[nkind] = word;
            count[nkind++] = 0;
        }
        if (j < nkind) {
            count[j]++;
        }
    }
    log_print(LOG_PRINT, "uselist: %d to use, %d movers", sc->nusables, sc->nmovers);
    for (j = 0; j < nkind; j++) {
        log_print(LOG_PRINT, "  %d %s", count[j], kind[j]);
    }
    npick = 0;
    for (i = 0; i < sc->nusables; i++) {
        float x, y, z, dx, dy, dz, d2;

        use_spot(sc, &sc->usables[i], &x, &y, &z);
        dx = x - cam->x;
        dy = y - cam->y;
        dz = z - cam->z;
        d2 = dx * dx + dy * dy + dz * dz;
        if (npick == show && d2 >= dist2[npick - 1]) {
            continue;
        }
        if (npick < show) {
            npick++;
        }
        for (j = npick - 1; j > 0 && dist2[j - 1] > d2; j--) {
            dist2[j] = dist2[j - 1];
            pick[j] = pick[j - 1];
        }
        dist2[j] = d2;
        pick[j] = i;
    }
    for (j = 0; j < npick; j++) {
        const Usable *u;
        float x, y, z;
        float d;
        float dist;
        int lock;

        d = dist2[j];
        __asm__ __volatile__("fsqrt" : "=t"(dist) : "0"(d));
        u = &sc->usables[pick[j]];
        use_spot(sc, u, &x, &y, &z);
        lock = u->kind == USE_MOVER && u->mover >= 0 && u->mover < sc->nmovers
            ? sc->movers[u->mover].lock : 0;
        log_print(LOG_PRINT, "  %d %s (%s%s) at %d %d %d, %d away, reach %d",
                  pick[j], u->label ? u->label : "?", use_kind(u->kind),
                  lock > 0 ? ", locked" : "",
                  df_iround(x), df_iround(y), df_iround(z),
                  df_iround(dist), df_iround(u->reach));
    }
}

/* The place's sound emitters: how many of each, then the nearest, nearest first. */
static void c_sndlist(int argc, char **argv)
{
    enum { KINDS = 16, SHOW_MAX = 64 };
    const char *kind[KINDS];
    int count[KINDS];
    int pick[SHOW_MAX];
    float dist2[SHOW_MAX];
    const Emitter *em;
    const Camera *cam;
    int nkind;
    int npick;
    int show;
    int n;
    int i;
    int j;

    show = 10;
    if (argc >= 2 && (!cmd_parse_int(argv[1], &show) || show < 1)) {
        log_text(LOG_PRINT, "sndlist: how many, 1..64");
        return;
    }
    if (show > SHOW_MAX) {
        show = SHOW_MAX;
    }
    cam = g_cam ? g_cam : g_player;
    n = ambience_emitters(&em);
    if (!cam || n < 1) {
        log_text(LOG_PRINT, "sndlist: no sound emitters in this place");
        return;
    }
    nkind = 0;
    for (i = 0; i < n; i++) {
        const char *word;

        word = em[i].label ? em[i].label : "?";
        for (j = 0; j < nkind && !df_eq(kind[j], word); j++) {
        }
        if (j == nkind && nkind < KINDS) {
            kind[nkind] = word;
            count[nkind++] = 0;
        }
        if (j < nkind) {
            count[j]++;
        }
    }
    log_print(LOG_PRINT, "sndlist: %d emitters", n);
    for (j = 0; j < nkind; j++) {
        log_print(LOG_PRINT, "  %d %s", count[j], kind[j]);
    }
    npick = 0;
    for (i = 0; i < n; i++) {
        float dx, dy, dz, d2;

        dx = em[i].x - cam->x;
        dy = em[i].y - cam->y;
        dz = em[i].z - cam->z;
        d2 = dx * dx + dy * dy + dz * dz;
        if (npick == show && d2 >= dist2[npick - 1]) {
            continue;
        }
        if (npick < show) {
            npick++;
        }
        for (j = npick - 1; j > 0 && dist2[j - 1] > d2; j--) {
            dist2[j] = dist2[j - 1];
            pick[j] = pick[j - 1];
        }
        dist2[j] = d2;
        pick[j] = i;
    }
    for (j = 0; j < npick; j++) {
        const Emitter *e;
        float d;
        float root;

        e = &em[pick[j]];
        d = dist2[j];
        __asm__ __volatile__("fsqrt" : "=t"(root) : "0"(d));
        log_print(LOG_PRINT, "#%d %s at %d %d %d, %d away, heard within %d, %s%s",
                  pick[j], e->label ? e->label : "?",
                  df_iround(e->x), df_iround(e->y), df_iround(e->z), df_iround(root),
                  df_iround(e->reach),
                  e->every > 0.0f ? "now and then" : "loops",
                  ambience_emitter_playing(pick[j]) ? ", playing" : (root < e->reach ? ", in reach" : ""));
    }
}

static int get_tcl(void)
{
    return walker_on();
}

static void set_tcl(int on)
{
    if (on && !g_player) {
        log_text(LOG_PRINT, "tcl: this program has no camera to walk");
        return;
    }
    walker_set_on(on);
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
    if (glide_tex_cap() > 0u) {
        log_print(LOG_PRINT, "texcap: the card has %u KB, held to %u KB for this test",
                  (unsigned int)glide_tex_card(), (unsigned int)glide_tex_cap());
    }
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
    cmd_add("time", c_time, "[hh:mm]", "the hour of the world clock, or set it");
    cmd_add("prof", c_prof, "", "where the frame's time goes, by stage, in ms (P in LEVELFALL)");
    cmd_add("date", c_date, "[day month [year]]",
            "the calendar's date, or set it; a town's pictures and sky follow its season");
    cmd_add("season", c_season, "[fall|spring|summer|winter]",
            "the season, or go to its first day");
    cmd_add("stuck", c_stuck, "", "with collision on, move out of whatever the body is caught in");
    cmd_add("weather", c_weather, "[kind|auto|on|off]",
            "the weather now; a kind holds it, auto gives it back to the odds");
    cmd_add("setpos", c_setpos, "x y z [yaw [pitch]]", "move the camera there");
    cmd_add("screenshot", c_screenshot, "[name]", "save the picture as a BMP beside the program");
    cmd_add("exec", c_exec, "file", "run the commands in a file beside the program");
    cmd_add("glinfo", c_glinfo, "", "the glide2x.dll, board, TMU, and mode in use");
    cmd_add("texmem", c_texmem, "", "pictures on the card and the room left");
    cmd_add("mem", c_mem, "", "engine heaps and the machine's free memory");
    cmd_var("fov", get_fov, set_fov, 45, 120, "horizontal field of view in degrees");
    cmd_var("tcl", get_tcl, set_tcl, 0, 1,
            "collision: 1 walks the camera under gravity, 0 flies it free");
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
    cmd_var("lights", light_on, light_set_on, 0, 1,
            "dungeon lights; off draws every place full bright");
    cmd_var("ambient", light_ambient, light_set_ambient, 0, 255,
            "brightness away from lights, in a lit place");
    cmd_var("lightrange", light_range, gfx_set_light_range, 1, 8,
            "how many times its radius a light reaches; cuts the decals again");
    cmd_var("lightshadow", light_shadow, gfx_set_light_shadow, 0, 1,
            "a light does not reach through a floor or a wall; cuts the decals again");
    cmd_var("lightview", light_view, light_set_view, 0, LIGHT_VIEW_MAX,
            "squares and rings for the lights within this many units; 0 is off, 1 is 512");
    cmd_var("timescale", clock_scale, clock_set_scale, 0, CLOCK_SCALE_MAX,
            "game seconds a real second; 0 stops the clock");
    cmd_var("sky", sky_on, sky_set_on, 0, 1, "the sky behind an outdoor place");
    cmd_var("daylight", daylight_on, daylight_set_on, 0, 1,
            "sun and moon light outdoors; off draws it full bright");
    cmd_var("texcap", gfx_texcap, gfx_set_texcap, 0, 16384,
            "debug: TMU0 holds at most this many KB, 2048 a Voodoo 1; 0 is the card's own");
    cmd_var("weathersim", weather_sim, weather_set_sim, 0, 1,
            "the weather sets the far clip and the fog outdoors; far is the ceiling");
    cmd_var("hud", hud_shown, hud_set, 0, 1, "position and fps corners");
    cmd_var("fps", hud_fps, hud_set_fps, 0, 1, "the fps corner of the hud; F3 flips it");
    cmd_var("logfile", logfile_on, logfile_set, 0, 1,
            "copy the log to console.log beside the program");
    cmd_add("sndinfo", c_sndinfo, "", "the sound card's way out, voices, the song and its voice limit");
    cmd_add("musdev", c_musdev, "[n|auto]", "the MIDI outputs, or play the music on one");
    cmd_add("song", c_song, "[name|stop]", "the song playing, or play one of the game's");
    cmd_add("play", c_play, "n", "play the game's sound effect n");
    cmd_add("sndlist", c_sndlist, "[n]", "the place's sound emitters by kind, and the n nearest (10)");
    cmd_var("sndview", ambience_view, ambience_set_view, 0, AMBIENCE_VIEW_MAX,
            "squares, words and rings for sound emitters within this many units; 0 is off, 1 is 1024");
    cmd_add("uselist", c_uselist, "[n]", "what can be used here by kind, and the n nearest (10)");
    cmd_var("useview", interact_view, interact_set_view, 0, INTERACT_VIEW_MAX,
            "boxes and words for what can be used within this many units; 0 is off, 1 is 512");
    cmd_var("locks", interact_locks, interact_set_locks, 0, 1,
            "locked doors stay shut; 0 opens them as if unlocked");
    cmd_var("sound", sound_on, sound_set_on, 0, 1, "sound effects");
    cmd_var("sndvol", sound_volume, sound_set_volume, 0, 100, "sound effects volume, percent");
    cmd_var("sndrate", sound_rate, sound_set_rate, 8000, 44100,
            "the card's rate; 11025 is the effects' own, more only steps through them");
    cmd_var("snddrv", sound_mode, sound_set_mode, AUDIO_AUTO, AUDIO_NONE,
            "way out: 0 chooses, 1 DirectSound primary, 2 a DirectSound buffer, 3 wave, 4 none");
    cmd_var("sndmmx", mix_mmx, mix_set_mmx, 0, 1, "the mixer's MMX loop; 0 is the C loop");
    cmd_var("music", music_on, music_set_on, 0, 1, "MIDI music");
    cmd_var("musvol", music_volume, music_set_volume, 0, 100, "music volume, percent");
    cmd_var("musvoices", music_voices, music_set_voices, 1, MUSIC_VOICES_MAX,
            "most notes the music plays at once; past 32 an AWE64 plays them on the CPU");
}
