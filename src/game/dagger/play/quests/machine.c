#include "internal.h"
#include "game/dagger/play/ui/movie.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/tables/combatdefs.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/character/ailments.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/tables/itemdefs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

static int g_initial_pending;
static void state_store(QuestState *q, int index, int value);

/* quest_travel_minutes: the quests' journey estimate plus two days, a half more, and a week more for each end of a kind of dungeon the map holds (4, 7, 10, 12, 14). */
static df_u32 travel_delay(const QuestPlace *a, const QuestPlace *b)
{
    int px, py, region, here, sx, sy, minutes;
    df_u32 delay;

    if (!quest_world || !quest_world->journey || !quest_world->here) { return 2880u * 384u >> 8; }
    quest_world->here(&region, &here, &px, &py);
    sx = a ? a->px : px; sy = a ? a->py : py;
    minutes = quest_world->journey(sx, sy, b->px, b->py) + 2880;
    delay = ((df_u32)minutes * 384u) >> 8;
    return delay;
}

static int far_kind(int type) { return type == 4 || type == 7 || type == 10 || type == 12 || type == 14; }

void quest_timer_start(QuestState *q, int t)
{
    const QbnTimer *tm;
    QuestTimer *rt;
    df_u32 delay;
    int l1, l2;

    tm = &q->qbn.timer[t]; rt = &q->timer[t];
    l1 = (tm->flags & 0x300) ? -1 : tm->link1; l2 = (tm->flags & 0x300) ? -1 : tm->link2;
    delay = 0;
    if (tm->type == 0) { delay = (df_u32)random_range((int)tm->minimum, (int)tm->maximum); }
    else if (tm->type == 1) { delay = (df_u32)tm->minimum; }
    else if (tm->type == 2 || tm->type == 4) {
        if (l1 >= 0 && l1 < q->qbn.n_place && q->place[l1].bound) {
            delay = travel_delay(0, &q->place[l1]);
            if (far_kind(q->place[l1].type)) { delay += 10080u; }
        }
    } else if (tm->type == 3) {
        if (l1 >= 0 && l2 >= 0 && l1 < q->qbn.n_place && l2 < q->qbn.n_place && q->place[l1].bound && q->place[l2].bound) {
            delay = travel_delay(&q->place[l1], &q->place[l2]);
            if (far_kind(q->place[l1].type) || far_kind(q->place[l2].type)) { delay += 10080u; }
        }
    } else if (tm->type == 5) {
        if (l1 >= 0 && l2 >= 0 && l1 < q->qbn.n_place && l2 < q->qbn.n_place && q->place[l1].bound && q->place[l2].bound) {
            delay = travel_delay(0, &q->place[l1]) + travel_delay(&q->place[l1], &q->place[l2]);
            if (far_kind(q->place[l1].type)) { delay += 10080u; }
            if (far_kind(q->place[l2].type)) { delay += 10080u; }
        }
    }
    if (rt->flags & 16) { delay <<= 1; }
    rt->delay = delay; rt->start = game_state()->minutes;
}

/* The state whose hash is the timer's takes whether it has expired. */
static void timer_state(QuestState *q, int t, int value)
{
    int i;

    for (i = 0; i < q->qbn.n_state; i++) { if (q->qbn.state[i].hash == q->qbn.timer[t].hash) { state_store(q, i, value); } }
}

/* quest_timer_update, after the ops of a run: a pulse's expired state lasts one run; a running timer that has run past its delay is expired (latched or pulsed; toggled; else nothing: 7.15),
 * and starts again with bit 3 or stops. */
static void timers_run(QuestState *q)
{
    int t, flags;
    df_u32 now;

    now = game_state()->minutes;
    for (t = 0; t < q->qbn.n_timer; t++) {
        flags = q->timer[t].flags;
        if (flags & 2) { flags &= ~128; timer_state(q, t, 0); }
        if ((flags & 64) && now - q->timer[t].start > q->timer[t].delay) {
            if (flags & 3) { flags |= 128; } else if (flags & 4) { flags ^= 128; }
            timer_state(q, t, (flags & 128) != 0);
            if (flags & 8) { q->timer[t].start = now; } else { flags &= ~64; }
        }
        q->timer[t].flags = flags;
    }
}

/* ---- States (quest_arg_state, quest_set_state) ---- */

static int state_value(const QuestState *q, int index)
{
    const QbnState *st;

    if (index < 0 || index >= q->qbn.n_state) { return 0; }
    st = &q->qbn.state[index];
    return st->is_global ? quest_runtime.globals[st->value & 63] : st->value;
}

static void state_store(QuestState *q, int index, int value)
{
    QbnState *st;

    if (index < 0 || index >= q->qbn.n_state) { return; }
    st = &q->qbn.state[index];
    if (st->is_global) { quest_runtime.globals[st->value & 63] = (df_u8)(value ? 1 : 0); } else { st->value = value ? 1 : 0; }
}

