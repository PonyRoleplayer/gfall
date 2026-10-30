#include "game/dagger/play/create.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/kit.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/formats/biog.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/items.h"
#include "engine/core/log.h"

/* See create.h. The rectangles are DFU's windows' unless a comment says ours. */

/* The classes the list shows: CLASS18.CFG is a second Knight, not one of them. */
#define CLASSES 18
#define FIRST_CLASS_WORDS 2100
#define FACES 10
#define NAME_MAX 24
#define WORDS_MAX 2048

/* A word's box over the map (ours: classic draws a scroll here), and its two answers. */
static const Rect g_box = { 30, 30, 260, 136 };
static const Rect g_box_text = { 38, 36, 244, 112 };
static const Rect g_answer_left = { 70, 150, 70, 12 };
static const Rect g_answer_right = { 180, 150, 70, 12 };
/* The class list (ours), a line each, over the map. */
static const Rect g_list_box = { 90, 16, 140, 168 };
#define LIST_X 100
#define LIST_Y 22

static const Rect g_ok = { 263, 172, 39, 22 };
static const Rect g_restart = { 263, 147, 39, 22 };
static const Rect g_reroll = { 263, 147, 39, 22 };
/* The name's line on the name screen and on the summary. */
#define NAME_X 80
#define NAME_Y 5
#define SUMMARY_NAME_X 100
/* The face's panel, a face centred in it, and its two buttons. */
static const Rect g_face_panel = { 247, 25, 64, 40 };
static const Rect g_face_previous = { 245, 69, 42, 9 };
static const Rect g_face_next = { 287, 69, 26, 9 };
/* The attributes' column: a button each, 22 apart; its value's line; the spinner beside the chosen one. */
#define ATTR_BUTTON_X 7
#define ATTR_BUTTON_Y 20
#define ATTR_BUTTON_W 36
#define ATTR_BUTTON_H 20
#define ATTR_STEP 22
#define ATTR_VALUE_Y 33
#define ATTR_SPINNER_X 44
#define ATTR_SPINNER_Y 21
/* CHAR02I1.IMG: its size, the up and down parts, and the points left between them. */
#define UPDOWN_W 15
#define UPDOWN_H 20
#define UPDOWN_ARM 7
#define UPDOWN_VALUE_Y 7
/* What the attributes make, beside CHAR02I0.IMG's own words (ours: placed by the picture). */
typedef struct DerivedSpot {
    df_u16 x;
    df_u16 y;
} DerivedSpot;
enum { DERIVED_DAMAGE, DERIVED_CARRY, DERIVED_SPELL, DERIVED_RESIST, DERIVED_TO_HIT, DERIVED_HEALTH, DERIVED_HEALING, DERIVED_COUNT };
static const DerivedSpot g_derived[DERIVED_COUNT] = {
    { 85, 22 }, { 105, 32 }, { 114, 49 }, { 123, 71 }, { 99, 93 }, { 103, 110 }, { 124, 120 }
};
/* The skills: a line each, a group's first at its top; the spinner by the chosen one. */
#define SKILL_GROUPS 3
#define SKILLS_SHOWN 12
#define SKILL_NAME_X 68
#define SKILL_VALUE_X 187
#define SKILL_ROW_W 106
#define SKILL_ROW_H 7
#define SKILL_STEP 10
#define SKILL_SPINNER_X 203
/* CHAR03I1.IMG: its size, the left and right parts, and the points left at its left. */
#define LEFTRIGHT_W 37
#define LEFTRIGHT_H 9
#define LEFTRIGHT_ARM 11
#define LEFTRIGHT_VALUE_Y 2
static const df_u8 g_group_first[SKILL_GROUPS] = { 0, 3, 6 };
static const df_u8 g_group_y[SKILL_GROUPS] = { 32, 81, 130 };
/* The reflexes' list, CHAR05I1.IMG's five slices, and where each screen shows it. */
#define REFLEX_W 66
#define REFLEX_ROW 9
#define REFLEX_ROWS 5
#define REFLEX_X 127
#define REFLEX_Y 148
#define SUMMARY_REFLEX_X 246
#define SUMMARY_REFLEX_Y 95
/* The questions: their two lines, and ten answers in two columns of five. */
#define QUESTION_X 30
#define QUESTION_Y 23
#define QUESTION_LINE 11
#define ANSWER_X 10
#define ANSWER_Y 71
#define ANSWER_W 149
#define ANSWER_H 24
#define ANSWER_TEXT_X 21
#define ANSWER_TEXT_Y 5

