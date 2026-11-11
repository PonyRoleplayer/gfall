#include "game/ogfall/play/quest.h"
#include "game/ogfall/play/state.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/ogfall/tables/questdata.h"
#include "game/ogfall/tables/combatdefs.h"
#include "game/ogfall/tables/datedefs.h"

/* See quest.h. */

static OgQuest g_quest[OG_QUESTS];
static df_u8 g_global[64];
static int (*g_read)(const char *quest, int message, int in_text_rsc, char *out, int cap);
static void (*g_say)(const char *text);
static int (*g_ask)(const char *words, void (*answer)(int yes));
static int g_offer_slot = -1;
static const OgQuestNames *g_names;
static int (*g_macros)(const char *in, char *out, int cap);
static const char *(*g_place)(void);
static OgLogSlot g_log[OG_LOG_SLOTS];

void og_quest_set_text(int (*read)(const char *quest, int message, int in_text_rsc, char *out, int cap)) { g_read = read; }
void og_quest_set_say(void (*say)(const char *text)) { g_say = say; }
void og_quest_set_ask(int (*ask)(const char *words, void (*answer)(int yes))) { g_ask = ask; }
const df_u8 *og_quest_globals(void) { return g_global; }
void og_quest_globals_clear(void) { mem_zero(g_global, sizeof(g_global)); }
void og_quest_reset(void) { mem_zero(g_quest, sizeof(g_quest)); mem_zero(g_global, sizeof(g_global)); mem_zero(g_log, sizeof(g_log)); g_offer_slot = -1; }
void og_quest_set_names(const OgQuestNames *names) { g_names = names; }
void og_quest_set_macros(int (*fill)(const char *in, char *out, int cap)) { g_macros = fill; }
void og_quest_set_place(const char *(*current)(void)) { g_place = current; }
int og_quest_count(void) { return OG_QUESTS; }
const OgQuest *og_quest(int slot) { return slot >= 0 && slot < OG_QUESTS && g_quest[slot].used ? &g_quest[slot] : 0; }

void og_quest_end(int slot)
{
    if (slot >= 0 && slot < OG_QUESTS) { mem_zero(&g_quest[slot], sizeof(g_quest[slot])); }
}

static int start_with(const char *name, const df_u8 *bytes, df_u32 size, int faction)
{
    int slot, id, taken, k;

    for (slot = 0; slot < OG_QUESTS && g_quest[slot].used; slot++) { }
    if (slot == OG_QUESTS) { return -1; }
    for (id = 1; id < 256; id++) {
        taken = 0;
        for (k = 0; k < OG_QUESTS; k++) { if (g_quest[k].used && g_quest[k].qbn.id == id) { taken = 1; } }
        if (!taken) { break; }
    }
    if (id == 256) { return -1; }
    mem_zero(&g_quest[slot], sizeof(g_quest[slot]));
    if (bytes ? !og_qbn_parse(bytes, size, &g_quest[slot].qbn) : !og_qbn_load(name, &g_quest[slot].qbn)) { return -1; }
    if (bytes) { for (k = 0; k < 8 && name[k]; k++) { g_quest[slot].qbn.name[k] = name[k]; } g_quest[slot].qbn.name[k] = 0; }
    g_quest[slot].used = 1;
    g_quest[slot].qbn.id = id; g_quest[slot].qbn.faction = faction;
    return slot;
}

int og_quest_start(const char *name, int faction) { return start_with(name, 0, 0, faction); }
int og_quest_start_bytes(const char *name, const df_u8 *bytes, df_u32 size, int faction) { return start_with(name, bytes, size, faction); }

int og_quest_pick(char letter, char letter2, char membership, int rank, char *name)
{
    int i, n, pick, count;
    const char *q;

    count = 0;
    for (i = 0; i < (int)(sizeof(g_quest_name) / sizeof(g_quest_name[0])); i++) {
        q = g_quest_name[i];
        if ((q[0] == letter || (letter2 && q[0] == letter2)) && q[1] == '0' && q[2] == membership && q[3] - '0' <= rank) { count++; }
    }
    if (!count) { return 0; }
    pick = og_random_range(0, count - 1); n = 0;
    for (i = 0; i < (int)(sizeof(g_quest_name) / sizeof(g_quest_name[0])); i++) {
        q = g_quest_name[i];
        if (!((q[0] == letter || (letter2 && q[0] == letter2)) && q[1] == '0' && q[2] == membership && q[3] - '0' <= rank)) { continue; }
        if (n++ == pick) { int k; for (k = 0; k < 8; k++) { name[k] = q[k]; } name[8] = 0; return 1; }
    }
    return 0;
}

