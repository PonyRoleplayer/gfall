#ifndef DF_BIOG_H
#define DF_BIOG_H

/*
 * BIOG##T0.TXT: a class's background questions, read from ARENA2. Each
 * file is twelve numbered questions, each with lettered answers, and under
 * an answer what it does (the file's grammar as DFU's BiogFile reads it):
 *
 *   22 +6        skill 22 gains 6
 *   GP +100      that much gold, more or less
 *   IT 3 18 1    an item: its group, its index in the group, its material
 *   #4150 !4156  words for the history (not kept yet)
 *   r2 +5        social group 2's reputation moves by 5
 *   rf48 -3      faction 48's reputation moves by -3
 *   RR +2        every reaction to the player, for good
 *   RP FT RD MR TH   other lasting modifiers (not kept yet)
 *
 * Only what the game can do yet is read: skills, gold, weapons and armour,
 * and reputations. The rest are skipped. Plain C: hostcheck runs it.
 */

#define BIOG_QUESTIONS 12
#define BIOG_ANSWERS 10
#define BIOG_EFFECTS 12
#define BIOG_LINE 72
/* An item's groups, by DFU's ItemGroups. */
#define BIOG_GROUP_ARMOR 2
#define BIOG_GROUP_WEAPONS 3

enum {
    BIOG_SKILL,
    BIOG_GOLD,
    BIOG_ITEM,
    BIOG_SOCIAL,
    BIOG_FACTION,
    BIOG_REACTION
};

typedef struct BiogEffect {
    short kind;
    /* A skill and its gain; gold's amount in a; an item's group, index and material. */
    short a;
    short b;
    short c;
} BiogEffect;

typedef struct BiogAnswer {
    char text[BIOG_LINE];
    int effects;
    BiogEffect effect[BIOG_EFFECTS];
} BiogAnswer;

typedef struct BiogQuestion {
    char line[2][BIOG_LINE];
    int answers;
    BiogAnswer answer[BIOG_ANSWERS];
} BiogQuestion;

typedef struct Biog {
    int questions;
    BiogQuestion question[BIOG_QUESTIONS];
} Biog;

/* The questions for class number klass (0..17). 0 when its file is missing or holds none. */
int biog_load(int klass, Biog *out);

#endif