static struct {
    int open;
    Composer composer;
    int step;
    int changed;
    const char *note;
    /* Each step's background, read when first wanted. */
    const df_u8 *back[CREATE_STEPS];
    const df_u8 *race_mask;
    const df_u8 *updown;
    const df_u8 *leftright;
    const df_u8 *reflex_marks;
    char words[WORDS_MAX];
    /* The choices. */
    int race;
    int female;
    int klass;
    char name[NAME_MAX];
    int face;
    int reflexes;
    /* The rolls, what they started at, and the points left to place. */
    int attribute[ATTR_COUNT];
    int attribute_rolled[ATTR_COUNT];
    int attribute_pool;
    int attribute_chosen;
    int skill[SKILLS_SHOWN];
    int skill_rolled[SKILLS_SHOWN];
    int skill_pool[SKILL_GROUPS];
    int skill_chosen;
    /* The class's questions, the one up, and what the answers came to. */
    Biog *biog;
    int question;
    int skill_gain[SKILL_COUNT];
    int gold_gain;
    Inventory answered_items;
    /* The race's and sex's faces, open while they are wanted. */
    CifSet faces;
    int faces_open;
    int made;
} g;

/* ---- The rolls. ---- */

static void roll_attributes(void)
{
    const CareerClass *k;
    int i;

    k = career_class(g.klass);
    for (i = 0; i < ATTR_COUNT; i++) {
        g.attribute[i] = (k ? k->attribute[i] : 50) + combat_roll(RULES_ATTRIBUTE_ROLLS);
        g.attribute_rolled[i] = g.attribute[i];
    }
    g.attribute_pool = RULES_POOL_LOW + combat_roll(RULES_POOL_ROLLS);
}

/* The class's skill shown on line i: primary, major, then minor. */
static int skill_on_line(int i)
{
    const CareerClass *k;

    k = career_class(g.klass);
    if (!k) {
        return 0;
    }
    if (i < 3) {
        return k->primary[i];
    }
    return i < 6 ? k->major[i - 3] : k->minor[i - 6];
}

static int group_of_line(int i)
{
    return i < 3 ? 0 : (i < 6 ? 1 : 2);
}

static void roll_skills(void)
{
    static const df_u8 base[SKILL_GROUPS] = { RULES_PRIMARY_BASE, RULES_MAJOR_BASE, RULES_MINOR_BASE };
    int i;

    for (i = 0; i < SKILLS_SHOWN; i++) {
        g.skill[i] = base[group_of_line(i)] + combat_roll(RULES_SKILL_ROLLS);
        g.skill_rolled[i] = g.skill[i];
    }
    for (i = 0; i < SKILL_GROUPS; i++) {
        g.skill_pool[i] = RULES_SKILL_POOL;
    }
}

/* ---- Moving between steps. ---- */

/* The race's faces for this sex, open from now until creation ends. */
static void open_faces(void)
{
    const CareerRace *race;

    if (g.faces_open) {
        cif_close(&g.faces);
        g.faces_open = 0;
    }
    race = career_race(g.race);
    if (race) {
        g.faces_open = cif_open(g.female ? race->heads_female : race->heads_male, &g.faces);
    }
}

static void go_to(int step)
{
    g.step = step;
    g.note = 0;
    g.changed = 1;
}

