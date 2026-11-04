#include "game/dagger/play/quest.h"
#include "game/dagger/play/atlas.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/journey.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/words.h"
#include "game/dagger/places/town.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/formats/qbn.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/formats/worldmap.h"
#include "game/dagger/tables/commerce.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/names.h"
#include "game/dagger/tables/quests.h"
#include "engine/core/log.h"
#include "engine/scene/clock.h"
#include "engine/core/mem.h"

/* See quest.h and docs/game.md (Quests). */

/* The opcodes done (UESP's Quest pseudo-codes). */
enum {
    OP_ITEM_TO_PERSON = 1,
    OP_KILLS = 2,
    OP_REWARD = 4,
    OP_END = 6,
    OP_CLEAR = 7,
    OP_TIMER_START = 12,
    OP_TIMER_STOP = 13,
    OP_TO_MAP = 19,
    OP_FOE_AT = 22,
    OP_LOG = 23,
    OP_UNLOG = 24,
    OP_GIVE_ITEM = 26,
    OP_MEET = 28,
    OP_PERSON_AT = 30,
    OP_RANDOM = 34,
    OP_CYCLE = 35,
    OP_HIDE = 46,
    OP_REVEAL = 48,
    OP_MESSAGE = 51,
    OP_AND = 52,
    OP_OR = 53,
    OP_REPUTATION = 61,
    OP_LEGAL = 65,
    OP_ITEM_TO_PLAYER = 76
};

/* An operand naming no state, and one that is a filler (AND: true; OR: false). */
#define NO_STATE 0xFFFFFFFFu
#define FILLER 0xFFFFFFFEu
/* Passes over the logic at most, each event: a quest's chains settle in a few. */
#define PASSES 8
#define MESSAGES 8
#define MESSAGE_TEXT 512
#define DAY 1440
/* A timer whose length the file leaves to the distance, when no place gives one: a week. */
#define TIMER_DEFAULT (7 * DAY)

static const RecField g_run_fields[] = {
    REC_FIELD("id", REC_UINT, QuestRun, id),
    REC_FIELD("quest", REC_INT, QuestRun, quest),
    REC_FIELD("started", REC_INT, QuestRun, started),
    REC_ARRAY("state", REC_UINT, QuestRun, state),
    REC_ARRAY("fired", REC_UINT, QuestRun, fired),
    REC_ARRAY("timer_end", REC_INT, QuestRun, timer_end),
    REC_ARRAY("gold", REC_INT, QuestRun, gold),
    REC_FIELD("item_held", REC_UINT, QuestRun, item_held),
    REC_FIELD("person_seed", REC_UINT, QuestRun, person_seed),
    REC_FIELD("person_hidden", REC_UINT, QuestRun, person_hidden),
    REC_ARRAY("place_dungeon", REC_INT, QuestRun, place_dungeon),
    REC_ARRAY("place_region", REC_INT, QuestRun, place_region),
    REC_ARRAY("place_index", REC_INT, QuestRun, place_index),
    REC_ARRAY("place_block", REC_INT, QuestRun, place_block),
    REC_ARRAY("place_record", REC_INT, QuestRun, place_record),
    REC_ARRAY("person_at", REC_INT, QuestRun, person_at),
    REC_ARRAY("foe_at", REC_INT, QuestRun, foe_at),
    REC_ARRAY("foe_killed", REC_INT, QuestRun, foe_killed),
    REC_ARRAY("log", REC_INT, QuestRun, log),
    REC_FIELD("giver_block", REC_INT, QuestRun, giver_block),
    REC_FIELD("giver_record", REC_INT, QuestRun, giver_record),
    REC_FIELD("giver_faction", REC_INT, QuestRun, giver_faction),
    REC_FIELD("region", REC_INT, QuestRun, region),
    REC_FIELD("index", REC_INT, QuestRun, index)
};
const RecSchema quest_run_schema = {
    "QUST", "quest_run", g_run_fields, (int)(sizeof(g_run_fields) / sizeof(g_run_fields[0])), (df_u16)sizeof(QuestRun), 0
};

/* A slot: its run, its QBN in a heap of its own, 1 while in use. */
typedef struct Slot {
    int used;
    QuestRun run;
    Qbn qbn;
    MemHeap *heap;
} Slot;

static Slot g_slots[QUEST_SLOTS];
static char g_messages[MESSAGES][MESSAGE_TEXT];
static int g_message_first;
static int g_message_count;
/* The player the logic acts on while it runs (a reward, a reputation). */
static Player *g_player;

