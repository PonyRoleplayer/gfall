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
    /* A fixed place's object, as the low word of its id (1 the location itself, 0xFA01.. its quest markers). */
    int marker;
    /* A building place (p1 0): which building of the town, as a door's tag names it; -1 for none (the whole location). */
    int block, record;
    char name[33];
} OgQuestPlace;
/* A timer's running state: its flags (the bits of 2.8, 64 running and 128 expired changing), the minute it started and its delay. */
/* A person a quest names, bound to someone in the world (quest_init_person, 2.3). Step 6 binds the questor (kind 21: the one who offered the quest) and the persons that are only a faction
 * (kind -8, type 65); the others wait for the town's buildings (step 9). who identifies a person of the world (the talk window's id), name and town are what text says of them. */
typedef struct OgQuestPerson {
    int bound, who, faction;
    /* The place (+1) op 30 put the person in; 0 for none. */
    int at;
    /* The building the person stands in (their site at the start, or the place op 30 put them in): the town and the building's block and record; homed 0 for none. */
    int homed, home_region, home_index, home_block, home_record;
    /* 1 when the site was picked by a building type (kinds 0 and up, -6): the person is one of that building's own people, not an added one. */
    int takeover;
    /* The person is a woman (flags 0x200, or a toss when neither sex is asked). */
    int female;
    char name[40], town[33];
} OgQuestPerson;
typedef struct OgQuestTimer {
    int flags;
    df_u32 start, delay;
} OgQuestTimer;

/* A foe the quest names: its number in the world (the counter every foe of every quest takes one from), whether op 22 placed it and where (a place of the quest, -1 near the player), and how many of it are live. */
typedef struct OgQuestFoe {
    int number, placed, place, live, next;
    /* How many of its count have been put in the world so far (their deaths do not make more). */
    int made;
} OgQuestFoe;

/* An item a quest names: gold's amount is rolled when the quest starts (quest_init_item); a reward is given once. Other items wait for step 9. */
typedef struct OgQuestItem {
    int gold, amount, given;
    /* The item made when the quest began (0 for gold, none made, or one forgotten, given away or deleted); op 0's place (-1 none) not yet laid there, the foe (+1) that carries it. */
    df_u32 object;
    int place, laid, carrier;
} OgQuestItem;

