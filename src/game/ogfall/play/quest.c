#include "game/ogfall/play/quest.h"
#include "game/ogfall/play/state.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/ogfall/tables/questdata.h"
#include "game/ogfall/tables/combatdefs.h"
#include "game/ogfall/play/law.h"
#include "game/ogfall/play/standing.h"
#include "game/ogfall/play/guild.h"
#include "game/ogfall/play/ailments.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/tables/itemdefs.h"
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
static const OgQuestWorld *g_world;
static int g_foe_counter = 1;
static int g_initial_pending;
static OgRumour g_rumour[OG_RUMOURS];
static OgQuestFace g_face[OG_QUEST_FACES];
static OgQuestAccess g_access[OG_QUEST_ACCESS];
/* Op 75's templates for the quest op 8 starts this run: the group and index of up to four items (-1 none). */
static int g_tpl_group[4] = { -1, -1, -1, -1 }, g_tpl_index[4];
static struct { int set, who, faction; char name[40], town[33]; } g_questor;
static void arg_set(OgQuest *q, const OgQbnOp *op, int n, int value);
static void lay_items(OgQuest *q, int region, int index);
static void rumour_add(int slot, int message, int target, int flags);
static void rumour_questor(int slot, int rewarded);
static void face_set(const OgQuest *q, int kind, int index, int on);
static int carried(df_u32 id);
static void make_item(OgQuest *q, int i);
static int arg_state(const OgQuest *q, const OgQbnOp *op, int n);
static void item_forget(OgQuest *q, int n);
static int in_place(const OgQuest *q, int n);
static void foe_dropped(OgQuest *q, int f);
static void done(OgQuest *q, OgQbnOp *op);
static int bind_resources(int slot);
static void state_store(OgQuest *q, int index, int value);

void og_quest_set_text(int (*read)(const char *quest, int message, int in_text_rsc, char *out, int cap)) { g_read = read; }
void og_quest_set_say(void (*say)(const char *text)) { g_say = say; }
void og_quest_set_ask(int (*ask)(const char *words, void (*answer)(int yes))) { g_ask = ask; }
const df_u8 *og_quest_globals(void) { return g_global; }
void og_quest_globals_clear(void) { mem_zero(g_global, sizeof(g_global)); }
void og_quest_reset(void) { mem_zero(g_rumour, sizeof(g_rumour)); mem_zero(g_face, sizeof(g_face)); mem_zero(g_access, sizeof(g_access)); mem_zero(g_quest, sizeof(g_quest)); mem_zero(g_global, sizeof(g_global)); mem_zero(g_log, sizeof(g_log)); g_offer_slot = -1; g_initial_pending = 0; g_foe_counter = 1; mem_zero(&g_questor, sizeof(g_questor)); }
void og_quest_set_names(const OgQuestNames *names) { g_names = names; }
void og_quest_set_world(const OgQuestWorld *world) { g_world = world; }
void og_quest_set_macros(int (*fill)(const char *in, char *out, int cap)) { g_macros = fill; }
void og_quest_set_place(const char *(*current)(void)) { g_place = current; }
int og_quest_count(void) { return OG_QUESTS; }
const OgQuest *og_quest(int slot) { return slot >= 0 && slot < OG_QUESTS && g_quest[slot].used ? &g_quest[slot] : 0; }

void og_quest_end(int slot)
{
    if (slot >= 0 && slot < OG_QUESTS) {
        int i;

        for (i = 0; i < OG_QUEST_FACES; i++) { if (g_face[i].used && g_face[i].quest_id == g_quest[slot].qbn.id) { g_face[i].used = 0; } }
        for (i = 0; i < OG_RUMOURS; i++) { if (g_rumour[i].used && (g_rumour[i].flags & 4) && g_rumour[i].quest_id == g_quest[slot].qbn.id) { g_rumour[i].used = 0; } }
        /* quest_end: every object with the quest's id goes, a quest item in the pack too. */
        for (i = 0; i < g_quest[slot].qbn.n_item; i++) { if (g_quest[slot].item[i].object) { og_object_delete(g_quest[slot].item[i].object); } } mem_zero(&g_quest[slot], sizeof(g_quest[slot])); }
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
    if (!bind_resources(slot)) {
        /* Any failure ends the quest and says TEXT.RSC 600. */
        mem_zero(&g_quest[slot], sizeof(g_quest[slot]));
        if (g_read && g_say) { char text[256]; text[0] = 0; if (g_read("", 600, 1, text, sizeof(text)) > 0) { g_say(text); } }
        return -1;
    }
    rumour_add(slot, 1005, 0, 4);
    return slot;
}

int og_quest_start(const char *name, int faction) { return start_with(name, 0, 0, faction); }
int og_quest_start_bytes(const char *name, const df_u8 *bytes, df_u32 size, int faction) { return start_with(name, bytes, size, faction); }

int og_quest_pick_seeded(char letter, char letter2, char membership, int rank, unsigned seed, char *name);
int og_quest_pick(char letter, char letter2, char membership, int rank, char *name)
{
    return og_quest_pick_seeded(letter, letter2, membership, rank, 0xffffffffu, name);
}

