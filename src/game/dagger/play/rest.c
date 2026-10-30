#include "game/dagger/play/rest.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/rules.h"
#include "engine/core/log.h"
#include "engine/scene/clock.h"

/* See rest.h. The numbers are DFU's DaggerfallRestWindow's. */

/* The panel of three, its size and place from its file; a button each, inside it. */
#define PANEL_X 83
#define PANEL_Y 50
#define PANEL_W 154
#define PANEL_H 41
static const Rect g_while_button = { PANEL_X + 4, PANEL_Y + 13, 48, 24 };
static const Rect g_healed_button = { PANEL_X + 53, PANEL_Y + 13, 48, 24 };
static const Rect g_loiter_button = { PANEL_X + 102, PANEL_Y + 13, 48, 24 };
/* The counter, the hours in its little window, and its Stop. */
#define COUNTER_X 107
#define COUNTER_Y 50
#define COUNTER_W 105
#define COUNTER_H 41
#define COUNT_X (COUNTER_X + 6)
#define COUNT_Y (COUNTER_Y + 12)
static const Rect g_stop_button = { COUNTER_X + 33, COUNTER_Y + 26, 40, 10 };
/* The question for the hours: a scroll under the panel. */
static const Rect g_ask_box = { 83, 95, 154, 44 };
#define ASK_X 95
#define ASK_Y 113

#define MINUTES_A_STEP 10.0f
#define STEPS_AN_HOUR 6
#define SECONDS_A_STEP_REST 0.075f
#define SECONDS_A_STEP_LOITER 0.125f
#define HOURS_MOST 99
#define HOURS_TEXT 3
#define ENCOUNTER_ODDS 36

enum {
    PHASE_CHOOSE,
    PHASE_ASK_HOURS,
    PHASE_RUNNING
};

enum {
    KIND_WHILE,
    KIND_HEALED,
    KIND_LOITER
};

static struct {
    int open;
    Player *player;
    Composer composer;
    const df_u8 *panel;
    const df_u8 *hours_past;
    const df_u8 *hours_left;
    int phase;
    int kind;
    char typed[HOURS_TEXT];
    /* Hours to rest (for a while, loitering), hours past; steps through this hour; time toward the next step. */
    int hours;
    int hours_done;
    int steps;
    float time;
    int changed;
    const char *note;
    const char *said;
} g;

int rest_open(Player *p)
{
    rest_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.player = p;
    g.panel = composer_read(&g.composer, "REST00I0.IMG");
    g.hours_past = composer_read(&g.composer, "REST01I0.IMG");
    g.hours_left = composer_read(&g.composer, "REST02I0.IMG");
    if (!g.panel || !g.hours_past || !g.hours_left) {
        log_text(LOG_WARN, "rest: its pictures did not read");
        rest_close();
        return 0;
    }
    return 1;
}

void rest_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

const df_u8 *rest_pixels(void)
{
    return g.composer.screen;
}

int rest_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

const char *rest_note(void)
{
    return g.note;
}

const char *rest_said(void)
{
    return g.said;
}

void rest_compose(void)
{
    static const Rect whole = { 0, 0, CANVAS_W, CANVAS_H };

    canvas_use(&g.composer);
    /* Classic blacks out the world behind it. */
    canvas_box(&whole, 0);
    if (g.phase == PHASE_RUNNING) {
        char number[8];

        /* A rest with a known end counts down; until healed counts up. */
        if (g.kind == KIND_HEALED) {
            canvas_copy_part(g.hours_past, COUNTER_W, 0, 0, COUNTER_W, COUNTER_H, COUNTER_X, COUNTER_Y);
            log_format(number, (int)sizeof(number), "%d", g.hours_done);
        } else {
            canvas_copy_part(g.hours_left, COUNTER_W, 0, 0, COUNTER_W, COUNTER_H, COUNTER_X, COUNTER_Y);
            log_format(number, (int)sizeof(number), "%d", g.hours - g.hours_done);
        }
        canvas_label(COUNT_X, COUNT_Y, number);
    } else {
        canvas_copy_part(g.panel, PANEL_W, 0, 0, PANEL_W, PANEL_H, PANEL_X, PANEL_Y);
    }
    if (g.phase == PHASE_ASK_HOURS) {
        char line[32];

        canvas_scroll(&g_ask_box);
        log_format(line, (int)sizeof(line), "How many hours? %s", g.typed);
        canvas_label(ASK_X, ASK_Y, line);
    }
    g.changed = 0;
}

/* The rest begins, for the kind chosen and its hours. */
static void begin_running(int kind, int hours)
{
    g.kind = kind;
    g.hours = hours;
    g.hours_done = 0;
    g.steps = 0;
    g.time = 0.0f;
    g.phase = PHASE_RUNNING;
    g.note = 0;
    g.changed = 1;
}

static void ask_hours(int kind)
{
    g.kind = kind;
    g.typed[0] = 0;
    g.phase = PHASE_ASK_HOURS;
    g.note = 0;
    g.changed = 1;
}

