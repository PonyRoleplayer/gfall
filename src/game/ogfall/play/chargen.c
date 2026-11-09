#include "game/ogfall/play/state.h"
#include "game/ogfall/play/chargen.h"

#include "game/ogfall/play/kit.h"
#include "game/ogfall/tables/tables.h"

#include "engine/core/mem.h"

/*
 * Title hits are point_in_rect, which is inclusive. The keys are the
 * "LSE" list: 0 load, 1 start, 2 exit. Escape is exit on the title
 * and a return to the title on the province map.
 */

static int g_mode;
static unsigned g_revision;
unsigned og_chargen_revision(void) { return g_revision; }
static int g_text;
static int g_lines;
static int g_pool;
static int g_attr_bonus[8];
static int g_skill_bonus[3];
static int g_attr_sel;
static int g_class_sel;
static int g_list_top;
static int g_list_sel;
static int g_question;
static OgCharacter g_before_background;
static OgClass g_custom;
static int g_maker_page, g_maker_top, g_maker_slot, g_maker_list, g_maker_kind;
static int g_special_count[2];
static int g_special_kind[2][7], g_special_param[2][7];
static int g_maker_pool;
static int g_custom_reputation[5];

int og_chargen_maker_reputation(int group) { return group >= 0 && group < 5 ? g_custom_reputation[group] : 0; }
int og_chargen_maker_page(void) { return g_maker_page; }
int og_chargen_maker_top(void) { return g_maker_top; }
int og_chargen_maker_kind(void) { return g_maker_kind; }
int og_chargen_maker_list(void) { return g_maker_list; }
int og_chargen_special_count(int list) { return g_special_count[list & 1]; }
int og_chargen_special_kind(int list, int slot) { return g_special_kind[list & 1][slot % 7]; }
int og_chargen_special_param(int list, int slot) { return g_special_param[list & 1][slot % 7]; }

static int g_asked[10];
static int g_answer_counts[3];
static int g_scroll;
static void open_class(int which);
static void accept_class(void);

int og_chargen_question_number(void) { return g_asked[g_question < 10 ? g_question : 9]; }
int og_chargen_question_scroll(void) { return g_scroll; }

static int g_scroll_drag;
static float g_repeat;
static int (*g_background)(int, int, int);
static int g_skill_sel[3];
static int g_skill_added[12];
static int g_saved_attr[8];
static int g_saved_bonus[8];
static int g_saved_pool;
static int g_have_saved;
static int g_rename;
static const char *g_notice = "";

void og_chargen_seed(unsigned seed)
{
    og_random_seed(seed);
}

static int roll(int lo, int hi)
{
    return og_random_range(lo, hi);
}

void og_chargen_background(int (*fn)(int, int, int))
{
    g_background = fn;
}

int og_chargen_attr_y(int attr)
{
    static const int y[] = { 20, 42, 64, 86, 108, 130, 151, 174 };
    return attr >= 0 && attr < 8 ? y[attr] : 20;
}

int og_chargen_skill_y(int slot)
{
    if (slot < 3) {
        return 31 + slot * 10;
    }
    if (slot < 6) {
        return 80 + (slot - 3) * 10;
    }
    return 129 + (slot - 6) * 10;
}

int og_chargen_attr_selected(void) { return g_attr_sel; }
int og_chargen_skill_selected(int group) { return group >= 0 && group < 3 ? g_skill_sel[group] : 0; }
const char *og_chargen_notice(void) { return g_notice; }

static void roll_attributes(void)
{
    const OgClass *row;
    OgCharacter *who;
    int i;
    int base;

    who = og_character();
    row = og_class(who->klass);
    for (i = 0; i < 8; i++) {
        base = row ? row->attribute[i] : 50;
        who->attribute[i] = roll(base, base + 10);
        if (who->attribute[i] > 100) {
            who->attribute[i] = 100;
        }
    }
    mem_zero(g_attr_bonus, sizeof(g_attr_bonus));
    g_pool = roll(6, 14);
}

static void prepare_skills(void)
{
    OgCharacter *who;
    int i;
    int skill;

    who = og_character();
    for (i = 0; i < OG_SKILLS; i++) {
        if (!who->skill[i]) {
            who->skill[i] = roll(3, 6);
        }
    }
    for (i = 0; i < 12; i++) {
        skill = og_class_skill(who->klass, i);
        if (skill >= 0 && skill < OG_SKILLS) {
            who->skill[skill] += i < 3 ? 25 : (i < 6 ? 15 : 10);
        }
    }
}

static void biography_answer(int answer)
{
    int count;

    count = g_background ? g_background(og_character()->klass, g_question, -1) : 8;
    if (count < 1) { count = 1; }
    if (answer < 0 || answer >= count) {
        return;
    }
    if (g_background) {
        g_background(og_character()->klass, g_question, answer);
    }
    g_question++;
    if (g_question == 12) {
        prepare_skills();
        g_mode = OG_UI_REPUTE;
    }
}

static char g_caption[64];

/* TEXT.RSC line counts (0xFD breaks, empty ones dropped) for the parchments. */
static int lines_for(int id)
{
    static const int race_lines[] = { 8, 9, 7, 8, 9, 9, 7, 6 };
    static const int class_lines[] = {
        7, 7, 6, 8, 6, 6, 7, 7, 6, 6, 6, 7, 6, 5, 6, 7, 6, 6
    };

    if (id >= 2000 && id <= 2007) {
        return race_lines[id - 2000];
    }
    if (id == 2200) {
        return 1;
    }
    if (id >= 2100 && id <= 2117) {
        return class_lines[id - 2100];
    }
    return 6;
}

