#include "internal.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/play/people/law.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

static struct { int set, who, faction; char name[40], town[33]; } g_questor;

/* ---- Place selection and binding ---- */

/* The kinds of location: towns 0..2, the dungeons 4, 7, 10, 12 and 13. */
static int is_town(int type) { return type >= 0 && type <= 2; }
int quest_location_is_dungeon(int type) { return type == 4 || type == 7 || type == 10 || type == 12 || type == 13; }

static int location_used(int slot, int place, int region, int index)
{
    int k, i;

    for (k = 0; k < QUESTS; k++) {
        if (!quest_runtime.slots[k].used) { continue; }
        for (i = 0; i < QBN_PLACES; i++) { if (!(k == slot && i == place) && quest_runtime.slots[k].place[i].bound && quest_runtime.slots[k].place[i].region == region && quest_runtime.slots[k].place[i].index == index) { return 1; } }
    }
    return 0;
}

static void place_from(QuestPlace *place, int ordinal, const LocationInfo *lo)
{
    place->block = -1; place->record = -1;
    place->bound = 1; place->ordinal = ordinal; place->region = lo->region; place->index = lo->index; place->type = lo->type;
    place->px = lo->px; place->py = lo->py;
    log_format(place->name, sizeof(place->name), "%s", lo->name);
}

/* quest_init_place for a place that is a location: a fixed one (scope 0) waits for the steps that read the world's records; scope 1 or below is where the player is; scope 2 and up a location
 * of the kind (p1: 0 a town, 1 a dungeon) in the player's region, other than the one they are in. A location another quest uses is drawn again, up to 100 times. */
static int bind_place(int slot, int i)
{
    QuestState *q;
    const QbnPlace *pl;
    LocationInfo lo;
    int region, here, px, py, count, n, tries, pick, k, ordinal;

    q = &quest_runtime.slots[slot]; pl = &q->qbn.place[i];
    if (!quest_world || !quest_world->count || !quest_world->location || !quest_world->here) { return 1; }
    if (pl->scope == 0) {
        /* A fixed place: (p1 << 16) | p2, the location's number and the low word of an object in it. Low word 2 names no object in the original (7.1: its tutorial step could never run); it is read as 1,
         * the location itself, as Daggerfall Unity does. The place has the location's bounds and no point of its own. */
        if (!quest_world->find_number || !quest_world->find_number(pl->p1, &region, &here)) { return 1; }
        for (k = 0; k < quest_world->count(); k++) {
            if (quest_world->location(k, &lo) && lo.region == region && lo.index == here) { place_from(&q->place[i], k, &lo); q->place[i].marker = (pl->p2 & 0xffff) == 2 ? 1 : (pl->p2 & 0xffff); return 1; }
        }
        return 1;
    }
    quest_world->here(&region, &here, &px, &py);
    count = quest_world->count();
    if (pl->scope <= 1) {
        for (k = 0; k < count; k++) {
            if (quest_world->location(k, &lo) && lo.region == region && lo.index == here) { place_from(&q->place[i], k, &lo); return 1; }
        }
        return 0;
    }
    for (tries = 0; tries < 100; tries++) {
        n = 0;
        for (k = 0; k < count; k++) {
            if (!quest_world->location(k, &lo) || lo.region != region || lo.index == here) { continue; }
            if (pl->p1 == 1 ? !quest_location_is_dungeon(lo.type) : pl->p1 == 0 ? !is_town(lo.type) : 1) { continue; }
            n++;
        }
        if (!n) { return 0; }
        pick = random_range(0, n - 1); ordinal = -1; n = 0;
        for (k = 0; k < count && ordinal < 0; k++) {
            if (!quest_world->location(k, &lo) || lo.region != region || lo.index == here) { continue; }
            if (pl->p1 == 1 ? !quest_location_is_dungeon(lo.type) : pl->p1 == 0 ? !is_town(lo.type) : 1) { continue; }
            if (n++ == pick) { ordinal = k; }
        }
        if (ordinal < 0 || !quest_world->location(ordinal, &lo)) { return 0; }
        if (location_used(slot, i, lo.region, lo.index)) { continue; }
        place_from(&q->place[i], ordinal, &lo);
        /* A building place: which building of the town, by the seed of this quest and place. */
        q->place[i].block = -1; q->place[i].record = -1;
        if (pl->p1 == 0 && quest_world->pick_building) {
            quest_world->pick_building(lo.region, lo.index, pl->p2, (unsigned)(q->qbn.id * 977 + i * 31 + random_range(0, 32767)), &q->place[i].block, &q->place[i].record);
        }
        return 1;
    }
    return 0;
}

/* quest_init_resources, the places and the timers of this step: a place that cannot be bound ends the quest (TEXT.RSC 600). */
/* quest_init_item's gold (quests-math.md): index and group are the least and most; index -1 scales by level (the guild's rank + 1 when the player is of the faction, else half the level + 1,
 * at most 10), the faction's power and the region's prices. At least 1. */
