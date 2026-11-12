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

void options_reset(void)
{
    Config config;
    g_up = 0;
    g_panel = PANEL_OPTIONS;
    g_drag = -1;
    g_error = "";
    config_defaults(&config);
    config_set(&config);
    g_revision++;
}

int options_up(void)
{
    return g_up;
}

int options_panel(void)
{
    return g_panel;
}

unsigned options_revision(void)
{
    return g_revision;
}

const char *options_error(void)
{
    return g_error;
}

void options_cancel_drag(void)
{
    g_drag = -1;
    controls_cancel_drag();
}

void options_open(void)
{
    if (!g_up && game_state()->ready && !state_mode_push(GAME_OPTIONS)) {
        return;
    }
    g_up = 1;
    g_panel = PANEL_OPTIONS;
    g_error = "";
    g_revision++;
}

void options_open_files(int saving)
{
    if (saving && character_state()->health <= 0) {
        msg_set("A dead character cannot be saved.");
        return;
    }
    if (!g_up) {
        options_open();
    }
    g_panel = PANEL_FILES;
    g_drag = -1;
    files_open(saving);
    g_revision++;
}

void options_back(void)
{
    g_panel = PANEL_OPTIONS;
    g_drag = -1;
    g_revision++;
}

void options_close(void)
{
    if (g_up && game_state()->ready) {
        state_mode_pop();
    }
    g_up = 0;
    g_drag = -1;
    g_revision++;
}

int options_commit(void)
{
    if (!config_write()) {
        g_error = config_error();
        g_revision++;
        msg_set(g_error);
    }
    options_close();
    return 1;
}

int options_flags(void)
{
    return game_state()->settings_flags;
}

int options_mouse(void)
{
    return game_state()->mouse_mode;
}

void options_set_flags(int flags)
{
    Config config;
    config = *game_config();
    config.flags = flags & 3;
    config_set(&config);
    g_revision++;
}

void options_set_mouse(int mode)
{
    Config config;
    config = *game_config();
    config.mouse_mode = mode ? 1 : 0;
    config_set(&config);
    g_revision++;
}

static void slider(int x)
{
    Config config;
    int value;
    value = (x - 91) * 128 / 108;
    if (value < 0) {
        value = 0;
    }
    if (value > 127) {
        value = 127;
    }
    config = *game_config();
    if (g_drag == 3) {
        config.sound_volume = (value * 100 + 63) / 127;
    }
    if (g_drag == 4) {
        config.music_volume = (value * 100 + 63) / 127;
    }
    if (g_drag == 5) {
        config.detail = value;
    }
    config_set(&config);
    g_revision++;
}

void options_drag(int x, int held)
{
    if (!(held & 1)) {
        g_drag = -1;
        return;
    }
    if (g_panel == PANEL_OPTIONS && g_drag >= 3) {
        slider(x);
    }
}

int options_click(int x, int y, int *sound, int *quit)
{
    int i;
    if (sound) {
        *sound = 0;
    }
    if (quit) {
        *quit = 0;
    }
    if (!g_up || g_panel != PANEL_OPTIONS) {
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
            options_open_files(1);
        }
        else if (i == 1) {
            options_open_files(0);
        }
        else if (i == OPT_CONTINUE) {
            options_commit();
        }
        else if (i == OPT_EXIT) {
            if (quit) {
                *quit = 1;
            }
        }
        else if (i == OPT_FULL) {
            options_set_flags(options_flags() ^ 1);
        }
        else if (i == OPT_BOB) {
            options_set_flags(options_flags() ^ 2);
        }
        else if (i == OPT_CONTROLS) {
            g_panel = PANEL_CONTROLS;
            controls_open();
            g_revision++;
        } else {
            g_drag = i;
            slider(x);
        }
        return 1;
    }
    return 0;
}
