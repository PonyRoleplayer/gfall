#include "game/dagger/play/talkwin.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/play.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/talkdata.h"

/* See talkwin.h. */

static OgTalkWin g;
static OgTalkWinWorld g_world;
static char g_key[65];

/* talk_buttons (FALL.EXE 0x179B78): x0, y0, x1, y1. */
static const short g_button[OG_TB_COUNT][4] = {
    {4, 4, 110, 13}, {4, 14, 110, 23}, {4, 26, 110, 35}, {4, 36, 110, 45}, {4, 46, 110, 55}, {4, 56, 110, 65},
    {258, 18, 263, 23}, {258, 28, 263, 33}, {258, 38, 263, 43}, {118, 158, 184, 175}, {118, 183, 184, 192},
    {102, 69, 110, 84}, {102, 161, 110, 176}, {4, 177, 19, 185}, {86, 177, 101, 185}, {7, 71, 101, 176},
    {303, 64, 311, 79}, {303, 176, 311, 191}, {4, 186, 110, 195}
};

void og_talkwin_world(const OgTalkWinWorld *world)
{
    g_world = *world;
}

const OgTalkWin *og_talkwin(void)
{
    return &g;
}

const short *og_talkwin_button(int button)
{
    return g_button[button];
}

const char *og_talkwin_key_text(void)
{
    return g_key;
}

static int text_width(const char *text)
{
    int n;

    if (g_world.width) {
        return g_world.width(text);
    }
    for (n = 0; text[n]; n++) { }
    return n * 5;
}

static void copy(char *to, int cap, const char *from)
{
    int i;

    for (i = 0; from[i] && i < cap - 1; i++) {
        to[i] = from[i];
    }
    to[i] = 0;
}

/* Say a record with the window's macros; the raw record when the game cannot say it. */
static void say(int text, const OgTalkWinSay *ctx, char *out, int cap)
{
    out[0] = 0;
    if (!text) {
        return;
    }
    if (g_world.say && g_world.say(text, ctx, out, cap) >= 0) {
        return;
    }
    copy(out, cap, og_words(text));
}

static void context(OgTalkWinSay *ctx)
{
    mem_zero(ctx, sizeof(*ctx));
    ctx->key = g_key;
    ctx->tone = g.tone;
}

/* ---- Lines: talk_wrap_text. Words and single spaces are the units; a line holds while its width stays under the box's. ---- */

static void push_answer(const char *text, int asked)
{
    int i;

    if (g.answer_count >= OG_TW_LINES) {
        for (i = 1; i < OG_TW_LINES; i++) {
            copy(g.answer[i - 1], OG_TW_LINE, g.answer[i]);
            g.asked[i - 1] = g.asked[i];
        }
        g.answer_count--;
    }
    copy(g.answer[g.answer_count], OG_TW_LINE, text);
    g.asked[g.answer_count++] = (unsigned char)asked;
}

static void push_question(const char *text)
{
    if (g.question_count < OG_TW_QUESTION) {
        copy(g.question[g.question_count++], OG_TW_LINE, text);
    }
}

/* Cut text into lines under limit pixels; each to add(). */
static void wrap(const char *text, int limit, void (*add)(const char *, int), int flag)
{
    char line[OG_TW_LINE], token[OG_TW_LINE];
    int used, len, t;

    line[0] = 0;
    used = 0;
    len = 0;
    while (*text) {
        t = 0;
        if (*text > ' ') {
            while (*text > ' ' && t < OG_TW_LINE - 1) {
                token[t++] = *text++;
            }
        } else {
            token[t++] = *text++;
        }
        token[t] = 0;
        if (used + text_width(token) < limit && len + t < OG_TW_LINE - 1) {
            copy(line + len, OG_TW_LINE - len, token);
            len += t;
            used += text_width(token);
        } else {
            add(line, flag);
            copy(line, OG_TW_LINE, token);
            len = t;
            used = text_width(token);
        }
    }
    add(line, flag);
}

static void answer_line(const char *text, int flag)
{
    push_answer(text, flag);
}

static void question_line(const char *text, int flag)
{
    (void)flag;
    push_question(text);
}

/* talk_say_text and talk_say_string: a blank line between sayings, then the words (190..302), the log at its end. */
static void say_lines(const char *text)
{
    if (g.answer_count) {
        push_answer("", 0);
    }
    wrap(text, 112, answer_line, 0);
    g.answer_scroll = g.answer_count - OG_TW_ANSWER_ROWS;
    if (g.answer_scroll < 0) {
        g.answer_scroll = 0;
    }
}

/* talk_log_question: a blank line and the question's lines, marked as asked, into the log; the question box empties. */
static void log_question(void)
{
    int i;

    push_answer("", 0);
    for (i = 0; i < g.question_count; i++) {
        push_answer(g.question[i], 1);
    }
    g.question_count = 0;
}

