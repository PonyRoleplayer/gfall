#ifndef DF_QUEST_H
#define DF_QUEST_H

#include "game/dagger/play/player.h"
#include "engine/assets/recfile.h"

/*
 * The quest machine: classic's quests run from their .QBN and .QRC in ARENA2
 * (formats/qbn.h). Read docs/game.md (Quests) before editing.
 *
 * A quest is started from its QUES row, its resources bound: the giver is
 * the person who offered it; another person gets a name; a place becomes a
 * building of the town, of another town of the region, or a dungeon of the
 * region; a timer gets its length; a foe its kind; gold its amount (DFU's
 * Item.CreateGold). Then its logic runs, record by record, until nothing more
 * changes, each time something happens:
 * - a record whose first operand is no state runs once, as the quest starts;
 * - a trigger sets its state when its event comes (the giver or a person met,
 *   a foe killed often enough, a timer run out), and says its message;
 * - AND and OR set their state from the others' continuously;
 * - an action runs when its state becomes true, and again only after it has
 *   been false (a state cleared re-arms it).
 * What a record does is UESP's Quest pseudo-codes; the twenty that the
 * offered quests use most are done, the rest do nothing yet.
 *
 * Messages come out of a queue, a record's QRC message with its symbols filled
 * (_x_ a thing's name or amount, __x_ its town, ___x_ its dungeon or house,
 * =x_ a timer's days, ==x_ a person's faction) and TEXT.RSC's % macros.
 *
 * Saved as rows (QUST), a quest a row; the logic is read again from its QBN.
 * Plain C: hostcheck runs a guild quest through.
 */

#define QUEST_SLOTS 6
#define QUEST_STATES 48
#define QUEST_LOGIC 128
#define QUEST_ITEMS 16
#define QUEST_PEOPLE 12
#define QUEST_PLACES 8
#define QUEST_TIMERS 8
#define QUEST_FOES 8
#define QUEST_LOG 4

/* Where and from whom a quest is started. */
typedef struct QuestStart {
    /* The town it is given in, by the map's numbers, and the player's place there. */
    int region;
    int index;
    /* The giver: the building's block and record, and their faction. */
    int giver_block;
    int giver_record;
    int giver_faction;
    /* The guild it comes from (its faction, 0 for none), the player's rank in it, its power (FACTION.TXT). */
    int guild;
    int rank;
    int power;
    /* The region's prices (1000 the mean), and the dice's seed. */
    int prices;
    unsigned seed;
} QuestStart;

/*
 * A quest running: its QUES row, its states, its bound resources, its log.
 * One row of a record file, under its 512 bytes; a place is the same slot in
 * the place_ arrays: a building (its town, block and record) or a dungeon
 * (place_dungeon 1, its region and map location).
 */
typedef struct QuestRun {
    df_u16 id;
    df_i32 quest;
    df_i32 started;
    df_u32 state[2];
    df_u32 fired[4];
    df_i32 timer_end[QUEST_TIMERS];
    df_i32 gold[QUEST_ITEMS];
    df_u32 item_held;
    df_u32 person_seed;
    df_u32 person_hidden;
    df_i16 place_dungeon[QUEST_PLACES];
    df_i16 place_region[QUEST_PLACES];
    df_i16 place_index[QUEST_PLACES];
    df_i16 place_block[QUEST_PLACES];
    df_i16 place_record[QUEST_PLACES];
    df_i16 person_at[QUEST_PEOPLE];
    df_i16 foe_at[QUEST_FOES];
    df_i16 foe_killed[QUEST_FOES];
    df_i16 log[QUEST_LOG];
    df_i32 giver_block;
    df_i32 giver_record;
    df_i32 giver_faction;
    df_i32 region;
    df_i32 index;
} QuestRun;

extern const RecSchema quest_run_schema;

void quest_new_game(void);
/* A quest's run by slot, 0 when the slot is empty; the slot of a QUES row running, -1. */
const QuestRun *quest_run(int slot);
int quest_running(int quest);
/* A saved run over a new game's (its logic read again). */
void quest_put(const QuestRun *run);

/*
 * QUES row quest offered at minute now: bound (its people, places and gold, so
 * its offer can name them) but not running. Its slot, or -1 (no room, its
 * files missing, a place it needs not found). Then accepted, its logic
 * running from now, or dropped (No).
 */
int quest_offer(Player *p, int quest, const QuestStart *how, int now);
int quest_accept(Player *p, int slot, int now);
void quest_drop(int slot);
/* Offered and accepted at once. */
int quest_start(Player *p, int quest, const QuestStart *how, int now);
/* A QRC message of slot's quest filled in, without the queue (the offer's words). 0 when it has none. */
int quest_text(int slot, int message, char *out, int cap);
/* Its file's name, "M0B00Y00". */
const char *quest_file(int slot);

/* Something happened: */
/* the player spoke to someone in a building (block, record) of the town, of faction; */
void quest_met(Player *p, int region, int index, int block, int record, int faction, int now);
/* a quest's own person (quest_people_in) was spoken to; */
void quest_met_person(Player *p, int slot, int person, int now);
/* a creature of kind was killed in the dungeon at region and dungeon index (travel's, TravelWhere.index; any, -1). */
void quest_killed(Player *p, int kind, int region, int index, int now);
/* a minute passed: timers. */
void quest_tick(Player *p, int now);

/* The next message to show, filled in; 0 when none waits. How many wait. */
int quest_take_message(char *out, int cap);
int quest_messages_waiting(void);
/* A quest's log line i (0..QUEST_LOG-1), filled in; 0 when none. */
int quest_log_line(int slot, int i, char *out, int cap);
/* A QRC message of the quest queued (its offer, 1000; its acceptance, 1002). */
void quest_say(int slot, int message);
/* The QRC messages every quest has (DFU's QuestMachine): the offer, refused, accepted. */
#define QUEST_SAY_OFFER 1000
#define QUEST_SAY_REFUSED 1001
#define QUEST_SAY_ACCEPTED 1002
/* The creature kinds quests have put in the dungeon at region and dungeon index (travel's), as many of each as they are, the dead too, into kinds (cap); how many. */
int quest_foes_at(int region, int index, int *kinds, int cap);

/* A quest person standing in a building: their quest and place in it, sex and faction. */
typedef struct QuestPersonHere {
    int slot;
    int person;
    int female;
    int faction;
} QuestPersonHere;
/* The quest people put in the building (block, record) of the town at region and index, not hidden, into out (cap); how many. */
int quest_people_in(int region, int index, int block, int record, QuestPersonHere *out, int cap);
/* A person flat's faction mark for quest person (slot, person), and back: the 15 bits under FLAT_PERSON (rmb.c). */
#define QUEST_PERSON_MARK 0x7000
#define QUEST_PERSON_TAG(slot, person) (QUEST_PERSON_MARK + (slot) * 16 + (person))
/* A quest item's Item.variant: this mark, its quest's slot and its place in the file (ITEM_QUEST, items.h); 0 once made ordinary. */
#define QUEST_ITEM_MARK 0x80
#define QUEST_ITEM_TAG(slot, item) (QUEST_ITEM_MARK + (slot) * 16 + (item))

/* For the tests: a state's value, by its record in the QBN. */
int quest_state(int slot, int state);

#endif
