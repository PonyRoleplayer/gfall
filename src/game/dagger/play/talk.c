#include "game/dagger/play/talk.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/words.h"
#include "game/dagger/places/town.h"
#include "game/dagger/formats/faces.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/commerce.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See talk.h. */

#define VK_ESCAPE_KEY 0x1B
#define VK_RETURN_KEY 0x0D

/* DFU's DaggerfallTalkWindow's rectangles. */
static const Rect g_tone_rect[RULES_TONES] = { { 258, 18, 6, 6 }, { 258, 28, 6, 6 }, { 258, 38, 6, 6 } };
static const Rect g_tell_rect = { 4, 4, 107, 10 };
static const Rect g_where_rect = { 4, 14, 107, 10 };
static const Rect g_location_rect = { 4, 26, 107, 10 };
static const Rect g_people_rect = { 4, 36, 107, 10 };
static const Rect g_things_rect = { 4, 46, 107, 10 };
static const Rect g_work_rect = { 4, 56, 107, 10 };
static const Rect g_okay_rect = { 4, 186, 107, 10 };
static const Rect g_goodbye_rect = { 118, 183, 67, 10 };
static const Rect g_list_rect = { 6, 71, 94, 104 };
static const Rect g_list_up = { 102, 69, 9, 16 };
static const Rect g_list_down = { 102, 161, 9, 16 };
static const Rect g_says_rect = { 189, 65, 114, 126 };
static const Rect g_says_up = { 303, 64, 9, 16 };
static const Rect g_says_down = { 303, 176, 9, 16 };
static const Rect g_question_rect = { 123, 8, 124, 38 };
static const Rect g_name_rect = { 117, 52, 197, 10 };
#define FACE_X 119
#define FACE_Y 65
/* TALK02I0.IMG, four bands of 107 x 10, Location on top; TALK03I0.IMG, two of 107 x 10, "Tell me about" on top. */
#define STRIP_W 107
#define STRIP_H 10
#define BAND_LOCATION 0
#define BAND_PEOPLE 1
#define BAND_THINGS 2
#define BAND_WORK 3
#define LIT_TELL 0
#define LIT_WHERE 1

#define LIST_ROWS 13
#define SAYS_ROWS 15
/* What can be listed, and the conversation kept: the oldest lines go. */
#define TOPICS 24
#define TOPIC_TEXT 32
#define LINES 96
#define LINE_TEXT 40
#define LONG_TEXT 640

/* Under ART_PAL.COL, the nearest to DFU's colours: the question's pale blue and the tone's red. */
#define INK_QUESTION 96
#define INK_TONE 244

/* TEXT.RSC. */
#define RECORD_NO_RESPONSE 7205
#define RECORD_GREETING_DISLIKE 7206
#define RECORD_QUESTION_WORK 7212
#define RECORD_QUESTION_PLACE 7225
#define RECORD_QUESTION_NEWS 7231
#define RECORD_WORK_WORD 7211
#define RECORD_HINT_WAY 7333
#define RECORD_NO_NEWS 1457
/* Ambient news: the ones spoken in the street, tried until one has only macros the game can say. */
#define RECORD_NEWS_FIRST 1450
#define RECORD_NEWS_LAST 1456
#define NEWS_TRIES 8

/* TalkManager's answersToDirections and answersToNonDirections: 3 x social group + how they take it, 15 on for one who knows. */
#define ANSWERS 30
static const df_u16 g_answers_way[ANSWERS] = {
    7251, 7266, 7281, 7250, 7265, 7280, 7252, 7267, 7282, 7253, 7268, 7283, 7304, 7269, 7284,
    7256, 7271, 7286, 7255, 7270, 7285, 7257, 7272, 7287, 7258, 7273, 7288, 7259, 7274, 7289
};
static const df_u16 g_answers_other[ANSWERS] = {
    7251, 7266, 7281, 7250, 7265, 7280, 7252, 7267, 7282, 7253, 7268, 7283, 7304, 7269, 7284,
    7261, 7276, 7291, 7260, 7275, 7290, 7262, 7277, 7292, 7263, 7278, 7293, 7264, 7279, 7294
};
static const char *const g_compass[8] = {
    "east", "northeast", "north", "northwest", "west", "southwest", "south", "southeast"
};

enum {
    MODE_WHERE,
    MODE_TELL
};

enum {
    TOPIC_PLACE,
    TOPIC_NEWS,
    TOPIC_WORK
};

typedef struct Topic {
    int kind;
    /* A place: its building type, and the nearest of them. */
    int type;
    float x;
    float z;
    int block;
    int record;
    char text[TOPIC_TEXT];
} Topic;