/* ---- The list under the tabs, rebuilt whenever what it depends on changes (the original builds it as it draws). ---- */

static void add_item(const char *text, int value)
{
    if (g.count < OG_TW_ITEMS) {
        copy(g.item[g.count].text, OG_TW_ITEM_TEXT, text);
        g.item[g.count++].value = value;
    }
}

/* talk_draw_regional_list: the places elsewhere asked after, the orders only in their own region. */
static void regional_items(void)
{
    char name[OG_TW_ITEM_TEXT];
    int i, has, key_type;

    for (i = 0; i < 28; i++) {
        if (i >= 8 && i <= 17) {
            if (g_world.region != g_regional_region[i - 8]) {
                continue;
            }
            key_type = 0;
        } else {
            key_type = i >= 20;
        }
        has = g_world.town_has ? g_world.town_has(g_regional_id[i], key_type) : 0;
        if (!has) {
            int n, k;

            for (n = 0; g_any_format[n] && g_any_format[n] != '%'; n++) {
                name[n] = g_any_format[n];
            }
            for (k = 0; g_regional_name[i][k] && n < OG_TW_ITEM_TEXT - 1; k++) {
                name[n++] = g_regional_name[i][k];
            }
            name[n] = 0;
            add_item(name, i);
        }
    }
}

static void rebuild(void)
{
    char name[OG_TW_ITEM_TEXT];
    int i, category;

    g.count = 0;
    if (g.mode == 0) {
        /* talk_draw_tell_list: the news, then the organisations (the quest's topics would come between). */
        add_item(g_any_news, -1);
        /* The quests' topics come between the news and the organisations. */
        for (i = 0; g_world.topic_count && g_world.topic_name && i < g_world.topic_count() && g.count < OG_TW_ITEMS - 34; i++) {
            if (g_world.topic_name(i, name, (int)sizeof(name)) && name[0]) { add_item(name, 1000 + i); }
        }
        for (i = 0; i < 34; i++) {
            const OgFactionDef *f;

            f = og_faction_def(og_faction_index(g_organisation[i]));
            add_item(f ? f->name : "", i);
        }
        return;
    }
    if (g.tab == 0) {
        if (g.categories) {
            for (category = 0; category < OG_TALK_CATEGORIES; category++) {
                if (og_talk_has_category(g.building, g.building_count, category)) {
                    add_item(og_talk_category_name(category), category);
                }
            }
            return;
        }
        add_item(g_previous_list, -1);
        if (g.category == OG_TALK_REGIONAL) {
            regional_items();
        } else {
            for (i = 0; i < g.topic_count; i++) {
                if (g.topic[i].category == g.category) {
                    if (g_world.building_name) {
                        g_world.building_name(g.topic[i].building, name, (int)sizeof(name));
                    } else {
                        name[0] = 0;
                    }
                    add_item(name, i);
                }
            }
        }
    }
}

/* What the selected row calls itself: the question's %key. */
static void select_key(void)
{
    int i;

    g_key[0] = 0;
    if (g.mode == 1 && g.tab == 0 && g.categories) {
        return;
    }
    if (g.mode == 1 && g.tab == 0) {
        /* The category list's row is the category; a place list's the row itself. */
        for (i = 0; i < g.count; i++) {
            if (i == g.row && g.item[i].value >= 0) {
                copy(g_key, (int)sizeof(g_key), g.item[i].text);
            }
        }
        return;
    }
    if (g.mode == 0 && g.row >= 0 && g.row < g.count) {
        copy(g_key, (int)sizeof(g_key), g.item[g.row].text);
    }
}

/* talk_draw: the question in the box above the list, by the tone, the way of asking and the tab. */
static void make_question(void)
{
    OgTalkWinSay ctx;
    char text[240];
    int base, news_row;

    g.question_count = 0;
    select_key();
    context(&ctx);
    if (g.tab == 3) {
        say(7211, &ctx, g_key, (int)sizeof(g_key));
        base = 7212;
    } else if (g.mode == 1) {
        if (g.categories && g.tab == 0) {
            return;
        }
        if (g.count == 0) {
            return;
        }
        base = 7225;
    } else {
        news_row = g.row == 0;
        base = news_row ? 7231 : 7212;
    }
    context(&ctx);
    say(base + g.tone, &ctx, text, (int)sizeof(text));
    wrap(text, 111, question_line, 0);
}

static void refresh(void)
{
    rebuild();
    make_question();
    g.revision++;
}

/* ---- Opening ---- */