int og_quest_active(const char *name)
{
    int i, k;

    for (i = 0; i < OG_QUESTS; i++) {
        if (!g_quest[i].used) { continue; }
        for (k = 0; k < 8 && name[k] && name[k] == g_quest[i].qbn.name[k]; k++) { }
        if (k == 8 || (!name[k] && !g_quest[i].qbn.name[k])) { return 1; }
    }
    return 0;
}

/* ---- Text (parse_expand, quest_symbol_text) ---- */

static int put(char *out, int cap, int at, const char *s)
{
    while (*s && at < cap - 1) { out[at++] = *s++; }
    return at;
}

/* A symbol said: items, places, persons, foes, timers, the first whose hash it is; blank for none. */
static int symbol_text(int slot, df_i32 hash, int form, df_i32 hash2, char *out, int cap)
{
    const OgQuest *q;
    int i;
    df_u32 delay;

    out[0] = 0;
    q = &g_quest[slot];
    for (i = 0; i < q->qbn.n_item; i++) { if (q->qbn.item[i].symbol == hash) { return g_names && g_names->item && g_names->item(slot, i, form, out, cap); } }
    for (i = 0; i < q->qbn.n_place; i++) { if (q->qbn.place[i].symbol == hash) { return g_names && g_names->place && g_names->place(slot, i, form, out, cap); } }
    for (i = 0; i < q->qbn.n_person; i++) { if (q->qbn.person[i].symbol == hash) { return g_names && g_names->person && g_names->person(slot, i, form, out, cap); } }
    for (i = 0; i < q->qbn.n_foe; i++) {
        if (q->qbn.foe[i].symbol == hash) {
            const OgFoeDef *def;

            def = og_foe_def(q->qbn.foe[i].type);
            return log_format(out, cap, "%s", def ? def->name : "");
        }
    }
    for (i = 0; i < q->qbn.n_timer; i++) {
        if (q->qbn.timer[i].hash == hash) {
            delay = q->qbn.timer[i].delay;
            /* _a`b_: the second timer's delay is added to the first's. */
            for (i = 0; hash2 && i < q->qbn.n_timer; i++) { if (q->qbn.timer[i].hash == hash2) { delay += q->qbn.timer[i].delay; } }
            return form == 0 ? log_format(out, cap, "%d", (int)delay) : log_format(out, cap, "%d", (int)((delay + 1439u) / 1440u));
        }
    }
    return 0;
}

int og_quest_expand(int slot, const char *in, char *out, int cap, df_u32 minutes, const char *place)
{
    static char stage[2048];
    char name[24], name2[24], said[256];
    int at, n, k, form, second;
    char lead;

    if (cap < 1) { return 0; }
    at = 0;
    while (*in && at < (int)sizeof(stage) - 1) {
        if ((*in == '_' || *in == '=') && slot >= 0 && slot < OG_QUESTS && g_quest[slot].used) {
            lead = *in; n = 0;
            while (in[n] == lead) { n++; }
            k = 0; second = 0;
            while (in[n + k] && in[n + k] != '_' && in[n + k] != ' ' && in[n + k] != '\n' && k < 20) {
                if (in[n + k] == '`') { second = k + 1; }
                k++;
            }
            if (in[n + k] == '_' && k > 0) {
                int j;

                for (j = 0; j < k && (!second || j < second - 1); j++) { name[j] = in[n + j]; }
                name[j] = 0; name2[0] = 0;
                if (second) { int m; for (m = 0; second + m < k; m++) { name2[m] = in[n + second + m]; } name2[m] = 0; }
                form = lead == '_' ? (n - 1) * 16 : n;
                said[0] = 0;
                symbol_text(slot, og_qbn_hash(name), form, name2[0] ? og_qbn_hash(name2) : 0, said, sizeof(said));
                at = put(stage, sizeof(stage), at, said);
                in += n + k + 1;
                continue;
            }
        }
        stage[at++] = *in++;
    }
    stage[at] = 0;
    /* %qdt: the date a log entry was written; %cn: its place, when it has one. */
    {
        static char second[2048];
        int s, d;
        char date[64];

        s = 0;
        for (d = 0; stage[d] && s < (int)sizeof(second) - 1; ) {
            if (stage[d] == '%' && stage[d + 1] == 'q' && stage[d + 2] == 'd' && stage[d + 3] == 't') {
                og_date_text(minutes, date, sizeof(date)); s = put(second, sizeof(second), s, date); d += 4;
            } else if (place && stage[d] == '%' && stage[d + 1] == 'c' && stage[d + 2] == 'n') {
                s = put(second, sizeof(second), s, place); d += 3;
            } else { second[s++] = stage[d++]; }
        }
        second[s] = 0;
        if (g_macros) { return g_macros(second, out, cap); }
        out[0] = 0; at = put(out, cap, 0, second); out[at] = 0;
        return at;
    }
}

