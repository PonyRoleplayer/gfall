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

#endif