/* quest_pick_file with the pick made by a seed (func_0004CD10 seeds rand with the person's id, so a person offers the same quest each time); 0xffffffff draws at random. */
int og_quest_pick_seeded(char letter, char letter2, char membership, int rank, unsigned seed, char *name)
{
    int i, n, pick, count;
    const char *q;

    count = 0;
    for (i = 0; i < (int)(sizeof(g_quest_name) / sizeof(g_quest_name[0])); i++) {
        q = g_quest_name[i];
        if ((q[0] == letter || (letter2 && q[0] == letter2)) && q[1] == '0' && q[2] == membership && q[3] - '0' <= rank) { count++; }
    }
    if (!count) { return 0; }
    pick = seed == 0xffffffffu ? og_random_range(0, count - 1) : (int)(seed % (unsigned)count); n = 0;
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

/* ---- Places (quest_init_place, quest_pick_location) and timers (quest_timer_update) ---- */

/* The kinds of location: towns 0..2, the dungeons 4, 7, 10, 12 and 13. */
static int is_town(int type) { return type >= 0 && type <= 2; }
static int is_dungeon(int type) { return type == 4 || type == 7 || type == 10 || type == 12 || type == 13; }

static int location_used(int slot, int place, int region, int index)
{
    int k, i;

    for (k = 0; k < OG_QUESTS; k++) {
        if (!g_quest[k].used) { continue; }
        for (i = 0; i < OG_QBN_PLACES; i++) { if (!(k == slot && i == place) && g_quest[k].place[i].bound && g_quest[k].place[i].region == region && g_quest[k].place[i].index == index) { return 1; } }
    }
    return 0;
}

static void place_from(OgQuestPlace *place, int ordinal, const OgLocation *lo)
{
    place->bound = 1; place->ordinal = ordinal; place->region = lo->region; place->index = lo->index; place->type = lo->type;
    place->px = lo->px; place->py = lo->py;
    log_format(place->name, sizeof(place->name), "%s", lo->name);
}

/* quest_init_place for a place that is a location: a fixed one (scope 0) waits for the steps that read the world's records; scope 1 or below is where the player is; scope 2 and up a location
 * of the kind (p1: 0 a town, 1 a dungeon) in the player's region, other than the one they are in. A location another quest uses is drawn again, up to 100 times. */
static int bind_place(int slot, int i)
{
    OgQuest *q;
    const OgQbnPlace *pl;
    OgLocation lo;
    int region, here, px, py, count, n, tries, pick, k, ordinal;

    q = &g_quest[slot]; pl = &q->qbn.place[i];
    if (!g_world || !g_world->count || !g_world->location || !g_world->here) { return 1; }
    if (pl->scope == 0) {
        /* A fixed place: (p1 << 16) | p2, the location's number and the low word of an object in it. Low word 2 names no object in the original (7.1: its tutorial step could never run); it is read as 1,
         * the location itself, as Daggerfall Unity does. The place has the location's bounds and no point of its own. */
        if (!g_world->find_number || !g_world->find_number(pl->p1, &region, &here)) { return 1; }
        for (k = 0; k < g_world->count(); k++) {
            if (g_world->location(k, &lo) && lo.region == region && lo.index == here) { place_from(&q->place[i], k, &lo); q->place[i].marker = (pl->p2 & 0xffff) == 2 ? 1 : (pl->p2 & 0xffff); return 1; }
        }
        return 1;
    }
    g_world->here(&region, &here, &px, &py);
    count = g_world->count();
    if (pl->scope <= 1) {
        for (k = 0; k < count; k++) {
            if (g_world->location(k, &lo) && lo.region == region && lo.index == here) { place_from(&q->place[i], k, &lo); return 1; }
        }
        return 0;
    }
    for (tries = 0; tries < 100; tries++) {
        n = 0;
        for (k = 0; k < count; k++) {
            if (!g_world->location(k, &lo) || lo.region != region || lo.index == here) { continue; }
            if (pl->p1 == 1 ? !is_dungeon(lo.type) : pl->p1 == 0 ? !is_town(lo.type) : 1) { continue; }
            n++;
        }
        if (!n) { return 0; }
        pick = og_random_range(0, n - 1); ordinal = -1; n = 0;
        for (k = 0; k < count && ordinal < 0; k++) {
            if (!g_world->location(k, &lo) || lo.region != region || lo.index == here) { continue; }
            if (pl->p1 == 1 ? !is_dungeon(lo.type) : pl->p1 == 0 ? !is_town(lo.type) : 1) { continue; }
            if (n++ == pick) { ordinal = k; }
        }
        if (ordinal < 0 || !g_world->location(ordinal, &lo)) { return 0; }
        if (location_used(slot, i, lo.region, lo.index)) { continue; }
        place_from(&q->place[i], ordinal, &lo);
        return 1;
    }
    return 0;
}

/* quest_travel_minutes: the quests' journey estimate plus two days, a half more, and a week more for each end of a kind of dungeon the map holds (4, 7, 10, 12, 14). */
static df_u32 travel_delay(const OgQuestPlace *a, const OgQuestPlace *b)
{
    int px, py, region, here, sx, sy, minutes;
    df_u32 delay;

    if (!g_world || !g_world->journey || !g_world->here) { return 2880u * 384u >> 8; }
    g_world->here(&region, &here, &px, &py);
    sx = a ? a->px : px; sy = a ? a->py : py;
    minutes = g_world->journey(sx, sy, b->px, b->py) + 2880;
    delay = ((df_u32)minutes * 384u) >> 8;
    return delay;
}

static int far_kind(int type) { return type == 4 || type == 7 || type == 10 || type == 12 || type == 14; }

static void timer_start(OgQuest *q, int t)
{
    const OgQbnTimer *tm;
    OgQuestTimer *rt;
    df_u32 delay;
    int l1, l2;

    tm = &q->qbn.timer[t]; rt = &q->timer[t];
    l1 = (tm->flags & 0x300) ? -1 : tm->link1; l2 = (tm->flags & 0x300) ? -1 : tm->link2;
    delay = 0;
    if (tm->type == 0) { delay = (df_u32)og_random_range((int)tm->minimum, (int)tm->maximum); }
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
    rt->delay = delay; rt->start = og_state()->minutes;
}

/* The state whose hash is the timer's takes whether it has expired. */
static void timer_state(OgQuest *q, int t, int value)
{
    int i;

    for (i = 0; i < q->qbn.n_state; i++) { if (q->qbn.state[i].hash == q->qbn.timer[t].hash) { state_store(q, i, value); } }
}

/* quest_timer_update, after the ops of a run: a pulse's expired state lasts one run; a running timer that has run past its delay is expired (latched or pulsed; toggled; else nothing: 7.15),
 * and starts again with bit 3 or stops. */
static void timers_run(OgQuest *q)
{
    int t, flags;
    df_u32 now;

    now = og_state()->minutes;
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

/* quest_init_resources, the places and the timers of this step: a place that cannot be bound ends the quest (TEXT.RSC 600). */
/* quest_init_item's gold (quests-math.md): index and group are the least and most; index -1 scales by level (the guild's rank + 1 when the player is of the faction, else half the level + 1,
 * at most 10), the faction's power and the region's prices. At least 1. */
static int roll_gold(const OgQuest *q, const OgQbnItem *it)
{
    int gold, level, power, adj, region, here, px, py;

    if (it->index != -1) { gold = og_random_range(it->index, it->group); }
    else {
        OgMembership *m;

        m = q->qbn.faction ? og_guild_find_faction(q->qbn.faction) : 0;
        level = m ? m->rank + 1 : og_character()->level / 2 + 1;
        if (level > 10) { level = 10; }
        power = q->qbn.faction ? og_faction_power(q->qbn.faction) : 50;
        region = 0;
        if (g_world && g_world->here) { g_world->here(&region, &here, &px, &py); }
        adj = og_region_price(region) / 2 + 500;
        gold = ((power + 50) * ((adj * og_random_range(level * 150, level * 200)) / 1000)) / 100;
    }
    return gold < 1 ? 1 : gold;
}

static int bind_resources(int slot)
{
    OgQuest *q;
    int i;

    q = &g_quest[slot];
    for (i = 0; i < q->qbn.n_timer; i++) { q->timer[i].flags = q->qbn.timer[i].flags & ~(128 | 1024); }
    for (i = 0; i < q->qbn.n_place; i++) { if (!bind_place(slot, i)) { return 0; } }
    /* quest_init_person (2.3), the kinds of this step: the questor, and a person that is only a faction. */
    for (i = 0; i < q->qbn.n_person; i++) {
        const OgQbnPerson *pe;

        pe = &q->qbn.person[i];
        q->qbn.person[i].flags &= 0x0fff;
        if (pe->kind == 21) {
            if (!g_questor.set || og_quest_person_busy(g_questor.who)) { return 0; }
            q->person[i].bound = 1; q->person[i].who = g_questor.who; q->person[i].faction = g_questor.faction;
            log_format(q->person[i].name, sizeof(q->person[i].name), "%s", g_questor.name);
            log_format(q->person[i].town, sizeof(q->person[i].town), "%s", g_questor.town);
        } else if (pe->kind == -8) {
            q->person[i].bound = 1; q->person[i].who = 0; q->person[i].faction = pe->faction;
        } else {
            /* Any other kind is a person not yet in the world: a named individual (kind -1 of a faction of its own) is found by their faction when their town loads, the others stand nowhere until
             * op 30 puts them in a place. They are bound, with an id of their own and no body (the towns' people are not made from the quests yet). */
            q->person[i].bound = 1; q->person[i].who = (int)(0x40000000u | (unsigned)(slot << 8) | (unsigned)i); q->person[i].faction = pe->kind == -1 ? pe->faction : 0;
        }
    }
    for (i = 0; i < q->qbn.n_op; i++) { q->qbn.op[i].last_minutes = og_state()->minutes; }
    for (i = 0; i < q->qbn.n_item; i++) {
        if (q->qbn.item[i].flags & 2) { q->item[i].gold = 1; q->item[i].amount = roll_gold(q, &q->qbn.item[i]); }
        else { make_item(q, i); }
        q->item[i].place = -1;
    }
    /* quest_init_foe: each foe takes the next number of the counter. */
    for (i = 0; i < q->qbn.n_foe; i++) { q->foe[i].number = g_foe_counter++; q->foe[i].place = -1; }
    for (i = 0; i < q->qbn.n_timer; i++) { timer_start(q, i); }
    return 1;
}

/* ---- Persons and talk ---- */

void og_quest_set_questor(int who, int faction, const char *name, const char *town)
{
    g_questor.set = 1; g_questor.who = who; g_questor.faction = faction;
    log_format(g_questor.name, sizeof(g_questor.name), "%s", name ? name : "");
    log_format(g_questor.town, sizeof(g_questor.town), "%s", town ? town : "");
}

int og_quest_person_busy(int who)
{
    int s, i;

    for (s = 0; s < OG_QUESTS; s++) {
        if (!g_quest[s].used) { continue; }
        for (i = 0; i < g_quest[s].qbn.n_person; i++) { if (g_quest[s].person[i].bound && g_quest[s].qbn.person[i].kind == 21 && g_quest[s].person[i].who == who) { return 1; } }
    }
    return 0;
}

/* Whether the one talked to is the quest's person (type 65, a faction, is any person of that faction). */
static int is_person(const OgQuest *q, int i, int who, int faction)
{
    if (i < 0 || i >= q->qbn.n_person || !q->person[i].bound) { return 0; }
    if (q->qbn.person[i].kind == -8 || (q->qbn.person[i].kind == -1 && !(q->qbn.person[i].flags & 0x100) && q->person[i].faction)) { return q->person[i].faction == faction; }
    return q->person[i].who == who;
}

int og_quest_talk(int who, int faction)
{
    int s, i, consumed;

    consumed = 0;
    for (s = 0; s < OG_QUESTS; s++) {
        OgQuest *q;

        q = &g_quest[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_op; i++) {
            OgQbnOp *op;

            op = &q->qbn.op[i];
            if (op->opcode == 28 && !(op->arg[0].value == -1) && is_person(q, op->arg[1].value, who, faction)) {
                arg_set(q, op, 0, 1); done(q, op);
                /* The click is consumed unless the person's talk passes through (flag 0x2000). */
                if (!(q->qbn.person[op->arg[1].value].flags & 0x2000)) { consumed = 1; }
            } else if (op->opcode == 71 && is_person(q, op->arg[3].value, who, faction)) {
                if (og_gold_can_afford(op->arg[2].value)) {
                    og_gold_spend(op->arg[2].value);
                    arg_set(q, op, 0, 1); arg_set(q, op, 1, 0); done(q, op);
                } else { arg_set(q, op, 0, 0); arg_set(q, op, 1, 1); }
                consumed = 1;
            } else if (op->opcode == 1 && !(op->flags & 1) && is_person(q, op->arg[2].value, who, faction)) {
                /* The item given to the person: carried, it is taken from the player and forgotten, and the state set. */
                int n;

                n = op->arg[1].value;
                if (n >= 0 && n < q->qbn.n_item && q->item[n].object && carried(q->item[n].object)) {
                    og_inventory_unequip(q->item[n].object);
                    og_object_delete(q->item[n].object); item_forget(q, n);
                    arg_set(q, op, 0, 1); done(q, op);
                    if (!(q->qbn.person[op->arg[2].value].flags & 0x2000)) { consumed = 1; }
                }
            } else if (op->opcode == 78 && op->arg[1].literal && op->arg[1].value == faction) {
                arg_set(q, op, 0, 1); consumed = 1;
            }
        }
    }
    return consumed;
}


/* The state's events that quests watch (the original's quest_event_object): an item the pack took (3) or put down (5), a spell cast (73). */
void og_quest_event(int kind, df_u32 subject, df_u32 other)
{
    int s, i, n;

    if (kind != OG_EVENT_GIVE && kind != OG_EVENT_DROP && kind != OG_EVENT_SPELL) { return; }
    for (s = 0; s < OG_QUESTS; s++) {
        OgQuest *q;

        q = &g_quest[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_op; i++) {
            OgQbnOp *op;

            op = &q->qbn.op[i];
            if (kind == OG_EVENT_GIVE && op->opcode == 3 && !op->arg[1].literal) {
                n = op->arg[1].value;
                if (n >= 0 && n < q->qbn.n_item && q->item[n].object == subject) { arg_set(q, op, 0, 1); done(q, op); }
            } else if (kind == OG_EVENT_DROP && op->opcode == 5 && !op->arg[1].literal) {
                n = op->arg[1].value;
                if (n >= 0 && n < q->qbn.n_item && q->item[n].object == subject && other != og_state()->inventory_id && in_place(q, op->arg[2].value)) { arg_set(q, op, 0, 1); done(q, op); }
            } else if (kind == OG_EVENT_SPELL && op->opcode == 73 && !(op->flags & 1) && arg_state(q, op, 0)) {
                const OgObject *spell;

                spell = og_object(subject);
                if (spell && spell->index == op->arg[2].value) { arg_set(q, op, 1, 1); op->flags |= 1; }
            }
        }
    }
}

/* ---- Foes in the world (quest_place_object) and the events they raise ---- */

/* Make a placed foe's creatures: as many as its count less the kills, one key each. */
static void spawn_foe(OgQuest *q, int f)
{
    int want, made;

    if (!g_world || !g_world->spawn_foe || q->foe[f].live) { return; }
    want = q->qbn.foe[f].count - q->qbn.foe[f].killed;
    if (want > 16) { want = 16; }
    for (made = 0; made < want; made++) {
        if (!g_world->spawn_foe(q->qbn.foe[f].type, OG_QUEST_FOE_THING(q->foe[f].number, made))) { break; }
    }
    q->foe[f].live = made;
}

void og_quest_location_loaded(int region, int index)
{
    int s, f;

    for (s = 0; s < OG_QUESTS; s++) {
        if (!g_quest[s].used) { continue; }
        lay_items(&g_quest[s], region, index);
        for (f = 0; f < g_quest[s].qbn.n_foe; f++) {
            OgQuestFoe *foe;
            const OgQuestPlace *place;

            foe = &g_quest[s].foe[f];
            if (!foe->placed || foe->live || foe->place < 0 || foe->place >= g_quest[s].qbn.n_place) { continue; }
            place = &g_quest[s].place[foe->place];
            if (place->bound && place->region == region && place->index == index) { spawn_foe(&g_quest[s], f); }
        }
    }
}

/* Event 2 and 21 for every quest that has the foe: the ops of that opcode on it. */
static void foe_event(int number, int opcode)
{
    int s, i, f;

    for (s = 0; s < OG_QUESTS; s++) {
        OgQuest *q;

        q = &g_quest[s];
        if (!q->used) { continue; }
        for (f = 0; f < q->qbn.n_foe && q->foe[f].number != number; f++) { }
        if (f == q->qbn.n_foe) { continue; }
        for (i = 0; i < q->qbn.n_op; i++) {
            OgQbnOp *op;

            op = &q->qbn.op[i];
            if (op->opcode != opcode || op->arg[1].literal || op->arg[1].value != f) { continue; }
            if (opcode == 2) {
                if (q->qbn.foe[f].killed < op->arg[2].value) { q->qbn.foe[f].killed++; }
                if (q->qbn.foe[f].killed >= op->arg[2].value) { arg_set(q, op, 0, 1); done(q, op); }
            } else { arg_set(q, op, 0, 1); }
        }
        if (opcode == 2 && q->foe[f].live > 0) { q->foe[f].live--; }
        if (opcode == 2) { foe_dropped(q, f); }
    }
}

void og_quest_foe_killed(int number) { foe_event(number, 2); }
void og_quest_foe_hit(int number) { foe_event(number, 21); }

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
    for (i = 0; i < q->qbn.n_place; i++) {
        if (q->qbn.place[i].symbol != hash) { continue; }
        /* A place bound to a location says its name (forms 0, 16, 32) or its region's (48). */
        if (q->place[i].bound) {
            if (form >= 48 && g_world && g_world->region_name) { return g_world->region_name(q->place[i].region, out, cap) > 0; }
            return log_format(out, cap, "%s", q->place[i].name) > 0;
        }
        return g_names && g_names->place && g_names->place(slot, i, form, out, cap);
    }
    for (i = 0; i < q->qbn.n_person; i++) {
        if (q->qbn.person[i].symbol != hash) { continue; }
        /* A person bound to someone says their name (0), their town (32) or the region (48); the building (16) is the town's here. */
        if (q->person[i].bound && q->qbn.person[i].kind == 21) {
            if (form >= 48 && g_world && g_world->region_name) { int region, here, px, py; g_world->here(&region, &here, &px, &py); return g_world->region_name(region, out, cap) > 0; }
            return log_format(out, cap, "%s", form >= 16 ? q->person[i].town : q->person[i].name) > 0;
        }
        return g_names && g_names->person && g_names->person(slot, i, form, out, cap);
    }
    for (i = 0; i < q->qbn.n_foe; i++) {
        if (q->qbn.foe[i].symbol == hash) {
            const OgFoeDef *def;

            def = og_foe_def(q->qbn.foe[i].type);
            return log_format(out, cap, "%s", def ? def->name : "");
        }
    }
    for (i = 0; i < q->qbn.n_timer; i++) {
        if (q->qbn.timer[i].hash == hash) {
            delay = q->timer[i].delay;
            /* _a`b_: the second timer's delay is added to the first's. */
            for (i = 0; hash2 && i < q->qbn.n_timer; i++) { if (q->qbn.timer[i].hash == hash2) { delay += q->timer[i].delay; } }
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

/* ---- Rumours ---- */

/* rumor_add_quest: the message's text, filled in, kept with the quest's id; 30 days, or 180 for the questor's. A full list loses its first. */
static void rumour_add(int slot, int message, int target, int flags)
{
    OgRumour *r;
    char raw[1024], said[1024];
    int i;

    if (!g_read) { return; }
    raw[0] = 0;
    if (g_read(g_quest[slot].qbn.name, message, 0, raw, sizeof(raw)) <= 0 || !raw[0]) { return; }
    og_quest_expand(slot, raw, said, sizeof(said), og_state()->minutes, 0);
    for (i = 0; i < OG_RUMOURS && g_rumour[i].used; i++) { }
    if (i == OG_RUMOURS) { for (i = 1; i < OG_RUMOURS; i++) { g_rumour[i - 1] = g_rumour[i]; } i = OG_RUMOURS - 1; }
    r = &g_rumour[i];
    mem_zero(r, sizeof(*r));
    r->used = 1; r->quest_id = g_quest[slot].qbn.id; r->message = message; r->flags = flags; r->target = target;
    r->expires = og_state()->minutes + (flags & 2 ? 180u : 30u) * 1440u;
    log_format(r->name, sizeof(r->name), "%s", g_quest[slot].qbn.name);
    log_format(r->text, sizeof(r->text), "%s", said);
}

/* The questor's own rumour at the quest's end: the person of kind 21. */
static void rumour_questor(int slot, int rewarded)
{
    int i;

    for (i = 0; i < g_quest[slot].qbn.n_person; i++) {
        if (g_quest[slot].qbn.person[i].kind == 21 && g_quest[slot].person[i].bound) { rumour_add(slot, rewarded ? 1008 : 1009, g_quest[slot].person[i].who, 2); return; }
    }
}

const OgRumour *og_rumour(int i) { return i >= 0 && i < OG_RUMOURS && g_rumour[i].used ? &g_rumour[i] : 0; }

int og_quest_news(int faction, char *out, int cap)
{
    int i, n, pick;

    (void)faction;
    n = 0;
    for (i = 0; i < OG_RUMOURS; i++) { if (g_rumour[i].used && (g_rumour[i].flags & 12) && g_rumour[i].expires > og_state()->minutes) { n++; } }
    if (!n) { return 0; }
    pick = og_random_range(0, n - 1);
    for (i = 0; i < OG_RUMOURS; i++) {
        if (g_rumour[i].used && (g_rumour[i].flags & 12) && g_rumour[i].expires > og_state()->minutes && pick-- == 0) { log_format(out, cap, "%s", g_rumour[i].text); return 1; }
    }
    return 0;
}

int og_quest_rumour_for(int who, char *out, int cap)
{
    int i;

    for (i = 0; i < OG_RUMOURS; i++) {
        if (g_rumour[i].used && (g_rumour[i].flags & 2) && g_rumour[i].target == who && g_rumour[i].expires > og_state()->minutes) { log_format(out, cap, "%s", g_rumour[i].text); return 1; }
    }
    return 0;
}

/* ---- Talk topics ---- */

/* The nth topic: its quest, the section (0 item, 3 person, 4 place) and the record. A resource is a topic when it is not hidden (an item and a place 128, a person 0x8000) and has a message. */
static int topic_find(int n, int *quest, int *section, int *index)
{
    int s, i;

    for (s = 0; s < OG_QUESTS; s++) {
        const OgQuest *q;

        q = &g_quest[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_item; i++) {
            if (!(q->qbn.item[i].flags & 128) && (q->qbn.item[i].message[0] > 0 || q->qbn.item[i].message[1] > 0) && n-- == 0) { *quest = s; *section = 0; *index = i; return 1; }
        }
        for (i = 0; i < q->qbn.n_person; i++) {
            if (!(q->qbn.person[i].flags & 0x8000) && (q->qbn.person[i].message[0] > 0 || q->qbn.person[i].message[1] > 0) && n-- == 0) { *quest = s; *section = 3; *index = i; return 1; }
        }
        for (i = 0; i < q->qbn.n_place; i++) {
            if (!(q->qbn.place[i].flags & 128) && (q->qbn.place[i].message[0] > 0 || q->qbn.place[i].message[1] > 0) && n-- == 0) { *quest = s; *section = 4; *index = i; return 1; }
        }
    }
    return 0;
}

int og_quest_topic_count(void)
{
    int n, s, i;

    n = 0;
    for (s = 0; s < OG_QUESTS; s++) {
        const OgQuest *q;

        q = &g_quest[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_item; i++) { if (!(q->qbn.item[i].flags & 128) && (q->qbn.item[i].message[0] > 0 || q->qbn.item[i].message[1] > 0)) { n++; } }
        for (i = 0; i < q->qbn.n_person; i++) { if (!(q->qbn.person[i].flags & 0x8000) && (q->qbn.person[i].message[0] > 0 || q->qbn.person[i].message[1] > 0)) { n++; } }
        for (i = 0; i < q->qbn.n_place; i++) { if (!(q->qbn.place[i].flags & 128) && (q->qbn.place[i].message[0] > 0 || q->qbn.place[i].message[1] > 0)) { n++; } }
    }
    return n;
}

int og_quest_topic_name(int i, char *out, int cap)
{
    int quest, section, index, ok;

    out[0] = 0;
    if (!topic_find(i, &quest, &section, &index) || !g_names) { return 0; }
    ok = 0;
    if (section == 0 && g_names->item) { ok = g_names->item(quest, index, 0, out, cap); }
    else if (section == 3 && g_names->person) { ok = g_names->person(quest, index, 0, out, cap); }
    else if (section == 4 && g_names->place) { ok = g_names->place(quest, index, 0, out, cap); }
    return ok;
}

int og_quest_topic_answer(int i, char *out, int cap)
{
    int quest, section, index, message;
    char raw[1024];

    out[0] = 0;
    if (!g_read || !topic_find(i, &quest, &section, &index)) { return 0; }
    if (section == 0) { message = g_quest[quest].qbn.item[index].message[1] > 0 ? g_quest[quest].qbn.item[index].message[1] : g_quest[quest].qbn.item[index].message[0]; }
    else if (section == 3) { message = g_quest[quest].qbn.person[index].message[1] > 0 ? g_quest[quest].qbn.person[index].message[1] : g_quest[quest].qbn.person[index].message[0]; }
    else { message = g_quest[quest].qbn.place[index].message[1] > 0 ? g_quest[quest].qbn.place[index].message[1] : g_quest[quest].qbn.place[index].message[0]; }
    raw[0] = 0;
    if (g_read(g_quest[quest].qbn.name, message, 0, raw, sizeof(raw)) <= 0 || !raw[0]) { return 0; }
    og_quest_expand(quest, raw, out, cap, og_state()->minutes, 0);
    return 1;
}

/* ---- Faces ---- */

const OgQuestAccess *og_quest_access(int i) { return i >= 0 && i < OG_QUEST_ACCESS && g_access[i].used ? &g_access[i] : 0; }
const OgQuestFace *og_quest_face(int i) { return i >= 0 && i < OG_QUEST_FACES && g_face[i].used ? &g_face[i] : 0; }

/* quest_face_add and its removal: one slot a person or foe of a quest; a full list takes none. */
static void face_set(const OgQuest *q, int kind, int index, int on)
{
    int i, free_slot;

    free_slot = -1;
    for (i = 0; i < OG_QUEST_FACES; i++) {
        if (g_face[i].used && g_face[i].quest_id == q->qbn.id && g_face[i].kind == kind && g_face[i].index == index) {
            if (!on) { g_face[i].used = 0; }
            return;
        }
        if (!g_face[i].used && free_slot < 0) { free_slot = i; }
    }
    if (on && free_slot >= 0) { g_face[free_slot].used = 1; g_face[free_slot].quest_id = q->qbn.id; g_face[free_slot].kind = kind; g_face[free_slot].index = index; }
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

/* The quest tree's record: what a quest's items hang from until they are given. */
static df_u32 quest_root(void)
{
    int i;

    for (i = 0; i < OG_OBJECTS; i++) { if (og_state()->object[i].id && og_state()->object[i].type == OG_RECORD_QUESTS) { return og_state()->object[i].id; } }
    return 0;
}

/* quest_init_item (2.5): a group below 0 is drawn among the groups that have items, an index below 0 among the group's; a template from the parent quest (group 100) is not made. A letter
 * (group 9, index 5) carries its QRC message. The item hangs from the quest tree. */
static void make_item(OgQuest *q, int i)
{
    const OgQbnItem *it;
    int group, index, count;

    it = &q->qbn.item[i];
    group = it->group; index = it->index;
    if (group == 100 && index >= 0 && index < 4 && g_tpl_group[index] >= 0) { group = g_tpl_group[index]; index = g_tpl_index[index]; }
    if (group == 100 || group > 27 || !quest_root()) { return; }
    if (group < 0) {
        int tries;

        for (tries = 0; tries < 200; tries++) { group = og_random_range(0, 27); if (og_item_def(group, 0)) { break; } }
    }
    if (!og_item_def(group, 0)) { return; }
    if (index < 0) {
        for (count = 0; og_item_def(group, count); count++) { }
        index = og_random_range(0, count - 1);
    }
    q->item[i].object = og_item_create(quest_root(), group, index, -1);
    if (q->item[i].object && group == 9 && index == 5 && it->message[1] > 0) { og_object_edit(q->item[i].object)->message = it->message[1]; }
}

/* Whether the player has the object: up its parents to the player's pack (the quest tree hangs from the same entity, and is not the pack). */
static int carried(df_u32 id)
{
    int guard;

    for (guard = 0; id && guard < 16; guard++) {
        const OgObject *o;

        if (id == og_state()->inventory_id) { return 1; }
        o = og_object(id);
        if (!o) { return 0; }
        id = o->parent;
    }
    return 0;
}

/* An item the quest forgets or deletes (ops 54 and 36), and every item it still holds when it ends (the quest's id is on them). */
static void item_forget(OgQuest *q, int n) { if (n >= 0 && n < q->qbn.n_item) { q->item[n].object = 0; q->item[n].carrier = 0; } }

static int in_place(const OgQuest *q, int n)
{
    int region, here, px, py;

    if (n < 0 || n >= q->qbn.n_place || !q->place[n].bound || !g_world || !g_world->here) { return 0; }
    g_world->here(&region, &here, &px, &py);
    return q->place[n].region == region && q->place[n].index == here;
}

/* An item set down in the world at the player's feet: the pile the game makes, the item moved into it. */
static void put_item_down(OgQuest *q, int i)
{
    df_u32 pile;

    if (!q->item[i].object || !g_world || !g_world->place_item) { return; }
    pile = g_world->place_item();
    if (pile) { og_object_reparent(q->item[i].object, pile); }
}

/* Items of op 0 whose place is the location that was just loaded are laid in it (quest_place_object when the place is the loaded one). */
static void lay_items(OgQuest *q, int region, int index)
{
    int i;

    for (i = 0; i < q->qbn.n_item; i++) {
        if (q->item[i].object && q->item[i].place >= 0 && !q->item[i].laid && q->place[q->item[i].place].bound &&
            q->place[q->item[i].place].region == region && q->place[q->item[i].place].index == index) {
            put_item_down(q, i); q->item[i].laid = 1;
        }
    }
}

/* The foe that carried an item (op 39) has died: the item lies where the player is. */
static void foe_dropped(OgQuest *q, int f)
{
    int i;

    for (i = 0; i < q->qbn.n_item; i++) { if (q->item[i].object && q->item[i].carrier == f + 1) { put_item_down(q, i); q->item[i].carrier = 0; } }
}

/* ---- The ops of this step ---- */

static int g_minute;                  /* set while a passing minute runs the quests (op 79) */
static int g_prompt_slot = -1, g_prompt_op = -1;   /* op 29's box open: the quest and the op that waits for the answer */

/* op 29's answer: the yes state or the no state is set, and the op is done. */
static void prompt_answer(int yes)
{
    OgQuest *q;
    OgQbnOp *op;

    if (g_prompt_slot < 0 || !g_quest[g_prompt_slot].used) { g_prompt_slot = -1; return; }
    q = &g_quest[g_prompt_slot]; op = &q->qbn.op[g_prompt_op];
    g_prompt_slot = -1;
    arg_set(q, op, yes ? 1 : 2, 1);
    op->flags |= 1;
}

/* A person's faction by the quest's own binding (a faction-only person, or the one who gave it). */
static int person_faction(const OgQuest *q, int n)
{
    return n >= 0 && n < q->qbn.n_person && q->person[n].bound ? q->person[n].faction : 0;
}

static int minute_of_day(void) { return (int)(og_state()->minutes % 1440u); }

/* Whether the player is in a town, a village or the like: the location they are in, not a dungeon (kinds 4, 7 and over 9 are the dungeons and the rest). */
static int town_here(void)
{
    int region, here, px, py, n;
    OgLocation lo;

    if (!g_world || !g_world->here || !g_world->count || !g_world->location) { return 0; }
    g_world->here(&region, &here, &px, &py);
    if (here < 0) { return 0; }
    for (n = 0; n < g_world->count(); n++) {
        if (g_world->location(n, &lo) && lo.region == region && lo.index == here) { return lo.type != 4 && lo.type != 7 && lo.type <= 9; }
    }
    return 0;
}

static int same_text(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void run_op(int slot, OgQuest *q, OgQbnOp *op)
{
    int i, n, count, pick, found;

    switch (op->opcode) {
    case 10:
    case 11:
        /* Once: the topics shown (10) or hidden (11): the place and the item by flag 128, the person by 0x8000. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            for (i = 1; i < 4 && i < op->argc; i++) {
                n = op->arg[i].value;
                if (op->arg[i].literal || n < 0) { continue; }
                if (i == 1 && n < q->qbn.n_place) { if (op->opcode == 11) { q->qbn.place[n].flags |= 128; } else { q->qbn.place[n].flags &= 127; } }
                if (i == 2 && n < q->qbn.n_person) { if (op->opcode == 11) { q->qbn.person[n].flags |= 0x8000; } else { q->qbn.person[n].flags &= ~0x8000; } }
                if (i == 3 && n < q->qbn.n_item) { if (op->opcode == 11) { q->qbn.item[n].flags |= 128; } else { q->qbn.item[n].flags &= 127; } }
            }
            done(q, op);
        }
        break;
    case 25:
        /* While the gate holds and the flag is clear: the counter counts down, and at 0 the flag is set. */
        if (arg_state(q, op, 0) && !(op->flags & 1) && op->arg[1].value != -1 && !arg_state(q, op, 1)) {
            if (op->arg[2].value != 0) { op->arg[2].value--; }
            if (op->arg[2].value == 0) { arg_set(q, op, 1, 1); done(q, op); }
        }
        break;
    case 44:
        /* Once: the person gone from the quest. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) { n = op->arg[1].value; if (n >= 0 && n < q->qbn.n_person) { q->person[n].bound = 0; } done(q, op); }
        break;
    case 46:
    case 48:
        /* Once: the person hidden (flags 0xA00) or shown. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_person) { if (op->opcode == 46) { q->qbn.person[n].flags |= 0xA00; } else { q->qbn.person[n].flags &= ~0xA00; } }
            done(q, op);
        }
        break;
    case 55:
    case 56:
    case 63:
    case 64:
        /* Once: a person's (55, 56) or a foe's (63, 64) face put at the top of the screen, or taken away. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < (op->opcode < 60 ? q->qbn.n_person : q->qbn.n_foe)) { face_set(q, op->opcode < 60 ? 1 : 2, n, op->opcode == 55 || op->opcode == 63); }
            done(q, op);
        }
        break;
    case 8:
        /* Once, outside region 31: another quest started, S plus seven digits drawn between the two numbers. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            int region, here, px, py;

            region = 0;
            if (g_world && g_world->here) { g_world->here(&region, &here, &px, &py); }
            if (region != 31) {
                char child[12];
                int k, lo, hi, active;

                lo = op->arg[1].value; hi = op->arg[2].value;
                log_format(child, sizeof(child), "S%07d", og_random_range(lo < hi ? lo : hi, lo < hi ? hi : lo));
                active = 0;
                for (k = 0; k < OG_QUESTS; k++) { if (g_quest[k].used && same_text(g_quest[k].qbn.name, child)) { active = 1; } }
                if (!active) { og_quest_start(child, 0); }
            }
            done(q, op);
        }
        break;
    case 9:
        /* A foe placed again and again: every `interval` minutes, with a chance in a hundred, `count` more (a count of -1 never runs out; 0 is a no-op). */
        if (arg_state(q, op, 0) && op->arg[4].value != 0 && op->arg[1].value >= 0 && op->arg[1].value < q->qbn.n_foe &&
            og_state()->minutes - op->last_minutes >= (df_u32)op->arg[2].value) {
            op->last_minutes = og_state()->minutes;
            if (og_random_range(0, 32767) % 100 <= op->arg[3].value) {
                OgQuestFoe *foe;
                int made;

                if (op->arg[4].value != -1) { op->arg[4].value--; }
                foe = &q->foe[op->arg[1].value];
                for (made = 0; g_world && g_world->spawn_foe && made < q->qbn.foe[op->arg[1].value].count && made < 16; made++) {
                    if (!g_world->spawn_foe(q->qbn.foe[op->arg[1].value].type, OG_QUEST_FOE_THING(foe->number, (foe->next++) & 15))) { break; }
                    foe->live++;
                }
            }
        }
        break;
    case 17:
        /* Every run while the gate holds: the place's building open to the access kind until the minutes are up (the grant of the same place and kind is renewed). */
        if (arg_state(q, op, 0)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_place && q->place[n].bound) {
                int free_slot;

                free_slot = -1;
                for (i = 0; i < OG_QUEST_ACCESS; i++) {
                    if (g_access[i].used && g_access[i].region == q->place[n].region && g_access[i].index == q->place[n].index && g_access[i].access == op->arg[2].value) { free_slot = i; break; }
                    if (!g_access[i].used && free_slot < 0) { free_slot = i; }
                }
                if (free_slot >= 0) {
                    g_access[free_slot].used = 1; g_access[free_slot].quest_id = q->qbn.id; g_access[free_slot].region = q->place[n].region; g_access[free_slot].index = q->place[n].index;
                    g_access[free_slot].access = op->arg[2].value; g_access[free_slot].until = og_state()->minutes + (df_u32)op->arg[3].value;
                }
            }
            done(q, op);
        }
        break;
    case 30:
        /* Once: the person put in the place (a person that is only a faction stays). With no person named: the player taken to the place of the person of the next argument. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            if (op->arg[1].value == -1) {
                n = op->arg[2].value;
                if (n >= 0 && n < q->qbn.n_person && q->person[n].at > 0 && q->place[q->person[n].at - 1].bound && g_world && g_world->go) {
                    g_world->go(q->place[q->person[n].at - 1].region, q->place[q->person[n].at - 1].index);
                }
            } else {
                n = op->arg[1].value;
                if (n >= 0 && n < q->qbn.n_person && q->person[n].bound && q->qbn.person[n].kind != -8 && op->arg[2].value >= 0 && op->arg[2].value < q->qbn.n_place) { q->person[n].at = op->arg[2].value + 1; }
            }
            done(q, op);
        }
        break;
    case 75:
        /* Every run while the gate holds: the items named are the templates of the quest op 8 starts. */
        if (arg_state(q, op, 0)) {
            for (i = 1; i < 5 && i < op->argc; i++) {
                n = op->arg[i].value;
                if (!op->arg[i].literal && n >= 0 && n < q->qbn.n_item) { g_tpl_group[i - 1] = q->qbn.item[n].group; g_tpl_index[i - 1] = q->qbn.item[n].index; }
            }
        }
        break;
    case 81:
        /* Once: the place and the person marked open to the quest (place flag 64, person 0x4000). */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (!op->arg[1].literal && n >= 0 && n < q->qbn.n_place) { q->qbn.place[n].flags |= 64; }
            n = op->arg[2].value;
            if (!op->arg[2].literal && n >= 0 && n < q->qbn.n_person) { q->qbn.person[n].flags |= 0x4000; }
        }
        break;
    case 83:
        /* Once: the player taken to the dungeon of a region whose map pixel is the number (map id: the pixel's row times 1000 and its column). */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            if (g_world && g_world->go && g_world->count && g_world->location) {
                OgLocation lo;

                for (n = 0; n < g_world->count(); n++) {
                    if (g_world->location(n, &lo) && lo.region == op->arg[2].value && lo.py * 1000 + lo.px == op->arg[3].value) { g_world->go(lo.region, lo.index); break; }
                }
            }
            op->flags |= 1;
        }
        break;
    case 4:
        /* Once: +5 with the quest's faction and the reward given. The reward box of the original holds gold as a purse here (it would open the inventory on it): the coins go straight to the
         * player. An item already given is passed over (7.4). Other items wait for step 9; the Brotherhood's and the Thieves' joining quests too. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            q->rewarded = 1;
            if (q->qbn.faction) { og_faction_change_reputation(q->qbn.faction, 5); }
            for (i = 1; i < op->argc; i++) {
                n = op->arg[i].value;
                if (op->arg[i].literal || n < 0 || n >= q->qbn.n_item || q->item[n].given) { continue; }
                if (q->item[n].gold) { og_character()->gold += q->item[n].amount; q->item[n].given = 1; }
            }
            done(q, op);
        }
        break;
    case 0:
        /* Once: the item set in its place: now when that is the location the player is in, else when it is loaded. An item already out in the world is taken back first. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_item && q->item[n].object) {
                q->item[n].place = op->arg[2].value >= 0 && op->arg[2].value < q->qbn.n_place ? op->arg[2].value : -1;
                q->item[n].laid = 0; q->item[n].carrier = 0;
                if (q->item[n].place >= 0 && in_place(q, q->item[n].place)) { put_item_down(q, n); q->item[n].laid = 1; }
            }
            done(q, op);
        }
        break;
    case 26:
        /* Once: the item given to the player (into the pack). A pack with no room leaves it for the next run. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_item && q->item[n].object && !og_inventory_transfer(q->item[n].object, og_state()->inventory_id)) { break; }
            done(q, op);
        }
        break;
    case 36:
        /* Once: the item deleted. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            done(q, op);
            if (n >= 0 && n < q->qbn.n_item && q->item[n].object) { og_object_delete(q->item[n].object); item_forget(q, n); }
        }
        break;
    case 54:
        /* Once: the item forgotten: it stays in the world, no longer the quest's. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) { item_forget(q, op->arg[1].value); done(q, op); }
        break;
    case 39:
        /* Once: the foe carries the item; it is set down where the player is when the foe dies. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_item && q->item[n].object && op->arg[2].value >= 0 && op->arg[2].value < q->qbn.n_foe) { q->item[n].carrier = op->arg[2].value + 1; q->item[n].place = -1; }
            done(q, op);
        }
        break;
    case 43:
        /* Every run while the gate holds: the result is whether the player is in the place; done when it is. */
        if (arg_state(q, op, 0) && op->arg[2].value >= 0) {
            found = in_place(q, op->arg[2].value);
            arg_set(q, op, 1, found);
            if (found) { done(q, op); }
        }
        break;
    case 57:
        /* Once the item has been used (flag 512): the result is set. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[2].value;
            if (n >= 0 && n < q->qbn.n_item && q->item[n].object && og_object(q->item[n].object) && (og_object(q->item[n].object)->flags & 512)) { arg_set(q, op, 1, 1); done(q, op); }
        }
        break;
    case 70:
        /* Once the player carries every item named: the result is set; until then it is clear. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            found = 1;
            for (i = 2; i < 5 && i < op->argc; i++) {
                n = op->arg[i].value;
                if (op->arg[i].literal || n == -1 || n == -2) { continue; }
                if (n < 0 || n >= q->qbn.n_item || !q->item[n].object || !carried(q->item[n].object)) { found = 0; }
            }
            arg_set(q, op, 1, found);
            if (found) { done(q, op); }
        }
        break;
    case 76:
        /* The item delivered later: pending, then handed over by chance on a passing minute when the player is in a town by day (the original also wants no creature near and the street, not
         * a building: no source for them here yet). */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n < 0 || n >= q->qbn.n_item || !q->item[n].object) { done(q, op); break; }
            if (g_minute && minute_of_day() >= 360 && minute_of_day() < 1080 && town_here() && og_random_range(1, 100) < 10 &&
                og_inventory_transfer(q->item[n].object, og_state()->inventory_id)) { done(q, op); }
        }
        break;
    case 6:
        /* The end: when set (no delivery is pending and the box is empty, both so with gold paid directly). A quest never rewarded costs -2 with its faction. The rumours are step 9. */
        if (arg_state(q, op, 0)) {
            if (!q->rewarded && q->qbn.faction) { og_faction_change_reputation(q->qbn.faction, -2); }
            done(q, op);
            rumour_add(slot, q->rewarded ? 1007 : 1006, 0, 8);
            rumour_questor(slot, q->rewarded);
            q->ended = 1;
        }
        break;
    case 7:
        if (arg_state(q, op, 0)) { for (i = 1; i < op->argc; i++) { arg_set(q, op, i, 0); } }
        break;
    case 12:
        /* Once: start the timer that is not running, its delay worked from now. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_timer && !(q->timer[n].flags & 64)) {
                timer_start(q, n); q->timer[n].flags = (q->timer[n].flags | 64) & ~128;
            }
            done(q, op);
        }
        break;
    case 13:
        if (arg_state(q, op, 0)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_timer) { q->timer[n].flags &= ~64; }
            done(q, op);
        }
        break;
    case 19:
    case 20:
        /* Once: the place's location shown on the map (7.12: the bound place, whatever its kind). */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (g_world && g_world->reveal && n >= 0 && n < q->qbn.n_place && q->place[n].bound) { g_world->reveal(q->place[n].ordinal); }
            done(q, op);
        }
        break;
    case 27:
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            if (g_world && g_world->reveal && g_world->count && g_world->location) {
                OgLocation lo;

                for (n = 0; n < g_world->count(); n++) {
                    if (g_world->location(n, &lo) && lo.region == op->arg[2].value && lo.index == op->arg[3].value) { g_world->reveal(n); break; }
                }
            }
            done(q, op);
        }
        break;
    case 22:
    case 33:
        /* Once: the foe placed (a place of the quest, or near the player when there is none); made now when the dungeon is the one the player is in, else when it is loaded. */
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            n = op->arg[1].value;
            if (n >= 0 && n < q->qbn.n_foe) {
                int where;

                where = !op->arg[2].literal && op->arg[2].value >= 0 ? op->arg[2].value : -1;
                q->foe[n].placed = 1; q->foe[n].place = where; q->foe[n].live = 0;
                if (g_world && g_world->here) {
                    int region, here, px, py;

                    g_world->here(&region, &here, &px, &py);
                    if (where < 0 ? here >= 0 : (q->place[where].bound && q->place[where].region == region && q->place[where].index == here)) { spawn_foe(q, n); }
                }
            }
            done(q, op);
        }
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
    case 29:
        /* Once: a yes/no box with the op's QRC message; the answer sets its yes or no state. One box at a time. */
        if (arg_state(q, op, 0) && !(op->flags & 1) && g_prompt_slot < 0 && g_ask && g_read) {
            char text[1024], said[1024];

            text[0] = 0;
            if (g_read(q->qbn.name, op->message & 0x7fff, (op->message & 0x8000) != 0, text, sizeof(text)) > 0 && text[0]) {
                og_quest_expand(slot, text, said, sizeof(said), og_state()->minutes, 0);
                g_prompt_slot = slot; g_prompt_op = (int)(op - q->qbn.op);
                if (!g_ask(said, prompt_answer)) { g_prompt_slot = -1; }
            } else { op->flags |= 1; }
        }
        break;
    case 37:
        /* Every run while the gate holds: the result is whether the person's faction thinks of the player at least `value` (and their social group's standing is added when it is under 5). */
        if (arg_state(q, op, 0) && op->arg[1].value != -1 && person_faction(q, op->arg[2].value)) {
            int faction, rep;
            const OgFactionDef *def;

            faction = person_faction(q, op->arg[2].value);
            rep = og_faction_rep(faction);
            def = og_faction_def(faction);
            if (def && def->social_group >= 0 && def->social_group < 5) { rep += og_character()->reputation[def->social_group]; }
            arg_set(q, op, 1, rep >= op->arg[3].value);
            done(q, op);
        }
        break;
    case 49:
        if (arg_state(q, op, 0) && !(op->flags & 1)) { og_ailments_cure(0, 0); done(q, op); }
        break;
    case 61:
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            if (person_faction(q, op->arg[1].value)) { og_faction_change_reputation(person_faction(q, op->arg[1].value), op->arg[2].value); }
            done(q, op);
        }
        break;
    case 65:
        if (arg_state(q, op, 0) && !(op->flags & 1)) {
            int region, here, px, py;

            region = 0;
            if (g_world && g_world->here) { g_world->here(&region, &here, &px, &py); }
            og_region_add_legal(region, op->arg[1].value);
            if (og_region_legal(region) > 100) { og_region_add_legal(region, 100 - og_region_legal(region)); }
            if (og_region_legal(region) < -100) { og_region_add_legal(region, -100 - og_region_legal(region)); }
            done(q, op);
        }
        break;
    case 72:
        if (arg_state(q, op, 0) && !(op->flags & 1)) { og_disease_infect(0, op->arg[1].value, 1); done(q, op); }
        break;
    case 79:
        /* Once a game minute while the state is clear: set when the faction's reputation (as it stands) is at least the value. */
        if (!arg_state(q, op, 0) && g_minute && op->arg[1].literal && op->arg[2].literal && og_faction_rep(op->arg[1].value) >= op->arg[2].value) { arg_set(q, op, 0, 1); }
        break;
    case 80:
        if (arg_state(q, op, 0)) { show(q, op->message); }
        break;
    case 82:
        /* The person's talk passes through (flag 0x2000) while the state is set. */
        n = op->arg[1].value;
        if (n >= 0 && n < q->qbn.n_person) { if (arg_state(q, op, 0)) { q->qbn.person[n].flags |= 0x2000; } else { q->qbn.person[n].flags &= ~0x2000; } }
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

