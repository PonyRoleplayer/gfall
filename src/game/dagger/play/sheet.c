#include "game/dagger/play/sheet.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/doll.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/tables/careers.h"
#include "engine/core/log.h"

/* See sheet.h. */

/* Where the doll stands: the corner every doll picture's own place counts from. */
#define DOLL_X 200
#define DOLL_Y 8
/* The attributes' column: a button each, 24 apart; its value's line; the spinner beside the chosen one. */
#define ATTR_X 141
#define ATTR_W 28
#define ATTR_BUTTON_Y 6
#define ATTR_BUTTON_H 20
#define ATTR_STEP 24
#define ATTR_VALUE_Y 17
#define SPINNER_X 176
#define SPINNER_W 15
#define SPINNER_H 20
#define SPINNER_ARM 7
#define SPINNER_VALUE_Y 7
/* The labels' places: each after its word in the picture. */
#define NAME_AT 41, 4
#define RACE_AT 41, 14
#define CLASS_AT 46, 24
#define LEVEL_AT 45, 34
#define GOLD_AT 39, 44
#define FATIGUE_AT 57, 54
#define HEALTH_AT 52, 64
#define CARRY_AT 90, 74
/* The skill lists' buttons, and the box one opens (ours: classic draws a scroll). */
enum {
    LIST_PRIMARY,
    LIST_MAJOR,
    LIST_MINOR,
    LIST_OTHER,
    LISTS,
    LIST_NONE = -1
};
static const Rect g_list_button[LISTS] = {
    { 11, 106, 115, 8 }, { 11, 116, 115, 8 }, { 11, 126, 115, 8 }, { 11, 136, 115, 8 }
};
static const Rect g_list_box = { 4, 8, 190, 184 };
#define LIST_TEXT_X 10
#define LIST_TEXT_Y 11
#define LIST_VALUE_X 160
static const Rect g_inventory_button = { 3, 151, 65, 12 };
static const Rect g_exit_button = { 50, 179, 39, 19 };

static struct {
    int open;
    Player *player;
    Composer composer;
    const df_u8 *back;
    const df_u8 *updown;
    df_u8 *doll;
    df_u8 *doll_owner;
    int doll_stale;
    /* A level being taken: the points left, and what the attributes were when it began. */
    int points;
    int attribute_before[ATTR_COUNT];
    int chosen;
    int list;
    int changed;
    const char *note;
} g;

/* The level due is taken: its number, its health, its points to place. */
static void take_level(Player *p)
{
    int i;

    if (!p->sheet.level_due) {
        return;
    }
    {
        const CareerClass *k;
        int span;

        k = career_class(p->sheet.klass);
        span = k ? k->hp_per_level - k->hp_per_level / 2 + 1 : 1;
        character_level_up(&p->sheet, combat_roll(span));
    }
    g.points = RULES_LEVEL_POOL_LOW + combat_roll(RULES_LEVEL_POOL_ROLLS);
    for (i = 0; i < ATTR_COUNT; i++) {
        g.attribute_before[i] = p->sheet.attribute[i];
    }
}

int sheet_open(Player *p)
{
    MemHeap *was;

    sheet_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.player = p;
    g.doll_stale = 1;
    g.list = LIST_NONE;
    was = mem_heap_use(g.composer.heap);
    g.doll = (df_u8 *)mem_alloc(DOLL_W * DOLL_H);
    g.doll_owner = (df_u8 *)mem_alloc(DOLL_W * DOLL_H);
    mem_heap_use(was);
    g.back = composer_read(&g.composer, "INFO00I0.IMG");
    g.updown = composer_read(&g.composer, "CHAR02I1.IMG");
    if (!g.doll || !g.doll_owner || !g.back || !g.updown) {
        log_text(LOG_WARN, "character sheet: its pictures did not read");
        sheet_close();
        return 0;
    }
    take_level(p);
    return 1;
}