/* The words of a TEXT.RSC record, for a box. */
static void read_words(int id)
{
    if (textrsc_read(id, g.words, WORDS_MAX) < 0) {
        g.words[0] = 0;
    }
}

/* The class's questions, read now; with none, on to the name. */
static void begin_questions(void)
{
    MemHeap *was;
    int ok;

    mem_zero(g.skill_gain, sizeof(g.skill_gain));
    g.gold_gain = 0;
    inventory_clear(&g.answered_items);
    g.question = 0;
    was = mem_heap_use(g.composer.heap);
    if (!g.biog) {
        g.biog = (Biog *)mem_alloc(sizeof(Biog));
    }
    ok = g.biog && biog_load(g.klass, g.biog);
    mem_heap_use(was);
    go_to(ok ? CREATE_QUESTIONS : CREATE_NAME);
}

/* An item an answer gives (DFU's BiogFile: a group, its index, a material); only weapons and armour yet. */
static void answer_gives_item(const BiogEffect *e)
{
    Item it;
    int made;

    made = 0;
    if (e->a == BIOG_GROUP_WEAPONS) {
        made = item_make(&it, ITEM_DAGGER + e->b, e->c);
    } else if (e->a == BIOG_GROUP_ARMOR) {
        /* DFU's ArmorMaterialTypes in order: leather, chain, then the metals. */
        int metal;

        metal = e->c == 0 ? MATERIAL_LEATHER : (e->c == 1 ? MATERIAL_CHAIN : e->c - 2);
        made = item_make(&it, ITEM_CUIRASS + e->b, metal);
    }
    if (made) {
        inventory_add(&g.answered_items, &it);
    }
}

/* An answer taken: what it does, then the next question or on to the name. */
static void answer_question(int answer)
{
    const BiogAnswer *a;
    int i;

    a = &g.biog->question[g.question].answer[answer];
    for (i = 0; i < a->effects; i++) {
        const BiogEffect *e;

        e = &a->effect[i];
        if (e->kind == BIOG_SKILL && e->a >= 0 && e->a < SKILL_COUNT) {
            g.skill_gain[e->a] += e->b;
        } else if (e->kind == BIOG_GOLD) {
            g.gold_gain += e->a;
        } else if (e->kind == BIOG_ITEM) {
            answer_gives_item(e);
        }
    }
    g.question++;
    g.changed = 1;
    if (g.question >= g.biog->questions) {
        go_to(CREATE_NAME);
    }
}

/* ---- Composing, a function a screen. ---- */

/* A step's background, read the first time it is wanted. */
static void show_back(int step, const char *name)
{
    if (!g.back[step]) {
        g.back[step] = composer_read(&g.composer, name);
    }
    if (g.back[step]) {
        canvas_copy_all(g.back[step]);
    }
}

/* A box over the map with words in it, and two answers under them. */
static void compose_box(const char *left, const char *right)
{
    show_back(CREATE_RACE, "TMAP00I0.IMG");
    canvas_box(&g_box, CANVAS_DARK);
    canvas_text(&g_box_text, g.words);
    canvas_label_centred(&g_answer_left, 2, left);
    canvas_label_centred(&g_answer_right, 2, right);
}

static void compose_class_list(void)
{
    int i;

    show_back(CREATE_RACE, "TMAP00I0.IMG");
    canvas_box(&g_list_box, CANVAS_DARK);
    for (i = 0; i < CLASSES; i++) {
        const CareerClass *k;

        k = career_class(i);
        canvas_label(LIST_X, LIST_Y + i * CANVAS_LINE + i / 2, k ? k->name : "?");
    }
}