int quest_state(int slot, int index) { return quest_at(slot) ? state_value(&quest_runtime.slots[slot], index) : 0; }
void quest_set_state(int slot, int index, int value) { if (quest_at(slot)) { state_store(&quest_runtime.slots[slot], index, value); } }

int quest_state_by_hash(int slot, df_i32 hash)
{
    int i;

    if (!quest_at(slot)) { return 0; }
    for (i = 0; i < quest_runtime.slots[slot].qbn.n_state; i++) { if (quest_runtime.slots[slot].qbn.state[i].hash == hash) { return state_value(&quest_runtime.slots[slot], i); } }
    return 0;
}

/* quest_arg_state: -1 is always true, -2 never; else the state's value, taken the other way by the negate bit. */
int quest_arg_state(const QuestState *q, const QbnOp *op, int n)
{
    const QbnArg *a;
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
void quest_arg_set(QuestState *q, const QbnOp *op, int n, int value)
{
    const QbnArg *a;

    if (n >= op->argc) { return; }
    a = &op->arg[n];
    if (a->literal || a->value < 0) { return; }
    state_store(q, a->value, a->negate ? !value : value);
}

/* ---- Rumours ---- */
/* quest_op_done: the done bit, and the message the first time. */
void quest_op_done(QuestState *q, QbnOp *op)
{
    if (op->flags & 1) { return; }
    op->flags |= 1;
    quest_show_message(q, op->message);
}

/* ---- The ops of this step ---- */

static int g_minute;                  /* set while a passing minute runs the quests (op 79) */
static int g_prompt_slot = -1, g_prompt_op = -1;   /* op 29's box open: the quest and the op that waits for the answer */

/* op 29's answer: the yes state or the no state is set, and the op is done. */
static void prompt_answer(int yes)
{
    QuestState *q;
    QbnOp *op;

    if (g_prompt_slot < 0 || !quest_runtime.slots[g_prompt_slot].used) { g_prompt_slot = -1; return; }
    q = &quest_runtime.slots[g_prompt_slot]; op = &q->qbn.op[g_prompt_op];
    g_prompt_slot = -1;
    quest_arg_set(q, op, yes ? 1 : 2, 1);
    op->flags |= 1;
}

/* A person's faction by the quest's own binding (a faction-only person, or the one who gave it). */
static int person_faction(const QuestState *q, int n)
{
    return n >= 0 && n < q->qbn.n_person && q->person[n].bound ? q->person[n].faction : 0;
}

static int minute_of_day(void) { return (int)(game_state()->minutes % MINUTES_PER_DAY); }

/* The courier of op 76 comes in the day, strictly between DAWN_MINUTE and DUSK_MINUTE (is_daytime), or by night for a vampire. */
#define COURIER_CHANCE 10
static int courier_hour(void)
{
    int day;

    day = minute_of_day() > DAWN_MINUTE && minute_of_day() < DUSK_MINUTE;
    return curse_is_vampire() ? !day : day;
}

/* Whether the player is in a town, a village or the like: the location they are in, not a dungeon (kinds 4, 7 and over 9 are the dungeons and the rest). */
static int town_here(void)
{
    static int seen_region = -1, seen_here = -1, seen_answer;
    int region, here, px, py, n;
    LocationInfo lo;

    if (!quest_world || !quest_world->here || !quest_world->count || !quest_world->location) { return 0; }
    quest_world->here(&region, &here, &px, &py);
    if (here < 0) { return 0; }
    /* Asked every daytime minute: the walk of every location is made once for each place the player stands in. */
    if (region == seen_region && here == seen_here) { return seen_answer; }
    seen_region = region; seen_here = here; seen_answer = 0;
    for (n = 0; n < quest_world->count(); n++) {
        if (quest_world->location(n, &lo) && lo.region == region && lo.index == here) { seen_answer = lo.type != 4 && lo.type != 7 && lo.type <= 9; break; }
    }
    return seen_answer;
}

static int same_name_blind(const char *a, const char *b)
{
    while (*a && ((*a ^ *b) & ~32) == 0) { a++; b++; }
    return *a == *b;
}

static int same_text(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void run_op(int slot, QuestState *q, QbnOp *op)
{
    int i, n, count, pick, found;

    switch (op->opcode) {
    case QOP_TOPICS_SHOW:
    case QOP_TOPICS_HIDE:
        /* Once: the topics shown (10) or hidden (11): the place and the item by flag 128, the person by 0x8000. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            for (i = 1; i < 4 && i < op->argc; i++) {
                n = op->arg[i].value;
                if (op->arg[i].literal || n < 0) { continue; }
                if (i == 1 && n < q->qbn.n_place) { if (op->opcode == QOP_TOPICS_HIDE) { q->qbn.place[n].flags |= 128; } else { q->qbn.place[n].flags &= 127; } }
                if (i == 2 && n < q->qbn.n_person) { if (op->opcode == QOP_TOPICS_HIDE) { q->qbn.person[n].flags |= 0x8000; } else { q->qbn.person[n].flags &= ~0x8000; } }
                if (i == 3 && n < q->qbn.n_item) { if (op->opcode == QOP_TOPICS_HIDE) { q->qbn.item[n].flags |= 128; } else { q->qbn.item[n].flags &= 127; } }
            }
            quest_op_done(q, op);
        }
        break;
    case QOP_COUNTDOWN:
        /* While the gate holds and the flag is clear: the counter counts down, and at 0 the flag is set. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1) && op->arg[1].value != -1 && !quest_arg_state(q, op, 1)) {
            if (op->arg[2].value != 0) { op->arg[2].value--; }
            if (op->arg[2].value == 0) { quest_arg_set(q, op, 1, 1); quest_op_done(q, op); }
        }
        break;
    case QOP_DELETE_PERSON:
        /* Once: the person gone from the quest. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) { n = op->arg[1].value; if (n >= 0 && n < q->qbn.n_person) { q->person[n].bound = 0; } quest_op_done(q, op); }
        break;
    case QOP_HIDE_PERSON:
    case QOP_SHOW_PERSON:
        /* Once: the person hidden (flags 0xA00) or shown. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_person) { if (op->opcode == QOP_HIDE_PERSON) { q->qbn.person[n].flags |= 0xA00; } else { q->qbn.person[n].flags &= ~0xA00; } }
            quest_op_done(q, op);
        }
        break;
    case QOP_FACE_PERSON_ON:
    case QOP_FACE_PERSON_OFF:
    case QOP_FACE_FOE_ON:
    case QOP_FACE_FOE_OFF:
        /* Once: a person's (55, 56) or a foe's (63, 64) face put at the top of the screen, or taken away. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < (op->opcode < QOP_FACE_FIRST_FOE ? q->qbn.n_person : q->qbn.n_foe)) { quest_face_set(q, op->opcode < QOP_FACE_FIRST_FOE ? 1 : 2, n, op->opcode == QOP_FACE_PERSON_ON || op->opcode == QOP_FACE_FOE_ON); }
            quest_op_done(q, op);
        }
        break;
    case QOP_START_QUEST:
        /* Once, outside region 31: another quest started, S plus seven digits drawn between the two numbers. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            int region, here, px, py;

            region = 0;
            if (quest_world && quest_world->here) { quest_world->here(&region, &here, &px, &py); }
            if (region != 31) {
                char child[12];
                int k, lo, hi, active;

                lo = op->arg[1].value; hi = op->arg[2].value;
                log_format(child, sizeof(child), "S%07d", random_range(lo < hi ? lo : hi, lo < hi ? hi : lo));
                active = 0;
                for (k = 0; k < QUESTS; k++) { if (quest_runtime.slots[k].used && same_text(quest_runtime.slots[k].qbn.name, child)) { active = 1; } }
                if (!active) { quest_start(child, 0); }
            }
            quest_op_done(q, op);
        }
        break;
    case QOP_SPAWN_REPEAT:
        /* A foe placed again and again: every `interval` minutes, with a chance in a hundred, `count` more (a count of -1 never runs out; 0 is a no-op). */
        if (quest_arg_state(q, op, 0) && op->arg[4].value != 0 && op->arg[1].value >= 0 && op->arg[1].value < q->qbn.n_foe &&
            game_state()->minutes - op->last_minutes >= (df_u32)op->arg[2].value) {
            op->last_minutes = game_state()->minutes;
            if (random_range(0, RAND_MAX_WATCOM) % PERCENT <= op->arg[3].value) {
                QuestFoe *foe;
                int made;

                foe = &q->foe[op->arg[1].value];
                for (made = 0; quest_world && quest_world->spawn_foe && made < q->qbn.foe[op->arg[1].value].count && made < 16; made++) {
                    if (!quest_world->spawn_foe(q->qbn.foe[op->arg[1].value].type, QUEST_FOE_THING(foe->number, foe->next & 15))) { break; }
                    foe->next++;
                    foe->live++;
                }
                if (made && op->arg[4].value > 0) { op->arg[4].value--; }
            }
        }
        break;
    case QOP_GRANT_ACCESS:
        /* Every run while the gate holds: the place's building open to the access kind until the minutes are up (the grant of the same place and kind is renewed). */
        if (quest_arg_state(q, op, 0)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_place && q->place[n].bound && q->place[n].block >= 0) {
                int free_slot;

                free_slot = -1;
                for (i = 0; i < QUEST_ACCESS; i++) {
                    if (quest_runtime.access[i].used && quest_runtime.access[i].region == q->place[n].region && quest_runtime.access[i].index == q->place[n].index && quest_runtime.access[i].block == q->place[n].block && quest_runtime.access[i].record == q->place[n].record && quest_runtime.access[i].access == op->arg[2].value) { free_slot = i; break; }
                    if (!quest_runtime.access[i].used && free_slot < 0) { free_slot = i; }
                }
                if (free_slot >= 0) {
                    quest_runtime.access[free_slot].used = 1; quest_runtime.access[free_slot].quest_id = q->qbn.id; quest_runtime.access[free_slot].region = q->place[n].region; quest_runtime.access[free_slot].index = q->place[n].index; quest_runtime.access[free_slot].block = q->place[n].block; quest_runtime.access[free_slot].record = q->place[n].record;
                    quest_runtime.access[free_slot].access = op->arg[2].value; quest_runtime.access[free_slot].until = game_state()->minutes + (df_u32)op->arg[3].value;
                }
            }
            quest_op_done(q, op);
        }
        break;
    case QOP_MOVE_PERSON:
        /* Once: the person put in the place (a person that is only a faction stays). With no person named: the player taken to the place of the person of the next argument. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            if (op->arg[1].value == -1) {
                n = op->arg[2].value;
                if (n >= 0 && n < q->qbn.n_person && q->person[n].at > 0 && q->place[q->person[n].at - 1].bound && quest_world && quest_world->go) {
                    quest_world->go(q->place[q->person[n].at - 1].region, q->place[q->person[n].at - 1].index);
                }
            } else {
                n = op->arg[1].value;
                if (n >= 0 && n < q->qbn.n_person && q->person[n].bound && q->qbn.person[n].kind != -8 && op->arg[2].value >= 0 && op->arg[2].value < q->qbn.n_place) {
                    int pl = op->arg[2].value;

                    q->person[n].at = pl + 1;
                    if (q->place[pl].bound && q->place[pl].block >= 0) { q->person[n].homed = 1; q->person[n].home_region = q->place[pl].region; q->person[n].home_index = q->place[pl].index; q->person[n].home_block = q->place[pl].block; q->person[n].home_record = q->place[pl].record; }
                    else if (q->place[pl].bound && quest_location_is_dungeon(q->place[pl].type)) {
                        /* A dungeon place: the person stands on its quest marker (home block -2), the engine's dungeon hook makes their body. */
                        q->person[n].homed = 1; q->person[n].takeover = 0; q->person[n].home_region = q->place[pl].region; q->person[n].home_index = q->place[pl].index;
                        q->person[n].home_block = -2; q->person[n].home_record = -1;
                    }
                }
            }
            quest_op_done(q, op);
        }
        break;
    case QOP_TEMPLATE_ITEMS:
        /* Every run while the gate holds: the items named are the templates of the quest op 8 starts. */
        if (quest_arg_state(q, op, 0)) {
            for (i = 1; i < 5 && i < op->argc; i++) {
                n = op->arg[i].value;
                if (!op->arg[i].literal && n >= 0 && n < q->qbn.n_item) { quest_item_template_set(i - 1, q->qbn.item[n].group, q->qbn.item[n].index); }
            }
        }
        break;
    case QOP_OPEN_BUILDING:
        /* Once: the place and the person marked open to the quest (place flag 64, person 0x4000). */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (!op->arg[1].literal && n >= 0 && n < q->qbn.n_place) { q->qbn.place[n].flags |= 64; }
            n = op->arg[2].value;
            if (!op->arg[2].literal && n >= 0 && n < q->qbn.n_person) { q->qbn.person[n].flags |= 0x4000; }
        }
        break;
    case QOP_TELEPORT_PLAYER:
        /* Once: the player taken to the dungeon of a region whose map pixel is the number (map id: the pixel's row times 1000 and its column). */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            if (quest_world && quest_world->go && quest_world->count && quest_world->location) {
                LocationInfo lo;

                for (n = 0; n < quest_world->count(); n++) {
                    if (quest_world->location(n, &lo) && lo.region == op->arg[2].value && lo.py * 1000 + lo.px == op->arg[3].value) { quest_world->go(lo.region, lo.index); break; }
                }
            }
            op->flags |= 1;
        }
        break;
    case QOP_REWARD:
        /* Once: show the quest's reward message, then give the reward and +5 with its faction.
         * Coins go straight to the player. An item already given is passed over. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            q->rewarded = 1;
            quest_op_done(q, op);
            quest_show_message(q, 1004);
            /* The two guilds' invitation quests join the guild instead (rank 0, nothing given, no reputation): by their names, case blind. */
            if (same_name_blind(q->qbn.name, "L0A01L00") || same_name_blind(q->qbn.name, "O0A0AL00")) {
                GuildStep joined;

                guild_join(q->qbn.name[0] == 'L' || q->qbn.name[0] == 'l' ? 0 : 3, q->qbn.name[0] == 'L' || q->qbn.name[0] == 'l' ? 108 : 42, game_state()->minutes, &joined);
                break;
            }
            if (q->qbn.faction) { faction_change_reputation(q->qbn.faction, 5); }
            for (i = 1; i < op->argc; i++) {
                n = op->arg[i].value;
                if (op->arg[i].literal || n < 0 || n >= q->qbn.n_item || q->item[n].given) { continue; }
                if (q->item[n].gold) { character_state()->gold += q->item[n].amount; q->item[n].given = 1; }
                else if (q->item[n].object) {
                    /* The reward box of the original opens the inventory on the items; they go into the pack here, or lie at the player's feet when it is full, and are the quest's no more (the box turns them into
                     * ordinary items when the inventory closes). */
                    if (!inventory_transfer(q->item[n].object, game_state()->inventory_id)) { quest_put_item_down(q, n); }
                    q->item[n].given = 1; quest_item_forget(q, n);
                }
            }
        }
        break;
    case QOP_PLACE_ITEM:
        /* Once: the item set in its place: now when that is the location the player is in, else when it is loaded. An item already out in the world is taken back first. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_item && q->item[n].object) {
                q->item[n].place = op->arg[2].value >= 0 && op->arg[2].value < q->qbn.n_place ? op->arg[2].value : -1;
                q->item[n].laid = 0; q->item[n].carrier = 0;
                if (q->item[n].place >= 0 && quest_in_place(q, q->item[n].place)) { quest_put_item_down(q, n); q->item[n].laid = 1; }
            }
            quest_op_done(q, op);
        }
        break;
    case QOP_GIVE_ITEM:
        /* Once: the item given to the player (into the pack). A pack with no room leaves it for the next run. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_item && q->item[n].object && !inventory_transfer(q->item[n].object, game_state()->inventory_id)) { break; }
            quest_op_done(q, op);
        }
        break;
    case QOP_DELETE_ITEM:
        /* Once: the item deleted. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            quest_op_done(q, op);
            if (n >= 0 && n < q->qbn.n_item && q->item[n].object) { object_delete(q->item[n].object); quest_item_forget(q, n); }
        }
        break;
    case QOP_FORGET_ITEM:
        /* Once: the item forgotten: it stays in the world, no longer the quest's. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) { quest_item_forget(q, op->arg[1].value); quest_op_done(q, op); }
        break;
    case QOP_FOE_CARRIES:
        /* Once: the foe carries the item; it is set down where the player is when the foe dies. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_item && q->item[n].object && op->arg[2].value >= 0 && op->arg[2].value < q->qbn.n_foe) { q->item[n].carrier = op->arg[2].value + 1; q->item[n].place = -1; }
            quest_op_done(q, op);
        }
        break;
    case QOP_IN_PLACE:
        /* Every run while the gate holds: the result is whether the player is in the place; done when it is. */
        if (quest_arg_state(q, op, 0) && op->arg[2].value >= 0) {
            found = quest_in_place(q, op->arg[2].value);
            quest_arg_set(q, op, 1, found);
            if (found) { quest_op_done(q, op); }
        }
        break;
    case QOP_ITEM_USED:
        /* Once the item has been used (flag 512): the result is set. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[2].value;
            if (n >= 0 && n < q->qbn.n_item && q->item[n].object && object_at(q->item[n].object) && (object_at(q->item[n].object)->flags & 512)) { quest_arg_set(q, op, 1, 1); quest_op_done(q, op); }
        }
        break;
    case QOP_CARRIES_ALL:
        /* Once the player carries every item named: the result is set; until then it is clear. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            found = 1;
            for (i = 2; i < 5 && i < op->argc; i++) {
                n = op->arg[i].value;
                if (op->arg[i].literal || n == -1 || n == -2) { continue; }
                if (n < 0 || n >= q->qbn.n_item || !q->item[n].object || !quest_item_carried(q->item[n].object)) { found = 0; }
            }
            quest_arg_set(q, op, 1, found);
            if (found) { quest_op_done(q, op); }
        }
        break;
    case QOP_DELIVER_LATER:
        /* The item delivered later (func_00031658): pending, then handed over by chance on a passing minute, only in the streets of a town (out of doors, not in a building or a dungeon)
         * and while the game runs in the world (not the rest screen, whose hours pass in blocks, nor a menu): by daylight (a vampire's courier comes by night). And none of the creatures loaded alive (creature_count). One roll a game minute, 1..100 under COURIER_CHANCE. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n < 0 || n >= q->qbn.n_item || !q->item[n].object) { quest_op_done(q, op); break; }
            if (g_minute && game_state()->mode == GAME_WORLD && courier_hour() && quest_world && quest_world->outdoors && quest_world->outdoors() && town_here() && (!quest_world->creatures || quest_world->creatures() == 0) &&
                random_range(1, PERCENT) < COURIER_CHANCE && inventory_transfer(q->item[n].object, game_state()->inventory_id)) { quest_op_done(q, op); }
        }
        break;
    case QOP_END:
        /* The end: when set (no delivery is pending and the box is empty, both so with gold paid directly). A quest never rewarded costs -2 with its faction. The rumours are step 9. */
        if (quest_arg_state(q, op, 0)) {
            if (!q->rewarded && q->qbn.faction) { faction_change_reputation(q->qbn.faction, -2); }
            quest_op_done(q, op);
            quest_rumour_add(slot, q->rewarded ? 1007 : 1006, 0, 8);
            quest_rumour_questor(slot, q->rewarded);
            q->ended = 1;
        }
        break;
    case QOP_CLEAR_STATES:
        if (quest_arg_state(q, op, 0)) { for (i = 1; i < op->argc; i++) { quest_arg_set(q, op, i, 0); } }
        break;
    case QOP_TIMER_START:
        /* Once: start the timer that is not running, its delay worked from now. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_timer && !(q->timer[n].flags & 64)) {
                quest_timer_start(q, n); q->timer[n].flags = (q->timer[n].flags | 64) & ~128;
            }
            quest_op_done(q, op);
        }
        break;
    case QOP_TIMER_STOP:
        if (quest_arg_state(q, op, 0)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_timer) { q->timer[n].flags &= ~64; }
            quest_op_done(q, op);
        }
        break;
    case QOP_REVEAL_PLACE:
    case QOP_REVEAL_PLACE_2:
        /* Once: the place's location shown on the map (7.12: the bound place, whatever its kind). */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (quest_world && quest_world->reveal && n >= 0 && n < q->qbn.n_place && q->place[n].bound) { quest_world->reveal(q->place[n].ordinal); }
            quest_op_done(q, op);
        }
        break;
    case QOP_REVEAL_LOCATION:
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            if (quest_world && quest_world->reveal && quest_world->count && quest_world->location) {
                LocationInfo lo;

                for (n = 0; n < quest_world->count(); n++) {
                    if (quest_world->location(n, &lo) && lo.region == op->arg[2].value && lo.index == op->arg[3].value) { quest_world->reveal(n); break; }
                }
            }
            quest_op_done(q, op);
        }
        break;
    case QOP_PLACE_FOE:
    case QOP_PLACE_FOE_2:
        /* Once: the foe placed (a place of the quest, or near the player when there is none); made now when the dungeon is the one the player is in, else when it is loaded. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_foe) {
                int where;

                where = !op->arg[2].literal && op->arg[2].value >= 0 ? op->arg[2].value : -1;
                q->foe[n].placed = 1; q->foe[n].place = where; q->foe[n].live = 0; q->foe[n].made = 0;
                quest_spawn_foe(q, n);
            }
            quest_op_done(q, op);
        }
        break;
    case QOP_DAY_RANGE:
        /* The day of the world in a range (no file uses it). */
        quest_arg_set(q, op, 0, (int)(game_state()->minutes / MINUTES_PER_DAY) >= op->arg[1].value && (int)(game_state()->minutes / MINUTES_PER_DAY) <= op->arg[2].value);
        break;
    case QOP_LOG_SET:
        /* Once: entry `index` of the quest's log is the message. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) { logbook_add(slot, op->arg[2].value, op->arg[1].value); quest_op_done(q, op); }
        break;
    case QOP_LOG_REMOVE:
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) { logbook_remove(slot, op->arg[1].value); quest_op_done(q, op); }
        break;
    case QOP_MINUTE_RANGE: {
        int from, to, m, inside;

        from = op->arg[1].value; to = op->arg[2].value; m = minute_of_day();
        inside = from >= to ? (m >= from || m <= to) : (m >= from && m <= to);
        quest_arg_set(q, op, 0, inside);
        break;
    }
    case QOP_SET_ONE_OF:
        /* Once: one of the given states at random. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            count = 0;
            for (i = 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value >= 0) { count++; } }
            if (count) {
                pick = random_range(1, count); n = 0;
                for (i = 1; i < op->argc; i++) {
                    if (op->arg[i].literal || op->arg[i].value < 0) { continue; }
                    if (++n == pick) { quest_arg_set(q, op, i, 1); break; }
                }
            }
            quest_op_done(q, op);
        }
        break;
    case QOP_CLEAR_AND_NEXT:
        /* Once: the first of the given states that is set is cleared and the next is set; none set: the first is. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            found = -1;
            for (i = 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value >= 0 && quest_arg_state(q, op, i)) { found = i; break; } }
            if (found < 0) {
                for (i = 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value >= 0) { quest_arg_set(q, op, i, 1); break; } }
            } else {
                quest_arg_set(q, op, found, 0);
                for (i = found + 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value >= 0) { quest_arg_set(q, op, i, 1); break; } }
            }
            quest_op_done(q, op);
        }
        break;
    case QOP_PLAY_MOVIE:
        /* Once (qcom.c op 50): ANIM<number>.VID, the number the second operand holds (the ghost's farewell of S0000015 is ANIM0013). */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) { movie_play(op->arg[1].value); quest_op_done(q, op); }
        break;
    case QOP_SAY:
        if (quest_arg_state(q, op, 0)) { quest_op_done(q, op); }
        break;
    case QOP_ALL_OF:
        /* Every run: all the given states true sets the state, which is never cleared by this op (7.9 kept). */
        n = 1;
        for (i = 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value == -2) { continue; } if (!quest_arg_state(q, op, i)) { n = 0; } }
        if (n) { quest_arg_set(q, op, 0, 1); }
        break;
    case QOP_ANY_OF:
        /* Every run: the state is whether any given state is true. */
        n = 0;
        for (i = 1; i < op->argc; i++) { if (!op->arg[i].literal && op->arg[i].value == -2) { continue; } if (quest_arg_state(q, op, i)) { n = 1; } }
        quest_arg_set(q, op, 0, n);
        break;
    case QOP_LEVEL_AT_LEAST:
        quest_arg_set(q, op, 0, character_state()->level >= op->arg[1].value);
        break;
    case QOP_ASK_YES_NO:
        /* Once: a yes/no box with the op's QRC message; the answer sets its yes or no state. One box at a time. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1) && g_prompt_slot < 0 && quest_text_hooks.ask && quest_text_hooks.read) {
            static char text[QUEST_DIALOG_CAP], said[QUEST_DIALOG_CAP];
            int message;

            /* The question's QRC id is the op's fourth operand, a number (the files hold -1 in the message field of this op); a message field, when set, is a TEXT.RSC or QRC id as the others. */
            message = op->message >= 0 ? op->message : op->argc > QOP_ASK_MESSAGE_ARG && op->arg[QOP_ASK_MESSAGE_ARG].literal ? op->arg[QOP_ASK_MESSAGE_ARG].value : -1;
            text[0] = 0;
            if (message >= 0 && quest_text_hooks.read(q->qbn.name, message & 0x7fff, (message & 0x8000) != 0, text, sizeof(text)) > 0 && text[0]) {
                quest_expand(slot, text, said, sizeof(said), game_state()->minutes, 0);
                g_prompt_slot = slot; g_prompt_op = (int)(op - q->qbn.op);
                if (!quest_text_hooks.ask(said, prompt_answer)) { g_prompt_slot = -1; }
            } else { op->flags |= 1; }
        }
        break;
    case QOP_REPUTATION_AT_LEAST:
        /* Every run while the gate holds: the result is whether the person's faction thinks of the player at least `value` (and their social group's standing is added when it is under 5). */
        if (quest_arg_state(q, op, 0) && op->arg[1].value != -1 && person_faction(q, op->arg[2].value)) {
            int faction, rep;
            const FactionDef *def;

            faction = person_faction(q, op->arg[2].value);
            rep = faction_rep(faction);
            def = faction_def(faction);
            if (def && def->social_group >= 0 && def->social_group < 5) { rep += character_state()->reputation[def->social_group]; }
            quest_arg_set(q, op, 1, rep >= op->arg[3].value);
            quest_op_done(q, op);
        }
        break;
    case QOP_CURE_DISEASES:
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) { ailments_cure(0, 0); quest_op_done(q, op); }
        break;
    case QOP_CHANGE_REPUTATION:
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            if (person_faction(q, op->arg[1].value)) { faction_change_reputation(person_faction(q, op->arg[1].value), op->arg[2].value); }
            quest_op_done(q, op);
        }
        break;
    case QOP_LEGAL_REPUTATION:
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            int region, here, px, py;

            region = 0;
            if (quest_world && quest_world->here) { quest_world->here(&region, &here, &px, &py); }
            region_add_legal(region, op->arg[1].value);
            if (region_legal(region) > 100) { region_add_legal(region, 100 - region_legal(region)); }
            if (region_legal(region) < -100) { region_add_legal(region, -100 - region_legal(region)); }
            quest_op_done(q, op);
        }
        break;
    case QOP_OP_58:
        /* Once: the cure of vampirism ($CUREVAM). */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) { curse_cure_vampirism(); quest_op_done(q, op); }
        break;
    case QOP_OP_59:
        /* Once: the cure of lycanthropy ($CUREWER). */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) { curse_cure_lycanthropy(); quest_op_done(q, op); }
        break;
    case QOP_PACIFY_FOE:
        if (quest_arg_state(q,op,0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_foe) { quest_set_foe_pacified(&q->foe[n],quest_arg_state(q,op,2)); }
            quest_op_done(q,op);
        }
        break;
    case QOP_INFECT:
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) { disease_infect(0, op->arg[1].value, 1); quest_op_done(q, op); }
        break;
    case QOP_FACTION_REPUTATION:
        /* Once a game minute while the state is clear: set when the faction's reputation (as it stands) is at least the value. */
        if (!quest_arg_state(q, op, 0) && g_minute && op->arg[1].literal && op->arg[2].literal && faction_rep(op->arg[1].value) >= op->arg[2].value) { quest_arg_set(q, op, 0, 1); }
        break;
    case QOP_SHOW_MESSAGE:
        if (quest_arg_state(q, op, 0)) { quest_show_message(q, op->message); }
        break;
    case QOP_SOUND:
        /* Every run while the state is set (the original's quirk, quest-machine.md 7.8): the sound. */
        if (quest_arg_state(q, op, 0)) { play_sound(op->arg[1].value); quest_op_done(q, op); }
        break;
    case QOP_CAST_ON_FOE:
        /* Once: the spell cast by the player on the foe's live copies. */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_foe) { quest_cast_on_foe(&q->foe[n], op->arg[2].value); }
            quest_op_done(q, op);
        }
        break;
    case QOP_FACTION_FLAG_SET:
    case QOP_FACTION_FLAG_CLEAR:
        /* Once: FACTION_FLAG_QUEST set or cleared on the person's faction. FALL.EXE keeps it in the faction's flags and never reads it, so the op is only done (and says its message). */
        if (quest_arg_state(q, op, 0) && !(op->flags & 1)) { quest_op_done(q, op); }
        break;
    case QOP_RESPAWN_FOE:
        /* Every run while the gate holds (qaction_op87_respawn): the count (-1 endless) not spent, the interval passed, the earlier copies gone, then the chance of a hundred decides whether it comes back. */
        if (quest_arg_state(q, op, 0) && op->arg[4].value != 0 && op->arg[1].value >= 0 && op->arg[1].value < q->qbn.n_foe &&
            game_state()->minutes - op->last_minutes >= (df_u32)op->arg[2].value) {
            QuestFoe *foe;

            op->last_minutes = game_state()->minutes;
            foe = &q->foe[op->arg[1].value];
            quest_remove_foe(foe);
            if (random_range(0, RAND_MAX_WATCOM) % PERCENT <= op->arg[3].value) {
                int made;

                if (op->arg[4].value != -1) { op->arg[4].value--; }
                for (made = 0; quest_world && quest_world->spawn_foe && made < q->qbn.foe[op->arg[1].value].count && made < 16; made++) {
                    if (!quest_world->spawn_foe(q->qbn.foe[op->arg[1].value].type, QUEST_FOE_THING(foe->number, (foe->next++) & 15))) { break; }
                    foe->live++;
                }
            }
        }
        break;
    case QOP_OP_84:
        /* Once the game minute changes, on every period's minute and by chance, while the state is set: the sound (S0000977's scream in Daggerfall at night). */
        if (quest_arg_state(q, op, 0) && g_minute && op->arg[2].literal && op->arg[3].literal && op->arg[2].value > 0 &&
            game_state()->minutes % (df_u32)op->arg[2].value == 0 && game_state()->minutes != op->last_minutes && random_range(1, 100) <= op->arg[3].value) {
            op->last_minutes = game_state()->minutes;
            play_sound(op->arg[1].value);
            quest_op_done(q, op);
        }
        break;
    case QOP_OP_82:
        /* The person's talk passes through (flag 0x2000) while the state is set. */
        n = op->arg[1].value;
        if (n >= 0 && n < q->qbn.n_person) { if (quest_arg_state(q, op, 0)) { q->qbn.person[n].flags |= 0x2000; } else { q->qbn.person[n].flags &= ~0x2000; } }
        break;
    default:
        break;
    }
    (void)slot;
}