/* ---- States and fired records. ---- */

static int state_of(const QuestRun *r, int i)
{
    return i >= 0 && i < QUEST_STATES && (r->state[i >> 5] >> (i & 31)) & 1u;
}

/* 1 when it changed. */
static int set_state(QuestRun *r, int i, int on)
{
    df_u32 bit;
    int was;

    if (i < 0 || i >= QUEST_STATES) {
        return 0;
    }
    bit = 1u << (i & 31);
    was = (r->state[i >> 5] & bit) != 0;
    if (on) {
        r->state[i >> 5] |= bit;
    } else {
        r->state[i >> 5] &= ~bit;
    }
    return was != (on != 0);
}

static int fired(const QuestRun *r, int i)
{
    return i >= 0 && i < QUEST_LOGIC && (r->fired[i >> 5] >> (i & 31)) & 1u;
}

static void set_fired(QuestRun *r, int i, int on)
{
    if (i < 0 || i >= QUEST_LOGIC) {
        return;
    }
    if (on) {
        r->fired[i >> 5] |= 1u << (i & 31);
    } else {
        r->fired[i >> 5] &= ~(1u << (i & 31));
    }
}

/* An operand's state's value, negated as it says. */
static int operand_true(const QuestRun *r, const QbnOperand *o)
{
    return state_of(r, (int)o->value) ^ o->negate;
}

/* ---- Names for the messages. ---- */

static const char *file_of(const Slot *s)
{
    const QuestKind *k;

    k = quest_kind(s->run.quest);
    return k ? k->name : "";
}

/* A map location's name (a town's, or a dungeon's from outside). */
static void location_name(int region, int index, char *out, int cap)
{
    static Dungeon d;

    out[0] = 0;
    if (region >= 0 && exterior_load_at(region, index, &d)) {
        df_copy(out, d.title, cap);
    }
}

/* The person's own name: their seed and their place in the file. */
static void person_name(const Slot *s, int i, char *out, int cap)
{
    const QbnPerson *who;

    who = &s->qbn.people[i];
    name_make(name_bank_of_region(s->run.region), who->gender == 1 || (who->gender == 0xFF && ((s->run.person_seed >> i) & 1u)),
              s->run.person_seed * 31u + (unsigned)i, out, cap);
}

/* A symbol's words: by its kind and how many marks led it (1: the thing, 2: its town, 3: its dungeon or house; equals for days, a faction). */
static void symbol_words(const Slot *s, const char *name, int marks, int equals, char *out, int cap)
{
    df_u32 h;
    int i;

    out[0] = 0;
    h = qbn_hash(name);
    for (i = 0; i < s->qbn.count[QBN_ITEMS] && i < QUEST_ITEMS; i++) {
        if (s->qbn.items[i].symbol == h) {
            if (s->qbn.items[i].reward == 2) {
                log_format(out, cap, "%d", s->run.gold[i]);
            } else {
                df_copy(out, "the parcel", cap);
            }
            return;
        }
    }
    for (i = 0; i < s->qbn.count[QBN_PEOPLE] && i < QUEST_PEOPLE; i++) {
        if (s->qbn.people[i].symbol == h) {
            int at;

            at = s->run.person_at[i];
            if (equals) {
                const Faction *f;

                f = factionfile_open() ? faction_find(s->qbn.people[i].faction ? s->qbn.people[i].faction : s->run.giver_faction) : 0;
                df_copy(out, f ? f->name : "", cap);
            } else if (marks >= 2) {
                location_name(at >= 0 ? s->run.place_region[at] : s->run.region, at >= 0 ? s->run.place_index[at] : s->run.index, out, cap);
            } else {
                person_name(s, i, out, cap);
            }
            return;
        }
    }
    for (i = 0; i < s->qbn.count[QBN_PLACES] && i < QUEST_PLACES; i++) {
        if (s->qbn.places[i].symbol == h) {
            const BuildingKind *kind;

            /* A dungeon is its own name at every depth; a building, its kind, then its town. */
            if (s->run.place_dungeon[i] || marks >= 2) {
                location_name(s->run.place_region[i], s->run.place_index[i], out, cap);
            } else {
                kind = building_kind(s->qbn.places[i].type < 0 ? 0 : s->qbn.places[i].type);
                log_format(out, cap, "the %s", kind ? kind->name : "house");
            }
            return;
        }
    }
    for (i = 0; i < s->qbn.count[QBN_TIMERS] && i < QUEST_TIMERS; i++) {
        if (s->qbn.timers[i].symbol == h) {
            int left;

            left = s->run.timer_end[i] - s->run.started;
            left = left < 0 ? 0 : left;
            log_format(out, cap, "%d", equals ? (left + DAY - 1) / DAY : left);
            return;
        }
    }
    for (i = 0; i < s->qbn.count[QBN_FOES] && i < QUEST_FOES; i++) {
        if (s->qbn.foes[i].symbol == h) {
            const EnemyKind *e;

            e = enemy_kind(s->qbn.foes[i].kind);
            df_copy(out, e ? e->name : "creature", cap);
            return;
        }
    }
}