static int roll_gold(const QuestState *q, const QbnItem *it)
{
    int gold, level, power, adj, region, here, px, py;

    if (it->index != -1) { gold = random_range(it->index, it->group); }
    else {
        Membership *m;

        m = q->qbn.faction ? guild_find_faction(q->qbn.faction) : 0;
        level = m ? m->rank + 1 : character_state()->level / 2 + 1;
        if (level > 10) { level = 10; }
        power = q->qbn.faction ? faction_power(q->qbn.faction) : 50;
        region = 0;
        if (quest_world && quest_world->here) { quest_world->here(&region, &here, &px, &py); }
        adj = region_price(region) / 2 + 500;
        gold = ((power + 50) * ((adj * random_range(level * 150, level * 200)) / 1000)) / 100;
    }
    return gold < 1 ? 1 : gold;
}

/* A town of the player's region, other than the one they are in, picked by the seed: its region and index; 0 when the region has none. */
static int random_town(unsigned seed, int *region, int *index)
{
    LocationInfo lo;
    int r, here, px, py, k, n, pick;

    if (!quest_world || !quest_world->count || !quest_world->location || !quest_world->here) { return 0; }
    quest_world->here(&r, &here, &px, &py);
    n = 0;
    for (k = 0; k < quest_world->count(); k++) { if (quest_world->location(k, &lo) && lo.region == r && lo.index != here && is_town(lo.type)) { n++; } }
    if (!n) { return 0; }
    pick = (int)(seed % (unsigned)n);
    for (k = 0; k < quest_world->count(); k++) {
        if (quest_world->location(k, &lo) && lo.region == r && lo.index != here && is_town(lo.type) && pick-- == 0) { *region = lo.region; *index = lo.index; return 1; }
    }
    return 0;
}

/* The town the person is made in (3.1: low byte 0 here, 255 a toss between here and elsewhere, else elsewhere): here when it is a town. */
static int person_town(const QuestState *q, int i, unsigned seed, int *region, int *index)
{
    LocationInfo lo;
    int here_region, here, px, py, k, want_here;
    int low;

    low = q->qbn.person[i].flags & 0xff;
    want_here = low == 0 || (low == 255 && (seed & 1u));
    if (!quest_world || !quest_world->here) { return 0; }
    if (want_here) {
        quest_world->here(&here_region, &here, &px, &py);
        for (k = 0; quest_world->count && quest_world->location && k < quest_world->count(); k++) {
            if (quest_world->location(k, &lo) && lo.region == here_region && lo.index == here && is_town(lo.type)) { *region = here_region; *index = here; return 1; }
        }
    }
    return random_town(seed / 2u, region, index);
}

/* A faction of the type, picked by the seed (type 10: one of the knightly orders); 0 for none. */
static int random_faction_of_type(int type, unsigned seed)
{
    static const short orders[10] = { 368, 408, 409, 410, 411, 413, 414, 415, 416, 417 };
    int k, n, pick;

    if (type == 10) { return orders[seed % 10u]; }
    n = 0;
    for (k = 0; k < faction_total(); k++) { if (faction_def(k) && faction_def(k)->type == type) { n++; } }
    if (!n) { return 0; }
    pick = (int)(seed % (unsigned)n);
    for (k = 0; k < faction_total(); k++) { if (faction_def(k) && faction_def(k)->type == type && pick-- == 0) { return faction_def(k)->id; } }
    return 0;
}

/* quest_init_person (2.3) for the kinds that stand in a building: the site (a building of a town of the region: the original picks among a location's quest sites, markers and people; we pick among
 * its buildings and the engine stands the person on a marker or beside the first person), the faction, the sex and the name. Persons linked to another (-3, -4, -5, -7) wait for it, up to 21 passes.
 * A person whose site cannot be found stands nowhere (the original fails the whole quest: 7.6). */