static struct {
    int open;
    Player *player;
    TalkWho who;
    Composer composer;
    const df_u8 *base;
    const df_u8 *grey;
    const df_u8 *lit;
    df_u8 *face;
    const Faction *faction;
    int sgroup;
    int reaction;
    int rejected;
    int tone;
    int tallied;
    int told_news;
    int mode;
    Topic topic[TOPICS];
    int topics;
    int chosen;
    int scroll;
    char lines[LINES][LINE_TEXT];
    df_u8 asked[LINES];
    int nlines;
    int says_scroll;
    char question[LONG_TEXT];
    Words words;
    int changed;
} g;

/* A number from who is asked and what about: the same each time, so a person does not change their answer. */
static int seeded(unsigned key, int n)
{
    unsigned v;

    v = (g.who.seed ^ key) * 1103515245u + 12345u;
    v = v * 1103515245u + 12345u;
    return (int)((v >> 16) % (unsigned)n);
}

/* ---- The conversation. ---- */

/* A finished line goes on the end; a full list loses its oldest. */
static void push_line(const char *text, int asked)
{
    int i;

    if (g.nlines == LINES) {
        for (i = 1; i < LINES; i++) {
            df_copy(g.lines[i - 1], g.lines[i], LINE_TEXT);
            g.asked[i - 1] = g.asked[i];
        }
        g.nlines--;
    }
    df_copy(g.lines[g.nlines], text, LINE_TEXT);
    g.asked[g.nlines] = (df_u8)asked;
    g.nlines++;
}

/* Text cut at spaces to the box's width, and at its own breaks, each as a line. */
static void add_text(const char *text, int asked)
{
    char line[LINE_TEXT];
    int n;
    int last_space;

    canvas_use(&g.composer);
    n = 0;
    last_space = -1;
    while (*text) {
        if (*text == '\n') {
            line[n] = 0;
            if (n) {
                push_line(line, asked);
            }
            n = 0;
            last_space = -1;
            text++;
            continue;
        }
        line[n++] = *text;
        line[n] = 0;
        if (*text == ' ') {
            last_space = n - 1;
        }
        text++;
        if (canvas_width(line) > g_says_rect.w || n >= LINE_TEXT - 1) {
            int back;

            back = last_space > 0 ? last_space : n - 1;
            {
                char keep[LINE_TEXT];
                int k;
                int m;

                m = 0;
                for (k = back + (line[back] == ' ' ? 1 : 0); k < n; k++) {
                    keep[m++] = line[k];
                }
                keep[m] = 0;
                line[back] = 0;
                push_line(line, asked);
                df_copy(line, keep, LINE_TEXT);
                n = m;
                last_space = -1;
            }
        }
    }
    if (n) {
        line[n] = 0;
        push_line(line, asked);
    }
    /* A gap after an answer, and the end in view. */
    if (!asked) {
        push_line("", 0);
    }
    g.says_scroll = g.nlines > SAYS_ROWS ? g.nlines - SAYS_ROWS : 0;
}

/* ---- What is listed. ---- */

/* The nearest building of each kind people ask the way to: no houses and no empty lots. */
static void list_places(void)
{
    const TownBuilding *rows;
    int count;
    int i;

    g.topics = 0;
    if (!g.who.located) {
        return;
    }
    rows = town_buildings(g.who.region, g.who.index, &count);
    for (i = 0; rows && i < count; i++) {
        const BuildingKind *kind;
        float dx;
        float dz;
        int k;

        kind = building_kind(rows[i].type);
        if (!kind || rows[i].type == 1 || rows[i].type == 4 || rows[i].type >= BUILDING_HOUSE1) {
            continue;
        }
        dx = rows[i].x - g.who.x;
        dz = rows[i].z - g.who.z;
        for (k = 0; k < g.topics && g.topic[k].type != rows[i].type; k++) {
        }
        if (k == g.topics) {
            if (g.topics == TOPICS) {
                continue;
            }
            g.topics++;
            g.topic[k].kind = TOPIC_PLACE;
            g.topic[k].type = rows[i].type;
            g.topic[k].x = rows[i].x;
            g.topic[k].z = rows[i].z;
            g.topic[k].block = rows[i].block;
            g.topic[k].record = rows[i].record;
            df_copy(g.topic[k].text, kind->name, TOPIC_TEXT);
        } else {
            float bx;
            float bz;

            bx = g.topic[k].x - g.who.x;
            bz = g.topic[k].z - g.who.z;
            if (dx * dx + dz * dz < bx * bx + bz * bz) {
                g.topic[k].x = rows[i].x;
                g.topic[k].z = rows[i].z;
                g.topic[k].block = rows[i].block;
                g.topic[k].record = rows[i].record;
            }
        }
    }
}