void quest_run_minute(void)
{
    g_minute = 1;
    quest_run();
    g_minute = 0;
}

void quest_start_initial_later(void) { g_initial_pending = 1; }

/* guild_check_invitations: a guild's invitation falls due while the player is out of doors: its quest starts, no prompt (the Thieves' by a letter the courier brings, the Brotherhood's at once). */
static void invitations(void)
{
    int kind;

    if (!quest_world || !quest_world->outdoors || !quest_world->outdoors()) { return; }
    if (guild_invitation(game_state()->minutes, 1, &kind)) { quest_start(kind == 3 ? "O0A0AL00" : "L0A01L00", 0); }
}

void quest_run(void)
{
    invitations();
    if (g_initial_pending && quest_world) {
        /* quests_start_initial, once the world is open: no offer box. Only _BRISIEN, which starts the main story: the tutorial _TUTOR__ is left out on purpose (a design choice: whoever plays knows Daggerfall). */
        g_initial_pending = 0;
        quest_start("_BRISIEN", 0);
    }
    int slot, i;

    for (slot = 0; slot < QUESTS; slot++) {
        QuestState *q;

        q = &quest_runtime.slots[slot];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_op && !q->ended; i++) { run_op(slot, q, &q->qbn.op[i]); }
        /* A foe whose floor was not there when it was placed (a marker far off) is made as the player comes near. */
        for (i = 0; i < q->qbn.n_foe && !q->ended; i++) { if (q->foe[i].placed && q->foe[i].made < q->qbn.foe[i].count) { quest_spawn_foe(q, i); } }
        /* The four template slots are cleared after each quest's run. */
        quest_item_templates_clear();
        if (!q->ended) { timers_run(q); }
        if (q->ended) { quest_end(slot); }
    }
}


void quest_reset_initial_start(void)
{
    g_initial_pending = 0;
    /* A box left open by the old game belongs to no quest of the new one. */
    g_prompt_slot = -1; g_prompt_op = -1;
}