int og_talkwin_open(const OgTalkWho *who, const OgTalkPlayer *player, int witnessed, int *text)
{
    OgTalk talk;
    int r, n;
    char greeting[400];
    OgTalkWinSay ctx;

    *text = 0;
    if (!og_talk_start(&talk, who, player, text)) {
        return 0;
    }
    r = og_talk_open(&talk, player, witnessed, text);
    if (r == OG_TALK_REFUSED || r == OG_TALK_HOSTILE) {
        return 0;
    }
    mem_zero(&g, sizeof(g));
    g.open = 1;
    g.talk = talk;
    g.player = *player;
    g.who = *who;
    g.tone = OG_TONE_NORMAL;
    g.mode = 1;
    g.tab = 0;
    g.categories = 1;
    g.row = 0;
    g_key[0] = 0;
    /* talk_open: the town's buildings (their distances taken now), the topics made of them, the quest sites a category of their own (none yet). */
    n = g_world.buildings ? g_world.buildings(g.building, OG_TALK_TOPICS) : 0;
    g.building_count = n;
    g.topic_count = og_talk_topics(g.topic, OG_TALK_TOPICS, g.building, n);
    mem_zero(&ctx, sizeof(ctx));
    ctx.tone = g.tone;
    /* The person who gave a quest greets with the rumour of it, when there is one. */
    if (g_world.rumour_for && g_world.rumour_for(who->npc, greeting, (int)sizeof(greeting)) && greeting[0]) { say_lines(greeting); refresh(); return 1; }
    say(*text, &ctx, greeting, (int)sizeof(greeting));
    say_lines(greeting);
    refresh();
    return 1;
}

void og_talkwin_close(void)
{
    g.open = 0;
    g.revision++;
}

/* ---- Buttons ---- */

static int inside(int button, int x, int y)
{
    return x > g_button[button][0] && x < g_button[button][2] && y > g_button[button][1] && y < g_button[button][3];
}

static void list_scroll(int delta)
{
    int top;

    if (g.mode == 1 && g.tab == 0 && g.categories) {
        return;
    }
    top = g.top + delta;
    if (delta < 0 && g.top == 0) {
        return;
    }
    if (delta > 0 && g.top + OG_TW_LIST_ROWS - 1 >= g.count) {
        return;
    }
    g.top = top;
    g.revision++;
}

/* talk_list_click(100): the category picked, its places listed with the first of them selected. */
static void pick_category(void)
{
    g.category = g.row;
    g.categories = 0;
    g.row = 1;
    g.top = 0;
    refresh();
}

/* talk_list_click: a click in the list, or Enter on the category. */
static void list_click(int y)
{
    int row, i;

    row = g.top + (y - 71) / 7;
    if (row >= g.count) {
        return;
    }
    if (g.mode == 1 && g.tab == 0) {
        if (g.categories) {
            g.row = g.item[row].value;
            (void)i;
        } else if (row == 0) {
            /* The first row is the way back to the categories (a double click in the original, Enter or this click here). */
            g.categories = 1;
            g.row = 0;
            g.top = 0;
        } else {
            g.row = row;
        }
    } else {
        g.row = row;
    }
    refresh();
}

/* The answer: talk_button_okay, after the question has been logged. */
static void ask(void)
{
    OgTalkAsk a;
    OgTalkWinSay ctx;
    char hint[400], text[600];
    int answer, id, noted, regional, found;
    int topic_text;

    if (g.mode == 1 && g.tab == 0 && g.categories) {
        /* talk_list_click(100): whatever row is selected becomes the category, listed or not. */
        pick_category();
        return;
    }
    if (g.count == 0 && g.tab != 3) {
        return;
    }
    select_key();
    log_question();
    mem_zero(&a, sizeof(a));
    a.mode = g.mode;
    a.tab = g.tab;
    a.category = g.category;
    a.row = g.row;
    a.news = 0;
    a.no_work = g_world.no_work;
    /* rumor_pick_news: the person's one piece of news, if the region has any. */
    /* A quest's topic answers in its own words. */
    if (g.mode == 0 && g.row >= 0 && g.row < g.count && g.item[g.row].value >= 1000 && g_world.topic_answer) {
        if (g_world.topic_answer(g.item[g.row].value - 1000, text, (int)sizeof(text))) { say_lines(text); }
        refresh();
        return;
    }
    if (g.mode == 0 && g.row == 0 && g_world.news && g_world.news(g.talk.faction, text, (int)sizeof(text))) {
        a.news = 1;
    }
    context(&ctx);
    answer = og_talk_answer(&g.talk, &a, &id);
    if (answer == OG_ANSWER_NEWS) {
        say_lines(text);
        refresh();
        return;
    }
    hint[0] = 0;
    if (g.tab != 3 && id != 1457 && id != 28) {
        if (g.mode == 1) {
            unsigned building;
            char place[OG_TW_ITEM_TEXT];

            building = 0;
            regional = g.category == OG_TALK_REGIONAL && g.tab == 0;
            place[0] = 0;
            found = 0;
            if (regional) {
                int index;

                index = g.row >= 0 && g.row < g.count ? g.item[g.row].value : -1;
                found = index >= 0 && g_world.regional_found ? g_world.regional_found(index, place, (int)sizeof(place)) : 0;
            } else if (g.row >= 0 && g.row < g.count && g.item[g.row].value >= 0) {
                building = g.topic[g.item[g.row].value].building;
                copy(place, (int)sizeof(place), g.item[g.row].text);
            }
            topic_text = og_talk_hint(&a, found, building && g_world.distance ? g_world.distance(building) : 99999, g_world.outdoors, &noted);
            context(&ctx);
            ctx.loc = place;
            ctx.building = building;
            say(topic_text, &ctx, hint, (int)sizeof(hint));
        } else if (g.row > 0 && g.row < g.count && g.item[g.row].value < 1000) {
            /* A topic's own record: organisation i is 860 + i, one on from 868 (talk_draw_tell_list). */
            topic_text = g.item[g.row].value > 7 ? g.item[g.row].value + 861 : g.item[g.row].value + 860;
            context(&ctx);
            say(topic_text, &ctx, hint, (int)sizeof(hint));
        }
    }
    context(&ctx);
    ctx.hint = hint;
    ctx.building = 0;
    say(id, &ctx, text, (int)sizeof(text));
    say_lines(text);
    refresh();
}