static void list_tell(void)
{
    g.topics = 1;
    g.topic[0].kind = TOPIC_NEWS;
    df_copy(g.topic[0].text, "Any news?", TOPIC_TEXT);
}

/* "The armorer", from "Armorer". */
static void key_of(const Topic *t, char *out, int cap)
{
    int i;

    if (t->kind != TOPIC_PLACE) {
        df_copy(out, t->text, cap);
        return;
    }
    log_format(out, cap, "the %s", t->text);
    for (i = 0; out[i]; i++) {
        if (out[i] >= 'A' && out[i] <= 'Z') {
            out[i] = (char)(out[i] - 'A' + 'a');
        }
    }
}

/* The question for the chosen topic, by the tone, as the player would ask it. */
static void make_question(void)
{
    static char key[TOPIC_TEXT + 8];
    static char work[64];
    const Topic *t;

    g.question[0] = 0;
    g.words.tone = g.tone;
    if (g.mode == MODE_TELL && g.chosen >= 0 && g.chosen < g.topics) {
        words_use(&g.words);
        words_say(RECORD_QUESTION_NEWS + g.tone, g.question, LONG_TEXT);
        return;
    }
    if (g.chosen == TOPICS) {
        words_use(&g.words);
        words_say(RECORD_WORK_WORD, work, (int)sizeof(work));
        g.words.key = work;
        words_say(RECORD_QUESTION_WORK + g.tone, g.question, LONG_TEXT);
        return;
    }
    if (g.chosen < 0 || g.chosen >= g.topics) {
        return;
    }
    t = &g.topic[g.chosen];
    key_of(t, key, (int)sizeof(key));
    g.words.key = key;
    words_use(&g.words);
    words_say(RECORD_QUESTION_PLACE + g.tone, g.question, LONG_TEXT);
}

/* ---- Answers. ---- */

/* How they take a question of this kind, by tone: 0 will not, 1 grudging, 2 glad. Using a tone's skill counts once a talk. */
static int outcome_for(int is_place)
{
    int skill;
    int value;

    skill = 0;
    if (g.tone == RULES_TONE_POLITE) {
        skill = g.player->sheet.skill[SKILL_ETIQUETTE];
    } else if (g.tone == RULES_TONE_BLUNT) {
        skill = g.player->sheet.skill[SKILL_STREETWISE];
    }
    if (g.tone != RULES_TONE_NORMAL && !g.tallied) {
        character_tally(&g.player->sheet, g.tone == RULES_TONE_POLITE ? SKILL_ETIQUETTE : SKILL_STREETWISE, 1);
        g.tallied = 1;
    }
    value = rules_talk_value(g.player->sheet.attribute[ATTR_PERSONALITY], is_place, g.reaction, g.tone, g.sgroup,
                             skill, seeded(1u + (unsigned)g.tone, 100));
    return rules_talk_outcome(value, seeded(7u + (unsigned)g.tone, 21));
}

/* A place's answer, into out. */
static void answer_place(const Topic *t, char *out, int cap)
{
    static char key[TOPIC_TEXT + 8];
    static char hint[256];
    float dx;
    float dz;
    int knows;
    int outcome;
    int here;

    key_of(t, key, (int)sizeof(key));
    dx = t->x - g.who.x;
    dz = t->z - g.who.z;
    here = t->block == g.who.block && t->record == g.who.record;
    g.words.key = key;
    g.words.loc = key;
    g.words.direction = g_compass[rules_compass(dx, dz)];
    words_use(&g.words);
    if (here) {
        df_copy(hint, "this place", (int)sizeof(hint));
    } else {
        words_say(RECORD_HINT_WAY, hint, (int)sizeof(hint));
    }
    g.words.hint = hint;
    /* One inside the place itself knows it. */
    knows = here || rules_talk_knows(g.sgroup, seeded(100u + (unsigned)t->type, 20) + 1);
    outcome = outcome_for(1);
    words_say(g_answers_way[3 * (g.sgroup < SOCIAL_COUNT ? g.sgroup : SOCIAL_MERCHANTS) + outcome + (knows ? 15 : 0)], out, cap);
}

/* The news: one piece a person, from those whose macros the game can say. */
static void answer_news(char *out, int cap)
{
    int i;

    words_use(&g.words);
    if (!g.told_news) {
        for (i = 0; i < NEWS_TRIES; i++) {
            if (words_say_plain(RECORD_NEWS_FIRST + combat_roll(RECORD_NEWS_LAST - RECORD_NEWS_FIRST + 1), out, cap) > 0) {
                g.told_news = 1;
                return;
            }
        }
    }
    words_say(RECORD_NO_NEWS, out, cap);
}