static int bind_persons(int slot)
{
    QuestState *q;
    int pass, i, pending, progress;
    unsigned char done_flag[QBN_PERSONS];

    q = &quest_runtime.slots[slot];
    for (i = 0; i < QBN_PERSONS; i++) { done_flag[i] = 1; }
    for (i = 0; i < q->qbn.n_person; i++) { done_flag[i] = (q->qbn.person[i].kind == 21 || q->qbn.person[i].kind == -8) ? 1 : 0; }
    for (pass = 0; pass < 21; pass++) {
        pending = 0; progress = 0;
        for (i = 0; i < q->qbn.n_person; i++) {
            const QbnPerson *pe;
            QuestPerson *p;
            int kind, faction, mode, value, link, tries, region = 0, index = 0, block = 0, record = 0, bfaction, found;
            unsigned seed;

            if (done_flag[i]) { continue; }
            pe = &q->qbn.person[i]; p = &q->person[i];
            kind = pe->kind; faction = pe->faction;
            seed = (unsigned)(q->qbn.id * 977 + i * 131) + (unsigned)random_range(0, 32767);
            p->female = (pe->flags & 0x200) ? 1 : (pe->flags & 0x400) ? 0 : (int)(seed >> 3 & 1u);
            link = -1;
            if (kind == -7 || kind == -5 || kind == -4 || kind == -3) {
                if (kind == -4 && faction == 10000) { kind = -6; }
                else {
                    link = faction;
                    if (link < 0 || link >= q->qbn.n_person || link == i) { kind = -6; link = -1; }
                    else if (!done_flag[link]) { pending++; continue; }
                }
            }
            if (link >= 0) {
                const QuestPerson *other = &q->person[link];
                const FactionDef *fd = other->faction ? faction_def(faction_index(other->faction)) : 0;

                if (kind == -7) { p->female = other->female; kind = -6; }
                else if (kind == -5) { faction = other->faction; kind = -1; }
                else {
                    int k, n, pick, list[3];

                    n = 0;
                    for (k = 0; fd && k < FACTION_LINKS; k++) { int id = kind == -4 ? fd->enemy[k] : fd->ally[k]; if (id) { list[n++] = id; } }
                    faction = n ? list[(int)(seed % (unsigned)n)] : 510;
                    pick = 0; (void)pick;
                    kind = -1;
                }
            }
            if (kind == -2) { faction = random_faction_of_type(faction, seed); kind = -1; }
            /* The person's faction and their kind of site. */
            mode = -1; value = 0; bfaction = faction;
            if (kind == -1) {
                const FactionDef *fd = faction ? faction_def(faction_index(faction)) : 0;

                /* A named individual (a faction of type 4, no 0x100) is the person already standing in a town of their own: found by their faction as the town loads. */
                if (!(pe->flags & 0x100) && fd && fd->type == 4) { p->faction = faction; p->bound = 1; done_flag[i] = 1; progress++; }
                else if (pe->flags & 0x100) { mode = 1; }   /* any building: a person asked for anywhere takes any site (kind -1 accepts any marker) */
                else { mode = 2; value = faction; }
            } else if (kind == -6) { mode = 1; }
            else if (kind >= 0 && kind != 40) { mode = 0; value = kind; }
            if (!done_flag[i]) {
                found = 0;
                if (mode >= 0 && quest_world && quest_world->pick_site) {
                    for (tries = 0; tries < 50 && !found; tries++) {
                        if (!person_town(q, i, seed + (unsigned)tries * 7919u, &region, &index)) { break; }
                        found = quest_world->pick_site(region, index, mode, value, seed + (unsigned)tries, &block, &record, &bfaction);
                        if (!found && mode != 1 && tries == 49) { mode = 1; tries = 0; }
                    }
                }
                /* A person who should stand in a building and has no site anywhere fails the quest (the original does, by accident of its code, 7.6: its fallback to any building never ran; ours runs it above): the offer is not made and nothing is started. */
                if (mode >= 0 && quest_world && quest_world->pick_site && !found) { return 0; }
                p->bound = 1;
                p->faction = kind == -1 ? faction : (found && bfaction ? bfaction : faction);
                if (found) { p->homed = 1; p->home_region = region; p->home_index = index; p->home_block = block; p->home_record = record; p->takeover = (kind == -6 || kind >= 0) && !(pe->flags & 0x100) ? 1 : 0; }
                done_flag[i] = 1; progress++;
            }
            p->who = (int)(0x40000000u | (unsigned)(slot << 8) | (unsigned)i);
            if (quest_world && quest_world->person_name) { quest_world->person_name(p->who, p->faction, p->female, p->name, (int)sizeof(p->name)); }
        }
        if (!pending) { break; }
        if (!progress) {
            /* A link that never resolves: those persons stand nowhere. */
            for (i = 0; i < q->qbn.n_person; i++) {
                if (!done_flag[i]) { q->person[i].bound = 1; q->person[i].who = (int)(0x40000000u | (unsigned)(slot << 8) | (unsigned)i); done_flag[i] = 1; }
            }
            break;
        }
    }
    return 1;
}

