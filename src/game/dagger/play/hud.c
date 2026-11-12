#include "game/dagger/play/hud.h"

#include "game/dagger/play/vitals.h"

#include "engine/core/df.h"

/*
 * Confirmed against FALL.EXE's hud_buttons. The inequalities are
 * strict, so the shared edge between two buttons belongs to neither.
 */
static const HudRect g_rect[HUD_BUTTONS] = {
    { 5, 159, 41, 194 },
    { 83, 154, 130, 176 },
    { 131, 154, 177, 176 },
    { 178, 154, 224, 176 },
    { 225, 154, 272, 176 },
    { 83, 177, 130, 199 },
    { 131, 177, 177, 199 },
    { 178, 177, 224, 199 },
    { 225, 177, 272, 199 },
    { 71, 154, 82, 199 },
    { 274, 154, 319, 199 }
};

static const char *const g_mode[] = { "grab", "info", "steal", "dialogue" };
static const char *const g_sentence[] = {
    "Interaction is now grab mode.",
    "Interaction is now info mode.",
    "Interaction is now steal mode.",
    "Interaction is now dialogue mode."
};

static int g_mode_now;
static int g_pressed;
static int g_ready;

const HudRect *hud_rect(int index)
{
    if (index < 0 || index >= HUD_BUTTONS) {
        return 0;
    }
    return &g_rect[index];
}

int hud_hit(int x, int y)
{
    int i;

    for (i = 0; i < HUD_BUTTONS; i++) {
        if (x > g_rect[i].x0 && x < g_rect[i].x1 && y > g_rect[i].y0 && y < g_rect[i].y1) {
            return i;
        }
    }
    return -1;
}

void hud_reset(void)
{
    g_mode_now = MODE_GRAB;
    g_pressed = HUD_NONE;
    g_ready = 0;
}

int hud_mode(void)
{
    return g_mode_now;
}

void hud_set_mode(int mode)
{
    g_mode_now = mode & 3;
}

int hud_weapon_ready(void)
{
    return g_ready;
}

int hud_toggle_ready(void)
{
    g_ready = g_ready ? 0 : 1;
    return g_ready;
}

int hud_pressed(void)
{
    return g_pressed;
}

static void copy_status(int mode, char *status, int cap)
{
    const char *s;
    int i;

    s = g_sentence[mode & 3];
    if (!status || cap < 1) {
        return;
    }
    for (i = 0; i < cap - 1 && s[i]; i++) {
        status[i] = s[i];
    }
    status[i] = 0;
}

int hud_cycle(int step, char *status, int cap)
{
    g_mode_now = (g_mode_now + step) & 3;
    copy_status(g_mode_now, status, cap);
    return g_mode_now;
}

const char *hud_mode_name(int mode)
{
    return g_mode[mode & 3];
}

int hud_buttons(int x, int y, int buttons, int prev, int view_mode, int cursor_active,
                   int *sound, int *which, int *step)
{
    int hit;
    int down;
    int was;

    if (sound) {
        *sound = 0;
    }
    if (which) {
        *which = -1;
    }
    if (step) {
        *step = 0;
    }
    if (view_mode && !cursor_active) {
        return 0;
    }
    down = (buttons & 3) != 0;
    was = (prev & 3) != 0;
    hit = hud_hit(x, y);
    if (!was && down && hit >= 0) {
        g_pressed = hit;
        return 0;
    }
    if (was && !down && hit >= 0 && g_pressed == hit) {
        g_pressed = HUD_NONE;
        if (sound) {
            *sound = 203;
        }
        if (which) {
            *which = hit;
        }
        if (hit == HUD_MODE) {
            int delta;

            /* Left button steps +1. Anything else that released is the right. */
            delta = (prev & 1) ? 1 : -1;
            if (step) {
                *step = delta;
            }
            hud_cycle(delta, 0, 0);
        }
        if (hit == HUD_WEAPON) {
            g_ready = g_ready ? 0 : 1;
        }
        return 1;
    }
    if (was && !down) {
        g_pressed = HUD_NONE;
    }
    return 0;
}

int hud_blocks_mouse(int y, int bar_up, int steer_key)
{
    if (!bar_up || y < HUD_BAR_Y) {
        return 0;
    }
    return steer_key < 0;
}

int hud_overlay(int health, int max_health, int under_spell, int poisoned, int diseased)
{
    if (max_health > 0 && health < max_health / 10) {
        return 2;
    }
    if (under_spell) {
        return 0;
    }
    if (poisoned) {
        return 3;
    }
    if (diseased) {
        return 1;
    }
    return -1;
}

int hud_compass_frame(int yaw)
{
    return (yaw & 0x7ff) >> 6;
}

int hud_bar(int which, int health, int max_health, int fatigue, int strength, int endurance,
               int magicka, int magicka_bonus, int max_magicka)
{
    if (which == 0) {
        return bar_ratio(health, max_health);
    }
    if (which == 1) {
        return bar_fatigue(fatigue, strength, endurance);
    }
    return bar_ratio(magicka + magicka_bonus, max_magicka);
}
