#include "internal.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/tables/itemdefs.h"
#include "engine/core/log.h"

/* Op 75's templates for the quest op 8 starts this run: the group and index of up to four items (-1 none). */
static int g_tpl_group[4] = { -1, -1, -1, -1 }, g_tpl_index[4];
static void lay_items(QuestState *q, int region, int index);

/* ---- Foes in the world (quest_place_object) and the events they raise ---- */

/* A quest marker of a place of the quest, in the dungeon the player is in: the place's own marker (a fixed place names its 0xFA0n), else one of the record picked by a seed of the quest and place. */
int quest_marker_point(const QuestState *q, int place, int record, int extra, float *x, float *y, float *z)
{
    int marker;

    if (!quest_world || !quest_world->marker_at || place < 0 || place >= q->qbn.n_place || !q->place[place].bound) { return 0; }
    marker = q->place[place].marker >= 0xFA01 ? q->place[place].marker - 0xFA01 : -1;
    return quest_world->marker_at(marker, record, (unsigned)(q->qbn.id * 977 + place * 31 + extra * 7), x, y, z);
}

/* Whether the player is where the foe was placed: in its place's location, or in any dungeon when it has none. */
static int g_loaded_region = -1, g_loaded_index = -1;   /* the location that is being loaded, before the player's place says so */

static int foe_here(const QuestState *q, int f)
{
    int region, here, px, py, where;

    if (!quest_world || !quest_world->here) { return 0; }
    quest_world->here(&region, &here, &px, &py);
    if (g_loaded_region >= 0) { region = g_loaded_region; here = g_loaded_index; }
    where = q->foe[f].place;
    if (where < 0) { return here >= 0; }
    return where < q->qbn.n_place && q->place[where].bound && q->place[where].region == region && q->place[where].index == here;
}

/* The foe's creatures not yet in the world are made, on the foe's marker when its place has one (the hold's, the dungeon's) else near the player. A floor not loaded yet leaves the rest for the next try. */
/* Make the living copies of a placed foe, each with its own key. */
void quest_spawn_foe(QuestState *q, int f)
{
    float x, y, z;
    int on_marker, made;

    if (!quest_world || !q->foe[f].placed || q->foe[f].made >= q->qbn.foe[f].count || !foe_here(q, f)) { return; }
    on_marker = quest_marker_point(q, q->foe[f].place, 11, f, &x, &y, &z) && quest_world->spawn_foe_at;
    for (made = q->foe[f].made; made < q->qbn.foe[f].count && made < 16; made++) {
        df_u32 thing;

        thing = QUEST_FOE_THING(q->foe[f].number, made);
        if (on_marker ? !quest_world->spawn_foe_at(q->qbn.foe[f].type, thing, x, y, z) : (!quest_world->spawn_foe || !quest_world->spawn_foe(q->qbn.foe[f].type, thing))) { break; }
        q->foe[f].live++;
    }
    q->foe[f].made = made;
}

void quest_location_loaded(int region, int index)
{
    int s, f;

    g_loaded_region = region; g_loaded_index = index;
    for (s = 0; s < QUESTS; s++) {
        if (!quest_runtime.slots[s].used) { continue; }
        lay_items(&quest_runtime.slots[s], region, index);
        for (f = 0; f < quest_runtime.slots[s].qbn.n_foe; f++) {
            QuestFoe *foe;
            const QuestPlace *place;

            foe = &quest_runtime.slots[s].foe[f];
            if (!foe->placed || foe->made >= quest_runtime.slots[s].qbn.foe[f].count || foe->place < 0 || foe->place >= quest_runtime.slots[s].qbn.n_place) { continue; }
            place = &quest_runtime.slots[s].place[foe->place];
            if (place->bound && place->region == region && place->index == index) { quest_spawn_foe(&quest_runtime.slots[s], f); }
        }
    }
    g_loaded_region = -1; g_loaded_index = -1;
}