/* ---- The logbook (logbook.c) ---- */

const OgLogSlot *og_logbook_slot(int i) { return i >= 0 && i < OG_LOG_SLOTS ? &g_log[i] : 0; }
void og_logbook_clear(void) { mem_zero(g_log, sizeof(g_log)); }

/* logbook_prune_quests: the slot of a quest that is not running is free. */
static void prune(void)
{
    int i, k, alive;

    for (i = 0; i < OG_LOG_SLOTS; i++) {
        if (!g_log[i].used) { continue; }
        alive = 0;
        for (k = 0; k < OG_QUESTS; k++) { if (g_quest[k].used && g_quest[k].qbn.id == g_log[i].quest_id) { alive = 1; } }
        if (!alive) { mem_zero(&g_log[i], sizeof(g_log[i])); }
    }
}

void og_logbook_add(int quest_slot, int index, int message)
{
    int i, found, k;
    const char *where;

    if (!og_quest(quest_slot)) { return; }
    prune();
    found = -1;
    for (i = 0; i < OG_LOG_SLOTS; i++) { if (g_log[i].used && g_log[i].quest_id == g_quest[quest_slot].qbn.id) { found = i; } }
    if (found < 0) {
        for (i = 0; i < OG_LOG_SLOTS && g_log[i].used; i++) { }
        /* A full log writes nothing and the quest goes on (7.7: the original wrote slot -1). */
        if (i == OG_LOG_SLOTS) { return; }
        found = i;
        mem_zero(&g_log[found], sizeof(g_log[found]));
        g_log[found].used = 1; g_log[found].quest_id = g_quest[quest_slot].qbn.id;
        for (k = 0; k < 8; k++) { g_log[found].name[k] = g_quest[quest_slot].qbn.name[k]; }
    }
    index = ((index % 10) + 10) % 10;
    g_log[found].message[index] = message;
    g_log[found].minutes[index] = og_state()->minutes;
    where = g_place ? g_place() : 0;
    for (k = 0; where && where[k] && k < 31; k++) { g_log[found].place[index][k] = where[k]; }
    g_log[found].place[index][k] = 0;
}

void og_logbook_remove(int quest_slot, int index)
{
    int i;

    if (!og_quest(quest_slot)) { return; }
    index = ((index % 10) + 10) % 10;
    for (i = 0; i < OG_LOG_SLOTS; i++) {
        if (g_log[i].used && g_log[i].quest_id == g_quest[quest_slot].qbn.id) { g_log[i].message[index] = 0; g_log[i].minutes[index] = 0; g_log[i].place[index][0] = 0; }
    }
}

int og_logbook_text(char *out, int cap)
{
    int i, k, n, quest;
    char raw[1024], said[1024];

    prune();
    n = 0; out[0] = 0;
    for (i = 0; i < OG_LOG_SLOTS; i++) {
        if (!g_log[i].used) { continue; }
        for (quest = 0; quest < OG_QUESTS && !(g_quest[quest].used && g_quest[quest].qbn.id == g_log[i].quest_id); quest++) { }
        if (quest == OG_QUESTS) { continue; }
        for (k = 0; k < 10; k++) {
            if (!g_log[i].message[k] || !g_read) { continue; }
            raw[0] = 0;
            if (g_read(g_log[i].name, g_log[i].message[k], 0, raw, sizeof(raw)) <= 0 || !raw[0]) { continue; }
            og_quest_expand(quest, raw, said, sizeof(said), g_log[i].minutes[k], g_log[i].place[k]);
            n += log_format(out + n, cap - n, "%s\n", said);
        }
    }
    return n;
}