/* A QRC message of slot s filled in: its symbols, then TEXT.RSC's % macros. */
static void fill_message(const Slot *s, int message, char *out, int cap)
{
    static char raw[MESSAGE_TEXT];
    static char mid[MESSAGE_TEXT];
    static Words words;
    char file[16];
    int i;
    int n;

    out[0] = 0;
    log_format(file, (int)sizeof(file), "%s.QRC", file_of(s));
    if (textrsc_variant_in(file, message, combat_roll(8), raw, (int)sizeof(raw)) < 0) {
        return;
    }
    n = 0;
    for (i = 0; raw[i] && n < MESSAGE_TEXT - 1;) {
        /* _name_, __name_, ___name_, =name_, ==name_: marks, a name of letters and digits, an underscore. */
        if (raw[i] == '_' || raw[i] == '=') {
            char name[24];
            char words_out[64];
            int marks;
            int equals;
            int k;
            int j;

            equals = raw[i] == '=';
            marks = 0;
            j = i;
            while (raw[j] == raw[i] && marks < 4) {
                marks++;
                j++;
            }
            for (k = 0; ((raw[j] >= 'a' && raw[j] <= 'z') || (raw[j] >= '0' && raw[j] <= '9')) && k < 23; k++) {
                name[k] = raw[j++];
            }
            name[k] = 0;
            if (k > 0 && raw[j] == '_') {
                symbol_words(s, name, marks, equals, words_out, (int)sizeof(words_out));
                for (k = 0; words_out[k] && n < MESSAGE_TEXT - 1; k++) {
                    mid[n++] = words_out[k];
                }
                i = j + 1;
                continue;
            }
        }
        /* %qdt: the day the quest began, as the log has it ("Day 3 of Hearthfire, 3E 405"). */
        if (raw[i] == '%' && raw[i + 1] == 'q' && raw[i + 2] == 'd' && raw[i + 3] == 't') {
            char date[48];
            int year;
            int month;
            int day;
            int k;

            clock_date_of(s->run.started / DAY, &year, &month, &day);
            log_format(date, (int)sizeof(date), "Day %d of %s, %s %d", day + 1, clock_month_name(month), clock_era(), year);
            for (k = 0; date[k] && n < MESSAGE_TEXT - 1; k++) {
                mid[n++] = date[k];
            }
            i += 4;
            continue;
        }
        mid[n++] = raw[i++];
    }
    mid[n] = 0;
    mem_zero(&words, sizeof(words));
    words.pc = g_player ? &g_player->sheet : 0;
    words_use(&words);
    words_expand(mid, out, cap);
}

static void say(const Slot *s, int message)
{
    char *to;

    if (message == QBN_NO_MESSAGE || message <= 0 || g_message_count == MESSAGES) {
        return;
    }
    to = g_messages[(g_message_first + g_message_count) % MESSAGES];
    fill_message(s, message, to, MESSAGE_TEXT);
    if (to[0]) {
        g_message_count++;
    }
}

int quest_take_message(char *out, int cap)
{
    if (!g_message_count) {
        return 0;
    }
    df_copy(out, g_messages[g_message_first], cap);
    g_message_first = (g_message_first + 1) % MESSAGES;
    g_message_count--;
    return 1;
}

/* ---- Binding. ---- */

static int is_dungeon_type(int type)
{
    return type == WORLDMAP_DUNGEON_LABYRINTH || type == WORLDMAP_DUNGEON_KEEP || type == WORLDMAP_DUNGEON_RUIN
           || type == WORLDMAP_GRAVEYARD || type == WORLDMAP_COVEN;
}