void sheet_close(void)
{
    if (!g.open) {
        return;
    }
    g.open = 0;
    doll_forget_files();
    composer_close(&g.composer);
}

const df_u8 *sheet_pixels(void)
{
    return g.composer.screen;
}

int sheet_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

const char *sheet_note(void)
{
    return g.note;
}

int sheet_points(void)
{
    return g.points;
}

/* A label of a number and its most: "45/60". */
static void label_pair(int x, int y, int value, int most)
{
    char text[24];

    log_format(text, (int)sizeof(text), "%d/%d", value, most);
    canvas_label(x, y, text);
}

static void compose_labels(void)
{
    const Character *c;
    const CareerRace *race;
    const CareerClass *k;
    char text[24];

    c = &g.player->sheet;
    race = career_race(c->race);
    k = career_class(c->klass);
    canvas_label(NAME_AT, c->name);
    canvas_label(RACE_AT, race ? race->name : "?");
    canvas_label(CLASS_AT, k ? k->name : "?");
    log_format(text, (int)sizeof(text), "%d", c->level);
    canvas_label(LEVEL_AT, text);
    log_format(text, (int)sizeof(text), "%u", (unsigned)g.player->pack.gold);
    canvas_label(GOLD_AT, text);
    label_pair(FATIGUE_AT, c->fatigue / CHARACTER_FATIGUE_UNIT, c->fatigue_max / CHARACTER_FATIGUE_UNIT);
    label_pair(HEALTH_AT, c->health, c->health_max);
    label_pair(CARRY_AT, df_iround(inventory_weight(&g.player->pack)), df_ifloor(rules_max_carry(c->attribute[ATTR_STRENGTH])));
}

static void compose_attributes(void)
{
    const Character *c;
    int i;

    c = &g.player->sheet;
    for (i = 0; i < ATTR_COUNT; i++) {
        char number[8];
        Rect line;

        line.x = ATTR_X;
        line.y = (df_u16)(ATTR_VALUE_Y + i * ATTR_STEP);
        line.w = ATTR_W;
        line.h = CANVAS_LINE;
        log_format(number, (int)sizeof(number), "%d", c->attribute[i]);
        canvas_label_centred(&line, 0, number);
    }
    if (g.points > 0) {
        char number[8];
        Rect line;
        int top;

        top = ATTR_BUTTON_Y + g.chosen * ATTR_STEP;
        canvas_cutout(g.updown, SPINNER_W, SPINNER_H, SPINNER_X, top);
        line.x = SPINNER_X;
        line.y = (df_u16)(top + SPINNER_VALUE_Y);
        line.w = SPINNER_W;
        line.h = CANVAS_LINE;
        log_format(number, (int)sizeof(number), "%d", g.points);
        canvas_label_centred(&line, 0, number);
    }
}

/* One skill on the open list's line. */
static void list_line(int line, int skill)
{
    const CareerSkill *s;
    char number[8];
    int y;

    s = career_skill(skill);
    y = LIST_TEXT_Y + line * CANVAS_LINE;
    canvas_label(LIST_TEXT_X, y, s ? s->name : "?");
    log_format(number, (int)sizeof(number), "%d", g.player->sheet.skill[skill]);
    canvas_label(LIST_VALUE_X, y, number);
}

/* 1 when a skill is none of its class's twelve: the miscellaneous list. */
static int skill_is_other(const CareerClass *k, int skill)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (k->primary[i] == skill || k->major[i] == skill) {
            return 0;
        }
    }
    for (i = 0; i < 6; i++) {
        if (k->minor[i] == skill) {
            return 0;
        }
    }
    return 1;
}

