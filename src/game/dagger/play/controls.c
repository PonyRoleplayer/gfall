#include "game/dagger/play/controls.h"
#include "game/dagger/play/config.h"
#include "engine/platform/input.h"
#include "engine/core/df.h"
#include "game/dagger/play/msg.h"

/* FALL.EXE 1.07.213 controls_buttons at 0x17B808, strict hit bounds. */
static const OgControlRect g_rect[42] = {
    { 55, 11, 104, 20 },
    { 55, 22, 104, 31 },
    { 55, 33, 104, 42 },
    { 55, 44, 104, 53 },
    { 55, 55, 104, 64 },
    { 55, 66, 104, 75 },
    { 162, 11, 211, 20 },
    { 162, 22, 211, 31 },
    { 162, 33, 211, 42 },
    { 162, 44, 211, 53 },
    { 162, 55, 211, 64 },
    { 162, 66, 211, 75 },
    { 268, 11, 317, 20 },
    { 268, 22, 317, 31 },
    { 268, 33, 317, 42 },
    { 268, 44, 317, 53 },
    { 268, 55, 317, 64 },
    { 268, 66, 317, 75 },
    { 268, 78, 317, 87 },
    { 268, 89, 317, 98 },
    { 268, 101, 317, 110 },
    { 268, 112, 317, 121 },
    { 268, 123, 317, 132 },
    { 268, 135, 317, 144 },
    { 268, 146, 317, 155 },
    { 268, 157, 317, 166 },
    { 268, 168, 317, 177 },
    { 268, 179, 317, 188 },
    { 100, 78, 149, 87 },
    { 100, 89, 149, 98 },
    { 100, 100, 149, 109 },
    { 100, 111, 149, 120 },
    { 100, 123, 149, 132 },
    { 100, 134, 149, 143 },
    { 100, 145, 149, 154 },
    { 100, 157, 149, 166 },
    { 100, 168, 149, 177 },
    { 100, 179, 149, 188 },
    { 0, 190, 79, 199 },
    { 80, 190, 159, 199 },
    { 160, 190, 239, 199 },
    { 240, 190, 319, 199 },
};
static int g_edit = -1;
static int g_mouse;
static int g_wait_release;
static int g_drag = -1;
static unsigned g_revision;
static const char *g_notice = "";

const OgControlRect *og_control_rect(int index)
{
    return index >= 0 && index < 42 ? &g_rect[index] : 0;
}

void og_controls_open(void)
{
    g_edit = -1;
    g_wait_release = 0;
    g_mouse = 0;
    g_drag = -1;
    g_notice = "";
    g_revision++;
}

int og_controls_editing(void)
{
    return g_edit;
}

int og_controls_mouse(void)
{
    return g_mouse;
}

const char *og_controls_notice(void)
{
    return g_notice;
}

unsigned og_controls_revision(void)
{
    return g_revision;
}

void og_controls_cancel_drag(void)
{
    g_drag = -1;
}

int og_controls_duplicate(int action)
{
    int i;
    for (i = 0; i < OG_ACTS; i++) {
        if (i != action && og_config()->key[i] == og_config()->key[action]) {
            return 1;
        }
    }
    return 0;
}

static int finish(void)
{
    int i;
    for (i = 0; i < OG_ACTS; i++) {
        if (og_controls_duplicate(i)) {
            g_notice = "Two actions share a key. Rebind the red entries.";
            return 0;
        }
    }
    if (!og_config_write()) {
        g_notice = og_config_error();
        og_msg_set(g_notice);
        return 1;
    }
    return 1;
}

int og_controls_key(int key)
{
    OgConfig config;
    g_revision++;
    if (!key) {
        return 0;
    }
    if (g_notice[0] && key == 0x1b && g_edit < 0) {
        g_notice = "";
        return 0;
    }
    if (g_edit >= 0) {
        if (g_wait_release) {
            return 0;
        }
        if (key == 0x1b) {
            g_edit = -1;
            g_notice = "";
            return 0;
        }
        if (!og_key_allowed(key)) {
            g_notice = "That key is reserved.";
            return 0;
        }
        config = *og_config();
        config.key[g_edit] = key;
        og_config_set(&config);
        g_edit = -1;
        g_notice = "";
        return 0;
    }
    if (key == 0x1b) {
        if (g_mouse) {
            g_mouse = 0;
            g_drag = -1;
            return 0;
        }
        return finish();
    }
    return 0;
}

