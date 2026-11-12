#include "game/dagger/play/options.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/controls.h"
#include "game/dagger/play/files.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/msg.h"

/* Inclusive, as options_frame tests them, FALL.EXE options_buttons. */
static const struct { int x0, y0, x1, y1; } g_rect[10] = {
    {89,44,133,59}, {137,44,182,59}, {186,44,230,59},
    {91,64,199,66}, {91,72,199,74}, {91,80,199,82},
    {89,87,159,95}, {160,87,230,95}, {89,100,159,117}, {160,100,230,117}
};
static int g_up, g_panel, g_drag = -1;
static unsigned g_revision;
static const char *g_error = "";

void og_options_reset(void)
{
    OgConfig config;
    g_up = 0;
    g_panel = OG_PANEL_OPTIONS;
    g_drag = -1;
    g_error = "";
    og_config_defaults(&config);
    og_config_set(&config);
    g_revision++;
}

int og_options_up(void)
{
    return g_up;
}

int og_options_panel(void)
{
    return g_panel;
}

unsigned og_options_revision(void)
{
    return g_revision;
}

const char *og_options_error(void)
{
    return g_error;
}

void og_options_cancel_drag(void)
{
    g_drag = -1;
    og_controls_cancel_drag();
}

void og_options_open(void)
{
    if (!g_up && og_state()->ready && !og_state_mode_push(OG_GAME_OPTIONS)) {
        return;
    }
    g_up = 1;
    g_panel = OG_PANEL_OPTIONS;
    g_error = "";
    g_revision++;
}

void og_options_open_files(int saving)
{
    if (saving && og_character()->health <= 0) {
        og_msg_set("A dead character cannot be saved.");
        return;
    }
    if (!g_up) {
        og_options_open();
    }
    g_panel = OG_PANEL_FILES;
    g_drag = -1;
    og_files_open(saving);
    g_revision++;
}

void og_options_back(void)
{
    g_panel = OG_PANEL_OPTIONS;
    g_drag = -1;
    g_revision++;
}

void og_options_close(void)
{
    if (g_up && og_state()->ready) {
        og_state_mode_pop();
    }
    g_up = 0;
    g_drag = -1;
    g_revision++;
}

int og_options_commit(void)
{
    if (!og_config_write()) {
        g_error = og_config_error();
        g_revision++;
        og_msg_set(g_error);
    }
    og_options_close();
    return 1;
}

int og_options_flags(void)
{
    return og_state()->settings_flags;
}

int og_options_mouse(void)
{
    return og_state()->mouse_mode;
}

void og_options_set_flags(int flags)
{
    OgConfig config;
    config = *og_config();
    config.flags = flags & 3;
    og_config_set(&config);
    g_revision++;
}

void og_options_set_mouse(int mode)
{
    OgConfig config;
    config = *og_config();
    config.mouse_mode = mode ? 1 : 0;
    og_config_set(&config);
    g_revision++;
}

static void slider(int x)
{
    OgConfig config;
    int value;
    value = (x - 91) * 128 / 108;
    if (value < 0) {
        value = 0;
    }
    if (value > 127) {
        value = 127;
    }
    config = *og_config();
    if (g_drag == 3) {
        config.sound_volume = (value * 100 + 63) / 127;
    }
    if (g_drag == 4) {
        config.music_volume = (value * 100 + 63) / 127;
    }
    if (g_drag == 5) {
        config.detail = value;
    }
    og_config_set(&config);
    g_revision++;
}

void og_options_drag(int x, int held)
{
    if (!(held & 1)) {
        g_drag = -1;
        return;
    }
    if (g_panel == OG_PANEL_OPTIONS && g_drag >= 3) {
        slider(x);
    }
}

int og_options_click(int x, int y, int *sound, int *quit)
{
    int i;
    if (sound) {
        *sound = 0;
    }
    if (quit) {
        *quit = 0;
    }
    if (!g_up || g_panel != OG_PANEL_OPTIONS) {
        return 0;
    }
    for (i = 0; i < 10; i++) {
        if (x < g_rect[i].x0 || x > g_rect[i].x1 || y < g_rect[i].y0 || y > g_rect[i].y1) {
            continue;
        }
        if (sound) {
            *sound = 203;
        }
        if (i == 0) {
            og_options_open_files(1);
        }
        else if (i == 1) {
            og_options_open_files(0);
        }
        else if (i == OG_OPT_CONTINUE) {
            og_options_commit();
        }
        else if (i == OG_OPT_EXIT) {
            if (quit) {
                *quit = 1;
            }
        }
        else if (i == OG_OPT_FULL) {
            og_options_set_flags(og_options_flags() ^ 1);
        }
        else if (i == OG_OPT_BOB) {
            og_options_set_flags(og_options_flags() ^ 2);
        }
        else if (i == OG_OPT_CONTROLS) {
            g_panel = OG_PANEL_CONTROLS;
            og_controls_open();
            g_revision++;
        } else {
            g_drag = i;
            slider(x);
        }
        return 1;
    }
    return 0;
}