static void set_mode(int mode)
{
    g.mode = mode;
    g.top = 0;
    g.scroll_x = 0;
    g.row = mode == 1 ? 1 : 0;
    refresh();
}

static void set_tab(int tab)
{
    g.scroll_x = 0;
    g.tab = tab;
    if (tab == 0) {
        g.row = 1;
    }
    refresh();
}

int og_talkwin_click(int x, int y)
{
    int b;

    if (!g.open) {
        return 0;
    }
    for (b = 0; b < OG_TB_COUNT; b++) {
        if (!inside(b, x, y)) {
            continue;
        }
        switch (b) {
        case OG_TB_TELL: set_mode(0); break;
        case OG_TB_WHERE: set_mode(1); break;
        case OG_TB_PLACES: set_tab(0); break;
        case OG_TB_PEOPLE: set_tab(1); break;
        case OG_TB_THINGS: set_tab(2); break;
        case OG_TB_WORK: set_tab(3); break;
        case OG_TB_POLITE: case OG_TB_NORMAL: case OG_TB_BLUNT:
            og_talk_tone(&g.talk, b - OG_TB_POLITE);
            g.tone = g.talk.tone;
            make_question();
            g.revision++;
            break;
        case OG_TB_COPY: break;
        case OG_TB_GOODBYE: og_talkwin_close(); break;
        case OG_TB_LIST_UP: list_scroll(-1); break;
        case OG_TB_LIST_DOWN: list_scroll(1); break;
        case OG_TB_LIST_LEFT: if (g.scroll_x < 0) { g.scroll_x += 4; g.revision++; } break;
        case OG_TB_LIST_RIGHT: g.scroll_x -= 4; g.revision++; break;
        case OG_TB_LIST: list_click(y); break;
        case OG_TB_ANSWER_UP: if (g.answer_scroll) { g.answer_scroll--; g.revision++; } break;
        case OG_TB_ANSWER_DOWN: if (g.answer_count - OG_TW_ANSWER_ROWS > g.answer_scroll) { g.answer_scroll++; g.revision++; } break;
        case OG_TB_OKAY: ask(); break;
        default: break;
        }
        return g.open;
    }
    return g.open;
}

int og_talkwin_key(int key)
{
    int i;

    if (!g.open) {
        return 0;
    }
    switch (key) {
    case OG_TW_KEY_ENTER: ask(); break;
    case OG_TW_KEY_UP: list_scroll(-1); break;
    case OG_TW_KEY_DOWN: list_scroll(1); break;
    case OG_TW_KEY_PGUP: for (i = 0; i < 10; i++) { list_scroll(-1); } break;
    case OG_TW_KEY_PGDN: for (i = 0; i < 10; i++) { list_scroll(1); } break;
    case OG_TW_KEY_ESC:
        /* talk_update: Escape on the categories (or any other tab) leaves; it also goes back to the categories. */
        if ((g.tab == 0 && g.categories) || g.tab != 0) {
            og_talkwin_close();
            break;
        }
        g.categories = 1;
        g.top = 0;
        g.row = 0;
        refresh();
        break;
    default: break;
    }
    return g.open;
}