static void compose_questions(void)
{
    const BiogQuestion *q;
    int i;

    show_back(CREATE_QUESTIONS, "BIOG00I0.IMG");
    q = &g.biog->question[g.question];
    canvas_label(QUESTION_X, QUESTION_Y, q->line[0]);
    canvas_label(QUESTION_X, QUESTION_Y + QUESTION_LINE, q->line[1]);
    for (i = 0; i < q->answers; i++) {
        canvas_label(ANSWER_X + (i & 1) * ANSWER_W + ANSWER_TEXT_X, ANSWER_Y + (i / 2) * ANSWER_H + ANSWER_TEXT_Y,
                     q->answer[i].text);
    }
}

/* A centred face in its panel. */
static void compose_face(void)
{
    const CifFrame *face;

    face = g.faces_open ? cif_frame(&g.faces, g.face) : 0;
    if (face) {
        canvas_cutout(face->pixels, face->w, face->h, g_face_panel.x + (g_face_panel.w - face->w) / 2,
                      g_face_panel.y + (g_face_panel.h - face->h) / 2);
    }
}

/* The attributes' values, in their column. */
static void compose_attribute_values(void)
{
    int i;

    for (i = 0; i < ATTR_COUNT; i++) {
        char number[8];
        Rect line;

        line.x = ATTR_BUTTON_X;
        line.y = (df_u16)(ATTR_VALUE_Y + i * ATTR_STEP);
        line.w = ATTR_BUTTON_W;
        line.h = CANVAS_LINE;
        log_format(number, (int)sizeof(number), "%d", g.attribute[i]);
        canvas_label_centred(&line, 0, number);
    }
}

/* One of the attributes' derived numbers at its place. */
static void derived(int which, int value)
{
    char number[12];

    log_format(number, (int)sizeof(number), "%d", value);
    canvas_label(g_derived[which].x, g_derived[which].y, number);
}

static void compose_attributes(void)
{
    const CareerClass *k;
    char number[8];
    Rect line;
    int top;

    show_back(CREATE_ATTRIBUTES, "CHAR02I0.IMG");
    compose_attribute_values();
    top = ATTR_SPINNER_Y + g.attribute_chosen * ATTR_STEP;
    canvas_cutout(g.updown, UPDOWN_W, UPDOWN_H, ATTR_SPINNER_X, top);
    line.x = ATTR_SPINNER_X;
    line.y = (df_u16)(top + UPDOWN_VALUE_Y);
    line.w = UPDOWN_W;
    line.h = CANVAS_LINE;
    log_format(number, (int)sizeof(number), "%d", g.attribute_pool);
    canvas_label_centred(&line, 0, number);
    k = career_class(g.klass);
    derived(DERIVED_DAMAGE, combat_strength_bonus(g.attribute[ATTR_STRENGTH]));
    derived(DERIVED_CARRY, df_ifloor(rules_max_carry(g.attribute[ATTR_STRENGTH])));
    derived(DERIVED_SPELL, rules_max_magicka(g.attribute[ATTR_INTELLIGENCE], k ? k->magicka : 0.0f));
    derived(DERIVED_RESIST, rules_magic_resist(g.attribute[ATTR_WILLPOWER]));
    derived(DERIVED_TO_HIT, rules_attribute_modifier(g.attribute[ATTR_AGILITY]));
    derived(DERIVED_HEALTH, rules_attribute_modifier(g.attribute[ATTR_ENDURANCE]));
    derived(DERIVED_HEALING, rules_attribute_modifier(g.attribute[ATTR_ENDURANCE]));
}

/* The line a skill stands on, top to bottom over the three groups. */
static int skill_line_y(int i)
{
    int group;

    group = group_of_line(i);
    return g_group_y[group] + (i - g_group_first[group]) * SKILL_STEP;
}

/* The twelve skills, their names and values. */
static void compose_skill_lines(void)
{
    int i;

    for (i = 0; i < SKILLS_SHOWN; i++) {
        const CareerSkill *s;
        char number[8];

        s = career_skill(skill_on_line(i));
        canvas_label(SKILL_NAME_X, skill_line_y(i), s ? s->name : "?");
        log_format(number, (int)sizeof(number), "%d", g.skill[i]);
        canvas_label(SKILL_VALUE_X, skill_line_y(i), number);
    }
}