static int is_town_type(int type)
{
    return type == WORLDMAP_TOWN_CITY || type == WORLDMAP_TOWN_HAMLET || type == WORLDMAP_TOWN_VILLAGE;
}

/* A map location of region, picked at random among those town (or dungeon) types; 0 when none. */
static int pick_location(int region, int dungeon, int not_index, int *index)
{
    int n;
    int k;
    int count;
    int pick;

    count = 0;
    n = worldmap_locations();
    for (k = 0; k < n; k++) {
        int px;
        int py;
        int r;
        int i;
        int type;
        int known;

        if (worldmap_location_info(k, &px, &py, &r, &i, &type, &known) && r == region && i != not_index
            && (dungeon ? is_dungeon_type(type) : is_town_type(type))) {
            count++;
        }
    }
    if (!count) {
        return 0;
    }
    pick = combat_roll(count);
    for (k = 0; k < n; k++) {
        int px;
        int py;
        int r;
        int i;
        int type;
        int known;

        if (worldmap_location_info(k, &px, &py, &r, &i, &type, &known) && r == region && i != not_index
            && (dungeon ? is_dungeon_type(type) : is_town_type(type)) && pick-- == 0) {
            *index = i;
            return 1;
        }
    }
    return 0;
}

/* A building of type (-1: any but a house or an empty lot) in the town; 0 when it has none. */
static int pick_building(int region, int index, int type, int *block, int *record)
{
    const TownBuilding *rows;
    int count;
    int matches;
    int pick;
    int i;

    rows = town_buildings(region, index, &count);
    matches = 0;
    for (i = 0; rows && i < count; i++) {
        matches += type >= 0 ? rows[i].type == type : (rows[i].type < BUILDING_HOUSE1 && rows[i].type != 1 && rows[i].type != 4);
    }
    if (!matches) {
        return 0;
    }
    pick = combat_roll(matches);
    for (i = 0; i < count; i++) {
        if ((type >= 0 ? rows[i].type == type : (rows[i].type < BUILDING_HOUSE1 && rows[i].type != 1 && rows[i].type != 4)) && pick-- == 0) {
            *block = rows[i].block;
            *record = rows[i].record;
            return 1;
        }
    }
    return 0;
}

/* Place i bound by its record: 0 when no such place was found (or the file names one outright, a storyline's). */
static int bind_place(Slot *s, int i, const QuestStart *how)
{
    const QbnPlace *q;
    QuestRun *r;
    int block;
    int record;
    int index;
    int tries;

    q = &s->qbn.places[i];
    r = &s->run;
    r->place_region[i] = (df_i16)how->region;
    if (q->general == 0) {
        return 0;
    }
    if (q->fine == 1) {
        static Dungeon outside;

        /* A dungeon: its map location (place_index, for the map and the journey) and the dungeon under it (place_block, as travel names it inside). */
        r->place_dungeon[i] = 1;
        for (tries = 0; tries < 8; tries++) {
            if (pick_location(how->region, 1, -1, &index) && exterior_load_at(how->region, index, &outside)
                && dungeon_under(how->region, outside.location_id) >= 0) {
                r->place_index[i] = (df_i16)index;
                r->place_block[i] = (df_i16)dungeon_under(how->region, outside.location_id);
                return 1;
            }
        }
        return 0;
    }
    /* A building: in this town, or in another of the region. */
    if (q->general == 1) {
        if (!pick_building(how->region, how->index, q->type, &block, &record)) {
            return 0;
        }
        r->place_index[i] = (df_i16)how->index;
    } else {
        for (tries = 0; tries < 8; tries++) {
            if (pick_location(how->region, 0, how->index, &index) && pick_building(how->region, index, q->type, &block, &record)) {
                break;
            }
        }
        if (tries == 8) {
            return 0;
        }
        r->place_index[i] = (df_i16)index;
    }
    r->place_block[i] = (df_i16)block;
    r->place_record[i] = (df_i16)record;
    return 1;
}

/* DFU's Item.CreateGold: 150..200 x (the guild rank + 1, else level / 2 + 1, to 10), by the region's prices and the guild's power. */
static int reward_gold(const QuestStart *how, int level)
{
    int mod;
    int power;
    int amount;

    mod = how->guild ? how->rank + 1 : level / 2 + 1;
    mod = mod > 10 ? 10 : mod;
    power = how->guild ? how->power : 50;
    amount = 150 * mod + combat_roll(50 * mod + 1);
    return amount * (how->prices / 2 + 500) / 1000 * (power + 50) / 100;
}

