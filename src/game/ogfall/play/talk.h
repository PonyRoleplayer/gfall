#ifndef OGFALL_TALK_H
#define OGFALL_TALK_H

#include "engine/core/df.h"

/*
 * Conversation with a person: who will talk, how they greet, and the rolls behind a question. Slice 6 of
 * people and law (docs/ogfall-people.md), from the decomp's talk_start (src/lifted/talk.c 356), talk_open
 * (src/hand/func_00014AE8.c), talk_faction_greeting, talk_faction_relation (src/hand/func_00018BDC.c),
 * talk_roll_attitude and talk_roll_knows (talk.c 1378..1457), with the notes in docs/ai-social-math.md
 * ("Dialogue disposition"). The greetings' record numbers, the chance to know and the attitudes are read out
 * of FALL.EXE (tables/talkdata.h, generated). Pure rules over the session: the window, its topics and the
 * answers they give (places, people, things, work, news) are the next part.
 *
 * Two quirks of the original are kept: the Streetwise tone's first use is tested on the neutral tone's cache,
 * and the third attitude cache is not cleared between conversations.
 */

/* The record numbers: a person who will not be spoken to, and one who has nothing to say to the player. */
#define OG_TALK_TEXT_NOT_A_PERSON 7204
#define OG_TALK_TEXT_REFUSES 7205
/* A person's mood when no faction greets: by the disposition, below nothing, under 10, under 30, more. */
#define OG_TALK_TEXT_HOSTILE 7206
#define OG_TALK_TEXT_COOL 7207
#define OG_TALK_TEXT_WARM 7208
#define OG_TALK_TEXT_FRIENDLY 7209

enum { OG_TONE_POLITE, OG_TONE_NORMAL, OG_TONE_BLUNT };
/* The question's kind: asked of "where is" (mode 0) or "tell me about" (mode 1) under a tab (0 places, 1 people, 2 things, 3 work). */

typedef struct OgTalk {
    int open;
    unsigned npc;
    /* The person's own faction, and the one whose reputation speaks for them: the first of type 2, 7 or 9 up the tree. */
    int own, faction;
    /* The disposition (talk_disposition), the score the greeting adds to (D_001966AC), whether the greeting was hostile. */
    int disposition, score, hostile;
    int tone, mode, tab, row, style, attitude;
    int cache[3];
    /* talk_news_asked: the news is given once a conversation (not by the king's or the underworld's person). */
    int news_asked;
} OgTalk;

typedef struct OgTalkWho {
    unsigned npc;
    /* The person's own faction id. */
    int faction;
    /* A person at all (type 8, 18 or 53), and one who will not talk (npc_flags 0x8000). */
    int person, blocked;
} OgTalkWho;

typedef struct OgTalkPlayer {
    /* The factions the player belongs to (their memberships), and a local temple's rank (guild_local_temple_rank). */
    const int *members;
    int member_count;
    int temple_rank;
} OgTalkPlayer;

/* talk_start's checks and the disposition. 1 to go on; 0 refused, with the record in *text. */
int og_talk_start(OgTalk *talk, const OgTalkWho *who, const OgTalkPlayer *player, int *text);

enum { OG_TALK_REFUSED, OG_TALK_OPEN, OG_TALK_HOSTILE };
/*
 * talk_open: the tone, the question's place, the speech style and the first attitude; then what the person says
 * first (*text): a faction's greeting for how they see the player, else a word by the disposition. A hostile
 * greeting (OG_TALK_HOSTILE) is shown and the talk closes. witnessed says a guard saw the player speak to the
 * underworld (social group 4): that is crime 7 and the watch, and no talk (OG_TALK_REFUSED).
 */
int og_talk_open(OgTalk *talk, const OgTalkPlayer *player, int witnessed, int *text);

/* talk_roll_attitude: 0 cold, 1 plain, 2 warm, from the tone and the player's skill, the question's kind and Personality. */
int og_talk_attitude(OgTalk *talk);
/* talk_roll_knows: whether the person knows what the row asks, from the speech style; 806 and 842 always do. */
int og_talk_knows(OgTalk *talk);

/* talk_button_tone: a new tone, its attitude rolled again. */
void og_talk_tone(OgTalk *talk, int tone);

/*
 * The window's topics and what a question gets back (talk_build_place_topics, talk_draw_categories,
 * talk_button_okay, talk_hint_text_id). A town's buildings come from the world (OgTalkBuilding); the original
 * counts every building of a town that is not a house (type 1) or past 16, and a quest site is a category of its
 * own ("General", 13).
 */
#define OG_TALK_CATEGORIES 15
#define OG_TALK_REGIONAL 14
#define OG_TALK_TOPICS 64

typedef struct OgTalkBuilding {
    unsigned id;
    int type, flags, distance;
} OgTalkBuilding;

typedef struct OgTalkTopic {
    int category, distance;
    /* The building's id; 0 for the regional entry that ends the list. */
    unsigned building;
} OgTalkTopic;

/* building_category: the category a building is listed under, or -1 for none (a house that is not a quest site). */
int og_talk_category(int building_type, int building_flags);
const char *og_talk_category_name(int category);
/* talk_build_place_topics: every listed building, then the regional entry; the buildings sorted nearest first (the original's bubble sort leaves the regional entry alone). Returns the count. */
int og_talk_topics(OgTalkTopic *out, int cap, const OgTalkBuilding *buildings, int count);
/* town_has_category: whether a category is on offer (the regional one always is). */
int og_talk_has_category(const OgTalkBuilding *buildings, int count, int category);

typedef struct OgTalkAsk {
    /* 0 "tell me about", 1 "where is". */
    int mode;
    /* The tab (0 places, 1 people, 2 things, 3 work) and, for places, the category chosen. */
    int tab, category;
    /* The row picked on the tab: row 0 of "tell me about" is the news. */
    int row;
    /* The news the person could tell (rumor_pick_news found one), its text kept by the caller. */
    int news;
    /* Work: the player is in a dungeon (environment 3), or no quest giver is to be found (quest_find_potential_questor). */
    int no_work;
} OgTalkAsk;

enum { OG_ANSWER_TEXT, OG_ANSWER_NEWS };
/*
 * talk_button_okay: what the person says. The knowing roll is made, the news given once, work answered by the
 * attitude, "regional" with record 28, else the answer table's record by knowing, style and attitude. A where-is
 * answer carries a hint macro; og_talk_hint says which record fills it. Returns OG_ANSWER_TEXT (*text) or
 * OG_ANSWER_NEWS.
 */
int og_talk_answer(OgTalk *talk, const OgTalkAsk *ask, int *text);

/*
 * talk_hint_text_id for a where-is question: 10 or 11 for a regional topic (the person knows one or not), 7332
 * when the building is within 2048 units or a one in four roll lets them recall it (and the player is out in the
 * streets), else 7333. *noted says the building goes on the town map.
 */
int og_talk_hint(const OgTalkAsk *ask, int regional_known, int distance, int outdoors, int *noted);

#endif
