#include <string.h>
#include <stdio.h>
#include "check_ogtalkwin.h"
#include "check_util.h"
#include "game/dagger/play/talkwin.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/play.h"

/* A town of a bank (type 3), two taverns (15), an armorer (2) and a house (1), at known distances from the player. */
static int town(OgTalkBuilding *out, int cap)
{
    static const int type[5] = { 3, 15, 15, 2, 1 };
    static const int dist[5] = { 900, 2500, 300, 600, 10 };
    int i;

    (void)cap;
    for (i = 0; i < 5; i++) {
        out[i].id = 100u + (unsigned)i;
        out[i].type = type[i];
        out[i].flags = 0;
        out[i].distance = dist[i];
    }
    return 5;
}

static void name(unsigned id, char *out, int cap)
{
    snprintf(out, (size_t)cap, "Place %u", id);
}

static int has(int key, int by_type)
{
    (void)key; (void)by_type;
    return 0;
}

static int regional(int index, char *out, int cap)
{
    (void)index;
    snprintf(out, (size_t)cap, "Elsewhere");
    return 1;
}

static int distance(unsigned id)
{
    return id == 100u ? 900 : 5000;
}

static int said_news;
static int news(int faction, char *out, int cap)
{
    (void)faction;
    snprintf(out, (size_t)cap, "A dragon was seen.");
    return said_news;
}

/* The record's raw words, with the hint and key put where the macros are, so a test can see them. */
static int say(int text, const OgTalkWinSay *ctx, char *out, int cap)
{
    snprintf(out, (size_t)cap, "[%d|%s|%s|%s]", text, ctx->key ? ctx->key : "", ctx->loc ? ctx->loc : "", ctx->hint ? ctx->hint : "");
    return (int)strlen(out);
}

static int width(const char *t)
{
    return (int)strlen(t) * 5;
}

static int click_in(int button)
{
    const short *b;

    b = og_talkwin_button(button);
    return og_talkwin_click(b[0] + 2, b[1] + 2);
}

/* A click on the list's row r (rows are seven pixels from 71). */
static int row_click(int r)
{
    return og_talkwin_click(20, 71 + r * 7 + 2);
}

