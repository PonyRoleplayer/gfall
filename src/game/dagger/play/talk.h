#ifndef DF_TALK_H
#define DF_TALK_H

#include "game/dagger/play/player.h"

/*
 * Classic's talk window (TALK01I0.IMG) in memory: a person's face and name,
 * the tones, the list of things to ask and the conversation. Read docs/game.md
 * (People, Talk) before editing.
 *
 * "Where is" lists the kinds of building in this town, and the answer gives the
 * nearest one's way by the compass. "Tell me about" has the news. Work is a
 * question with no list. What a person says is TEXT.RSC's, chosen by how they
 * take the player (rules_talk_value); TalkManager's tables say which record.
 *
 * A person's answers are seeded by who they are, so asking twice gets the same
 * yes or no. One piece of news a person, as DFU has it.
 *
 * Plain C, drawing nothing and reaching no card: hostcheck drives it by clicks.
 */

/* Who is being talked to, and where. */
typedef struct TalkWho {
    int faction;
    int face;
    /* A number for the person's own: anything that differs between people and stays the same for one. */
    unsigned seed;
    /* The town the talk is in, by the map's numbers; located 0: no town to ask the way in. */
    int located;
    int region;
    int index;
    /* The player in the town's frame. */
    float x;
    float z;
    /* The building the talk is in, by its block and record; -1 in the street. */
    int block;
    int record;
    /* The town's name, for the macros. */
    const char *city;
    const char *region_name;
} TalkWho;

enum {
    TALK_STAY,
    TALK_LEAVE
};

/* 0 when a picture is missing. */
int talk_open(Player *p, const TalkWho *who);
void talk_close(void);
void talk_compose(void);
const unsigned char *talk_pixels(void);
int talk_changed(void);
int talk_click(int x, int y);
/* The wheel rolled with the pointer at x, y: the list or the conversation under it. Notches above 0 away from the player. */
void talk_wheel(int x, int y, int notches);
/* Escape and Enter. */
int talk_key(int vk);

/* What a test reads: how many lines the conversation has, the i-th of them; the topics now listed and the i-th. */
int talk_lines(void);
const char *talk_line(int i);
int talk_topics(void);
const char *talk_topic(int i);

#endif