void og_quest_run_minute(void)
{
    g_minute = 1;
    og_quest_run();
    g_minute = 0;
}

void og_quest_start_initial_later(void) { g_initial_pending = 1; }

void og_quest_run(void)
{
    if (g_initial_pending && g_world) {
        /* quests_start_initial, once the world is open: no offer box (neither name is a Y quest). */
        g_initial_pending = 0;
        og_quest_start("_BRISIEN", 0);
        og_quest_start("_TUTOR__", 0);
    }
    int slot, i;

    for (slot = 0; slot < OG_QUESTS; slot++) {
        OgQuest *q;

        q = &g_quest[slot];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_op && !q->ended; i++) { run_op(slot, q, &q->qbn.op[i]); }
        /* The four template slots are cleared after each quest's run. */
        for (i = 0; i < 4; i++) { g_tpl_group[i] = -1; }
        if (!q->ended) { timers_run(q); }
        if (q->ended) { og_quest_end(slot); }
    }
}

/* ---- Saving ---- */

typedef struct { df_u8 *p; int n, cap, bad; } QWire;

static void qput(QWire *w, df_u32 v)
{
    if (w->n + 4 > w->cap) { w->bad = 1; return; }
    w->p[w->n++] = (df_u8)v; w->p[w->n++] = (df_u8)(v >> 8); w->p[w->n++] = (df_u8)(v >> 16); w->p[w->n++] = (df_u8)(v >> 24);
}