int quest_bind_resources(int slot)
{
    QuestState *q;
    int i;

    q = &quest_runtime.slots[slot];
    for (i = 0; i < q->qbn.n_timer; i++) { q->timer[i].flags = q->qbn.timer[i].flags & ~(128 | 1024); }
    for (i = 0; i < q->qbn.n_place; i++) {
        if (!bind_place(slot, i)) { log_print(LOG_WARN, "quest: %s cannot bind place %d (scope %d)", q->qbn.name, i, q->qbn.place[i].scope); return 0; }
    }
    /* quest_init_person (2.3), the kinds of this step: the questor, and a person that is only a faction. */
    for (i = 0; i < q->qbn.n_person; i++) {
        const QbnPerson *pe;

        pe = &q->qbn.person[i];
        q->qbn.person[i].flags &= 0x0fff;
        if (pe->kind == 21) {
            if (!g_questor.set || quest_person_busy(g_questor.who)) {
                log_print(LOG_WARN, "quest: %s has no questor for person %d (%s)", q->qbn.name, i, g_questor.set ? "busy" : "none set");
                return 0;
            }
            q->person[i].bound = 1; q->person[i].who = g_questor.who; q->person[i].faction = g_questor.faction;
            log_format(q->person[i].name, sizeof(q->person[i].name), "%s", g_questor.name);
            log_format(q->person[i].town, sizeof(q->person[i].town), "%s", g_questor.town);
        } else if (pe->kind == -8) {
            q->person[i].bound = 1; q->person[i].who = 0; q->person[i].faction = pe->faction;
        }
    }
    if (!bind_persons(slot)) { log_print(LOG_WARN, "quest: %s cannot bind its persons", q->qbn.name); return 0; }
    for (i = 0; i < q->qbn.n_op; i++) { q->qbn.op[i].last_minutes = game_state()->minutes; }
    for (i = 0; i < q->qbn.n_item; i++) {
        if (q->qbn.item[i].flags & 2) { q->item[i].gold = 1; q->item[i].amount = roll_gold(q, &q->qbn.item[i]); }
        else { quest_make_item(q, i); }
        q->item[i].place = -1;
    }
    /* quest_init_foe: each foe takes the next number of the counter. */
    for (i = 0; i < q->qbn.n_foe; i++) { q->foe[i].number = quest_foe_counter++; q->foe[i].place = -1; }
    for (i = 0; i < q->qbn.n_timer; i++) { quest_timer_start(q, i); }
    return 1;
}

/* ---- Persons and talk ---- */

void quest_set_questor(int who, int faction, const char *name, const char *town)
{
    g_questor.set = 1; g_questor.who = who; g_questor.faction = faction;
    log_format(g_questor.name, sizeof(g_questor.name), "%s", name ? name : "");
    log_format(g_questor.town, sizeof(g_questor.town), "%s", town ? town : "");
}

int quest_person_busy(int who)
{
    int s, i;

    for (s = 0; s < QUESTS; s++) {
        if (!quest_runtime.slots[s].used) { continue; }
        for (i = 0; i < quest_runtime.slots[s].qbn.n_person; i++) { if (quest_runtime.slots[s].person[i].bound && quest_runtime.slots[s].qbn.person[i].kind == 21 && quest_runtime.slots[s].person[i].who == who) { return 1; } }
    }
    return 0;
}

int quest_people_in(int region, int index, int block, int record, QuestHere *out, int cap)
{
    int s, i, n;

    n = 0;
    for (s = 0; s < QUESTS; s++) {
        const QuestState *q;

        q = &quest_runtime.slots[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_person && n < cap; i++) {
            const QuestPerson *p;

            p = &q->person[i];
            if (!p->bound || !p->homed || q->qbn.person[i].kind == -8 || q->qbn.person[i].kind == 21 || (q->qbn.person[i].flags & 0xA00) == 0xA00) { continue; }
            if (p->home_region != region || p->home_index != index || p->home_block != block || p->home_record != record || (block < 0 && block != -2)) { continue; }
            out[n].has_point = 0;
            if (block == -2 && p->at > 0) { out[n].has_point = quest_marker_point(q, p->at - 1, 11, 200 + i, &out[n].x, &out[n].y, &out[n].z); }
            out[n].slot = s; out[n].person = i; out[n].female = p->female; out[n].faction = p->faction; out[n].takeover = p->takeover; out[n].seed = (unsigned)(q->qbn.id * 977 + i * 131);
            n++;
        }
    }
    return n;
}

int quest_person_of_tag(int tag, int *who, int *faction, int *female, char *name, int cap)
{
    int s, i;

    if (tag < QUEST_PERSON_MARK) { return 0; }
    s = (tag - QUEST_PERSON_MARK) / 16; i = (tag - QUEST_PERSON_MARK) % 16;
    if (s < 0 || s >= QUESTS || !quest_runtime.slots[s].used || i >= quest_runtime.slots[s].qbn.n_person || !quest_runtime.slots[s].person[i].bound) { return 0; }
    *who = quest_runtime.slots[s].person[i].who; *faction = quest_runtime.slots[s].person[i].faction; *female = quest_runtime.slots[s].person[i].female;
    log_format(name, cap, "%s", quest_runtime.slots[s].person[i].name);
    return 1;
}


void quest_reset_questor(void)
{
    mem_zero(&g_questor, sizeof(g_questor));
}