/*
 * msgbox_render's button row. Font 4 is FONT0003, height 7. The border
 * tile is SPOP.RCI's 22. Kind 5 adds 20 for the buttons (text_format_flags
 * bit 2). The early -20 on the cursor is overwritten by the tile layout.
 */
int og_box_button_y(int lines)
{
    int mh;
    int hh;
    int rows;
    int boxh;
    int y0;
    int y;

    if (lines < 1) {
        lines = 1;
    }
    mh = lines * 7 + 10 + 20;
    hh = mh + 10;
    rows = (hh + 21) / 22;
    if (rows < 2) {
        rows = 2;
    }
    boxh = rows * 22;
    y0 = 100 - (boxh >> 1);
    y = y0 + 5 + ((boxh - hh + 10) >> 1);
    y += lines * 7;
    return y + 4;
}

static void caption(const char *s)
{
    int i;

    for (i = 0; i < 63 && s[i]; i++) {
        g_caption[i] = s[i];
    }
    g_caption[i] = 0;
}

static int in_rect(int x, int y, int x0, int y0, int x1, int y1)
{
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

static int ok_click(int x, int y)
{
    return x > 263 && x < 301 && y > 172 && y < 193;
}

static void start_questions(void)
{
    int i;
    int j;
    int number;
    int used;

    mem_zero(g_answer_counts, sizeof(g_answer_counts));
    for (i = 0; i < 10; i++) {
        do {
            number = roll(1, 40);
            used = 0;
            for (j = 0; j < i; j++) { if (g_asked[j] == number) { used = 1; } }
        } while (used);
        g_asked[i] = number;
    }
    g_question = 0;
    g_scroll = 0;
    g_mode = OG_UI_QUESTIONS;
}

static void question_answer(int answer)
{
    int klass;
    int kind;

    kind = og_question_kind(og_chargen_question_number(), answer);
    g_answer_counts[kind]++;
    g_question++;
    g_scroll = 0;
    if (g_question == 10) {
        klass = og_class_from_answers(g_answer_counts);
        if (klass >= 0) { open_class(klass); }
        else { g_mode = OG_UI_CLASS_LIST; }
    }
}

static void copy_class(int index)
{
    const OgClass *row;
    OgCharacter *who;
    int i;

    who = og_character();
    who->klass = index;
    row = og_class(index);
    if (!row) {
        return;
    }
    for (i = 0; i < OG_ATTRS; i++) {
        who->attribute[i] = row->attribute[i];
    }
    who->class_flags = row->flags;
    if (row->hp_per_level > 0) {
        who->max_health = 25 + row->hp_per_level;
    }
}

static int *custom_skill(int slot)
{
    if (slot < 3) { return &g_custom.primary[slot]; }
    if (slot < 6) { return &g_custom.major[slot - 3]; }
    return &g_custom.minor[slot - 6];
}

int og_chargen_maker_skill(int row)
{
    int i, j, used;
    for (i = 0; i < OG_SKILLS; i++) {
        used = 0;
        for (j = 0; j < 12; j++) { if (*custom_skill(j) == i) { used = 1; } }
        if (!used && row-- == 0) { return i; }
    }
    return -1;
}

static void custom_publish(void)
{
    g_pool = g_maker_pool;
    og_class_set(18, &g_custom);
}

static void custom_begin(void)
{
    int i;
    mem_zero(&g_custom, sizeof(g_custom));
    mem_zero(g_special_count, sizeof(g_special_count));
    mem_zero(g_custom_reputation, sizeof(g_custom_reputation));
    g_custom.hp_per_level = 8;
    g_custom.flags = 0x1400;
    g_custom.advancement = 65536;
    for (i = 0; i < 8; i++) { g_custom.attribute[i] = 50; }
    for (i = 0; i < 12; i++) { *custom_skill(i) = -1; }
    g_maker_page = 0;
    g_maker_pool = 0;
    g_attr_sel = 0;
    custom_publish();
    g_mode = OG_UI_MAKER;
}

/* Rebuild from the picked specials, so removing one cannot leave stale bits.
 * custom.c classmaker_set_advantage/disadvantage and update_advancement. */
static void custom_specials(void)
{
    int list, i, kind, param, bit, cost;
    OgClass *c;
    const OgSpecial *s;
    c = &g_custom;
    c->flags = 0x1400;
    c->resistance = c->immunity = c->tolerance = c->weakness = 0;
    c->absorption = c->rapid = c->regeneration = c->attack = c->expert = 0;
    c->forbidden_materials = c->forbidden_equipment = 0;
    cost = 65536 + (c->hp_per_level - 8) * 3277 * (c->hp_per_level < 8 ? 2 : 1);
    for (list = 0; list < 2; list++) {
        for (i = 0; i < g_special_count[list]; i++) {
            kind = g_special_kind[list][i]; param = g_special_param[list][i]; bit = 1 << param;
            s = og_special(list, kind);
            cost += (list ? -1 : 1) * s->cost[param];
            if (!list) {
                switch (kind) {
                case 0: c->resistance |= bit; break;
                case 1: c->immunity |= bit; break;
                case 2: c->flags |= 1; break;
                case 3: c->absorption |= bit; break;
                case 4: c->rapid |= bit; break;
                case 5: c->regeneration |= bit; break;
                case 6: c->attack |= bit; break;
                case 7: c->flags |= 2; break;
                case 8: c->flags = (c->flags & ~0x1c00) | (param << 10); break;
                case 9: c->flags |= 4; break;
                case 10: c->expert |= bit; break;
                }
            } else {
                switch (kind) {
                case 0: c->flags |= 8; break;
                case 1: c->flags |= bit << 4; break;
                case 2: c->attack |= bit << 4; break;
                case 3: c->flags |= bit << 6; break;
                case 4: c->flags |= bit << 8; break;
                case 5: c->forbidden_equipment |= bit; break;
                case 6: c->tolerance |= bit; break;
                case 7: c->weakness |= bit; break;
                case 8: c->forbidden_equipment |= bit << 6; break;
                case 9: c->forbidden_equipment |= bit << 9; break;
                case 10: c->forbidden_materials |= bit; break;
                }
            }
        }
    }
    c->advancement = cost < 6554 ? 6554 : cost;
    custom_publish();
}

static void custom_add_special(int param)
{
    int i, list, kind, other;
    list = g_maker_list; kind = g_maker_kind;
    for (i = 0; i < g_special_count[list]; i++) {
        if (g_special_kind[list][i] == kind && (g_special_param[list][i] == param || (!list && kind == 8))) {
            g_notice = "That special is already selected."; return;
        }
    }
    if ((!list && kind < 2) || (list && (kind == 6 || kind == 7))) {
        for (i = 0; i < g_special_count[!list]; i++) {
            other = g_special_kind[!list][i];
            if (g_special_param[!list][i] == param && (list ? other < 2 : (other == 6 || other == 7))) {
                g_notice = "That advantage and disadvantage conflict."; return;
            }
        }
    }
    i = g_special_count[list]++;
    g_special_kind[list][i] = kind; g_special_param[list][i] = param;
    custom_specials();
    g_maker_page = list ? 3 : 2;
}

static void enter_world(void)
{
    OgCharacter *who;
    int max;

    who = og_character();
    if (who->max_health <= 0) {
        who->max_health = 25 + who->attribute[OG_END] / 2;
    }
    if (who->max_health < 1) {
        who->max_health = 1;
    }
    who->health = who->max_health;
    max = (who->attribute[OG_STR] + who->attribute[OG_END]) << 6;
    who->fatigue = max > 0 ? max : 64;
    who->max_magicka = og_magicka(who->attribute[OG_INT], who->class_flags);
    who->magicka = who->max_magicka;
    who->level = 1;
    if (who->gold <= 0) {
        who->gold = 100;
    }
    og_kit_give(who);
    if (!og_state_begin()) {
        caption("Cannot create the game state");
        return;
    }
    g_mode = OG_UI_WORLD;
    caption("Privateer's Hold");
}

void og_chargen_begin(void)
{
    og_state_clear();
    g_scroll_drag = 0;
    g_repeat = 0.0f;
    g_revision++;
    g_mode = OG_UI_TITLE;
    g_text = 0;
    g_lines = 1;
    g_pool = 10;
    g_attr_sel = 0;
    g_class_sel = -1;
    g_list_top = 0;
    g_list_sel = 0;
    g_question = 0;
    g_skill_sel[0] = 0;
    g_skill_sel[1] = 3;
    g_skill_sel[2] = 6;
    g_have_saved = 0;
    g_rename = 0;
    g_notice = "";
    mem_zero(g_skill_added, sizeof(g_skill_added));
    mem_zero(g_attr_bonus, sizeof(g_attr_bonus));
    mem_zero(g_skill_bonus, sizeof(g_skill_bonus));
    og_character_clear();
    caption("L load   S start   E exit");
}

int og_chargen_mode(void)
{
    return g_mode;
}

const char *og_chargen_caption(void)
{
    return g_caption;
}

int og_chargen_text_id(void)
{
    return g_text;
}

int og_chargen_choice_y(void)
{
    if (g_mode != OG_UI_RACE && g_mode != OG_UI_GENDER && g_mode != OG_UI_CLASS_YES) {
        return -1;
    }
    return og_box_button_y(g_lines);
}

int og_chargen_count_lines(const char *text)
{
    int n;
    int any;

    n = 0;
    any = 0;
    if (!text) {
        return 1;
    }
    for (; *text; text++) {
        if (*text == '\n') {
            if (any) {
                n++;
            }
            any = 0;
        } else {
            any = 1;
        }
    }
    if (any) {
        n++;
    }
    if (n < 1) {
        n = 1;
    }
    if (n > 16) {
        n = 16;
    }
    return n;
}

void og_chargen_set_lines(int lines)
{
    if (lines < 1) {
        lines = 1;
    }
    if (lines > 16) {
        lines = 16;
    }
    g_lines = lines;
}

int og_chargen_lines(void)
{
    return g_lines;
}

int og_chargen_list_top(void)
{
    return g_list_top;
}

int og_chargen_list_sel(void)
{
    return g_list_sel;
}

int og_chargen_question(void)
{
    return g_question;
}

int og_chargen_attr_pool(void)
{
    return g_pool;
}

int og_chargen_attr_bonus(int attr)
{
    if (attr < 0 || attr >= 8) {
        return 0;
    }
    return g_attr_bonus[attr];
}

int og_chargen_skill_bonus(int group)
{
    if (group < 0 || group >= 3) {
        return 0;
    }
    return g_skill_bonus[group];
}

static void change_attr(int delta)
{
    OgCharacter *who;

    who = og_character();
    if (delta > 0) {
        if (g_pool <= 0 || who->attribute[g_attr_sel] >= 100) {
            return;
        }
    } else if (!g_attr_bonus[g_attr_sel]) {
        return;
    }
    who->attribute[g_attr_sel] += delta;
    g_attr_bonus[g_attr_sel] += delta;
    g_pool -= delta;
}

static void change_skill(int group, int delta)
{
    OgCharacter *who;
    int slot;
    int skill;

    slot = g_skill_sel[group];
    skill = og_class_skill(og_character()->klass, slot);
    if (skill < 0 || skill >= OG_SKILLS) {
        return;
    }
    if ((delta > 0 && g_skill_bonus[group] == 6)
        || (delta < 0 && g_skill_added[slot] == 0)) {
        return;
    }
    who = og_character();
    who->skill[skill] += delta;
    g_skill_added[slot] += delta;
    g_skill_bonus[group] += delta;
}

static void title_click(int x, int y)
{
    if (in_rect(x, y, 72, 46, 217, 58)) {
        g_mode = OG_UI_LOAD;
        caption("Load game");
        return;
    }
    if (in_rect(x, y, 74, 100, 217, 112)) {
        og_chargen_begin();
        g_mode = OG_UI_MAP;
        caption("Select your home province");
        return;
    }
    if (in_rect(x, y, 125, 145, 165, 158)) {
        g_mode = OG_UI_TITLE;
        caption("");
        /* The caller treats a repeated exit caption as quit via key E.
         * A click on Exit sets a sentinel mode by clearing the caption
         * and raising text id -1. */
        g_text = -1;
    }
}

/* title_menu: pick is the mask byte, 1..8. 0 is the sea and stays on the map. */
static void map_pixel(int pixel)
{
    OgCharacter *who;

    if (pixel < 1 || pixel > 8) {
        return;
    }
    who = og_character();
    who->race = pixel - 1;
    g_text = og_race_text(who->race);
    g_lines = lines_for(g_text);
    g_mode = OG_UI_RACE;
    caption(og_race_name(who->race));
}

static void back_to_map(void)
{
    g_mode = OG_UI_MAP;
    g_text = 0;
    caption("Select your home province");
}

static void accept_race(void)
{
    g_text = 2200;
    g_lines = lines_for(2200);
    g_mode = OG_UI_GENDER;
    caption("M male, F female");
}

static void pick_sex(int female)
{
    og_character()->female = female;
    g_mode = OG_UI_CLASS_HOW;
    caption("Class from a list, or questions.");
}

/* Strict edges, as msgbox_button_at: the face of the 32 x 16 button. */
static int choice_at(int x, int y, int x0)
{
    int top;

    top = og_box_button_y(g_lines);
    return x > x0 && x < x0 + OG_CHOICE_W && y > top && y < top + OG_CHOICE_H;
}

/* picklist_poll: the row under the cursor, or -1 outside the list. */
void og_chargen_scroll_thumb(int top, int count, int *y, int *height)
{
    int h;
    h = count <= 9 ? 82 : 81 * 9 / count;
    if (y) {
        *y = 59 + (count > 9 ? top * 83 / count : 0);
    }
    if (height) {
        *height = h - 1;
    }
}

static int scroll_list(int x, int y)
{
    int *top;
    int count;
    int next;
    int step;

    if (g_mode == OG_UI_CLASS_LIST) {
        top = &g_list_top;
        count = OG_LIST_N;
    } else if (g_mode == OG_UI_MAKER && (g_maker_page == 1 || g_maker_page == 4 || g_maker_page == 5)) {
        top = &g_maker_top;
        count = g_maker_page == 4 ? 11 : (g_maker_page == 5 ? og_special(g_maker_list, g_maker_kind)->count : 0);
        if (g_maker_page == 1) {
            while (og_chargen_maker_skill(count) >= 0) {
                count++;
            }
        }
    } else {
        return 0;
    }
    if (x < 239 || x > 247) {
        return 0;
    }
    next = *top;
    step = y >= 47 && y <= 56 ? -1 : (y >= 145 && y <= 154 ? 1 : 0);
    if (step) {
        if (g_mode == OG_UI_CLASS_LIST) {
            g_list_sel += step;
            if (g_list_sel < 0) {
                g_list_sel = 0;
            }
            if (g_list_sel >= count) {
                g_list_sel = count - 1;
            }
            if (g_list_sel < next) {
                next = g_list_sel;
            }
            if (g_list_sel >= next + 9) {
                next = g_list_sel - 8;
            }
        } else {
            next += step;
        }
    } else if (y >= 58 && y <= 140) {
        next = count <= 9 ? 0 : count * (y - 58) / 83;
        if (g_mode == OG_UI_CLASS_LIST) {
            g_list_sel = next;
        }
    } else {
        return 0;
    }
    if (next > count - 9) {
        next = count - 9;
    }
    if (next < 0) {
        next = 0;
    }
    *top = next;
    if (g_mode == OG_UI_CLASS_LIST && g_list_sel >= count) {
        g_list_sel = count - 1;
    }
    return 1;
}

void og_chargen_scroll_frame(int x, int y, int held, int pressed, float dt)
{
    if (!(held & 1)) {
        g_scroll_drag = 0;
        g_repeat = 0.0f;
        return;
    }
    if (pressed & 1) {
        g_scroll_drag = x >= 239 && x <= 247 && y >= 58 && y <= 140;
        g_repeat = 0.0f;
        return;
    }
    if (g_scroll_drag) {
        if (y < 58) {
            y = 58;
        }
        if (y > 140) {
            y = 140;
        }
        if (scroll_list(243, y)) {
            g_revision++;
        }
    } else {
        g_repeat += dt;
        if (g_repeat >= 0.22f) {
            g_repeat = 0.0f;
            if (scroll_list(x, y)) {
                g_revision++;
            }
        }
    }
}

static int list_row(int x, int y)
{
    int row;

    if (x < OG_LIST_X || x > OG_LIST_X + OG_LIST_W - 1 || y < OG_LIST_Y || y > OG_LIST_Y + OG_LIST_H - 1) {
        return -1;
    }
    row = g_list_top + (y - OG_LIST_Y) / OG_LIST_ROW;
    if (row < 0 || row >= OG_LIST_N) {
        return -1;
    }
    return row;
}

static void open_class(int which)
{
    if (which == 18) {
        custom_begin();
        caption("Custom class");
        return;
    }
    if (which < 0 || which >= OG_CLASSES) {
        return;
    }
    g_class_sel = which;
    g_text = 2100 + which;
    g_lines = lines_for(g_text);
    g_mode = OG_UI_CLASS_YES;
    caption(og_class(which) ? og_class(which)->name : "This class?");
}

static void accept_class(void)
{
    if (g_class_sel < 0 || g_class_sel >= OG_CLASSES) {
        return;
    }
    {
        int race, female, i;
        race = og_character()->race; female = og_character()->female;
        og_character_clear();
        og_character()->race = race; og_character()->female = female;
        if (g_class_sel == 18) {
            for (i = 0; i < 5; i++) { og_character()->reputation[i] = g_custom_reputation[i]; }
        }
    }
    copy_class(g_class_sel);
    og_character()->gold = 100;
    g_before_background = *og_character();
    g_question = 0;
    g_mode = OG_UI_BG_HOW;
    caption("Generated background, or questions.");
}

/* career_answer_boxes[1..8], strict, as career_wait_answer. 0 is the question. */
static int answer_box(int x, int y)
{
    static const int box[] = {
        10, 71, 158, 92, 160, 71, 308, 92,
        10, 95, 158, 116, 160, 95, 308, 116,
        10, 119, 158, 140, 160, 119, 308, 140,
        10, 143, 158, 164, 160, 143, 308, 164
    };
    int i;

    for (i = 0; i < 8; i++) {
        int x0;
        int y0;
        int x1;
        int y1;

        x0 = box[i * 4];
        y0 = box[i * 4 + 1];
        x1 = box[i * 4 + 2];
        y1 = box[i * 4 + 3];
        if (x > x0 && x < x1 && y > y0 && y < y1) {
            return i + 1;
        }
    }
    return 0;
}

void og_chargen_click(int x, int y, int pixel)
{
    int which;
    int i;
    int y0;
    int summary;
    OgCharacter *who;

    g_revision++;
    if (g_notice[0]) { g_notice = ""; return; }
    if (scroll_list(x, y)) { return; }
    if (g_mode == OG_UI_TITLE) {
        title_click(x, y);
        return;
    }
    /* The outer controller owns the save browser and its clicks. */
    if (g_mode == OG_UI_LOAD) { return; }
    if (g_mode == OG_UI_MAP) {
        map_pixel(pixel);
        return;
    }
    /* Yes continues. No puts the map back. A click on the land does not. */
    if (g_mode == OG_UI_RACE) {
        if (choice_at(x, y, OG_CHOICE_X0)) {
            accept_race();
        } else if (choice_at(x, y, OG_CHOICE_X1)) {
            back_to_map();
        }
        return;
    }
    if (g_mode == OG_UI_GENDER) {
        if (choice_at(x, y, OG_CHOICE_X0)) {
            pick_sex(0);
        } else if (choice_at(x, y, OG_CHOICE_X1)) {
            pick_sex(1);
        }
        return;
    }
    /* title_menu: the top of BUTN01I0 is the list, the bottom the questions. */
    if (g_mode == OG_UI_CLASS_HOW) {
        if (x > 68 && x < 251 && y > 60 && y < 111) {
            g_mode = OG_UI_CLASS_LIST;
            g_list_top = 0;
            g_list_sel = -1;
            caption("Choose a class");
        } else if (x > 68 && x < 251 && y > 112 && y < 171) {
            start_questions();
            caption("Class questions: A, B or C. Arrows scroll.");
        }
        return;
    }
    if (g_mode == OG_UI_MAKER) {
        if (g_maker_page == 1 || g_maker_page == 4 || g_maker_page == 5) {
            int limit;
            limit = g_maker_page == 4 ? 11 : (g_maker_page == 5 ? og_special(g_maker_list, g_maker_kind)->count : 0);
            if (g_maker_page == 1) { while (og_chargen_maker_skill(limit) >= 0) { limit++; } }
            if (x < OG_LIST_X || x > OG_LIST_X + OG_LIST_W || y < OG_LIST_Y || y >= OG_LIST_Y + OG_LIST_VIS * 8) { return; }
            which = g_maker_top + (y - OG_LIST_Y) / 8;
            if (which >= limit) { return; }
            if (g_maker_page == 1) {
                *custom_skill(g_maker_slot) = og_chargen_maker_skill(which);
                custom_publish(); g_maker_page = 0;
            } else if (g_maker_page == 4) {
                g_maker_kind = which;
                if (og_special(g_maker_list, which)->count == 1) { custom_add_special(0); }
                else { g_maker_page = 5; g_maker_top = 0; }
            } else { custom_add_special(which); }
            return;
        }
        if (g_maker_page == 2 || g_maker_page == 3) {
            if (x > 3 && x < 164 && y > 176 && y < 198) { g_maker_page = 0; return; }
            if (x > 99 && x < 154 && y > 6 && y < 24 && g_special_count[g_maker_list] < 7) {
                g_maker_page = 4; g_maker_top = 0; return;
            }
            if (x > 3 && x < 164 && y > 32 && y < 174) {
                which = (y - 32) / 20;
                if (which < g_special_count[g_maker_list]) {
                    for (i = which; i < g_special_count[g_maker_list] - 1; i++) {
                        g_special_kind[g_maker_list][i] = g_special_kind[g_maker_list][i + 1];
                        g_special_param[g_maker_list][i] = g_special_param[g_maker_list][i + 1];
                    }
                    g_special_count[g_maker_list]--; custom_specials();
                }
            }
            return;
        }
        if (g_maker_page == 6) {
            if (x > 168 && x < 201 && y > 170 && y < 184) {
                which = 0; for (i = 0; i < 5; i++) { which += g_custom_reputation[i]; }
                if (which) { g_notice = "Reputation points must balance to zero."; }
                else { g_maker_page = 0; }
                return;
            }
            for (i = 0; i < 5; i++) {
                if (x > 42 + i * 33 && x < 46 + i * 33 && y > 30 && y < 132) {
                    g_custom_reputation[i] = (81 - y) / 5;
                }
            }
            return;
        }
        if (g_maker_page == 7) { return; }
        if (ok_click(x, y)) {
            for (i = 0; i < 12; i++) { if (*custom_skill(i) < 0) { g_notice = "Select all twelve class skills."; return; } }
            if (!g_custom.name[0]) { g_notice = "Name your class."; return; }
            if (g_maker_pool) { g_notice = "Distribute the attribute points."; return; }
            which = g_custom.advancement >= 65536
                ? 115 - (g_custom.advancement - 65536) * 69 / 262144
                : 115 + (65536 - g_custom.advancement) * 69 / 65536;
            if (which < 81 || which > 161) {
                g_notice = "Skill advancement must be between 0.3x and 3x."; return;
            }
            which = 0;
            for (i = 0; i < 5; i++) { which += g_custom_reputation[i]; }
            if (which) { g_notice = "Reputation points must balance to zero."; return; }
            g_class_sel = 18; accept_class(); return;
        }
        if (y > 2 && y < 13) { g_maker_page = 7; return; }
        for (i = 0; i < 12; i++) {
            y0 = og_chargen_skill_y(i);
            if (x > 66 && x < 174 && y > y0 && y < y0 + 8) { g_maker_page = 1; g_maker_slot = i; g_maker_top = 0; return; }
        }
        for (i = 0; i < 8; i++) {
            y0 = og_chargen_attr_y(i);
            if (x > 7 && x < 42 && y > y0 && y < y0 + 19) { g_attr_sel = i; }
        }
        y0 = og_chargen_attr_y(g_attr_sel) + 1;
        if (x > 45 && x < 57) {
            if (y > y0 && y < y0 + 6 && g_custom.attribute[g_attr_sel] < 75) { g_custom.attribute[g_attr_sel]++; g_maker_pool--; }
            if (y > y0 + 13 && y < y0 + 19 && g_custom.attribute[g_attr_sel] > 10) { g_custom.attribute[g_attr_sel]--; g_maker_pool++; }
            custom_publish();
        }
        if (x > 252 && x < 260) {
            if (y > 46 && y < 56 && g_custom.hp_per_level < 30) { g_custom.hp_per_level++; }
            if (y > 57 && y < 67 && g_custom.hp_per_level > 1) { g_custom.hp_per_level--; }
            custom_specials();
        }
        if (x > 249 && x < 315) {
            if (y > 74 && y < 96) { g_notice = "Choose twelve skills, a name and attributes.\nBalance HP and specials, then Exit to continue."; }
            if (y > 98 && y < 120) { g_maker_list = 0; g_maker_page = 2; }
            if (y > 122 && y < 144) { g_maker_list = 1; g_maker_page = 3; }
            if (y > 146 && y < 168) { g_maker_page = 6; }
        }
        return;
    }
    if (g_mode == OG_UI_QUESTIONS) {
        const char *text;
        int row;
        int n;
        int answer;
        text = og_question_text(og_chargen_question_number());
        if (y > 120 && y < 135) { if (g_scroll > 0) { g_scroll--; } return; }
        if (y > 183 && y < 200) { if (g_scroll < 100) { g_scroll++; } return; }
        row = 0;
        answer = -1;
        while (*text) {
            if (text[0] == ' ' && text[1] >= 'a' && text[1] <= 'c' && text[2] == ')') { answer = text[1] - 'a'; }
            n = 0;
            while (text[n] && text[n] != '\n') { n++; }
            if (answer >= 0 && y >= 135 + row * 7 - g_scroll && y < 142 + row * 7 - g_scroll && y >= 135 && y < 183) {
                question_answer(answer); return;
            }
            text += n;
            if (*text) { text++; }
            row++;
        }
        return;
    }
    if (g_mode == OG_UI_CLASS_LIST) {
        which = list_row(x, y);
        if (which < 0) {
            return;
        }
        /* A second click on the highlighted row is the list's double-click. */
        if (which == g_list_sel) {
            open_class(which);
        } else {
            g_list_sel = which;
        }
        return;
    }
    if (g_mode == OG_UI_CLASS_YES) {
        if (choice_at(x, y, OG_CHOICE_X0)) {
            accept_class();
        } else if (choice_at(x, y, OG_CHOICE_X1)) {
            g_mode = OG_UI_CLASS_LIST;
            caption("Choose a class");
        }
        return;
    }
    /* butn02i0.img: the top generates the background, the bottom asks. */
    if (g_mode == OG_UI_BG_HOW) {
        if (x > 76 && x < 243 && y > 58 && y < 111) {
            *og_character() = g_before_background;
            g_question = 0;
            while (g_question < 12) {
                int count;
                count = g_background ? g_background(og_character()->klass, g_question, -1) : 8;
                if (count < 1) { count = 1; }
                biography_answer(roll(0, count - 1));
            }
            g_mode = OG_UI_REPUTE;
            caption("Reputation. Enter continues.");
        } else if (x > 76 && x < 243 && y > 130 && y < 176) {
            *og_character() = g_before_background;
            g_question = 0;
            g_mode = OG_UI_BIOG;
            caption("Biography. Y accepts, Escape returns.");
        }
        return;
    }
    if (g_mode == OG_UI_BIOG) {
        which = answer_box(x, y);
        if (which) {
            biography_answer(which - 1);
        }
        return;
    }
    if (g_mode == OG_UI_REPUTE) {
        g_mode = OG_UI_NAME;
        caption("Name");
        return;
    }
    if (g_mode == OG_UI_NAME) {
        if (ok_click(x, y)) {
            og_chargen_key(13);
        }
        return;
    }
    if (g_mode == OG_UI_FACE || g_mode == OG_UI_SUMMARY) {
        who = og_character();
        /* chargen_face_previous / chargen_face_next. */
        if (x > 245 && x < 286 && y > 69 && y < 77) {
            who->face = who->face <= 0 ? 9 : who->face - 1;
            return;
        }
        if (x > 287 && x < 312 && y > 69 && y < 77) {
            who->face = (who->face + 1) % 10;
            return;
        }
    }
    if (g_mode == OG_UI_FACE || g_mode == OG_UI_ATTR || g_mode == OG_UI_SKILL
        || g_mode == OG_UI_REFLEX || g_mode == OG_UI_SUMMARY) {
        if (ok_click(x, y)) {
            if (g_mode == OG_UI_FACE) {
                g_mode = OG_UI_ATTR;
                roll_attributes();
                caption("Bonus points. Select an attribute, then the diamond.");
            } else if (g_mode == OG_UI_ATTR) {
                if (g_pool) { g_notice = "You must distribute your bonus points."; return; }
                g_mode = OG_UI_SKILL;
                caption("Skill bonuses. Six in each group.");
            } else if (g_mode == OG_UI_SKILL) {
                if (g_skill_bonus[0] != 6 || g_skill_bonus[1] != 6 || g_skill_bonus[2] != 6) {
                    g_notice = "You must distribute your bonus points."; return;
                }
                g_mode = OG_UI_REFLEX;
                caption("Reflexes, average.");
            } else if (g_mode == OG_UI_REFLEX) {
                g_mode = OG_UI_SUMMARY;
                caption("Summary.");
            } else {
                if (g_pool || g_skill_bonus[0] != 6 || g_skill_bonus[1] != 6 || g_skill_bonus[2] != 6) {
                    g_notice = "You must distribute your bonus points."; return;
                }
                enter_world();
            }
            return;
        }
    }
    summary = g_mode == OG_UI_SUMMARY;
    if (summary && in_rect(x, y, 8, 2, 315, 13)) {
        g_rename = 1;
        g_mode = OG_UI_NAME;
        return;
    }
    if (g_mode == OG_UI_ATTR || summary) {
        for (i = 0; i < 8; i++) {
            y0 = og_chargen_attr_y(i);
            if (x > 7 && x < 42 && y > y0 && y < y0 + 19) {
                g_attr_sel = i;
            }
        }
        y0 = og_chargen_attr_y(g_attr_sel) + 1;
        if (x > 45 && x < 57) {
            if (y > y0 && y < y0 + 6) { change_attr(1); }
            if (y > y0 + 13 && y < y0 + 19) { change_attr(-1); }
        }
        if (x > 263 && x < 301 && y > 142 && y < 168) {
            if (summary) {
                og_chargen_begin();
                g_mode = OG_UI_MAP;
            } else {
                roll_attributes();
            }
        }
        if (!summary && x > 162 && x < 232) {
            who = og_character();
            if (y > 162 && y < 169) {
                mem_copy(g_saved_attr, who->attribute, sizeof(g_saved_attr));
                mem_copy(g_saved_bonus, g_attr_bonus, sizeof(g_saved_bonus));
                g_saved_pool = g_pool;
                g_have_saved = 1;
            } else if (y > 172 && y < 178 && g_have_saved) {
                mem_copy(who->attribute, g_saved_attr, sizeof(g_saved_attr));
                mem_copy(g_attr_bonus, g_saved_bonus, sizeof(g_saved_bonus));
                g_pool = g_saved_pool;
            }
        }
    }
    if (g_mode == OG_UI_SKILL || summary) {
        for (i = 0; i < 12; i++) {
            y0 = og_chargen_skill_y(i);
            if (x > 66 && x < 174 && y > y0 && y < y0 + 8) {
                g_skill_sel[i < 3 ? 0 : (i < 6 ? 1 : 2)] = i;
            }
        }
        for (i = 0; i < 3; i++) {
            y0 = og_chargen_skill_y(g_skill_sel[i]);
            if (y > y0 && y < y0 + 8) {
                if (x > 203 && x < 213) { change_skill(i, -1); }
                if (x > 229 && x < 239) { change_skill(i, 1); }
            }
        }
    }
    if (g_mode == OG_UI_REFLEX || summary) {
        if (summary) { x -= 119; y += 53; }
        for (i = 0; i < 5; i++) {
            if (x > 127 && x < 192 && y > 148 + i * 9 && y < 156 + i * 9) {
                og_character()->reflexes = i;
            }
        }
    }
}

void og_chargen_key(int ch)
{
    OgCharacter *who;
    int n;

    if (ch == 0) {
        return;
    }
    g_revision++;
    if (g_notice[0]) { g_notice = ""; return; }
    if (g_mode == OG_UI_TITLE) {
        if (ch == 's' || ch == 'S') {
            og_chargen_begin();
            g_mode = OG_UI_MAP;
            caption("Select your home province");
        } else if (ch == 'l' || ch == 'L') {
            g_mode = OG_UI_LOAD;
            caption("Load game");
        } else if (ch == 'e' || ch == 'E' || ch == 27) {
            g_text = -1;
            caption("");
        }
        return;
    }
    if (g_mode == OG_UI_LOAD) {
        if (ch == 27) {
            g_mode = OG_UI_TITLE;
            g_text = 0;
            caption("L load   S start   E exit");
        }
        return;
    }
    if (g_mode == OG_UI_MAP && ch == 27) {
        g_mode = OG_UI_TITLE;
        g_text = 0;
        caption("L load   S start   E exit");
        return;
    }
    if (g_mode == OG_UI_RACE) {
        if (ch == 'y' || ch == 'Y') {
            accept_race();
        } else if (ch == 'n' || ch == 'N' || ch == 27) {
            /* Escape closes the box and the map loop runs again. */
            back_to_map();
        }
        return;
    }
    if (g_mode == OG_UI_GENDER) {
        if (ch == 'm' || ch == 'M') {
            pick_sex(0);
        } else if (ch == 'f' || ch == 'F') {
            pick_sex(1);
        }
        return;
    }
    if (g_mode == OG_UI_CLASS_YES && (ch == 'y' || ch == 'Y')) {
        accept_class();
        return;
    }
    if (ch == 27 && (g_mode == OG_UI_CLASS_LIST || g_mode == OG_UI_CLASS_HOW || g_mode == OG_UI_BG_HOW)) {
        g_mode = g_mode == OG_UI_BG_HOW ? OG_UI_CLASS_LIST : (g_mode == OG_UI_CLASS_LIST ? OG_UI_CLASS_HOW : OG_UI_MAP);
        return;
    }
    if (g_mode == OG_UI_CLASS_LIST && (ch == 1001 || ch == 1002)) {
        g_list_sel += ch == 1001 ? -1 : 1;
        if (g_list_sel < 0) { g_list_sel = 0; }
        if (g_list_sel >= OG_LIST_N) { g_list_sel = OG_LIST_N - 1; }
        if (g_list_sel < g_list_top) { g_list_top = g_list_sel; }
        if (g_list_sel >= g_list_top + OG_LIST_VIS) { g_list_top = g_list_sel - OG_LIST_VIS + 1; }
        return;
    }
    if (g_mode == OG_UI_CLASS_LIST && ch == 13) {
        open_class(g_list_sel);
        return;
    }
    if (g_mode == OG_UI_CLASS_YES && (ch == 27 || ch == 'n' || ch == 'N')) {
        g_mode = OG_UI_CLASS_LIST;
        caption("Choose a class");
        return;
    }
    if (g_mode == OG_UI_QUESTIONS) {
        if (ch == 27) { g_mode = OG_UI_CLASS_HOW; }
        else if (ch >= 'a' && ch <= 'c') { question_answer(ch - 'a'); }
        else if (ch >= 'A' && ch <= 'C') { question_answer(ch - 'A'); }
        else if (ch == 1001 && g_scroll > 0) { g_scroll--; }
        else if (ch == 1002 && g_scroll < 100) { g_scroll++; }
        return;
    }
    if (g_mode == OG_UI_MAKER) {
        if (ch == 27) {
            if (g_maker_page) { g_maker_page = 0; }
            else { g_mode = OG_UI_CLASS_LIST; }
        } else if (g_maker_page == 7) {
            n = 0; while (g_custom.name[n]) { n++; }
            if (ch == 13) { g_maker_page = 0; }
            else if (ch == 8 && n) { g_custom.name[n - 1] = 0; }
            else if (ch >= 32 && ch <= 126 && n < 14) { g_custom.name[n] = (char)ch; g_custom.name[n + 1] = 0; }
            custom_publish();
        }
        return;
    }
    if (g_mode == OG_UI_BIOG) {
        if (ch >= '1' && ch <= '8') { biography_answer(ch - '1'); return; }
        if (ch == 27) {
            g_mode = OG_UI_BG_HOW;
            caption("Generated background, or questions.");
        } else if (ch == 'y' || ch == 'Y') {
            biography_answer(0);
        }
        return;
    }
    if (g_mode == OG_UI_REPUTE && ch == 13) {
        g_mode = OG_UI_NAME;
        caption("Name");
        return;
    }
    if (g_mode == OG_UI_NAME) {
        who = og_character();
        n = 0;
        while (who->name[n] && n < OG_NAME - 1) {
            n++;
        }
        if (ch == 13 && n > 0) {
            g_mode = g_rename ? OG_UI_SUMMARY : OG_UI_FACE;
            g_rename = 0;
            caption("Face");
            return;
        }
        if (ch == 8 && n > 0) {
            who->name[n - 1] = 0;
            return;
        }
        if (ch >= 33 && ch <= 126 && n < OG_NAME - 1) {
            who->name[n] = (char)ch;
            who->name[n + 1] = 0;
        } else if (ch == ' ' && n > 0 && n < OG_NAME - 1) {
            who->name[n] = ' ';
            who->name[n + 1] = 0;
        }
    }
}

/* Exit click uses text id -1 so the frame can see it without a second channel. */
int og_chargen_quit(void)
{
    return g_mode == OG_UI_TITLE && g_text < 0;
}

void og_chargen_resume(void)
{
    g_mode = OG_UI_WORLD;
    g_text = 0;
    g_revision++;
    caption("Privateer's Hold");
}
