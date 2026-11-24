#include "internal.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/tables/itemdefs.h"
#include "engine/core/log.h"
#include "engine/scene/ent.h"
#include "engine/scene/keep.h"

/* Op 75's templates for the quest op 8 starts this run: the group and index of up to four items (-1 none). */
static int g_tpl_group[4] = { -1, -1, -1, -1 }, g_tpl_index[4];
static void lay_items(QuestState *q, int region, int index);

/* ---- Foes in the world (quest_place_object) and the events they raise ---- */

/* A timed spawn can be gated by the state written by "player is in place". */
int quest_repeat_place(const QuestState *q, const QbnOp *repeat)
{
    const QbnArg *gate = &repeat->arg[0];
    int i;

    if (gate->literal || gate->negate || gate->value < 0) { return -1; }
    for (i = 0; i < q->qbn.n_op; i++) {
        const QbnOp *op = &q->qbn.op[i];
        const QbnArg *state = &op->arg[1], *place = &op->arg[2];

        if (op->opcode == QOP_IN_PLACE && !state->literal && !state->negate &&
            state->section == gate->section && state->value == gate->value &&
            !place->literal && place->value >= 0 && place->value < q->qbn.n_place) { return place->value; }
    }
    return -1;
}

static int site_target(const QuestState *q, int target, int region, int index, int block, int record)
{
    const QuestPlace *p;

    if (target < 0 || target >= q->qbn.n_place) { return 0; }
    p = &q->place[target];
    return p->bound && p->region == region && p->index == index && p->block == block && p->record == record;
}

int quest_foe_at_site(const QuestState *q, int foe, int region, int index, int block, int record)
{
    int i;

    if (q->foe[foe].placed && q->foe[foe].made < q->qbn.foe[foe].count &&
        site_target(q, q->foe[foe].place, region, index, block, record)) { return 1; }
    for (i = 0; i < q->qbn.n_op; i++) {
        const QbnOp *op = &q->qbn.op[i];

        if (op->arg[1].literal || op->arg[1].value != foe) { continue; }
        if ((op->opcode == QOP_PLACE_FOE || op->opcode == QOP_PLACE_FOE_2) && !(op->flags & 1) &&
            !op->arg[2].literal && site_target(q, op->arg[2].value, region, index, block, record)) { return 1; }
        if (op->opcode == QOP_SPAWN_REPEAT && op->arg[4].value &&
            site_target(q, quest_repeat_place(q, op), region, index, block, record)) { return 1; }
    }
    return 0;
}

/* A quest marker of a place of the quest, in the dungeon the player is in: the place's own marker (a fixed place names its 0xFA0n), else one of the record picked by a seed of the quest and place. */
int quest_marker_point(const QuestState *q, int place, int record, int extra, float *x, float *y, float *z)
{
    int marker;

    if (!quest_world || !quest_world->marker_at || place < 0 || place >= q->qbn.n_place || !q->place[place].bound) { return 0; }
    marker = q->place[place].marker >= 0xFA01 ? q->place[place].marker - 0xFA01 : -1;
    if (q->place[place].block >= 0 && q->qbn.place[place].p3 >= 0) {
        record = q->qbn.place[place].p3 == 1 ? 18 : 11;
    }
    return quest_world->marker_at(marker, record, (unsigned)(q->qbn.id * 977 + place * 31 + extra * 7), x, y, z);
}

/* Whether the player is where the foe was placed: in its place's location, or in any dungeon when it has none. */
static int g_loaded_region = -1, g_loaded_index = -1;   /* the location that is being loaded, before the player's place says so */

static int matches_place(const QuestPlace *place, int region, int index)
{
    return place->bound && place->region == region && place->index == index &&
        (place->block < 0 || !quest_world->inside ||
         quest_world->inside(region, index, place->block, place->record));
}

static int foe_here(const QuestState *q, int f)
{
    int region, here, px, py, where;

    if (!quest_world || !quest_world->here) { return 0; }
    quest_world->here(&region, &here, &px, &py);
    if (g_loaded_region >= 0) { region = g_loaded_region; here = g_loaded_index; }
    where = q->foe[f].place;
    if (where < 0) { return here >= 0; }
    return where < q->qbn.n_place && matches_place(&q->place[where], region, here);
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

static const QuestFoe *quest_foe_find(int number)
{
    int slot, foe;

    for (slot = 0; slot < QUESTS; slot++) {
        if (!quest_runtime.slots[slot].used) { continue; }
        for (foe = 0; foe < quest_runtime.slots[slot].qbn.n_foe; foe++) {
            if (quest_runtime.slots[slot].foe[foe].number == number) { return &quest_runtime.slots[slot].foe[foe]; }
        }
    }
    return 0;
}

int quest_foe_pacified(int number)
{
    const QuestFoe *foe = quest_foe_find(number);

    return foe && foe->pacified;
}

static void forget_foe_row(KeepKey key)
{
    df_u32 loot = loot_container_find(key.place, key.thing);

    if (loot) { object_purge(loot); }
    keep_forget(key);
}

void quest_forget_foe(const QuestFoe *foe)
{
    df_u32 thing = QUEST_FOE_THING(foe->number, 0);
    int i;

    for (i = dungeon_count() - 1; i >= 0; i--) {
        Entity *entity = dungeon_entity(i);

        if ((entity->flags & ENT_KEPT) && (entity->key.thing & ~15u) == thing) {
            EntId id = ent_id(entity);

            forget_foe_row(entity->key);
            dungeon_release_actor(i);
            ent_remove(id);
        }
    }
    for (i = keep_count() - 1; i >= 0; i--) {
        const Keep *row = keep_at(i);

        if (row->kind == KEEP_ENTITY && row->made && (row->key.thing & ~15u) == thing) { forget_foe_row(row->key); }
    }
}

void quest_prune_foes(void)
{
    int i;

    for (i = keep_count() - 1; i >= 0; i--) {
        const Keep *row = keep_at(i);

        if (row->kind == KEEP_ENTITY && row->made && QUEST_FOE_IS(row->key.thing) &&
            !quest_foe_find(QUEST_FOE_NUMBER(row->key.thing))) { forget_foe_row(row->key); }
    }
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

/* Op 69 (qaction_op69_cast_spell_on_foe): the spell, by its id, cast by the player on each live copy of the foe, with the resistance rolls of any other cast. The player's level is the caster's. */
void quest_cast_on_foe(const QuestFoe *foe, int spell)
{
    int i;
    df_u32 key;

    key = QUEST_FOE_THING(foe->number,0);
    for (i = 0; i < dungeon_count(); i++) {
        if ((dungeon_entity(i)->key.thing & ~15u) == key && dungeon_entity(i)->live && !dungeon_entity(i)->dead) { magic_apply(spell, QUEST_CASTER_PLAYER, i, character_state()->level, 0); }
    }
}

/* Op 87 (qaction_op87_respawn): the foe's earlier copies go before it is placed again, a body or a living one (the original deletes the twin). */
void quest_remove_foe(const QuestFoe *foe)
{
    int i;
    df_u32 key;

    key = QUEST_FOE_THING(foe->number,0);
    for (i = 0; i < dungeon_count(); i++) {
        if ((dungeon_entity(i)->key.thing & ~15u) == key && dungeon_entity(i)->live) { dungeon_remove(i); }
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
    return matches_place(&q->place[n], region, here);
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
        if (q->item[i].object && q->item[i].place >= 0 && !q->item[i].laid &&
            matches_place(&q->place[q->item[i].place], region, index)) {
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