/* Asks what is chosen: the question and the answer go in the conversation. */
static void ask(void)
{
    char answer[LONG_TEXT];
    int row;

    if (g.chosen == TOPICS) {
        make_question();
    } else if (g.chosen < 0 || g.chosen >= g.topics) {
        return;
    }
    add_text(g.question, 1);
    words_use(&g.words);
    if (g.rejected) {
        words_say(RECORD_NO_RESPONSE, answer, (int)sizeof(answer));
    } else if (g.chosen == TOPICS) {
        /* No one has work to give yet (quests, P10): what they say of what they do not know. */
        row = 3 * (g.sgroup < SOCIAL_COUNT ? g.sgroup : SOCIAL_MERCHANTS) + outcome_for(0);
        words_say(g_answers_other[row], answer, (int)sizeof(answer));
    } else if (g.topic[g.chosen].kind == TOPIC_NEWS) {
        answer_news(answer, (int)sizeof(answer));
    } else {
        answer_place(&g.topic[g.chosen], answer, (int)sizeof(answer));
    }
    add_text(answer, 0);
}

/* ---- Open and close. ---- */

int talk_open(Player *p, const TalkWho *who)
{
    MemHeap *was;
    df_u32 size;
    char greeting[LONG_TEXT];
    int tier;

    talk_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.player = p;
    g.who = *who;
    g.base = composer_table(&g.composer, "TALK01I0.IMG", &size);
    g.grey = composer_table(&g.composer, "TALK02I0.IMG", &size);
    g.lit = composer_table(&g.composer, "TALK03I0.IMG", &size);
    was = mem_heap_use(g.composer.heap);
    g.face = (df_u8 *)mem_alloc(FACE_W * FACE_H);
    mem_heap_use(was);
    if (!g.base || !g.grey || !g.lit || !g.face || !faces_read(who->face, g.face)) {
        log_text(LOG_WARN, "talk: its pictures did not read");
        talk_close();
        return 0;
    }
    g.faction = faction_find(who->faction);
    g.sgroup = g.faction ? g.faction->sgroup : SOCIAL_COMMONERS;
    g.reaction = standing_reaction(who->faction);
    /* DFU's: past this they will not answer at all. */
    g.rejected = g.reaction < -20;
    g.tone = RULES_TONE_NORMAL;
    g.mode = MODE_WHERE;
    g.chosen = -1;
    g.words.pc = &p->sheet;
    g.words.city = who->city;
    g.words.region = who->region_name;
    g.words.faction = g.faction ? g.faction->name : "";
    g.words.reaction = g.reaction;
    g.words.tone = g.tone;
    list_places();
    /* The greeting by how they like the player: 7206 dislike, on up to very like. */
    tier = g.reaction >= 30 ? 3 : (g.reaction >= 10 ? 2 : (g.reaction >= 0 ? 1 : 0));
    words_use(&g.words);
    if (g.rejected) {
        words_say(RECORD_NO_RESPONSE, greeting, (int)sizeof(greeting));
    } else {
        words_say(RECORD_GREETING_DISLIKE + tier, greeting, (int)sizeof(greeting));
    }
    add_text(greeting, 0);
    g.changed = 1;
    return 1;
}

void talk_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

/* ---- Composing. ---- */

/* One band of a strip file over a button. */
static void band(const df_u8 *from, int which, const Rect *r)
{
    canvas_copy_part(from, STRIP_W, 0, which * STRIP_H, STRIP_W, STRIP_H, r->x, r->y);
}

