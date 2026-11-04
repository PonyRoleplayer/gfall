#ifndef DF_QUESTS_H
#define DF_QUESTS_H

#include "engine/assets/recfile.h"

/*
 * The quests that can be offered, as records (docs/data.md): Daggerfall Unity's
 * QuestList-Classic, what classic's quest names encode. Read docs/game.md
 * (Quests) before editing.
 *
 *   QUES: a quest by its place in the list: its file's name in ARENA2 (its
 *         .QBN and .QRC, read in place, never copied), who offers it (a guild
 *         group from FACTION.TXT's ggroup, or a social group, QUEST_SOCIAL +
 *         sgroup), to whom (QUEST_TO_*: members, others, prospects, a god's
 *         own; a social group's men or women), the least it asks (under 10 a
 *         guild rank or the player's level, from 10 a reputation), and its
 *         flags (once only, adult).
 *
 * The main quest, the tutorials and the cures are started by the game, not
 * offered: they are not here. A mod offers a quest of its own by a row and
 * its two files beside the program or in ARENA2.
 */

#define QUEST_NAME 9
/* The group of a quest no guild or person offers: the main quest's, the tutorial's, the cures' (started by the game and by each other). */
#define QUEST_GROUP_MAIN 250
/* Who offers it, past a guild group: a social group, this plus its sgroup. */
#define QUEST_SOCIAL 100

/* To whom (DFU's membership letters). */
#define QUEST_TO_MEMBERS 'M'
#define QUEST_TO_OTHERS 'N'
#define QUEST_TO_PROSPECTS 'P'

/* Its flags. */
#define QUEST_ONCE 0x01
#define QUEST_ADULT 0x02

typedef struct QuestKind {
    df_u16 id;
    char name[QUEST_NAME];
    df_u8 group;
    df_u8 to;
    df_u8 least;
    df_u8 flags;
} QuestKind;

extern const RecSchema quest_schema;
RecTable *quest_table(void);
/* The quest at a place in the list; 0 for none. */
const QuestKind *quest_kind(int id);

#endif