static int inside(int x, int y, int x0, int y0, int x1, int y1)
{
    return x > x0 && x < x1 && y > y0 && y < y1;
}

static void mouse_slider(int x)
{
    OgConfig config;
    int value;
    config = *og_config();
    value = (x - 139) / (g_drag == 2 ? 7 : 6) + (g_drag == 2 ? 0 : 1);
    if (value < (g_drag == 2 ? 0 : 1)) {
        value = g_drag == 2 ? 0 : 1;
    }
    if (value > 15) {
        value = 15;
    }
    if (g_drag == 0) {
        config.sensitivity_x = value;
    }
    if (g_drag == 1) {
        config.sensitivity_y = value;
    }
    if (g_drag == 2) {
        config.turn_rate = value;
    }
    og_config_set(&config);
    g_revision++;
}

int og_controls_click(int x, int y)
{
    OgConfig config, defaults;
    int i;
    g_revision++;
    if (g_edit >= 0) {
        return 0;
    }
    if (g_notice[0]) {
        g_notice = "";
        return 0;
    }
    if (g_mouse) {
        config = *og_config();
        if (inside(x,y,67,44,160,90)) {
            config.mouse_mode = 0;
        }
        else if (inside(x,y,161,44,253,90)) {
            config.mouse_mode = 1;
        }
        else if (inside(x,y,139,108,258,114)) {
            g_drag = 0;
        }
        else if (inside(x,y,139,121,258,127)) {
            g_drag = 1;
        }
        else if (inside(x,y,137,132,250,140)) {
            g_drag = 2;
        }
        else if (inside(x,y,71,145,150,164)) {
            config.reverse_y ^= 1;
        }
        else if (inside(x,y,159,145,248,164)) {
            g_mouse = 0;
            g_drag = -1;
        }
        og_config_set(&config);
        if (g_drag >= 0) {
            mouse_slider(x);
        }
        return 0;
    }
    for (i = 0; i < 42; i++) {
        if (!inside(x,y,g_rect[i].x0,g_rect[i].y0,g_rect[i].x1,g_rect[i].y1)) {
            continue;
        }
        if (i < OG_ACTS) {
            g_edit = i;
            g_wait_release = 1;
        }
        else if (i == 38) {
            g_notice = "Joystick input is not available.";
        }
        else if (i == 39) {
            g_mouse = 1;
        }
        else if (i == 40) {
            config = *og_config();
            og_config_defaults(&defaults);
            for (i = 0; i < OG_ACTS; i++) {
                config.key[i] = defaults.key[i];
            }
            og_config_set(&config);
        } else {
            return finish();
        }
        return 0;
    }
    return 0;
}

void og_controls_drag(int x, int y, int held)
{
    (void)y;
    if (!(held & 3)) {
        g_wait_release = 0;
    }
    if (!(held & 1)) {
        g_drag = -1;
        return;
    }
    if (g_mouse && g_drag >= 0) {
        mouse_slider(x);
    }
}

void og_control_key_name(int action, char *text, int capacity)
{
    int key;
    key = og_config()->key[action];
    if (key == 0x21) {
        df_copy(text, "PgUp", capacity);
        return;
    }
    if (key == 0x22) {
        df_copy(text, "PgDn", capacity);
        return;
    }
    if (key == 0x08) {
        df_copy(text, "Bkspace", capacity);
        return;
    }
    if (key == 1) {
        df_copy(text, "Mouse 1", capacity);
    } else if (key == 2) {
        df_copy(text, "Mouse 2", capacity);
    } else if (key == 4) {
        df_copy(text, "Mouse 3", capacity);
    } else if (key == 0xa0) {
        df_copy(text, "L Shift", capacity);
    } else if (key == 0xa1) {
        df_copy(text, "R Shift", capacity);
    } else {
        input_key_name(key, text, capacity);
    }
}
