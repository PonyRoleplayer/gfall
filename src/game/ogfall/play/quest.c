#include "game/ogfall/play/quest.h"
#include "game/ogfall/play/state.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/ogfall/tables/questdata.h"

/* See quest.h. */

static OgQuest g_quest[OG_QUESTS];
static df_u8 g_global[64];
static int (*g_read)(const char *quest, int message, int in_text_rsc, char *out, int cap);
static void (*g_say)(const char *text);
static int (*g_ask)(const char *words, void (*answer)(int yes));
static int g_offer_slot = -1;

void og_quest_set_text(int (*read)(const char *quest, int message, int in_text_rsc, char *out, int cap)) { g_read = read; }
void og_quest_set_say(void (*say)(const char *text)) { g_say = say; }
void og_quest_set_ask(int (*ask)(const char *words, void (*answer)(int yes))) { g_ask = ask; }
const df_u8 *og_quest_globals(void) { return g_global; }
void og_quest_globals_clear(void) { mem_zero(g_global, sizeof(g_global)); }
void og_quest_reset(void) { mem_zero(g_quest, sizeof(g_quest)); mem_zero(g_global, sizeof(g_global)); g_offer_slot = -1; }
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
    char text[1024];

    if (message == -1 || !g_read || !g_say) { return; }
    text[0] = 0;
    if (g_read(q->qbn.name, message & 0x7fff, (message & 0x8000) != 0, text, sizeof(text)) > 0 && text[0]) { g_say(text); }
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
    if (g_read && g_say && g_read(g_quest[slot].qbn.name, yes ? 1002 : 1001, 0, text, sizeof(text)) > 0 && text[0]) { g_say(text); }
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
        g_offer_slot = slot;
        if (!g_ask(text, offer_answer)) { g_offer_slot = -1; og_quest_end(slot); return -1; }
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
