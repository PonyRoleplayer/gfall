#ifndef OGFALL_TALKWIN_H
#define OGFALL_TALKWIN_H

#include "game/ogfall/play/talk.h"

/*
 * The talk window's behaviour: what talk_update, talk_draw and the 19 buttons of talk_buttons do (src/lifted/talk.c
 * 228..720, 805..1300, src/hand/func_00014AE8.c talk_open, func_0001889E.c talk_draw_tell_list), without drawing.
 * Slice 6 of people and law (docs/ogfall-people.md), over slice 6's rules (talk.c). The seam paints the window
 * from this state on TALK01I0.IMG and answers the clicks here. Everything the window knows of the world comes
 * through OgTalkWinWorld, so it runs on the host with a stand-in.
 *
 * The window is one conversation: the tone (polite, normal, blunt), the way of asking (tell me about, where
 * is), the tab (places, people, things, work), the list under them and the question it makes, OKAY to ask,
 * and the log of what was asked and answered. Clicks are strictly inside a button's rectangle, as the original's.
 * The quest topics are in "Tell me about" (value 1000 and up); the quests' places and people in Where is wait for them to stand in a town (the lists stay empty). the copy to the logbook, the news
 * (the original reads rumours its region events write), horizontal list scroll is kept.
 */

#define OG_TW_ITEMS 96
#define OG_TW_ITEM_TEXT 40
#define OG_TW_LINES 160
#define OG_TW_LINE 100
#define OG_TW_QUESTION 8
/* Rows of the list shown at once (talk_list_bottom = top + 13), and of the answer. */
#define OG_TW_LIST_ROWS 14
#define OG_TW_ANSWER_ROWS 18

typedef struct OgTalkWinSay {
    /* What is asked about (%key) and the place named (%loc), the tone, and the hint already said (%hnt). */
    const char *key, *loc, *hint;
    int tone;
    /* The building a where-is is about (0 for a regional question), so the seam can say which way it lies. */
    unsigned building;
} OgTalkWinSay;

typedef struct OgTalkWinWorld {
    /* The town's buildings, each with its distance from the player now (building_distance); how many, 0 away from a town. */
    int (*buildings)(OgTalkBuilding *out, int cap);
    /* The sign over a door (building_name). */
    void (*building_name)(unsigned id, char *out, int cap);
    /* town_has_building: a building of this faction (by_type 0) or type (1) stands in this town. */
    int (*town_has)(int key, int by_type);
    /* talk_find_regional: a place elsewhere in the region with the service; its name in out. */
    int (*regional_found)(int index, char *out, int cap);
    /* How far the building is from the player now, in the game's units. */
    int (*distance)(unsigned building);
    /* rumor_pick_news: one piece of news for the faction; 0 when none. */
    int (*news)(int faction, char *out, int cap);
    /* The quests' topics of "Tell me about" (a name and its answer, by number), and the rumour the person who gave a quest tells in their greeting (rumor_find_for_npc); 0 for none. */
    int (*topic_count)(void);
    int (*topic_name)(int i, char *out, int cap);
    int (*topic_answer)(int i, char *out, int cap);
    int (*rumour_for)(unsigned npc, char *out, int cap);
    /* A record said with its macros filled in; the length, or -1. */
    int (*say)(int text, const OgTalkWinSay *ctx, char *out, int cap);
    /* The width of text in the window's font. */
    int (*width)(const char *text);
    /* The player is out in the streets, and the region the town is in. */
    int outdoors, region;
    /* No work can be given here (a dungeon, or quest_find_potential_questor found no one). */
    int no_work;
} OgTalkWinWorld;

typedef struct OgTwItem {
    char text[OG_TW_ITEM_TEXT];
    /* A category, a topic's index, a regional entry's number, an organisation's number; -1 for "Previous List". */
    int value;
} OgTwItem;

typedef struct OgTalkWin {
    int open;
    OgTalk talk;
    OgTalkPlayer player;
    OgTalkWho who;
    int tone, mode, tab;
    /* Places: the categories are shown until one is picked (talk_showing_categories); the category picked. */
    int categories, category;
    /* The selected row, the first one shown and how many there are; a list's own horizontal scroll. */
    int row, top, count, scroll_x;
    OgTwItem item[OG_TW_ITEMS];
    OgTalkTopic topic[OG_TALK_TOPICS];
    int topic_count;
    OgTalkBuilding building[OG_TALK_TOPICS];
    int building_count;
    char answer[OG_TW_LINES][OG_TW_LINE];
    unsigned char asked[OG_TW_LINES];
    int answer_count, answer_scroll;
    char question[OG_TW_QUESTION][OG_TW_LINE];
    int question_count;
    /* The window closes after this text: a hostile greeting (record shown as a message). */
    int done;
    unsigned revision;
} OgTalkWin;

void og_talkwin_world(const OgTalkWinWorld *world);
/*
 * talk_start and talk_open: 1 when the window opens (the greeting is the first line said). 0 when the person will not
 * talk: *text is the record to show as a message (OG_TALK_TEXT_*, or a hostile greeting). witnessed is
 * people_check_witnesses & 2 for an underworld person.
 */
int og_talkwin_open(const OgTalkWho *who, const OgTalkPlayer *player, int witnessed, int *text);
const OgTalkWin *og_talkwin(void);
void og_talkwin_close(void);

/* A click at a classic pixel (strictly inside a rectangle counts). 1 when the window is still open. */
int og_talkwin_click(int x, int y);
/* Keys: Enter asks, Up and Down scroll the list (PgUp and PgDn ten), Escape goes back or leaves. */
enum { OG_TW_KEY_ENTER = 1, OG_TW_KEY_UP, OG_TW_KEY_DOWN, OG_TW_KEY_PGUP, OG_TW_KEY_PGDN, OG_TW_KEY_ESC };
int og_talkwin_key(int key);

/* The buttons' rectangles (x0, y0, x1, y1 exclusive of the edge), by talk_buttons' order, for the painter. */
enum { OG_TB_TELL, OG_TB_WHERE, OG_TB_PLACES, OG_TB_PEOPLE, OG_TB_THINGS, OG_TB_WORK, OG_TB_POLITE, OG_TB_NORMAL, OG_TB_BLUNT,
       OG_TB_COPY, OG_TB_GOODBYE, OG_TB_LIST_UP, OG_TB_LIST_DOWN, OG_TB_LIST_LEFT, OG_TB_LIST_RIGHT, OG_TB_LIST,
       OG_TB_ANSWER_UP, OG_TB_ANSWER_DOWN, OG_TB_OKAY, OG_TB_COUNT };
const short *og_talkwin_button(int button);
/* The question's key: what the selected row calls itself (talk_key_text). */
const char *og_talkwin_key_text(void);

#endif