int quest_foe_pacified(int number)
{
    int slot, foe;

    for (slot = 0; slot < QUESTS; slot++) {
        if (!quest_runtime.slots[slot].used) { continue; }
        for (foe = 0; foe < quest_runtime.slots[slot].qbn.n_foe; foe++) {
            if (quest_runtime.slots[slot].foe[foe].number == number) { return quest_runtime.slots[slot].foe[foe].pacified; }
        }
    }
    return 0;
}

void quest_set_foe_pacified(QuestFoe *foe, int pacified)
{
    int i;
    df_u32 key;

    foe->pacified = pacified != 0;
    key = QUEST_FOE_THING(foe->number,0);
    for (i = 0; i < dungeon_count(); i++) {
        if ((dungeon_entity(i)->key.thing & ~15u) == key) { dungeon_set_pacified(i,pacified); }
    }
}

/* ---- Access to quest buildings ---- */

int quest_building_open(int region, int index, int block, int record)
{
    int s, i;

    for (s = 0; s < QUESTS; s++) {
        const QuestState *q;

        q = &quest_runtime.slots[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_place; i++) {
            if ((q->qbn.place[i].flags & 64) && q->place[i].bound && q->place[i].region == region && q->place[i].index == index && q->place[i].block == block && q->place[i].record == record) { return 1; }
        }
        for (i = 0; i < q->qbn.n_person; i++) {
            if ((q->qbn.person[i].flags & 0x4000) && q->person[i].bound && q->person[i].homed && q->person[i].home_region == region && q->person[i].home_index == index &&
                q->person[i].home_block == block && q->person[i].home_record == record) { return 1; }
        }
    }
    return 0;
}

/* The last word of a name. */
static void last_word(const char *name, char *out, int cap)
{
    const char *w;
    int n;

    w = name;
    for (n = 0; name[n]; n++) { if (name[n] == ' ' && name[n + 1]) { w = name + n + 1; } }
    log_format(out, cap, "%s", w);
}

int quest_building_site(int region, int index, int block, int record, char *surname, int cap)
{
    int s, i;

    if (surname) { surname[0] = 0; }
    for (s = 0; s < QUESTS; s++) {
        const QuestState *q;

        q = &quest_runtime.slots[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_person; i++) {
            if (q->person[i].bound && q->person[i].homed && q->person[i].home_region == region && q->person[i].home_index == index && q->person[i].home_block == block &&
                q->person[i].home_record == record && !(q->qbn.person[i].flags & 0x8000)) {
                if (surname) { last_word(q->person[i].name, surname, cap); }
                return 1;
            }
        }
        for (i = 0; i < q->qbn.n_place; i++) {
            if (q->place[i].bound && q->place[i].region == region && q->place[i].index == index && q->place[i].block == block && q->place[i].record == record && !(q->qbn.place[i].flags & 128)) {
                if (surname && quest_world && quest_world->person_name) {
                    char made[40];

                    quest_world->person_name(0x50000000 + block * 251 + record, 0, 0, made, (int)sizeof(made));
                    last_word(made, surname, cap);
                }
                return 1;
            }
        }
    }
    return 0;
}

int quest_building_access(int region, int index, int block, int record, df_u32 now)
{
    int i, best;

    best = 0;
    for (i = 0; i < QUEST_ACCESS; i++) {
        if (quest_runtime.access[i].used && quest_runtime.access[i].region == region && quest_runtime.access[i].index == index && quest_runtime.access[i].block == block && quest_runtime.access[i].record == record && quest_runtime.access[i].until > now && quest_runtime.access[i].access > best) { best = quest_runtime.access[i].access; }
    }
    return best;
}

const QuestAccess *quest_access(int i) { return i >= 0 && i < QUEST_ACCESS && quest_runtime.access[i].used ? &quest_runtime.access[i] : 0; }

/* The quest tree's record: what a quest's items hang from until they are given. */
static df_u32 quest_root(void)
{
    int i;

    for (i = 0; i < OBJECTS; i++) { if (game_state()->object[i].id && game_state()->object[i].type == RECORD_QUESTS) { return game_state()->object[i].id; } }
    return 0;
}

/* quest_init_item (2.5): a group below 0 is drawn among the groups that have items, an index below 0 among the group's; a template from the parent quest (group 100) is not made. A letter
 * (group 9, index 5) carries its QRC message. The item hangs from the quest tree. */
