#ifndef OGFALL_QUEST_H
#define OGFALL_QUEST_H

/*
 * The quest machine (qcom.c, qinit.c, tamriel.c quests_run_all of the decompilation; docs/quest-machine.md there, docs/ogfall-quests.md here for the plan and the bugs
 * fixed). A quest is its QBN (formats/qbn.h) and the state it has run to: the states, each op's done bit, the timers. Quests run every frame of play and every passing
 * minute: each op in file order, looked at once; the states change as the ops run, so an op sees what the ones before it did this run.
 *
 * An op's first argument is its state: for most ops a gate ("when set"), for the ones that compute a result (52, 53, 31, 77, 16 ...) and the events (1, 2, 28 ...) the
 * state they set. -1 is always true. Most ops act once: the done bit, and the op's message, shown the first time. Step 1 (this file): loading, the states and the ops that
 * need no world: 6, 7, 16, 31, 34, 35, 51, 52, 53, 77. The rest are inert and are added by the steps of the plan, each keeping its place in the one switch.
 */
#include "game/ogfall/formats/qbn.h"

#define OG_QUESTS 32

typedef struct OgQuest {
    int used, ended;
    OgQbn qbn;
} OgQuest;

/* The text of a message of a quest's QRC (id, or a TEXT.RSC id when asked): the game fills it in; 0 when there is none. And what shows it (a message box). */
void og_quest_set_text(int (*read)(const char *quest, int message, int in_text_rsc, char *out, int cap));
void og_quest_set_say(void (*say)(const char *text));

/* The 64 global states every quest shares (the main story's). */
const df_u8 *og_quest_globals(void);
void og_quest_globals_clear(void);

void og_quest_reset(void);
/* quest_start's file part: NAME.QBN read into the first free slot with its id (1..255 not in use); the slot, or -1. Binding to the world is later steps. */
int og_quest_start(const char *name, int faction);
/* The same from bytes already in memory (a quest of a test). */
int og_quest_start_bytes(const char *name, const df_u8 *bytes, df_u32 size, int faction);
int og_quest_count(void);
const OgQuest *og_quest(int slot);
/* A quest ended (op 6): its slot is free. */
void og_quest_end(int slot);

/* quest_run_opcodes for every quest: from the frame and from each passing minute. */
void og_quest_run(void);

/* The states, by hash or index, for tests and the later steps (the timer's state is by its hash). */
int og_quest_state(int slot, int index);
void og_quest_set_state(int slot, int index, int value);
int og_quest_state_by_hash(int slot, df_i32 hash);

#endif