typedef struct OgQuest {
    int used, ended, rewarded;
    OgQuestItem item[OG_QBN_ITEMS];
    OgQbn qbn;
    OgQuestPlace place[OG_QBN_PLACES];
    OgQuestTimer timer[OG_QBN_TIMERS];
    OgQuestFoe foe[OG_QBN_FOES];
    OgQuestPerson person[OG_QBN_PERSONS];
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
    /* The player taken to a place on the map (op 30 and op 83): the Mages Guild's teleport. */
    int (*go)(int region, int index);
    /* A pile at the player's feet for an item (a quest's item set down in its place, or dropped by its foe): the container's id, 0 for none. */
    df_u32 (*place_item)(void);
    /* Where the location numbered so is (a fixed place's p1: the outside's number, its dungeon's one more): the region and the index of the outside in it; 1 when found. */
    int (*find_number)(int number, int *region, int *index);
    /* A quest marker of the dungeon the player is in, its point in the game's frame: marker >= 0 the n-th (0xFA01 is 0), else one of the record (11 a person's or foe's, 18 an item's) picked by the seed;
     * 1 when the dungeon is open and has it. A foe placed on the floor near a point (0 while that floor is not loaded: tried again), and a pile for an item at a point. */
    int (*marker_at)(int marker, int record, unsigned seed, float *x, float *y, float *z);
    int (*spawn_foe_at)(int kind, df_u32 thing, float x, float y, float z);
    df_u32 (*place_item_at)(float x, float y, float z);
    /* A building of a town for a place that is one (p1 0): the town (region, index), the building's type (below 0 any shop-like one, 17..20 any house, 11 only a Mages Guild) and a seed that picks among them; its block and record. 1 when there is one. */
    int (*pick_building)(int region, int index, int type, unsigned seed, int *block, int *record);
    /* A building of a town for a person's site (quest_init_person): mode 0 of the type value (17..20 any house, 11 only a Mages Guild), 1 any shop-like one, 2 of the faction value, 3 of a faction of the type
     * value; picked by the seed; its block, record and faction. 1 when there is one. */
    int (*pick_site)(int region, int index, int mode, int value, unsigned seed, int *block, int *record, int *faction);
    /* A name for a person of the quests (the same each time for the same id). */
    void (*person_name)(int who, int faction, int female, char *out, int cap);
    /* The player is out of doors (the streamed world's streets and fields, not a building or a dungeon): a guild's invitation comes then. */
    int (*outdoors)(void);
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

/* Rumours (3.6): a line of text a quest adds when it starts (1005), ends (1006 lost, 1007 won) and for its questor (1008 and 1009, to be told in the greeting). People tell them as news
 * (flags 4 and 8) for 30 days, the questor's for 180. A quest that ended takes its start rumour with it. */
#define OG_RUMOURS 32
typedef struct OgRumour {
    int used, quest_id, message, flags, target;
    df_u32 expires;
    char name[9];
    char text[200];
} OgRumour;
const OgRumour *og_rumour(int i);
/* rumor_pick_news: one rumour of the news kind, at random, not yet gone stale; 0 when there is none. */
int og_quest_news(int faction, char *out, int cap);
/* rumor_find_for_npc: the questor's own rumour (the person is the one who gave the quest); 0 when there is none. */
int og_quest_rumour_for(int who, char *out, int cap);

/* The talk window's quest topics ("Tell me about": every place, person and item the quest names that is not hidden and has a message): how many, a topic's name and its answer. */
int og_quest_topic_count(void);
int og_quest_topic_name(int i, char *out, int cap);
int og_quest_topic_answer(int i, char *out, int cap);

/* Building access a quest granted (op 17): the building's place, the access kind and the minute it lasts to. Nothing reads them until a quest's buildings stand in a town. */
#define OG_QUEST_ACCESS 16
typedef struct OgQuestAccess { int used, quest_id, region, index, block, record, access; df_u32 until; } OgQuestAccess;
/* A building (the town's region and index, the building's block and record) a running quest has marked open to it (op 81's place flag 64 on a place of it, or the flag 0x4000 on a person who stands in it): the door lets the player in. */
int og_quest_building_open(int region, int index, int block, int record);
/* quest_find_site_for_building as the talk window and building_name use it: 1 when a running quest has a place that is this building or a person standing in it; surname the quest person's, or one made from
 * the building when it is a place (a quest residence is "The Surname Residence"). */
int og_quest_building_site(int region, int index, int block, int record, char *surname, int cap);
/* The access level op 17 granted the building, while its minutes last (building_access_level: 10 and over opens its door to a lockpicker); 0 for none. */
int og_quest_building_access(int region, int index, int block, int record, df_u32 now);
const OgQuestAccess *og_quest_access(int i);

/* The quest people standing in a building (op 30 put them in a place that is one): the building's block and record in the town at region and index. Each is made a person of the building when
 * it is entered (the seam installs the engine's people hook); their flat carries OG_QUEST_PERSON_TAG, which a use of them is known by. */
/* takeover: the person is one of the building's own people (a quest person of a building type takes over an NPC of it: the original's NPC site); seed picks which. */
typedef struct OgQuestHere { int slot, person, female, faction, takeover; unsigned seed; } OgQuestHere;
int og_quest_people_in(int region, int index, int block, int record, OgQuestHere *out, int cap);
#define OG_QUEST_PERSON_MARK 0x7000
#define OG_QUEST_PERSON_TAG(slot, person) (OG_QUEST_PERSON_MARK + (slot) * 16 + (person))
/* The person a flat's tag names: their id, faction, sex and name; 0 when it names none. */
int og_quest_person_of_tag(int tag, int *who, int *faction, int *female, char *name, int cap);

/* The words of a quest's letter item (group 9, index 5): its QRC message of the quest that still holds it, filled in; 0 when no running quest has the item. */
int og_quest_item_text(df_u32 item, char *out, int cap);

/* The faces at the top of the screen (3.11, ops 55, 56, 63 and 64): ten slots of a quest's person or foe. kind 1 person, 2 foe. */
#define OG_QUEST_FACES 10
/* face is the original's byte: bit 7 a woman, bit 6 the name bank, bit 4 a child's (faction 514: KIDS00I0), bits 0..3 the picture. */
typedef struct OgQuestFace { int used, quest_id, kind, index, face; } OgQuestFace;
const OgQuestFace *og_quest_face(int i);

/* The 64 global states every quest shares (the main story's). */
const df_u8 *og_quest_globals(void);
void og_quest_globals_clear(void);
/* A dungeon's action 31 (RDB: a clickable model, Mantellan Crux's sets 36, Lysandus' Tomb's 43): the global state set. */
void og_quest_global_set(int state);

/* quests_run_all for a passing minute: as og_quest_run, and the ops that watch the clock (79) look. */
void og_quest_run_minute(void);
/* quests_start_initial: the quest that starts with a new game (_BRISIEN; _TUTOR__ is left out), begun at the first frame of play in the world. */
void og_quest_start_initial_later(void);
void og_quest_reset(void);
/* The quests as bytes for a save and back (step 8): the global states, the logbook, the foe counter and each running quest's name, id, faction and what changed since its QBN was read (the
 * states, ops done, foes killed, timers, bound places, persons and items). A quest comes back by reading its QBN again. pack returns the length, or -1 when it does not fit; unpack
 * returns 0 for bytes it cannot read (nothing is then running). */
int og_quest_pack(df_u8 *out, int cap);
int og_quest_unpack(const df_u8 *in, int len);
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
int og_quest_pick_seeded(char letter, char letter2, char membership, int rank, unsigned seed, char *name);
/* Whether a quest of that name is running (quest_is_active). */
int og_quest_active(const char *name);
/* What asks the player (a yes/no box) and says the quest's words, set by the game. */
void og_quest_set_ask(int (*ask)(const char *words, void (*answer)(int yes)));
/* quest_start and the offer (quest_offer_prompt): the quest is started with the giver's faction when its name has a Y at its sixth place (else none); a name other than S0000xxx
 * and a Y quest or a Daedra's is asked yes or no (QRC 1000), answered by 1002 or 1001, and a no ends it. Returns the slot, -1 for none started (a missing file, a refused name,
 * a quest of that name running, no free slot). */
int og_quest_begin(const char *name, int faction);

/* The person the player has just spoken to, who a kind-21 person binds to when a quest starts (the last clicked NPC): their id, faction, name and town. */
void og_quest_set_questor(int who, int faction, const char *name, const char *town);
/* Whether the person already belongs to a quest (a quest giver with a quest does not offer another). */
int og_quest_person_busy(int who);
/* npc_talk's quest part (click.c 695): the events of talking (28 and 1 and 71 to the quests of the person, 78 by their faction): 1 when a quest took it and the talk window should not open. */
int og_quest_talk(int who, int faction);

/* A dungeon (a location) has just been loaded: the foes placed there and not yet live are made. */
void og_quest_location_loaded(int region, int index);
/* Event 2 (a creature died) and 21 (the player hit one): the foe's number, read from the creature's key. Ops 2 count the kills to their number and set their state. */
/* An event of the state (the pack took or lost an item, a spell was cast): ops 3, 5 and 73 watch for them. */
void og_quest_event(int kind, df_u32 subject, df_u32 other);
void og_quest_foe_killed(int number);
void og_quest_foe_hit(int number);

/* quest_run_opcodes for every quest: from the frame and from each passing minute. */
void og_quest_run(void);

/* The states, by hash or index, for tests and the later steps (the timer's state is by its hash). */
int og_quest_state(int slot, int index);
void og_quest_set_state(int slot, int index, int value);
int og_quest_state_by_hash(int slot, df_i32 hash);

#endif