static df_u32 qget(QWire *w)
{
    df_u32 v;

    if (w->n + 4 > w->cap) { w->bad = 1; return 0; }
    v = (df_u32)w->p[w->n] | (df_u32)w->p[w->n + 1] << 8 | (df_u32)w->p[w->n + 2] << 16 | (df_u32)w->p[w->n + 3] << 24;
    w->n += 4;
    return v;
}

static void qbytes(QWire *w, const char *b, int len, int read)
{
    int i;

    if (w->n + len > w->cap) { w->bad = 1; return; }
    for (i = 0; i < len; i++) { if (read) { ((char *)b)[i] = (char)w->p[w->n + i]; } else { w->p[w->n + i] = (df_u8)b[i]; } }
    w->n += len;
}

/* One quest's changing parts, written or read by the same walk (a read fills what a write puts). The sparse tables (ops, places, persons) lead with a count of the rows that are not empty. */
static void qwire_quest(QWire *w, OgQuest *q, int read)
{
    int i, k, count;
    df_u32 v;

#define QI(x) do { if (read) { (x) = (int)qget(w); } else { qput(w, (df_u32)(x)); } } while (0)
#define QU(x) do { if (read) { (x) = qget(w); } else { qput(w, (x)); } } while (0)
    QI(q->rewarded); QI(q->ended);
    for (i = 0; i < q->qbn.n_state; i++) { if (!q->qbn.state[i].is_global) { QI(q->qbn.state[i].value); } }
    for (i = 0; i < q->qbn.n_foe; i++) { QI(q->qbn.foe[i].killed); QI(q->foe[i].number); QI(q->foe[i].placed); QI(q->foe[i].place); QI(q->foe[i].live); QI(q->foe[i].next); }
    for (i = 0; i < q->qbn.n_timer; i++) { QI(q->timer[i].flags); QU(q->timer[i].start); QU(q->timer[i].delay); }
    for (i = 0; i < q->qbn.n_item; i++) { QI(q->item[i].gold); QI(q->item[i].amount); QI(q->item[i].given); QU(q->item[i].object); QI(q->item[i].place); QI(q->item[i].laid); QI(q->item[i].carrier); }
    count = 0;
    if (!read) { for (i = 0; i < q->qbn.n_op; i++) { if (q->qbn.op[i].flags || q->qbn.op[i].last_minutes || q->qbn.op[i].opcode == 25 || q->qbn.op[i].opcode == 9) { count++; } } }
    QI(count);
    if (read && (count < 0 || count > q->qbn.n_op)) { w->bad = 1; return; }
    if (read) { for (k = 0; k < count && !w->bad; k++) { i = (int)qget(w); if (i < 0 || i >= q->qbn.n_op) { w->bad = 1; return; } q->qbn.op[i].flags = (int)qget(w); q->qbn.op[i].last_minutes = qget(w); q->qbn.op[i].arg[2].value = (int)qget(w); q->qbn.op[i].arg[4].value = (int)qget(w); } }
    else { for (i = 0; i < q->qbn.n_op; i++) { if (q->qbn.op[i].flags || q->qbn.op[i].last_minutes || q->qbn.op[i].opcode == 25 || q->qbn.op[i].opcode == 9) { v = (df_u32)i; qput(w, v); qput(w, (df_u32)q->qbn.op[i].flags); qput(w, q->qbn.op[i].last_minutes); qput(w, (df_u32)q->qbn.op[i].arg[2].value); qput(w, (df_u32)q->qbn.op[i].arg[4].value); } } }
    for (i = 0; i < q->qbn.n_item; i++) { QI(q->qbn.item[i].flags); }
    for (i = 0; i < q->qbn.n_place; i++) {
        QI(q->qbn.place[i].flags);
        QI(q->place[i].bound); QI(q->place[i].marker);
        if (q->place[i].bound) { QI(q->place[i].ordinal); QI(q->place[i].region); QI(q->place[i].index); QI(q->place[i].type); QI(q->place[i].px); QI(q->place[i].py); qbytes(w, q->place[i].name, 33, read); }
    }
    for (i = 0; i < q->qbn.n_person; i++) {
        QI(q->qbn.person[i].flags);
        QI(q->person[i].bound);
        QI(q->person[i].at);
        if (q->person[i].bound) { QI(q->person[i].who); QI(q->person[i].faction); qbytes(w, q->person[i].name, 40, read); qbytes(w, q->person[i].town, 33, read); }
    }
#undef QI
#undef QU
}