/* ---- Running. ---- */

static int find_state_by_symbol(const Slot *s, df_u32 symbol)
{
    int i;

    for (i = 0; i < s->qbn.count[QBN_STATES]; i++) {
        if (s->qbn.states[i].symbol == symbol) {
            return i;
        }
    }
    return -1;
}

/* Timer t's length in minutes: random, fixed, or by the way to its place and back with a day to spare. */
static int timer_length(const Slot *s, int t)
{
    const QbnTimer *q;
    int at;

    q = &s->qbn.timers[t];
    if (q->type == 0 && q->most > q->least) {
        return (int)q->least + combat_roll((int)(q->most - q->least) + 1);
    }
    if (q->type == 1 && q->least > 0) {
        return (int)q->least;
    }
    at = (int)q->link[0];
    if (at >= 0 && at < s->qbn.count[QBN_PLACES] && at < QUEST_PLACES) {
        int sx;
        int sy;
        int ex;
        int ey;

        if (worldmap_pixel_of(s->run.region, s->run.index, &sx, &sy)
            && worldmap_pixel_of(s->run.place_region[at], s->run.place_index[at], &ex, &ey)) {
            Journey j;

            journey_plan(sx, sy, ex, ey, 0, 1, 0, JOURNEY_FOOT, 0, &j);
            return 2 * j.minutes + DAY;
        }
    }
    return TIMER_DEFAULT;
}

/* What an action record does, its state now true. 1 when it ended the quest. */
static int act(Slot *s, const QbnLogic *l, int now)
{
    QuestRun *r;
    int k;

    r = &s->run;
    switch (l->opcode) {
    case OP_TIMER_START:
        if (l->arg[1].value < QUEST_TIMERS && (int)l->arg[1].value < s->qbn.count[QBN_TIMERS]) {
            r->timer_end[l->arg[1].value] = now + timer_length(s, (int)l->arg[1].value);
        }
        break;
    case OP_TIMER_STOP:
        if (l->arg[1].value < QUEST_TIMERS) {
            r->timer_end[l->arg[1].value] = 0;
        }
        break;
    case OP_LOG:
        r->log[l->arg[2].value % QUEST_LOG] = (df_i16)l->arg[1].value;
        break;
    case OP_UNLOG:
        r->log[l->arg[1].value % QUEST_LOG] = 0;
        break;
    case OP_TO_MAP:
        if (l->arg[1].value < QUEST_PLACES) {
            int px;
            int py;

            if (worldmap_pixel_of(r->place_region[l->arg[1].value], r->place_index[l->arg[1].value], &px, &py)) {
                atlas_learn(worldmap_location_id(px, py));
            }
        }
        break;
    case OP_FOE_AT:
        if (l->arg[1].value < QUEST_FOES && l->arg[2].value < QUEST_PLACES) {
            r->foe_at[l->arg[1].value] = (df_i16)l->arg[2].value;
        }
        break;
    case OP_PERSON_AT:
        if (l->arg[1].value < QUEST_PEOPLE && l->arg[2].value < QUEST_PLACES) {
            r->person_at[l->arg[1].value] = (df_i16)l->arg[2].value;
        }
        break;
    case OP_GIVE_ITEM:
    case OP_ITEM_TO_PLAYER:
        /* To the player (no person, or the 0x4C form): held by the quest, shown in its log. */
        if (l->arg[1].value < QUEST_ITEMS && (l->opcode == OP_ITEM_TO_PLAYER || l->arg[2].value == NO_STATE)) {
            r->item_held |= 1u << l->arg[1].value;
        }
        break;
    case OP_REWARD:
        for (k = 1; k < l->count; k++) {
            df_u32 v;

            v = l->arg[k].value;
            if (v < QUEST_ITEMS && (int)v < s->qbn.count[QBN_ITEMS] && s->qbn.items[v].reward == 2 && g_player) {
                g_player->pack.gold += (df_u32)r->gold[v];
            }
        }
        break;
    case OP_CLEAR:
        for (k = 1; k < l->count; k++) {
            if (l->arg[k].value != FILLER) {
                set_state(r, (int)l->arg[k].value, 0);
            }
        }
        break;
    case OP_RANDOM:
    {
        int choices[4];
        int n;

        n = 0;
        for (k = 1; k < l->count && n < 4; k++) {
            if (l->arg[k].value != FILLER) {
                choices[n++] = (int)l->arg[k].value;
            }
        }
        if (n) {
            set_state(r, choices[combat_roll(n)], 1);
        }
        break;
    }
    case OP_CYCLE:
        /* The state set now cleared and the next set, round to the first. */
        for (k = 1; k < l->count; k++) {
            if (state_of(r, (int)l->arg[k].value)) {
                set_state(r, (int)l->arg[k].value, 0);
                set_state(r, (int)l->arg[k + 1 < l->count ? k + 1 : 1].value, 1);
                break;
            }
        }
        break;
    case OP_HIDE:
    case OP_REVEAL:
        if (l->arg[1].value < QUEST_PEOPLE) {
            if (l->opcode == OP_HIDE) {
                r->person_hidden |= 1u << l->arg[1].value;
            } else {
                r->person_hidden &= ~(1u << l->arg[1].value);
            }
        }
        break;
    case OP_REPUTATION:
        if (l->arg[1].value < QUEST_PEOPLE && (int)l->arg[1].value < s->qbn.count[QBN_PEOPLE]) {
            int faction;

            faction = s->qbn.people[l->arg[1].value].faction ? s->qbn.people[l->arg[1].value].faction : r->giver_faction;
            standing_change(faction, (int)(df_i32)l->arg[2].value, 1);
        }
        break;
    case OP_LEGAL:
    {
        RegionStanding *reg;

        reg = standing_region(r->region);
        if (reg) {
            reg->legal = (short)(reg->legal + (int)(df_i32)l->arg[1].value);
        }
        break;
    }
    case OP_END:
        say(s, l->message);
        return 1;
    default:
        break;
    }
    say(s, l->message);
    return 0;
}