void check_ogtalkwin(void)
{
    static OgTalkWinWorld world;
    OgTalkWho who;
    OgTalkPlayer player;
    static const int none[1] = { 0 };
    const OgTalkWin *w;
    int text, i, found, person, group;

    og_standing_defaults(og_standing());
    for (i = 0; i < 5; i++) { og_character()->reputation[i] = 0; }
    world.buildings = town; world.building_name = name; world.town_has = has; world.regional_found = regional;
    world.distance = distance; world.news = news; world.say = say; world.width = width;
    world.outdoors = 1; world.region = 17; world.no_work = 0;
    og_talkwin_world(&world);
    player.members = none; player.member_count = 0; player.temple_rank = 0;

    /* Any individual under a group (type 4) in the real table. */
    person = 0;
    for (i = 0; i < og_faction_count() && !person; i++) {
        const OgFactionDef *d;

        d = og_faction_def(i);
        if (d->type == 15 && d->parent) { person = d->id; group = d->parent; }
    }
    (void)group;
    check_expect(person != 0, "talkwin: no regional person in FACTION.TXT");
    if (!person) { return; }
    who.npc = 555u; who.faction = person; who.person = 1; who.blocked = 0;

    check_expect(og_talkwin_open(&who, &player, 0, &text) && og_talkwin()->open, "talkwin: a person would not talk");
    w = og_talkwin();
    check_expect(w->categories && w->mode == 1 && w->tone == OG_TONE_NORMAL && w->answer_count > 0, "talkwin: the window did not open on the categories with a greeting");
    /* Where-is lists the categories the town has (bank 2, armorer 1, tavern 12, and "General" is for none here) and Regional. */
    check_expect(w->count == 4 && !strcmp(w->item[0].text, "Armorers") && !strcmp(w->item[1].text, "Banks") &&
                 !strcmp(w->item[2].text, "Taverns") && !strcmp(w->item[3].text, "Regional"), "talkwin: the categories are not the town's");

    /* Pick Taverns: the first row is the way back, then the nearest first. */
    row_click(2);
    check_expect(w->row == 12, "talkwin: a category click did not select its number");
    click_in(OG_TB_OKAY);
    check_expect(!w->categories && w->category == 12 && w->count == 3 && !strcmp(w->item[0].text, "Previous List") &&
                 !strcmp(w->item[1].text, "Place 102") && !strcmp(w->item[2].text, "Place 101") && w->row == 1, "talkwin: the tavern list is not nearest first");
    check_expect(w->question_count > 0 && strstr(w->question[0], "[7226|Place 102") != 0, "talkwin: the question for the nearest tavern is not 7226 (normal tone) with its name");

    /* Tones change the question's record. */
    click_in(OG_TB_BLUNT);
    check_expect(w->tone == OG_TONE_BLUNT && strstr(w->question[0], "[7227|") != 0, "talkwin: a blunt tone did not change the question");
    click_in(OG_TB_NORMAL);

    /* Ask: the question is logged marked asked, and an answer follows with the hint said; the hint for a far place is 7333. */
    i = w->answer_count;
    click_in(OG_TB_OKAY);
    check_expect(w->answer_count > i + 2, "talkwin: asking did not log and answer");
    found = 0;
    for (i = 0; i < w->answer_count; i++) { if (w->asked[i]) { found = 1; } }
    check_expect(found, "talkwin: the question was not marked asked in the log");
    found = 0;
    for (i = 0; i < w->answer_count; i++) { if (strstr(w->answer[i], "|[7333|") || strstr(w->answer[i], "|[7332|")) { found = 1; } }
    check_expect(found, "talkwin: a far tavern's answer carries neither directions (7332) nor the vague one (7333)");

    /* Back to the categories by the first row; Escape on them leaves. */
    row_click(0);
    check_expect(w->categories, "talkwin: Previous List did not go back");
    check_expect(!og_talkwin_key(OG_TW_KEY_ESC) && !og_talkwin()->open, "talkwin: Escape on the categories did not leave");

    /* Tell me about: news row first, then the organisations; the news once, then 1457 (no roll). */
    check_expect(og_talkwin_open(&who, &player, 0, &text), "talkwin: a person would not talk again");
    said_news = 1;
    click_in(OG_TB_TELL);
    check_expect(w->mode == 0 && w->count == 35 && !strcmp(w->item[0].text, "Any news?") && w->row == 0, "talkwin: tell me about does not list the news and 34 organisations");
    check_expect(strstr(w->question[0], "[7232|") != 0, "talkwin: the news question is not 7232 at a normal tone");
    click_in(OG_TB_OKAY);
    check_expect(strstr(w->answer[w->answer_count - 1], "dragon") != 0, "talkwin: the news was not said");
    click_in(OG_TB_OKAY);
    check_expect(strstr(w->answer[w->answer_count - 1], "1457") != 0, "talkwin: the news was told twice");

    /* Work: a question of its own, answered by 8078 when there is none; asked after the news it is still 1457 (the news check comes first). */
    click_in(OG_TB_WORK);
    click_in(OG_TB_OKAY);
    check_expect(strstr(w->answer[w->answer_count - 1], "1457") != 0, "talkwin: work after the news was not 1457");
    og_talkwin_close();
    check_expect(og_talkwin_open(&who, &player, 0, &text), "talkwin: a person would not talk a third time");
    world.no_work = 1;
    said_news = 0;
    og_talkwin_world(&world);
    click_in(OG_TB_TELL);
    click_in(OG_TB_WORK);
    click_in(OG_TB_OKAY);
    check_expect(strstr(w->answer[w->answer_count - 1], "8078") != 0, "talkwin: no work was not 8078");
    og_talkwin_close();
}