/* An hour of rest: what it gives back, and a use of Medical (DFU's formulas, rules.c). */
static void hour_of_rest(void)
{
    Character *c;

    c = &g.player->sheet;
    c->health += rules_rest_health(c->attribute[ATTR_ENDURANCE], c->skill[SKILL_MEDICAL], c->health_max);
    c->health = c->health > c->health_max ? c->health_max : c->health;
    c->fatigue += rules_rest_fatigue(c->fatigue_max / CHARACTER_FATIGUE_UNIT) * CHARACTER_FATIGUE_UNIT;
    c->fatigue = c->fatigue > c->fatigue_max ? c->fatigue_max : c->fatigue;
    c->magicka += rules_rest_magicka(c->magicka_max);
    c->magicka = c->magicka > c->magicka_max ? c->magicka_max : c->magicka;
    character_tally(c, SKILL_MEDICAL, 1);
}

static int is_whole(const Character *c)
{
    return c->health >= c->health_max && c->fatigue >= c->fatigue_max && c->magicka >= c->magicka_max;
}

/* The rest ended, with a line for the game to say. */
static int end_rest(const char *line)
{
    g.said = line;
    return REST_LEAVE;
}

/*
 * The clock on by a step, turning the day at midnight. Set directly, not by
 * clock_advance: that goes by the clock's scale, which the console can stop,
 * and a rest passes its hours whatever real time is doing.
 */
static void pass_a_step(void)
{
    float minute;

    minute = clock_minute() + MINUTES_A_STEP;
    if (minute >= CLOCK_DAY) {
        minute -= CLOCK_DAY;
        clock_set_day(clock_day() + 1);
    }
    clock_set_minute(minute);
}

/* Ten game minutes, and what an hour's end brings. REST_LEAVE when the rest is over. */
static int one_step(void)
{
    pass_a_step();
    g.steps++;
    if (g.steps < STEPS_AN_HOUR) {
        return REST_STAY;
    }
    g.steps = 0;
    g.hours_done++;
    g.changed = 1;
    if (g.kind != KIND_LOITER) {
        hour_of_rest();
    }
    /* An hour's chance of being found (ours, one in 36: DFU's dungeon roll, whose frequency was not read). */
    if (combat_roll(ENCOUNTER_ODDS) == 0) {
        foes_rouse_nearest(g.player->body.feet);
    }
    if (foes_awake() > 0) {
        return end_rest("You are awoken by an enemy.");
    }
    if (g.kind == KIND_HEALED) {
        return is_whole(&g.player->sheet) ? end_rest("You are fully healed.") : REST_STAY;
    }
    if (g.hours_done >= g.hours) {
        return end_rest(g.kind == KIND_LOITER ? "You finish loitering." : "You wake up.");
    }
    return REST_STAY;
}

int rest_step(float dt)
{
    float each;

    if (g.phase != PHASE_RUNNING) {
        return REST_STAY;
    }
    each = g.kind == KIND_LOITER ? SECONDS_A_STEP_LOITER : SECONDS_A_STEP_REST;
    g.time += dt;
    while (g.time >= each) {
        g.time -= each;
        if (one_step() == REST_LEAVE) {
            return REST_LEAVE;
        }
    }
    return REST_STAY;
}

int rest_click(int x, int y)
{
    if (g.phase == PHASE_RUNNING) {
        return rect_has(&g_stop_button, x, y) ? end_rest(0) : REST_STAY;
    }
    if (g.phase != PHASE_CHOOSE) {
        return REST_STAY;
    }
    if (rect_has(&g_while_button, x, y)) {
        ask_hours(KIND_WHILE);
    } else if (rect_has(&g_loiter_button, x, y)) {
        ask_hours(KIND_LOITER);
    } else if (rect_has(&g_healed_button, x, y)) {
        begin_running(KIND_HEALED, 0);
    }
    return REST_STAY;
}

/* The hours typed so far as a number. */
static int typed_hours(void)
{
    int hours;
    int i;

    hours = 0;
    for (i = 0; g.typed[i]; i++) {
        hours = hours * 10 + (g.typed[i] - '0');
    }
    return hours;
}

int rest_key(int vk)
{
    if (vk == 0x1B) {
        return end_rest(0);
    }
    if (g.phase != PHASE_ASK_HOURS) {
        return REST_STAY;
    }
    if (vk >= '0' && vk <= '9') {
        int n;

        n = 0;
        while (g.typed[n]) {
            n++;
        }
        if (n < HOURS_TEXT - 1) {
            g.typed[n] = (char)vk;
            g.typed[n + 1] = 0;
            g.changed = 1;
        }
    } else if (vk == 0x08) {
        int n;

        n = 0;
        while (g.typed[n]) {
            n++;
        }
        if (n > 0) {
            g.typed[n - 1] = 0;
            g.changed = 1;
        }
    } else if (vk == 0x0D) {
        int hours;

        hours = typed_hours();
        if (hours < 1) {
            return end_rest(0);
        }
        begin_running(g.kind, hours > HOURS_MOST ? HOURS_MOST : hours);
    }
    return REST_STAY;
}