static void compose_skills(void)
{
    char number[8];
    Rect line;
    int top;

    show_back(CREATE_SKILLS, "CHAR03I0.IMG");
    compose_skill_lines();
    top = skill_line_y(g.skill_chosen) - 1;
    canvas_cutout(g.leftright, LEFTRIGHT_W, LEFTRIGHT_H, SKILL_SPINNER_X, top);
    line.x = SKILL_SPINNER_X;
    line.y = (df_u16)(top + LEFTRIGHT_VALUE_Y);
    line.w = LEFTRIGHT_ARM + LEFTRIGHT_ARM + (LEFTRIGHT_W - 2 * LEFTRIGHT_ARM);
    line.h = CANVAS_LINE;
    log_format(number, (int)sizeof(number), "%d", g.skill_pool[group_of_line(g.skill_chosen)]);
    canvas_label_centred(&line, 0, number);
}

/* The chosen reflexes' slice of CHAR05I1.IMG, over its own line in the list. */
static void compose_reflex_mark(int x, int y)
{
    canvas_copy_part(g.reflex_marks, REFLEX_W, 0, g.reflexes * REFLEX_ROW, REFLEX_W, REFLEX_ROW, x,
                     y + g.reflexes * REFLEX_ROW);
}

static void compose_summary(void)
{
    show_back(CREATE_SUMMARY, "CHAR04I0.IMG");
    canvas_label(SUMMARY_NAME_X, NAME_Y, g.name);
    compose_attribute_values();
    compose_skill_lines();
    compose_face();
    compose_reflex_mark(SUMMARY_REFLEX_X, SUMMARY_REFLEX_Y);
}

void create_compose(void)
{
    canvas_use(&g.composer);
    g.changed = 0;
    switch (g.step) {
    case CREATE_RACE:
        show_back(CREATE_RACE, "TMAP00I0.IMG");
        {
            Rect top;

            top.x = 0;
            top.y = 16;
            top.w = CANVAS_W;
            top.h = CANVAS_LINE;
            canvas_label_centred(&top, 0, "Please select your home province");
        }
        break;
    case CREATE_RACE_CONFIRM:
    case CREATE_CLASS_CONFIRM:
        compose_box("Yes", "No");
        break;
    case CREATE_SEX:
        compose_box("Female", "Male");
        break;
    case CREATE_CLASS:
        compose_class_list();
        break;
    case CREATE_QUESTIONS:
        compose_questions();
        break;
    case CREATE_NAME:
        show_back(CREATE_NAME, "CHAR00I0.IMG");
        canvas_label(NAME_X, NAME_Y, g.name);
        break;
    case CREATE_FACE:
        show_back(CREATE_FACE, "CHAR01I0.IMG");
        compose_face();
        break;
    case CREATE_ATTRIBUTES:
        compose_attributes();
        break;
    case CREATE_SKILLS:
        compose_skills();
        break;
    case CREATE_REFLEXES:
        show_back(CREATE_REFLEXES, "CHAR05I0.IMG");
        compose_reflex_mark(REFLEX_X, REFLEX_Y);
        break;
    default:
        compose_summary();
        break;
    }
}

/* ---- Clicks, a function a screen. ---- */

static void click_race(int x, int y)
{
    int race;

    race = g.race_mask ? g.race_mask[y * CANVAS_W + x] : 0;
    if (!career_race(race)) {
        return;
    }
    g.race = race;
    read_words(career_race(race)->description);
    go_to(CREATE_RACE_CONFIRM);
}