/* Whether a record is a trigger: its state is set by an event, not acted on. */
static int is_trigger(int opcode)
{
    return opcode == OP_ITEM_TO_PERSON || opcode == OP_KILLS || opcode == OP_MEET;
}

static void end_quest(Slot *s)
{
    s->used = 0;
    if (s->heap) {
        mem_heap_delete(s->heap);
        s->heap = 0;
    }
}

/* The logic over and over until it settles: AND and OR set their states; actions run once a state comes true. */
static void run(Slot *s, int now)
{
    QuestRun *r;
    int pass;
    int i;

    r = &s->run;
    for (pass = 0; pass < PASSES; pass++) {
        int changed;

        changed = 0;
        for (i = 0; i < s->qbn.count[QBN_LOGIC] && i < QUEST_LOGIC; i++) {
            const QbnLogic *l;
            int on;
            int k;

            l = &s->qbn.logic[i];
            if (l->opcode == OP_AND || l->opcode == OP_OR) {
                on = l->opcode == OP_AND;
                for (k = 1; k < l->count; k++) {
                    if (l->arg[k].value == FILLER) {
                        continue;
                    }
                    if (l->opcode == OP_AND) {
                        on = on && operand_true(r, &l->arg[k]);
                    } else {
                        on = on || operand_true(r, &l->arg[k]);
                    }
                }
                changed |= set_state(r, (int)l->arg[0].value, on ^ l->arg[0].negate);
                continue;
            }
            if (is_trigger(l->opcode)) {
                /* A kill count's state follows the kills; the others are set by quest_met. */
                if (l->opcode == OP_KILLS && l->arg[1].value < QUEST_FOES && !state_of(r, (int)l->arg[0].value)
                    && r->foe_killed[l->arg[1].value] >= (int)l->arg[2].value && l->arg[2].value > 0) {
                    set_state(r, (int)l->arg[0].value, 1);
                    say(s, l->message);
                    changed = 1;
                }
                continue;
            }
            on = l->arg[0].value == NO_STATE ? 1 : operand_true(r, &l->arg[0]);
            if (on && !fired(r, i)) {
                set_fired(r, i, 1);
                changed = 1;
                if (act(s, l, now)) {
                    end_quest(s);
                    return;
                }
            } else if (!on && fired(r, i) && l->arg[0].value != NO_STATE) {
                /* False again: it may act again when it next comes true. */
                set_fired(r, i, 0);
            }
        }
        if (!changed) {
            return;
        }
    }
}

/* ---- The slots. ---- */

void quest_new_game(void)
{
    int i;

    for (i = 0; i < QUEST_SLOTS; i++) {
        end_quest(&g_slots[i]);
        mem_zero(&g_slots[i].run, sizeof(g_slots[i].run));
    }
    g_message_count = 0;
}