void talk_compose(void)
{
    int i;

    canvas_use(&g.composer);
    canvas_copy_all(g.base);
    canvas_copy_part(g.face, FACE_W, 0, 0, FACE_W, FACE_H, FACE_X, FACE_Y);
    canvas_box(&g_tone_rect[g.tone], INK_TONE);
    /* The way of asking lit; what is not there greyed. */
    if (g.mode == MODE_WHERE) {
        band(g.lit, LIT_WHERE, &g_where_rect);
    } else {
        band(g.lit, LIT_TELL, &g_tell_rect);
        band(g.grey, BAND_LOCATION, &g_location_rect);
        band(g.grey, BAND_WORK, &g_work_rect);
    }
    band(g.grey, BAND_PEOPLE, &g_people_rect);
    band(g.grey, BAND_THINGS, &g_things_rect);
    if (g.faction) {
        canvas_label_centred(&g_name_rect, 1, g.faction->name);
    }
    for (i = 0; i < LIST_ROWS; i++) {
        int k;

        k = g.scroll + i;
        if (k >= g.topics) {
            break;
        }
        canvas_set_ink(k == g.chosen ? CANVAS_DARK : CANVAS_INK, k == g.chosen ? -1 : CANVAS_INK_SHADOW);
        if (k == g.chosen) {
            Rect bar;

            bar.x = g_list_rect.x;
            bar.y = (df_u16)(g_list_rect.y + i * CANVAS_LINE);
            bar.w = g_list_rect.w;
            bar.h = CANVAS_LINE;
            canvas_box(&bar, CANVAS_INK);
        }
        canvas_label(g_list_rect.x + 1, g_list_rect.y + i * CANVAS_LINE, g.topic[k].text);
    }
    canvas_set_ink(INK_QUESTION, CANVAS_INK_SHADOW);
    canvas_text(&g_question_rect, g.question);
    for (i = 0; i < SAYS_ROWS; i++) {
        int k;

        k = g.says_scroll + i;
        if (k >= g.nlines) {
            break;
        }
        canvas_set_ink(g.asked[k] ? INK_QUESTION : CANVAS_INK, CANVAS_INK_SHADOW);
        canvas_label(g_says_rect.x, g_says_rect.y + i * CANVAS_LINE, g.lines[k]);
    }
    canvas_set_ink(CANVAS_INK, CANVAS_INK_SHADOW);
    g.changed = 0;
}

const unsigned char *talk_pixels(void)
{
    return g.composer.screen;
}

int talk_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

/* ---- Clicks and keys. ---- */

/* The way of asking changed: its list, nothing chosen. */
static void set_mode(int mode)
{
    g.mode = mode;
    g.scroll = 0;
    g.chosen = -1;
    g.question[0] = 0;
    if (mode == MODE_WHERE) {
        list_places();
    } else {
        list_tell();
    }
}

int talk_click(int x, int y)
{
    int i;

    g.changed = 1;
    if (rect_has(&g_goodbye_rect, x, y)) {
        return TALK_LEAVE;
    }
    for (i = 0; i < RULES_TONES; i++) {
        if (rect_has(&g_tone_rect[i], x, y)) {
            g.tone = i;
            make_question();
            return TALK_STAY;
        }
    }
    if (rect_has(&g_tell_rect, x, y)) {
        set_mode(MODE_TELL);
    } else if (rect_has(&g_where_rect, x, y) || (g.mode == MODE_WHERE && rect_has(&g_location_rect, x, y))) {
        set_mode(MODE_WHERE);
    } else if (g.mode == MODE_WHERE && rect_has(&g_work_rect, x, y)) {
        /* Work has no list: the question is asked at once on Okay. */
        g.chosen = TOPICS;
        make_question();
    } else if (rect_has(&g_okay_rect, x, y)) {
        ask();
    } else if (rect_has(&g_list_up, x, y)) {
        g.scroll = g.scroll > 0 ? g.scroll - 1 : 0;
    } else if (rect_has(&g_list_down, x, y)) {
        g.scroll = g.scroll + LIST_ROWS < g.topics ? g.scroll + 1 : g.scroll;
    } else if (rect_has(&g_says_up, x, y)) {
        g.says_scroll = g.says_scroll > 0 ? g.says_scroll - 1 : 0;
    } else if (rect_has(&g_says_down, x, y)) {
        g.says_scroll = g.says_scroll + SAYS_ROWS < g.nlines ? g.says_scroll + 1 : g.says_scroll;
    } else if (rect_has(&g_list_rect, x, y)) {
        i = g.scroll + (y - g_list_rect.y) / CANVAS_LINE;
        if (i < g.topics) {
            /* A second click on what is chosen asks it. */
            if (i == g.chosen) {
                ask();
            } else {
                g.chosen = i;
                make_question();
            }
        }
    }
    return TALK_STAY;
}

int talk_key(int vk)
{
    g.changed = 1;
    if (vk == VK_ESCAPE_KEY) {
        return TALK_LEAVE;
    }
    if (vk == VK_RETURN_KEY) {
        ask();
    }
    return TALK_STAY;
}

int talk_lines(void)
{
    return g.nlines;
}

const char *talk_line(int i)
{
    return i >= 0 && i < g.nlines ? g.lines[i] : "";
}

int talk_topics(void)
{
    return g.topics;
}

const char *talk_topic(int i)
{
    return i >= 0 && i < g.topics ? g.topic[i].text : "";
}