static void click_class(int y)
{
    int row;

    if (y < LIST_Y) {
        return;
    }
    /* Each line is CANVAS_LINE tall, and half a pixel more on average (compose_class_list). */
    row = (y - LIST_Y) * 2 / (2 * CANVAS_LINE + 1);
    if (row < 0 || row >= CLASSES || !career_class(row)) {
        return;
    }
    g.klass = row;
    read_words(FIRST_CLASS_WORDS + row);
    go_to(CREATE_CLASS_CONFIRM);
}

static void click_questions(int x, int y)
{
    int col;
    int row;
    int answer;

    if (x < ANSWER_X || y < ANSWER_Y) {
        return;
    }
    col = (x - ANSWER_X) / ANSWER_W;
    row = (y - ANSWER_Y) / ANSWER_H;
    answer = row * 2 + col;
    if (col < 2 && row < 5 && answer < g.biog->question[g.question].answers) {
        answer_question(answer);
    }
}

static void click_face(int x, int y)
{
    if (rect_has(&g_face_previous, x, y)) {
        g.face = (g.face + FACES - 1) % FACES;
        g.changed = 1;
    } else if (rect_has(&g_face_next, x, y)) {
        g.face = (g.face + 1) % FACES;
        g.changed = 1;
    } else if (rect_has(&g_ok, x, y)) {
        roll_attributes();
        go_to(CREATE_ATTRIBUTES);
    }
}

/* A point on or off the chosen attribute, never below its roll nor past the most. */
static void attribute_point(int more)
{
    int *value;

    value = &g.attribute[g.attribute_chosen];
    if (more && g.attribute_pool > 0 && *value < RULES_ATTRIBUTE_MAX) {
        (*value)++;
        g.attribute_pool--;
    } else if (!more && *value > g.attribute_rolled[g.attribute_chosen]) {
        (*value)--;
        g.attribute_pool++;
    }
}

static void click_attributes(int x, int y)
{
    int top;
    int i;

    g.changed = 1;
    if (rect_has(&g_reroll, x, y)) {
        roll_attributes();
        return;
    }
    if (rect_has(&g_ok, x, y)) {
        if (g.attribute_pool > 0) {
            g.note = "You must distribute all of your bonus points.";
            return;
        }
        roll_skills();
        go_to(CREATE_SKILLS);
        return;
    }
    top = ATTR_SPINNER_Y + g.attribute_chosen * ATTR_STEP;
    if (x >= ATTR_SPINNER_X && x < ATTR_SPINNER_X + UPDOWN_W) {
        if (y >= top && y < top + UPDOWN_ARM) {
            attribute_point(1);
        } else if (y >= top + UPDOWN_H - UPDOWN_ARM && y < top + UPDOWN_H) {
            attribute_point(0);
        }
        return;
    }
    for (i = 0; i < ATTR_COUNT; i++) {
        int row;

        row = ATTR_BUTTON_Y + i * ATTR_STEP;
        if (x >= ATTR_BUTTON_X && x < ATTR_BUTTON_X + ATTR_BUTTON_W && y >= row && y < row + ATTR_BUTTON_H) {
            g.attribute_chosen = i;
        }
    }
}

/* A point on or off the chosen skill, out of its group's, never below its roll. */
static void skill_point(int more)
{
    int group;
    int *value;

    group = group_of_line(g.skill_chosen);
    value = &g.skill[g.skill_chosen];
    if (more && g.skill_pool[group] > 0) {
        (*value)++;
        g.skill_pool[group]--;
    } else if (!more && *value > g.skill_rolled[g.skill_chosen]) {
        (*value)--;
        g.skill_pool[group]++;
    }
}