/* ---- States (quest_arg_state, quest_set_state) ---- */

static int state_value(const OgQuest *q, int index)
{
    const OgQbnState *st;

    if (index < 0 || index >= q->qbn.n_state) { return 0; }
    st = &q->qbn.state[index];
    return st->is_global ? g_global[st->value & 63] : st->value;
}

static void state_store(OgQuest *q, int index, int value)
{
    OgQbnState *st;

    if (index < 0 || index >= q->qbn.n_state) { return; }
    st = &q->qbn.state[index];
    if (st->is_global) { g_global[st->value & 63] = (df_u8)(value ? 1 : 0); } else { st->value = value ? 1 : 0; }
}

int og_quest_state(int slot, int index) { return og_quest(slot) ? state_value(&g_quest[slot], index) : 0; }
void og_quest_set_state(int slot, int index, int value) { if (og_quest(slot)) { state_store(&g_quest[slot], index, value); } }

int og_quest_state_by_hash(int slot, df_i32 hash)
{
    int i;

    if (!og_quest(slot)) { return 0; }
    for (i = 0; i < g_quest[slot].qbn.n_state; i++) { if (g_quest[slot].qbn.state[i].hash == hash) { return state_value(&g_quest[slot], i); } }
    return 0;
}

/* quest_arg_state: -1 is always true, -2 never; else the state's value, taken the other way by the negate bit. */
static int arg_state(const OgQuest *q, const OgQbnOp *op, int n)
{
    const OgQbnArg *a;
    int v;

    if (n >= op->argc) { return 1; }
    a = &op->arg[n];
    if (a->literal) { return a->negate ? 0 : 1; }
    if (a->value == -1) { return 1; }
    if (a->value == -2) { return 0; }
    v = state_value(q, a->value) != 0;
    return a->negate ? !v : v;
}

/* quest_set_state: a state named by argument n takes value, the other way when negated; "always" and "none" take nothing. */
static void arg_set(OgQuest *q, const OgQbnOp *op, int n, int value)
{
    const OgQbnArg *a;

    if (n >= op->argc) { return; }
    a = &op->arg[n];
    if (a->literal || a->value < 0) { return; }
    state_store(q, a->value, a->negate ? !value : value);
}

/* ---- The messages ---- */

static void show(const OgQuest *q, int message)
{
    char text[1024], said[1024];

    if (message == -1 || !g_read || !g_say) { return; }
    text[0] = 0;
    if (g_read(q->qbn.name, message & 0x7fff, (message & 0x8000) != 0, text, sizeof(text)) > 0 && text[0]) {
        og_quest_expand((int)(q - g_quest), text, said, sizeof(said), og_state()->minutes, 0);
        g_say(said);
    }
}

/* quest_op_done: the done bit, and the message the first time. */
static void done(OgQuest *q, OgQbnOp *op)
{
    if (op->flags & 1) { return; }
    op->flags |= 1;
    show(q, op->message);
}

/* ---- The ops of this step ---- */

static int minute_of_day(void) { return (int)(og_state()->minutes % 1440u); }