int og_quest_pack(df_u8 *out, int cap)
{
    QWire w;
    int s, i, k, used;

    w.p = out; w.n = 0; w.cap = cap; w.bad = 0;
    qbytes(&w, (const char *)g_global, 64, 0);
    qput(&w, (df_u32)g_foe_counter);
    used = 0;
    for (i = 0; i < OG_LOG_SLOTS; i++) { if (g_log[i].used) { used++; } }
    qput(&w, (df_u32)used);
    for (i = 0; i < OG_LOG_SLOTS; i++) {
        if (!g_log[i].used) { continue; }
        qput(&w, (df_u32)i); qput(&w, (df_u32)g_log[i].quest_id); qbytes(&w, g_log[i].name, 9, 0);
        for (k = 0; k < 10; k++) { qput(&w, (df_u32)g_log[i].message[k]); qput(&w, g_log[i].minutes[k]); qbytes(&w, g_log[i].place[k], 32, 0); }
    }
    used = 0;
    for (s = 0; s < OG_QUESTS; s++) { if (g_quest[s].used) { used++; } }
    qput(&w, (df_u32)used);
    for (s = 0; s < OG_QUESTS; s++) {
        if (!g_quest[s].used) { continue; }
        qput(&w, (df_u32)s); qbytes(&w, g_quest[s].qbn.name, 9, 0); qput(&w, (df_u32)g_quest[s].qbn.id); qput(&w, (df_u32)g_quest[s].qbn.faction);
        qwire_quest(&w, &g_quest[s], 0);
    }
    for (i = 0; i < OG_RUMOURS; i++) {
        qput(&w, (df_u32)g_rumour[i].used);
        if (!g_rumour[i].used) { continue; }
        qput(&w, (df_u32)g_rumour[i].quest_id); qput(&w, (df_u32)g_rumour[i].message); qput(&w, (df_u32)g_rumour[i].flags); qput(&w, (df_u32)g_rumour[i].target); qput(&w, g_rumour[i].expires);
        qbytes(&w, g_rumour[i].name, 9, 0); qbytes(&w, g_rumour[i].text, 200, 0);
    }
    for (i = 0; i < OG_QUEST_ACCESS; i++) { qput(&w, (df_u32)g_access[i].used); qput(&w, (df_u32)g_access[i].quest_id); qput(&w, (df_u32)g_access[i].region); qput(&w, (df_u32)g_access[i].index); qput(&w, (df_u32)g_access[i].access); qput(&w, g_access[i].until); }
    for (i = 0; i < OG_QUEST_FACES; i++) { qput(&w, (df_u32)g_face[i].used); qput(&w, (df_u32)g_face[i].quest_id); qput(&w, (df_u32)g_face[i].kind); qput(&w, (df_u32)g_face[i].index); }
    return w.bad ? -1 : w.n;
}