static void click_skills(int x, int y)
{
    int top;
    int i;

    g.changed = 1;
    if (rect_has(&g_ok, x, y)) {
        for (i = 0; i < SKILL_GROUPS; i++) {
            if (g.skill_pool[i] > 0) {
                g.note = "You must distribute all of your bonus points.";
                return;
            }
        }
        g.reflexes = RULES_REFLEX_AVERAGE;
        go_to(CREATE_REFLEXES);
        return;
    }
    top = skill_line_y(g.skill_chosen) - 1;
    if (y >= top && y < top + LEFTRIGHT_H) {
        if (x >= SKILL_SPINNER_X && x < SKILL_SPINNER_X + LEFTRIGHT_ARM) {
            skill_point(0);
            return;
        }
        if (x >= SKILL_SPINNER_X + LEFTRIGHT_W - LEFTRIGHT_ARM && x < SKILL_SPINNER_X + LEFTRIGHT_W) {
            skill_point(1);
            return;
        }
    }
    for (i = 0; i < SKILLS_SHOWN; i++) {
        if (x >= SKILL_NAME_X && x < SKILL_NAME_X + SKILL_ROW_W && y >= skill_line_y(i)
            && y < skill_line_y(i) + SKILL_ROW_H) {
            g.skill_chosen = i;
        }
    }
}

static void click_reflexes(int x, int y)
{
    if (x >= REFLEX_X && x < REFLEX_X + REFLEX_W && y >= REFLEX_Y && y < REFLEX_Y + REFLEX_ROW * REFLEX_ROWS) {
        g.reflexes = (y - REFLEX_Y) / REFLEX_ROW;
        g.changed = 1;
    } else if (rect_has(&g_ok, x, y)) {
        go_to(CREATE_SUMMARY);
    }
}

int create_click(int x, int y)
{
    g.note = 0;
    switch (g.step) {
    case CREATE_RACE:
        click_race(x, y);
        break;
    case CREATE_RACE_CONFIRM:
        if (rect_has(&g_answer_left, x, y)) {
            read_words(0);
            log_format(g.words, WORDS_MAX, "Are you a woman or a man?");
            go_to(CREATE_SEX);
        } else if (rect_has(&g_answer_right, x, y)) {
            go_to(CREATE_RACE);
        }
        break;
    case CREATE_SEX:
        if (rect_has(&g_answer_left, x, y) || rect_has(&g_answer_right, x, y)) {
            g.female = rect_has(&g_answer_left, x, y);
            open_faces();
            go_to(CREATE_CLASS);
        }
        break;
    case CREATE_CLASS:
        click_class(y);
        break;
    case CREATE_CLASS_CONFIRM:
        if (rect_has(&g_answer_left, x, y)) {
            begin_questions();
        } else if (rect_has(&g_answer_right, x, y)) {
            go_to(CREATE_CLASS);
        }
        break;
    case CREATE_QUESTIONS:
        click_questions(x, y);
        break;
    case CREATE_NAME:
        if (rect_has(&g_ok, x, y)) {
            return create_key(0x0D);
        }
        break;
    case CREATE_FACE:
        click_face(x, y);
        break;
    case CREATE_ATTRIBUTES:
        click_attributes(x, y);
        break;
    case CREATE_SKILLS:
        click_skills(x, y);
        break;
    case CREATE_REFLEXES:
        click_reflexes(x, y);
        break;
    default:
        if (rect_has(&g_restart, x, y)) {
            go_to(CREATE_RACE);
        } else if (rect_has(&g_ok, x, y)) {
            g.made = 1;
            return CREATE_DONE;
        }
        break;
    }
    return CREATE_STAY;
}

/* A key on the name line: a letter or a space on the end, or the last one off. Each word starts with a capital. */
static void name_key(int vk)
{
    int n;

    n = 0;
    while (g.name[n]) {
        n++;
    }
    if (vk == 0x08) {
        if (n > 0) {
            g.name[n - 1] = 0;
            g.changed = 1;
        }
    } else if (n < NAME_MAX - 1 && (vk == 0x20 || (vk >= 'A' && vk <= 'Z'))) {
        int starts_word;

        starts_word = n == 0 || g.name[n - 1] == ' ';
        g.name[n] = (char)(vk == 0x20 || starts_word ? vk : vk + ('a' - 'A'));
        g.name[n + 1] = 0;
        g.changed = 1;
    }
}