const QuestRun *quest_run(int slot)
{
    return slot >= 0 && slot < QUEST_SLOTS && g_slots[slot].used ? &g_slots[slot].run : 0;
}

int quest_running(int quest)
{
    int i;

    for (i = 0; i < QUEST_SLOTS; i++) {
        if (g_slots[i].used && g_slots[i].run.quest == quest) {
            return i;
        }
    }
    return -1;
}

const char *quest_file(int slot)
{
    return slot >= 0 && slot < QUEST_SLOTS && g_slots[slot].used ? file_of(&g_slots[slot]) : "";
}

/* The slot's QBN read into a heap of its own. 0 when it does not read. */
static int load_logic(Slot *s)
{
    MemHeap *was;
    char file[16];
    int ok;

    s->heap = mem_heap_new();
    if (!s->heap) {
        return 0;
    }
    was = mem_heap_use(s->heap);
    log_format(file, (int)sizeof(file), "%s.QBN", file_of(s));
    ok = qbn_load(file, &s->qbn);
    mem_heap_use(was);
    if (!ok) {
        mem_heap_delete(s->heap);
        s->heap = 0;
    }
    return ok;
}

int quest_start(Player *p, int quest, const QuestStart *how, int now)
{
    Slot *s;
    int slot;
    int i;

    if (!quest_kind(quest) || quest_running(quest) >= 0) {
        return -1;
    }
    for (slot = 0; slot < QUEST_SLOTS && g_slots[slot].used; slot++) {
    }
    if (slot == QUEST_SLOTS) {
        return -1;
    }
    s = &g_slots[slot];
    mem_zero(&s->run, sizeof(s->run));
    s->run.id = (df_u16)slot;
    s->run.quest = quest;
    if (!load_logic(s)) {
        return -1;
    }
    s->run.started = now;
    s->run.person_seed = how->seed;
    s->run.giver_block = how->giver_block;
    s->run.giver_record = how->giver_record;
    s->run.giver_faction = how->giver_faction;
    s->run.region = how->region;
    s->run.index = how->index;
    for (i = 0; i < QUEST_PEOPLE; i++) {
        s->run.person_at[i] = -1;
    }
    for (i = 0; i < QUEST_FOES; i++) {
        s->run.foe_at[i] = -1;
    }
    for (i = 0; i < s->qbn.count[QBN_PLACES] && i < QUEST_PLACES; i++) {
        if (!bind_place(s, i, how)) {
            log_print(LOG_INFO, "quest: %s's place %d not found", file_of(s), i);
            end_quest(s);
            return -1;
        }
    }
    for (i = 0; i < s->qbn.count[QBN_ITEMS] && i < QUEST_ITEMS; i++) {
        if (s->qbn.items[i].reward == 2) {
            s->run.gold[i] = reward_gold(how, p->sheet.level);
        }
    }
    s->used = 1;
    g_player = p;
    run(s, now);
    return s->used ? slot : -1;
}

void quest_put(const QuestRun *saved)
{
    Slot *s;
    int slot;

    slot = saved->id;
    if (slot < 0 || slot >= QUEST_SLOTS) {
        return;
    }
    s = &g_slots[slot];
    end_quest(s);
    s->run = *saved;
    s->used = load_logic(s);
}

/* The person of slot s met here, or -1: the giver by their building and faction; another by the building they were put in. */
static int person_here(const Slot *s, int i, int region, int index, int block, int record, int faction)
{
    const QbnPerson *who;
    int at;

    who = &s->qbn.people[i];
    if (s->run.person_hidden & (1u << i)) {
        return 0;
    }
    if (who->symbol == qbn_hash("qgiver") || who->symbol == qbn_hash("questgiver")) {
        return region == s->run.region && index == s->run.index && block == s->run.giver_block && record == s->run.giver_record
               && faction == s->run.giver_faction;
    }
    at = s->run.person_at[i];
    /* Until quest people stand in the world, anyone of their faction (any, for none) in their building is them. */
    return at >= 0 && !s->run.place_dungeon[at] && s->run.place_region[at] == region && s->run.place_index[at] == index
           && s->run.place_block[at] == block && s->run.place_record[at] == record && (!who->faction || who->faction == faction);
}

