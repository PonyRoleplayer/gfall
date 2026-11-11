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
#include "game/ogfall/play/travel.h"

#define OG_QUESTS 32

/* A place a quest names, bound to a location of the map (quest_init_place, qinit.c 2.4): the location's number in the map's list, its region and index there, its kind, its pixel, name. */
/* The key of a quest's creature: this bit, the foe's number and the copy (up to 16 of a foe). A kill reads the number back. */
#define OG_QUEST_FOE_KEY 0xe0000000u
#define OG_QUEST_FOE_THING(number, copy) (OG_QUEST_FOE_KEY | (((df_u32)(number) & 0xffffffu) << 4) | (df_u32)(copy))

typedef struct OgQuestPlace {
    int bound, ordinal, region, index, type, px, py;
    char name[33];
} OgQuestPlace;
/* A timer's running state: its flags (the bits of 2.8, 64 running and 128 expired changing), the minute it started and its delay. */
typedef struct OgQuestTimer {
    int flags;
    df_u32 start, delay;
} OgQuestTimer;

/* A foe the quest names: its number in the world (the counter every foe of every quest takes one from), whether op 22 placed it and where (a place of the quest, -1 near the player), and how many of it are live. */
typedef struct OgQuestFoe {
    int number, placed, place, live;
} OgQuestFoe;

typedef struct OgQuest {
    int used, ended;
    OgQbn qbn;
    OgQuestPlace place[OG_QBN_PLACES];
    OgQuestTimer timer[OG_QBN_TIMERS];
    OgQuestFoe foe[OG_QBN_FOES];
} OgQuest;

/* What the quests ask of the map: the locations (the travel list's), where the player is, a region's name and a journey's minutes by the quests' estimate (quest_travel_minutes). */
typedef struct OgQuestWorld {
    int (*count)(void);
    int (*location)(int ordinal, OgLocation *out);
    /* The player's region, the location they are in (-1 outdoors) and their map pixel. */
    void (*here)(int *region, int *index, int *px, int *py);
    int (*region_name)(int region, char *out, int cap);
    int (*journey)(int sx, int sy, int ex, int ey);
    /* The map shows a location (location_reveal). */
    void (*reveal)(int ordinal);
    /* A creature of a kind placed in the dungeon the player is in, unseen and near (quest_place_object); thing is its key (OG_QUEST_FOE_KEY and the number and copy). 1 when placed. */
    int (*spawn_foe)(int kind, df_u32 thing);
} OgQuestWorld;
void og_quest_set_world(const OgQuestWorld *world);

/* The text of a message of a quest's QRC (id, or a TEXT.RSC id when asked): the game fills it in; 0 when there is none. And what shows it (a message box). */
void og_quest_set_text(int (*read)(const char *quest, int message, int in_text_rsc, char *out, int cap));
void og_quest_set_say(void (*say)(const char *text));

/* What the world says of a resource a quest names (3.2 of the document): the text of its item, place or person in a form (0 the first of its kinds, 16 and 32 one more, 48 the last; 1 and up for `=`), 0
 * when it has none to give (a resource not bound yet reads blank). Set by the game as the steps that bind them are built. */
typedef struct OgQuestNames {
    int (*item)(int slot, int index, int form, char *out, int cap);
    int (*place)(int slot, int index, int form, char *out, int cap);
    int (*person)(int slot, int index, int form, char *out, int cap);
} OgQuestNames;
void og_quest_set_names(const OgQuestNames *names);
/* The macros of TEXT.RSC filled in a text (%pcn, %cn ...): set by the game. */
void og_quest_set_macros(int (*fill)(const char *in, char *out, int cap));
/* The current place's name, kept with a logbook entry and said by its %cn. */
void og_quest_set_place(const char *(*current)(void));
/* parse_expand: the symbols in a quest's text (`_name_`, `___name_`, `=name_`, `_a`b_`) said by what they name, then %qdt (the date of minutes) and %cn (place when given), then the
 * game's macros. Items, places, persons, foes and timers are looked up in that order, the first hash that matches (7.2). Returns the length. */
int og_quest_expand(int slot, const char *in, char *out, int cap, df_u32 minutes, const char *place);

/* The logbook (logbook.c, 3.4): 32 slots of ten entries, an entry a QRC message with the minute and the place it was written; the slot of a quest that has ended is free. */
#define OG_LOG_SLOTS 32
typedef struct OgLogSlot {
    int used, quest_id;
    char name[9];
    int message[10];
    df_u32 minutes[10];
    char place[10][32];
} OgLogSlot;
const OgLogSlot *og_logbook_slot(int i);
void og_logbook_clear(void);
/* logbook_add_entry (op 23) and op 24: entry index % 10 of the quest's slot. */
void og_logbook_add(int quest_slot, int index, int message);
void og_logbook_remove(int quest_slot, int index);
/* logbook_build_entries: every entry of the quests that are running, in order, as text; its length. */
int og_logbook_text(char *out, int cap);

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

/* quest_pick_file (func_0004C274): the quest files whose name begins with a letter or the other (0 for none), then '0', then the membership letter ('B' members only, 'C' anyone), whose
 * rank digit is at most the rank, one picked at random into name (9 bytes). 0 when none qualifies. (DAGGER.GRD's hiding of X quests is not read: there is no such file.) */
int og_quest_pick(char letter, char letter2, char membership, int rank, char *name);
/* Whether a quest of that name is running (quest_is_active). */
int og_quest_active(const char *name);
/* What asks the player (a yes/no box) and says the quest's words, set by the game. */
void og_quest_set_ask(int (*ask)(const char *words, void (*answer)(int yes)));
/* quest_start and the offer (quest_offer_prompt): the quest is started with the giver's faction when its name has a Y at its sixth place (else none); a name other than S0000xxx
 * and a Y quest or a Daedra's is asked yes or no (QRC 1000), answered by 1002 or 1001, and a no ends it. Returns the slot, -1 for none started (a missing file, a refused name,
 * a quest of that name running, no free slot). */
int og_quest_begin(const char *name, int faction);

/* A dungeon (a location) has just been loaded: the foes placed there and not yet live are made. */
void og_quest_location_loaded(int region, int index);
/* Event 2 (a creature died) and 21 (the player hit one): the foe's number, read from the creature's key. Ops 2 count the kills to their number and set their state. */
void og_quest_foe_killed(int number);
void og_quest_foe_hit(int number);

/* quest_run_opcodes for every quest: from the frame and from each passing minute. */
void og_quest_run(void);

/* The states, by hash or index, for tests and the later steps (the timer's state is by its hash). */
int og_quest_state(int slot, int index);
void og_quest_set_state(int slot, int index, int value);
int og_quest_state_by_hash(int slot, df_i32 hash);

#endif