int create_key(int vk)
{
    if (g.step == CREATE_NAME) {
        if (vk == 0x0D) {
            if (!g.name[0]) {
                g.note = "Name thyself.";
                g.changed = 1;
                return CREATE_STAY;
            }
            go_to(CREATE_FACE);
            return CREATE_STAY;
        }
        name_key(vk);
        return CREATE_STAY;
    }
    if (vk != 0x1B) {
        return CREATE_STAY;
    }
    /* Escape on the map starts at once as the default; in the questions it answers the rest at random. */
    if (g.step == CREATE_RACE) {
        return CREATE_SKIPPED;
    }
    if (g.step == CREATE_QUESTIONS) {
        while (g.step == CREATE_QUESTIONS) {
            answer_question(combat_roll(g.biog->question[g.question].answers));
        }
    }
    return CREATE_STAY;
}

/* ---- Beginning and end. ---- */

int create_open(void)
{
    create_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.step = CREATE_RACE;
    g.klass = CLASS_WARRIOR;
    g.race_mask = composer_read(&g.composer, "TAMRIEL2.IMG");
    g.updown = composer_read(&g.composer, "CHAR02I1.IMG");
    g.leftright = composer_read(&g.composer, "CHAR03I1.IMG");
    g.reflex_marks = composer_read(&g.composer, "CHAR05I1.IMG");
    if (!g.race_mask || !g.updown || !g.leftright || !g.reflex_marks) {
        log_text(LOG_WARN, "creation: its pictures did not read");
        create_close();
        return 0;
    }
    return 1;
}

void create_close(void)
{
    if (!g.open) {
        return;
    }
    g.open = 0;
    if (g.faces_open) {
        cif_close(&g.faces);
        g.faces_open = 0;
    }
    composer_close(&g.composer);
}

const df_u8 *create_pixels(void)
{
    return g.composer.screen;
}

int create_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

const char *create_note(void)
{
    return g.note;
}

int create_step(void)
{
    return g.step;
}

int create_result(Character *sheet, Inventory *pack)
{
    const CareerClass *k;
    int i;

    if (!g.made || !character_new(sheet, g.race, g.klass, g.female, g.name)) {
        return 0;
    }
    k = career_class(g.klass);
    for (i = 0; i < ATTR_COUNT; i++) {
        sheet->attribute[i] = g.attribute[i];
    }
    /* Any skill not among the twelve: 3..6. Then the twelve, then what the answers gave. */
    for (i = 0; i < SKILL_COUNT; i++) {
        sheet->skill[i] = RULES_OTHER_BASE + combat_roll(RULES_SKILL_ROLLS);
    }
    for (i = 0; i < SKILLS_SHOWN; i++) {
        sheet->skill[skill_on_line(i)] = g.skill[i];
    }
    for (i = 0; i < SKILL_COUNT; i++) {
        sheet->skill[i] += g.skill_gain[i];
    }
    sheet->face = g.face;
    sheet->reflexes = g.reflexes;
    sheet->health_max = rules_max_health(1, k ? k->hp_per_level : 0);
    character_attributes_changed(sheet);
    character_rest_full(sheet);
    character_start_levelling(sheet);
    kit_give(g.klass, pack);
    /* Gold never goes below nothing (BiogFile). */
    pack->gold = g.gold_gain < 0 && (df_u32)(-g.gold_gain) > pack->gold ? 0u : pack->gold + (df_u32)g.gold_gain;
    for (i = 0; i < g.answered_items.count; i++) {
        inventory_add(pack, &g.answered_items.item[i]);
    }
    return 1;
}

int create_default(Character *sheet, Inventory *pack)
{
    if (!character_new(sheet, CHARACTER_START_RACE, CHARACTER_START_CLASS, CHARACTER_START_FEMALE, "Player")) {
        return 0;
    }
    kit_give(CHARACTER_START_CLASS, pack);
    return 1;
}