void quest_make_item(QuestState *q, int i)
{
    const QbnItem *it;
    int group, index, count;

    it = &q->qbn.item[i];
    group = it->group; index = it->index;
    if (group == 100 && index >= 0 && index < 4 && g_tpl_group[index] >= 0) { group = g_tpl_group[index]; index = g_tpl_index[index]; }
    if (group == 100 || group > 27 || !quest_root()) { return; }
    if (group < 0) {
        int tries;

        for (tries = 0; tries < 200; tries++) { group = random_range(0, 27); if (item_def(group, 0)) { break; } }
    }
    if (group != 4 && group != 5 && !item_def(group, 0)) { return; }
    if (index < 0 && group != 4 && group != 5) {
        for (count = 0; item_def(group, count); count++) { }
        index = random_range(0, count - 1);
    }
    q->item[i].object = item_create(quest_root(), group, index, -1);
    if (q->item[i].object && group == 9 && index == 5 && it->message[1] > 0) { object_edit(q->item[i].object)->message = it->message[1]; }
}

/* Whether the player has the object: up its parents to the player's pack (the quest tree hangs from the same entity, and is not the pack). */
int quest_item_carried(df_u32 id)
{
    int guard;

    for (guard = 0; id && guard < 16; guard++) {
        const ObjectInfo *o;

        if (id == game_state()->inventory_id) { return 1; }
        o = object_at(id);
        if (!o) { return 0; }
        id = o->parent;
    }
    return 0;
}

/* An item the quest forgets or deletes (ops 54 and 36), and every item it still holds when it ends (the quest's id is on them). */
void quest_item_forget(QuestState *q, int n) { if (n >= 0 && n < q->qbn.n_item) { q->item[n].object = 0; q->item[n].carrier = 0; } }

int quest_in_place(const QuestState *q, int n)
{
    int region, here, px, py;

    if (n < 0 || n >= q->qbn.n_place || !q->place[n].bound || !quest_world || !quest_world->here) { return 0; }
    quest_world->here(&region, &here, &px, &py);
    return q->place[n].region == region && q->place[n].index == here;
}

/* An item set down in the world at the player's feet: the pile the game makes, the item moved into it. */
void quest_put_item_down(QuestState *q, int i)
{
    df_u32 pile;

    float x, y, z;

    if (!q->item[i].object || !quest_world || !quest_world->place_item) { return; }
    /* On the item marker (199.18) of its place when the dungeon has one, else at the player's feet. */
    if (q->item[i].place >= 0 && quest_world->place_item_at && quest_marker_point(q, q->item[i].place, 18, 100 + i, &x, &y, &z)) { pile = quest_world->place_item_at(x, y, z); }
    else { pile = quest_world->place_item(); }
    if (pile) { object_reparent(q->item[i].object, pile); }
}

/* Items of op 0 whose place is the location that was just loaded are laid in it (quest_place_object when the place is the loaded one). */
static void lay_items(QuestState *q, int region, int index)
{
    int i;

    for (i = 0; i < q->qbn.n_item; i++) {
        if (q->item[i].object && q->item[i].place >= 0 && !q->item[i].laid && q->place[q->item[i].place].bound &&
            q->place[q->item[i].place].region == region && q->place[q->item[i].place].index == index) {
            quest_put_item_down(q, i); q->item[i].laid = 1;
        }
    }
}

/* The foe that carried an item (op 39) has died: the item lies where the player is. */
void quest_foe_dropped(QuestState *q, int f)
{
    int i;

    for (i = 0; i < q->qbn.n_item; i++) { if (q->item[i].object && q->item[i].carrier == f + 1) { quest_put_item_down(q, i); q->item[i].carrier = 0; } }
}


void quest_item_template_set(int slot, int group, int index)
{
    g_tpl_group[slot] = group;
    g_tpl_index[slot] = index;
}

void quest_item_templates_clear(void)
{
    int i;

    for (i = 0; i < 4; i++) { g_tpl_group[i] = -1; }
}