void quest_met(Player *p, int region, int index, int block, int record, int faction, int now)
{
    int slot;

    g_player = p;
    for (slot = 0; slot < QUEST_SLOTS; slot++) {
        Slot *s;
        int i;

        s = &g_slots[slot];
        if (!s->used) {
            continue;
        }
        for (i = 0; i < s->qbn.count[QBN_LOGIC] && i < QUEST_LOGIC; i++) {
            const QbnLogic *l;
            df_u32 who;

            l = &s->qbn.logic[i];
            if (l->opcode != OP_MEET && l->opcode != OP_ITEM_TO_PERSON) {
                continue;
            }
            who = l->opcode == OP_MEET ? l->arg[1].value : l->arg[2].value;
            if (who >= QUEST_PEOPLE || (int)who >= s->qbn.count[QBN_PEOPLE] || state_of(&s->run, (int)l->arg[0].value)
                || !person_here(s, (int)who, region, index, block, record, faction)) {
                continue;
            }
            /* An item given: only one the quest has handed the player, and it is gone. */
            if (l->opcode == OP_ITEM_TO_PERSON) {
                if (l->arg[1].value >= QUEST_ITEMS || !(s->run.item_held & (1u << l->arg[1].value))) {
                    continue;
                }
                s->run.item_held &= ~(1u << l->arg[1].value);
            }
            set_state(&s->run, (int)l->arg[0].value, 1);
            say(s, l->message);
        }
        run(s, now);
    }
}

void quest_killed(Player *p, int kind, int region, int index, int now)
{
    int slot;

    /* Combat knows no player: the last one the quests saw stands. */
    if (p) {
        g_player = p;
    }
    for (slot = 0; slot < QUEST_SLOTS; slot++) {
        Slot *s;
        int i;

        s = &g_slots[slot];
        if (!s->used) {
            continue;
        }
        for (i = 0; i < s->qbn.count[QBN_FOES] && i < QUEST_FOES; i++) {
            int at;

            at = s->run.foe_at[i];
            if (at >= 0 && s->qbn.foes[i].kind == kind && s->run.place_dungeon[at]
                && (index < 0 || (s->run.place_region[at] == region && s->run.place_block[at] == index))) {
                s->run.foe_killed[i]++;
                break;
            }
        }
        run(s, now);
    }
}

void quest_tick(Player *p, int now)
{
    int slot;

    g_player = p;
    for (slot = 0; slot < QUEST_SLOTS; slot++) {
        Slot *s;
        int t;

        s = &g_slots[slot];
        if (!s->used) {
            continue;
        }
        for (t = 0; t < s->qbn.count[QBN_TIMERS] && t < QUEST_TIMERS; t++) {
            if (s->run.timer_end[t] > 0 && now >= s->run.timer_end[t]) {
                s->run.timer_end[t] = 0;
                set_state(&s->run, find_state_by_symbol(s, s->qbn.timers[t].symbol), 1);
            }
        }
        run(s, now);
    }
}

int quest_log_line(int slot, int i, char *out, int cap)
{
    const Slot *s;

    if (slot < 0 || slot >= QUEST_SLOTS || !g_slots[slot].used || i < 0 || i >= QUEST_LOG || !g_slots[slot].run.log[i]) {
        return 0;
    }
    s = &g_slots[slot];
    fill_message(s, s->run.log[i], out, cap);
    return out[0] != 0;
}

int quest_foes_at(int region, int index, int *kinds, int cap)
{
    int n;
    int slot;

    n = 0;
    for (slot = 0; slot < QUEST_SLOTS; slot++) {
        const Slot *s;
        int i;

        s = &g_slots[slot];
        for (i = 0; s->used && i < s->qbn.count[QBN_FOES] && i < QUEST_FOES; i++) {
            int at;
            int k;

            at = s->run.foe_at[i];
            if (at < 0 || !s->run.place_dungeon[at] || s->run.place_region[at] != region || s->run.place_block[at] != index) {
                continue;
            }
            for (k = s->run.foe_killed[i]; k < s->qbn.foes[i].count && n < cap; k++) {
                kinds[n++] = s->qbn.foes[i].kind;
            }
        }
    }
    return n;
}

void quest_say(int slot, int message)
{
    if (slot >= 0 && slot < QUEST_SLOTS && g_slots[slot].used) {
        say(&g_slots[slot], message);
    }
}

int quest_state(int slot, int state)
{
    return slot >= 0 && slot < QUEST_SLOTS && g_slots[slot].used && state_of(&g_slots[slot].run, state);
}