/* The open list of skills, in a box over the left of the sheet. */
static void compose_list(void)
{
    const CareerClass *k;
    int line;
    int i;

    k = career_class(g.player->sheet.klass);
    if (g.list == LIST_NONE || !k) {
        return;
    }
    canvas_box(&g_list_box, CANVAS_DARK);
    line = 0;
    if (g.list == LIST_OTHER) {
        for (i = 0; i < SKILL_COUNT; i++) {
            if (skill_is_other(k, i)) {
                list_line(line++, i);
            }
        }
        return;
    }
    for (i = 0; i < (g.list == LIST_MINOR ? 6 : 3); i++) {
        const df_u8 *group;

        group = g.list == LIST_PRIMARY ? k->primary : (g.list == LIST_MAJOR ? k->major : k->minor);
        list_line(line++, group[i]);
    }
}

void sheet_compose(void)
{
    canvas_use(&g.composer);
    canvas_copy_all(g.back);
    if (g.doll_stale) {
        MemHeap *was;

        was = composer_scratch_begin(&g.composer);
        doll_compose(g.doll, g.doll_owner, &g.player->sheet, &g.player->pack);
        composer_scratch_end(was);
        g.doll_stale = 0;
    }
    canvas_copy_part(g.doll, DOLL_W, 0, 0, DOLL_W, DOLL_H, DOLL_X, DOLL_Y);
    compose_labels();
    compose_attributes();
    compose_list();
    g.changed = 0;
}

/* A point on or off the chosen attribute, never below what it was nor past the most. */
static void attribute_point(int more)
{
    Character *c;
    int *value;

    c = &g.player->sheet;
    value = &c->attribute[g.chosen];
    if (more && g.points > 0 && *value < RULES_ATTRIBUTE_MAX) {
        (*value)++;
        g.points--;
    } else if (!more && *value > g.attribute_before[g.chosen]) {
        (*value)--;
        g.points++;
    } else {
        return;
    }
    character_attributes_changed(c);
}

/* A click on the attributes' column while a level is being taken. 1 when it was on one. */
static int click_points(int x, int y)
{
    int top;
    int i;

    top = ATTR_BUTTON_Y + g.chosen * ATTR_STEP;
    if (x >= SPINNER_X && x < SPINNER_X + SPINNER_W && y >= top && y < top + SPINNER_H) {
        if (y < top + SPINNER_ARM) {
            attribute_point(1);
        } else if (y >= top + SPINNER_H - SPINNER_ARM) {
            attribute_point(0);
        }
        return 1;
    }
    for (i = 0; i < ATTR_COUNT; i++) {
        int row;

        row = ATTR_BUTTON_Y + i * ATTR_STEP;
        if (x >= ATTR_X && x < ATTR_X + ATTR_W && y >= row && y < row + ATTR_BUTTON_H) {
            g.chosen = i;
            return 1;
        }
    }
    return 0;
}

int sheet_may_leave(void)
{
    int i;

    if (g.points <= 0) {
        return 1;
    }
    for (i = 0; i < ATTR_COUNT; i++) {
        if (g.player->sheet.attribute[i] < RULES_ATTRIBUTE_MAX) {
            g.note = "You must distribute all of your bonus points.";
            g.changed = 1;
            return 0;
        }
    }
    return 1;
}

int sheet_click(int x, int y)
{
    int i;

    g.note = 0;
    g.changed = 1;
    /* A list shown takes the click that puts it away. */
    if (g.list != LIST_NONE) {
        g.list = LIST_NONE;
        return SHEET_STAY;
    }
    if (g.points > 0 && click_points(x, y)) {
        return SHEET_STAY;
    }
    if (rect_has(&g_exit_button, x, y)) {
        return sheet_may_leave() ? SHEET_LEAVE : SHEET_STAY;
    }
    if (rect_has(&g_inventory_button, x, y)) {
        return sheet_may_leave() ? SHEET_INVENTORY : SHEET_STAY;
    }
    for (i = 0; i < LISTS; i++) {
        if (rect_has(&g_list_button[i], x, y)) {
            g.list = i;
        }
    }
    return SHEET_STAY;
}