static void run_op(int slot, OgQuest *q, OgQbnOp *op)
{
    int i, n, count, pick, found;

    switch (op->opcode) {
    case 6:
        /* The end: when set (no delivery pending and an empty reward box come with the rewards, step 7). The reputation, the rumours and the objects are later steps. */
        if (arg_state(q, op, 0)) { done(q, op); q->ended = 1; }
        break;
    case 7:
        if (arg_state(q, op, 0)) { for (i = 1; i < op->argc; i++) { arg_set(q, op, i, 0); } }
        break;
    case 16:
        /* The day of the world in a range (no file uses it). */
        arg_set(q, op, 0, (int)(og_state()->minutes / 1440u) >= op->arg[1].value && (int)(og_state()->minutes / 1440u) <= op->arg[2].value);
        break;
    case 23:
        /* Once: entry `index` of the quest's log is the message. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) { og_logbook_add(slot, op->arg[2].value, op->arg[1].value); done(q, op); }
        break;
    case 24:
        if (arg_state(q, op, 0) && !(op->flags & 1)) { og_logbook_remove(slot, op->arg[1].value); done(q, op); }
        break;
    case 31: {
        int from, to, m, inside;

        from = op->arg[1].value; to = op->arg[2].value; m = minute_of_day();
        inside = from >= to ? (m >= from || m <= to) : (m >= from && m <= to);
        arg_set(q, op, 0, inside);
        break;
    }
    case 34:
        /* Once: one of the given states at random. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            count = 0;
            for (i = 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value >= 0) { count++; } }
            if (count) {
                pick = og_random_range(1, count); n = 0;
                for (i = 1; i < op->argc; i++) {
                    if (op->arg[i].literal || op->arg[i].value < 0) { continue; }
                    if (++n == pick) { arg_set(q, op, i, 1); break; }
                }
            }
            done(q, op);
        }
        break;
    case 35:
        /* Once: the first of the given states that is set is cleared and the next is set; none set: the first is. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            found = -1;
            for (i = 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value >= 0 && arg_state(q, op, i)) { found = i; break; } }
            if (found < 0) {
                for (i = 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value >= 0) { arg_set(q, op, i, 1); break; } }
            } else {
                arg_set(q, op, found, 0);
                for (i = found + 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value >= 0) { arg_set(q, op, i, 1); break; } }
            }
            done(q, op);
        }
        break;
    case 51:
        if (arg_state(q, op, 0)) { done(q, op); }
        break;
    case 52:
        /* Every run: all the given states true sets the state, which is never cleared by this op (7.9 kept). */
        n = 1;
        for (i = 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value == -2) { continue; } if (!arg_state(q, op, i)) { n = 0; } }
        if (n) { arg_set(q, op, 0, 1); }
        break;
    case 53:
        /* Every run: the state is whether any given state is true. */
        n = 0;
        for (i = 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value == -2) { continue; } if (arg_state(q, op, i)) { n = 1; } }
        arg_set(q, op, 0, n);
        break;
    case 77:
        arg_set(q, op, 0, og_character()->level >= op->arg[1].value);
        break;
    default:
        break;
    }
    (void)slot;
}

/* The player's answer to the offer: 1002 says yes, 1001 no, and a no ends the quest. */
static void offer_answer(int yes)
{
    int slot;
    char text[1024];

    slot = g_offer_slot;
    g_offer_slot = -1;
    if (slot < 0 || !g_quest[slot].used) { return; }
    text[0] = 0;
    if (g_read && g_say && g_read(g_quest[slot].qbn.name, yes ? 1002 : 1001, 0, text, sizeof(text)) > 0 && text[0]) {
        char said[1024];

        og_quest_expand(slot, text, said, sizeof(said), og_state()->minutes, 0);
        g_say(said);
    }
    if (!yes) { og_quest_end(slot); }
}

int og_quest_begin(const char *name, int faction)
{
    int slot, asked;
    char text[1024];

    /* quest_start refuses an empty name, N0C00Y01 and a quest already running. */
    if (!name || !name[0] || og_quest_active(name)) { return -1; }
    {
        static const char refused[] = "N0C00Y01";
        int k;

        for (k = 0; k < 8 && name[k] == refused[k]; k++) { }
        if (k == 8) { return -1; }
    }
    slot = og_quest_start(name, name[5] == 'Y' ? faction : 0);
    if (slot < 0) { return -1; }
    if (name[0] == 'S' || name[0] == 's') { return slot; }
    /* The Daedra's offers (T..Z, 1..9) and a Y quest ask; the princes' FLC is not played. Anything else is accepted silently. */
    text[0] = 0;
    asked = name[5] == 'Y' || (name[0] >= '1' && name[0] <= '9') || (name[0] >= 'T' && name[0] <= 'Z');
    if (asked && g_read && g_ask && g_read(g_quest[slot].qbn.name, 1000, 0, text, sizeof(text)) > 0 && text[0]) {
        char said[1024];

        og_quest_expand(slot, text, said, sizeof(said), og_state()->minutes, 0);
        g_offer_slot = slot;
        if (!g_ask(said, offer_answer)) { g_offer_slot = -1; og_quest_end(slot); return -1; }
    }
    return slot;
}

void og_quest_run(void)
{
    int slot, i;

    for (slot = 0; slot < OG_QUESTS; slot++) {
        OgQuest *q;

        q = &g_quest[slot];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_op && !q->ended; i++) { run_op(slot, q, &q->qbn.op[i]); }
        if (q->ended) { og_quest_end(slot); }
    }
}