int og_quest_unpack(const df_u8 *in, int len)
{
    QWire w;
    int i, k, n, s;

    og_quest_reset();
    if (len <= 0) { return len == 0; }
    w.p = (df_u8 *)in; w.n = 0; w.cap = len; w.bad = 0;
    qbytes(&w, (const char *)g_global, 64, 1);
    g_foe_counter = (int)qget(&w);
    n = (int)qget(&w);
    if (n < 0 || n > OG_LOG_SLOTS) { w.bad = 1; }
    for (k = 0; k < n && !w.bad; k++) {
        i = (int)qget(&w);
        if (i < 0 || i >= OG_LOG_SLOTS) { w.bad = 1; break; }
        g_log[i].used = 1; g_log[i].quest_id = (int)qget(&w); qbytes(&w, g_log[i].name, 9, 1);
        for (s = 0; s < 10; s++) { g_log[i].message[s] = (int)qget(&w); g_log[i].minutes[s] = qget(&w); qbytes(&w, g_log[i].place[s], 32, 1); g_log[i].place[s][31] = 0; }
    }
    n = w.bad ? 0 : (int)qget(&w);
    if (n < 0 || n > OG_QUESTS) { w.bad = 1; }
    for (k = 0; k < n && !w.bad; k++) {
        OgQuest *q;
        char name[9];

        s = (int)qget(&w);
        if (s < 0 || s >= OG_QUESTS || g_quest[s].used) { w.bad = 1; break; }
        qbytes(&w, name, 9, 1); name[8] = 0;
        q = &g_quest[s];
        mem_zero(q, sizeof(*q));
        if (w.bad || !og_qbn_load(name, &q->qbn)) { w.bad = 1; break; }
        q->used = 1; q->qbn.id = (int)qget(&w); q->qbn.faction = (int)qget(&w);
        qwire_quest(&w, q, 1);
    }
    for (i = 0; i < OG_RUMOURS && !w.bad; i++) {
        g_rumour[i].used = (int)qget(&w);
        if (!g_rumour[i].used) { continue; }
        g_rumour[i].quest_id = (int)qget(&w); g_rumour[i].message = (int)qget(&w); g_rumour[i].flags = (int)qget(&w); g_rumour[i].target = (int)qget(&w); g_rumour[i].expires = qget(&w);
        qbytes(&w, g_rumour[i].name, 9, 1); qbytes(&w, g_rumour[i].text, 200, 1);
        g_rumour[i].name[8] = 0; g_rumour[i].text[199] = 0;
    }
    for (i = 0; i < OG_QUEST_ACCESS && !w.bad; i++) { g_access[i].used = (int)qget(&w); g_access[i].quest_id = (int)qget(&w); g_access[i].region = (int)qget(&w); g_access[i].index = (int)qget(&w); g_access[i].access = (int)qget(&w); g_access[i].until = qget(&w); }
    for (i = 0; i < OG_QUEST_FACES && !w.bad; i++) { g_face[i].used = (int)qget(&w); g_face[i].quest_id = (int)qget(&w); g_face[i].kind = (int)qget(&w); g_face[i].index = (int)qget(&w); }
    if (w.bad || w.n != len) { og_quest_reset(); return 0; }
    return 1;
}
