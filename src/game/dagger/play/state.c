#include "game/dagger/play/state.h"
#include "engine/core/mem.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/play/enchant.h"
#include "game/dagger/play/quest.h"
#include "game/dagger/tables/combatdefs.h"
#include "engine/scene/clock.h"

static GameState g_state;
WorldState *world_where(void) { return &g_state.world_state; }
static void (*g_observer)(const QuestEvent *event);
static const char *g_error = "";
MagicState *magic_state(void) { return &g_state.magic; }

void state_stealth_checked(void) { g_state.stealth_minute = g_state.minutes; }
ProgressState *progress_state(void) { return &g_state.progress; }
Survival *survival_state(void) { return &g_state.survival; }
EncounterState *encounter_state(void) { return &g_state.encounters; }
void state_clear_skill_uses(int skill)
{
    if (skill >= 0 && skill < SKILLS) { g_state.skill_uses[skill] = 0; }
}
void state_base_attribute(int attribute, int value)
{
    if (attribute >= 0 && attribute < ATTRS && value >= 0 && value <= 100) { g_state.base_attribute[attribute] = value; }
}

CharacterState *character_state(void)
{
    return &g_state.who;
}

const GameState *game_state(void)
{
    return &g_state;
}

StandingState *standing_state(void)
{
    return &g_state.standing;
}

BankState *bank_state(void)
{
    return &g_state.bank;
}

CustomSpell *custom_spell_edit(int slot)
{
    return slot >= 0 && slot < CUSTOM_SPELLS ? &g_state.custom_spell[slot] : 0;
}

ItemEnchant *item_enchant_edit(int slot)
{
    return slot >= 0 && slot < ENCHANTED_ITEMS ? &g_state.enchant[slot] : 0;
}

GuildsState *guilds_state(void)
{
    return &g_state.guilds;
}

LodgingState *lodging_state(void)
{
    return &g_state.lodging;
}

LawState *law_state(void)
{
    return &g_state.law;
}

void skill_add_uses(int skill, int n)
{
    long uses;

    if (skill < 0 || skill >= SKILLS) { return; }
    uses = (long)g_state.skill_uses[skill] + n;
    g_state.skill_uses[skill] = uses < 0 ? 0 : uses > 20000 ? 20000 : (df_u32)uses;
}

void skill_use(int skill)
{
    if (skill >= 0 && skill < SKILLS && g_state.skill_uses[skill] != 0xffffffffu) {
        g_state.skill_uses[skill]++;
    }
}

const char *state_error(void)
{
    return g_error;
}

void character_clear(void)
{
    mem_zero(&g_state.who, sizeof(g_state.who));
    g_state.who.reflexes = 2;
    g_state.who.level = 1;
    g_state.who.klass = -1;
    g_state.who.race = -1;
}

void state_clear(void)
{
    df_u32 seed;
    int flags, mouse;
    seed = g_state.random;
    flags = g_state.settings_flags;
    mouse = g_state.mouse_mode;
    mem_zero(&g_state, sizeof(g_state));
    g_state.version = STATE_VERSION;
    g_state.random = seed;
    g_state.settings_flags = flags;
    g_state.mouse_mode = mouse;
    g_state.next_low = 10000;
    g_state.region = g_state.location = -1;
    spells_sync_custom(&g_state);
    quest_reset();
    character_clear();
}

void random_seed(df_u32 seed)
{
    g_state.random = seed;
}

int random_range(int low, int high)
{
    if (high < low || ((df_u32)high - (df_u32)low + 1u) == 0) {
        return low;
    }
    /* Watcom rand at code VA 0009DC25, verified against FALL.EXE bytes.
     * Seed zero is valid. One stream is shared by creation and gameplay. */
    g_state.random = g_state.random * 1103515245u + 12345u;
    return low + (int)(((g_state.random >> 16) & 0x7fffu) %
                        ((df_u32)high - (df_u32)low + 1u));
}

static int object_slot(const GameState *s, df_u32 id)
{
    int i;

    if (!id) {
        return -1;
    }
    for (i = 0; i < OBJECTS; i++) {
        if (s->object[i].id == id) {
            return i;
        }
    }
    return -1;
}

const ObjectInfo *object_at(df_u32 id)
{
    int i;
    i = object_slot(&g_state, id);
    return i < 0 ? 0 : &g_state.object[i];
}

ObjectInfo *object_edit(df_u32 id)
{
    int slot;
    slot = object_slot(&g_state, id);
    return slot < 0 ? 0 : &g_state.object[slot];
}

int state_equip(int slot, df_u32 id)
{
    const ObjectInfo *item;
    if (slot < 0 || slot >= 27) { return 0; }
    item = object_at(id);
    if (id && (!item || item->type != RECORD_ITEM || item->parent != g_state.inventory_id)) { return 0; }
    /* item_remove_equip_effects, then the slot, then item_apply_equip_effects: the one place an item goes into or out of a slot, so every path of the original (and the ones it forgot) applies and removes. */
    if (g_state.equipped[slot] == id) { return 1; }
    if (g_state.equipped[slot]) { enchant_unequip(g_state.equipped[slot], slot); }
    g_state.equipped[slot] = id;
    if (id) { enchant_equip(id, slot); }
    return 1;
}

static ObjectInfo *make_object(df_u32 id, int type, df_u32 parent, int index)
{
    int i;

    for (i = 0; i < OBJECTS; i++) {
        if (!g_state.object[i].id) {
            ObjectInfo *o;
            o = &g_state.object[i];
            mem_zero(o, sizeof(*o));
            o->id = id;
            o->type = type;
            o->parent = parent;
            o->index = index;
            o->count = 1;
            return o;
        }
    }
    g_error = "GFALL object capacity reached";
    return 0;
}

df_u32 object_new(int type, df_u32 parent, int index, unsigned high)
{
    df_u32 id;
    int i;

    if (!object_at(parent) || type <= 0 || high > 65535u) {
        return 0;
    }
    /* object_new_id: low IDs 10000..62999, then wrap, checking both roots. */
    for (i = 0; i < 53000; i++) {
        if (g_state.next_low >= 63000u) {
            g_state.next_low = 10000u;
        }
        id = ((df_u32)high << 16) | g_state.next_low++;
        if (!object_at(id)) {
            return make_object(id, type, parent, index) ? id : 0;
        }
    }
    return 0;
}

static int protected_id(df_u32 id)
{
    const ObjectInfo *o;
    o = object_at(id);
    if (!o) {
        return 0;
    }
    return id == WORLD_ROOT || id == PLAYER_ROOT ||
        id == g_state.entity_id || id == g_state.move_id || id == g_state.eye_id ||
        (o->parent == g_state.entity_id &&
         (o->type == RECORD_CONTAINER || o->type == RECORD_QUESTS ||
          o->type == RECORD_OPTIONS || o->type == RECORD_LOGBOOK || o->type == RECORD_BANK));
}

int object_reparent(df_u32 id, df_u32 parent)
{
    int slot, i;
    df_u32 p;
    slot = object_slot(&g_state, id);
    if (slot < 0 || !object_at(parent) || protected_id(id)) {
        return 0;
    }
    for (i = 0; i < 27; i++) { if (g_state.equipped[i] == id) { return 0; } }
    p = parent;
    for (i = 0; p && i < OBJECTS; i++) {
        if (p == id) {
            return 0;
        }
        p = object_at(p)->parent;
    }
    if (p) {
        return 0;
    }
    g_state.object[slot].parent = parent;
    return 1;
}

int object_delete(df_u32 id)
{
    int i, slot;
    slot = object_slot(&g_state, id);
    if (slot < 0 || protected_id(id)) {
        return 0;
    }
    for (i = 0; i < OBJECTS; i++) {
        if (g_state.object[i].id && g_state.object[i].parent == id) {
            return 0;
        }
    }
    for (i = 0; i < 27; i++) {
        if (g_state.equipped[i] == id) {
            return 0;
        }
    }
    if (g_state.magic.ready == id) { g_state.magic.ready = 0; }
    if (g_state.magic.last == id) { g_state.magic.last = 0; g_state.magic.paid = 0; }
    mem_zero(&g_state.object[slot], sizeof(ObjectInfo));
    return 1;
}

void state_observe(void (*fn)(const QuestEvent *event))
{
    g_observer = fn;
}

void state_event(int kind, df_u32 subject, df_u32 other, int value)
{
    QuestEvent e;
    e.kind = kind;
    e.subject = subject;
    e.other = other;
    e.value = value;
    g_state.last_event = e;
    g_state.event_serial++;
    /* A kill by the player starts Health leech's clock again. */
    if (kind == EVENT_KILL && other && other == g_state.entity_id) { g_state.last_kill = g_state.minutes; }
    quest_event(kind, subject, other);
    if (g_observer) {
        g_observer(&e);
    }
}

int state_begin(void)
{
    CharacterState who;
    const ClassInfo *career;
    df_u32 id;
    int i;
    who = g_state.who;
    career = class_at(who.klass);
    if (!career || who.race < 0 || who.race > 7) {
        return 0;
    }
    state_clear();
    standing_defaults(&g_state.standing);
    g_state.who = who;
    g_state.who.weapon = g_state.who.weapon_sound = 0;
    g_state.career = *career;
    for (i = 0; i < ATTRS; i++) {
        g_state.base_attribute[i] = who.attribute[i];
    }
    /* init_player_records, original type numbers; no engine slot identities. */
    make_object(WORLD_ROOT, RECORD_WORLD, 0, 0);
    make_object(PLAYER_ROOT, RECORD_NONWORLD, 0, 0);
    g_state.move_id = object_new(RECORD_MOVE, PLAYER_ROOT, 0, 100);
    g_state.eye_id = object_new(RECORD_EYE, g_state.move_id, 0, 100);
    g_state.entity_id = object_new(RECORD_ENTITY, g_state.move_id, 0, 100);
    for (i = 0; i <= 8; i++) {
        /* Wagon exists only when owned. */
        if (i == 4) {
            continue;
        }
        id = object_new(RECORD_CONTAINER, g_state.entity_id, i, 100);
        if (i == 0) {
            g_state.inventory_id = id;
        }
    }
    object_new(RECORD_QUESTS, g_state.entity_id, 0, 100);
    object_new(RECORD_OPTIONS, g_state.entity_id, 0, 100);
    object_new(RECORD_LOGBOOK, g_state.entity_id, 0, 100);
    object_new(RECORD_BANK, g_state.entity_id, 0, 100);
    for (i = 0; i < who.gifts && i < 32; i++) {
        id = item_create(g_state.inventory_id, who.gift[i].group,
                            who.gift[i].index, who.gift[i].material);
        if (!id) { return 0; }
        if (who.gift[i].group == 3 && who.gift[i].index == 18) {
            object_edit(id)->count = who.arrows ? who.arrows : 24;
        }
        if (who.gift[i].group == 27 && who.gift[i].index == 0) { inventory_start_spells(id); }
        if (i == who.gifts-2 && who.klass != 13 && who.gift[i].group == 3) {
            /* starting_equipment_give fixes a standard class weapon's value. */
            object_edit(id)->value = 15;
        }
        if (who.gift[i].group == 6 || who.gift[i].group == 12) {
            inventory_equip(id);
        }
    }
    g_state.minutes = 523530u; /* Initial value, func_0004ED36; game_reset differs. */
    g_state.ready = 1;
    progress_begin();
    automap_clear();
    state_calendar();
    return 1;
}

void state_settings(int flags, int mouse_mode)
{
    g_state.settings_flags = flags & 3;
    g_state.mouse_mode = mouse_mode ? 1 : 0;
}

int state_mode_push(int mode)
{
    if (!g_state.ready || mode < 0 || mode > 19 || g_state.depth >= MODES) {
        return 0;
    }
    if (g_state.mode == GAME_WORLD) {
        g_state.ticks = 0;
        g_state.tick_fraction = 0;
        g_state.frame_fraction = 0;
    }
    g_state.mode_stack[g_state.depth++] = g_state.mode;
    g_state.mode = mode;
    return 1;
}

int state_mode_pop(void)
{
    if (!g_state.depth) {
        return 0;
    }
    g_state.mode = g_state.mode_stack[--g_state.depth];
    g_state.mode_stack[g_state.depth] = 0;
    return 1;
}

void state_set_place(int region, int location, int environment)
{
    if (!g_state.ready) {
        return;
    }
    g_state.region = region;
    g_state.location = location;
    g_state.environment = environment;
    state_event(EVENT_PLACE, 0, 0, location);
}

void state_pose(float x, float eye_y, float z, int yaw, int pitch)
{
    if (!g_state.ready) {
        return;
    }
    g_state.x = x;
    g_state.eye_y = eye_y;
    g_state.z = z;
    g_state.yaw = yaw & 2047;
    g_state.pitch = pitch;
}

void state_date(int *year, int *month, int *day, int *weekday)
{
    df_u32 days;

    days = g_state.minutes / 1440u;
    if (year) {
        *year = 400 + (int)(days / 360u);
    }
    if (month) {
        *month = (int)((days % 360u) / 30u);
    }
    if (day) {
        *day = (int)(days % 30u);
    }
    if (weekday) {
        *weekday = (int)(days % 7u);
    }
}

void state_calendar(void)
{
    Calendar c;
    clock_calendar_default(&c);
    c.week_restarts = 0;
    c.start_year = 400;
    c.start_month = c.start_day = 0;
    clock_set_calendar(&c);
    clock_set_scale(0); /* GFALL owns time; rendering cannot advance it again. */
    clock_set_day(400 * 360 + (int)(g_state.minutes / 1440u));
    clock_set_minute((float)(g_state.minutes % 1440u));
}

void state_pass_minutes(df_u32 minutes)
{
    df_u32 i, start;
    if (!g_state.ready) {
        return;
    }
    /* time_pass_minutes treats even a negative (unsigned) request as one. */
    if (minutes > 200000u) {
        minutes = 1;
    }
    start = g_state.minutes;
    if (start > 0x7fffffffu - minutes) {
        return;
    }
    g_state.minutes += minutes;
    magic_minutes(start,minutes);
    enchant_minutes();
    /* quests_run_all for each passing minute, unless a sentence is being served. */
    if (!law_is_serving()) { quest_run_minute(); }
    for (i = 0; i < minutes; i++) {
        state_event(EVENT_MINUTE, 0, 0, (int)(start + i + 1));
        /* time_pass: each 161280 minutes the standings drift one point toward nothing (reputation_decay). */
        if ((start + i + 1) % DECAY_MINUTES == 0) {
            standing_decay(law_is_serving());
        }
    }
    blessing_expire(g_state.minutes);
    state_calendar();
}

void state_advance_ms(df_u32 milliseconds)
{
    df_u32 step, fraction, ticks;
    if (!g_state.ready || g_state.mode != GAME_WORLD || g_state.depth) {
        return;
    }
    /* PIT 1193182 / 65536 BIOS ticks/sec; 90 ticks per game minute.
     * Bounded multiplies keep the period CPU on 32-bit integer arithmetic. */
    while (milliseconds) {
        step = milliseconds > 1000u ? 1000u : milliseconds;
        milliseconds -= step;
        fraction = g_state.tick_fraction + step * 1193182u;
        ticks = g_state.ticks + fraction / 65536000u;
        g_state.tick_fraction = fraction % 65536000u;
        g_state.ticks = ticks % 90u;
        if (ticks >= 90u) {
            state_pass_minutes(ticks / 90u);
        }
    }
}

void state_frame(float seconds)
{
    float total;
    df_u32 ms;
    if (!g_state.ready || g_state.mode != GAME_WORLD || g_state.depth ||
        !(seconds > 0.0f && seconds <= 1.0f)) { return; }
    total = g_state.frame_fraction + seconds * 1000.0f;
    ms = (df_u32)df_ifloor(total);
    g_state.frame_fraction = total - (float)ms;
    state_advance_ms(ms);
    enchant_frame();
    /* quests_run_all, every frame of play (the state's frame runs in the world only: the inventory suspends them). */
    quest_run();
}

static int character_valid(const GameState *state)
{
    const CharacterState *who;
    const ClassInfo *career;
    int i;

    who = &state->who;
    career = &state->career;
    if (who->race < 0 || who->race > 7 || who->female < 0 || who->female > 1 ||
        who->klass < 0 || who->klass >= CLASSES || who->name[NAME_LEN - 1] ||
        who->gifts < 0 || who->gifts > 32 || who->reflexes < 0 || who->reflexes > 4 ||
        who->gold < 0 || who->gold > 100000000 || who->level < 1 || who->max_health < 1 || who->max_health > 1000000 ||
        who->health < 0 || who->health > who->max_health ||
        who->max_magicka < 0 || who->max_magicka > 1000000 ||
        who->magicka < 0 || who->magicka > who->max_magicka ||
        who->fatigue < 0 || who->fatigue > 1000000 || career->name[15]) {
        return 0;
    }
    /* Generous bounds allow later buffs without permitting arithmetic overflow. */
    for (i = 0; i < ATTRS; i++) {
        if (who->attribute[i] < 0 || who->attribute[i] > 1000 ||
            state->base_attribute[i] < 0 || state->base_attribute[i] > 1000) {
            return 0;
        }
    }
    for (i = 0; i < SKILLS; i++) {
        if (who->skill[i] < 0 || who->skill[i] > 1000) {
            return 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if (career->primary[i] < 0 || career->primary[i] >= SKILLS ||
            career->major[i] < 0 || career->major[i] >= SKILLS) {
            return 0;
        }
    }
    for (i = 0; i < 6; i++) {
        if (career->minor[i] < 0 || career->minor[i] >= SKILLS) {
            return 0;
        }
    }
    return 1;
}

static int record_matches(const GameState *state, df_u32 id, int type, df_u32 parent)
{
    int slot;

    slot = object_slot(state, id);
    return slot >= 0 && state->object[slot].type == type &&
        state->object[slot].parent == parent;
}

static int objects_valid(const GameState *state)
{
    const ObjectInfo *object;
    df_u32 parent;
    int i, j, slot, depth;

    if (!record_matches(state, WORLD_ROOT, RECORD_WORLD, 0) ||
        !record_matches(state, PLAYER_ROOT, RECORD_NONWORLD, 0) ||
        !record_matches(state, state->move_id, RECORD_MOVE, PLAYER_ROOT) ||
        !record_matches(state, state->eye_id, RECORD_EYE, state->move_id) ||
        !record_matches(state, state->entity_id, RECORD_ENTITY, state->move_id) ||
        !record_matches(state, state->inventory_id, RECORD_CONTAINER, state->entity_id)) {
        return 0;
    }
    for (i = 0; i < OBJECTS; i++) {
        object = &state->object[i];
        if (!object->id) {
            continue;
        }
        if (object->type <= 0 || object->count < 0 ||
            !(object->x >= -100000000 && object->x <= 100000000) ||
            !(object->y >= -100000000 && object->y <= 100000000) ||
            !(object->z >= -100000000 && object->z <= 100000000)) {
            return 0;
        }
        for (j = i + 1; j < OBJECTS; j++) {
            if (state->object[j].id == object->id) {
                return 0;
            }
        }
        if (!object->parent && object->id != WORLD_ROOT && object->id != PLAYER_ROOT) {
            return 0;
        }
        parent = object->parent;
        for (depth = 0; parent && depth < OBJECTS; depth++) {
            slot = object_slot(state, parent);
            if (parent == object->id || slot < 0) {
                return 0;
            }
            parent = state->object[slot].parent;
        }
        if (parent) {
            return 0;
        }
    }
    for (i = 0; i < 27; i++) {
        slot = object_slot(state, state->equipped[i]);
        if (state->equipped[i] && (slot < 0 || state->object[slot].type != RECORD_ITEM)) {
            return 0;
        }
    }
    return 1;
}

static int clock_and_view_valid(const GameState *state)
{
    int i;
    if (state->progress.checked > state->minutes || state->progress.points < 0 || state->progress.points > 6 ||
        state->progress.active < 0 || state->progress.active > 1 ||
        (!state->progress.active && state->progress.points) ||
        state->progress.base_sum < -1500 || state->progress.base_sum > 600 || state->progress.raised_hi > 7) { return 0; }
    if (state->survival.breath < 0 || state->survival.breath > 80 ||
        !(state->survival.fraction >= 0 && state->survival.fraction < 1) ||
        !(state->survival.climb_time >= 0 && state->survival.climb_time <= 1) ||
        state->survival.climb_minute > state->minutes || state->survival.climbing < 0 ||
        state->survival.climbing > 1 || state->survival.climb_failed < 0 || state->survival.climb_failed > 1) { return 0; }
    for (i = 0; i < ATTRS; i++) {
        if (state->progress.floor[i] < 0 || state->progress.floor[i] > 100) { return 0; }
    }

    if (state->next_low < 10000u || state->next_low > 63000u ||
        state->minutes > 0x7fffffffu || state->stealth_minute > state->minutes ||
        state->ticks >= 90u || state->tick_fraction >= 65536000u ||
        !(state->frame_fraction >= 0 && state->frame_fraction < 1) ||
        state->settings_flags < 0 || state->settings_flags > 3 ||
        state->mouse_mode < 0 || state->mouse_mode > 1 ||
        state->depth < 0 || state->depth > MODES || state->mode < 0 || state->mode > 19 ||
        state->yaw < 0 || state->yaw >= 2048 || state->pitch < -256 || state->pitch > 256 ||
        !(state->x >= -100000000 && state->x <= 100000000) ||
        !(state->eye_y >= -100000000 && state->eye_y <= 100000000) ||
        !(state->z >= -100000000 && state->z <= 100000000)) {
        return 0;
    }
    for (i = 0; i < state->depth; i++) {
        if (state->mode_stack[i] < 0 || state->mode_stack[i] > 19) {
            return 0;
        }
    }
    return 1;
}

static int position_valid(float value)
{
    return value >= -100000000 && value <= 100000000;
}

static int encounter_kind_valid(int kind)
{
    return (kind >= 0 && kind < 43) || (kind >= 128 && kind <= 146);
}

static int encounters_valid(const EncounterState *state)
{
    const EncounterRoll *actor;
    int i, j, archive;
    df_u32 serial;

    if (state->serial > 0xffffffu) { return 0; }
    for (i = 0; i < 6; i++) { if (!encounter_kind_valid(state->choices[i])) { return 0; } }
    for (i = 0; i < ENCOUNTERS; i++) {
        actor = &state->actor[i];
        if (!actor->key.thing) { continue; }
        serial = actor->key.thing & 0xffffffu;
        if (!state->place || !actor->key.place || !serial || serial > state->serial ||
            actor->key.thing != (0x80000000u | serial) || !encounter_kind_valid(actor->kind) ||
            !position_valid(actor->x) || !position_valid(actor->y) || !position_valid(actor->z)) { return 0; }
        archive = actor->kind < 43 ? 255+actor->kind : actor->kind == 146 ? 399 :
                  432+foe_texture_kind(actor->kind);
        if (actor->archive != archive &&
            !(actor->kind >= 128 && actor->kind < 146 && actor->archive == archive+1)) { return 0; }
        for (j = 0; j < i; j++) {
            if (state->actor[j].key.thing == actor->key.thing) { return 0; }
        }
    }
    return 1;
}

/* A kept creature's row (docs/entities.md, Kept): flagged kept, a rest or quest key, and an owner that names a kind of the tables with its own picture archive. */
static int kept_row_valid(const Keep *row)
{
    int kind, archive, expect;

    kind = (int)(row->owner & 255u); archive = (int)((row->owner >> 8) & 4095u);
    expect = kind < 43 ? 255+kind : kind == 146 ? 399 : 432+foe_texture_kind(kind);
    return (row->flags & ENT_KEPT) && (row->key.thing & 0x80000000u) && row->made == 2 && encounter_kind_valid(kind) &&
           (archive == expect || (kind >= 128 && kind < 146 && archive == expect+1));
}

static int saved_world_valid(const GameState *state)
{
    const Keep *row;
    const MotionState *motion;
    int i, j;

    if (state->world_count < 0 || state->world_count > WORLD_ROWS ||
        state->save_label[31] || state->preview_valid < 0 || state->preview_valid > 1 ||
        state->motion_valid < 0 || state->motion_valid > 1 ||
        state->hud_mode < 0 || state->hud_mode > 3 ||
        state->weapon_ready < 0 || state->weapon_ready > 1 ||
        state->cursor_active < 0 || state->cursor_active > 1) { return 0; }
    motion = &state->motion;
    if (state->motion_valid) {
        if (motion->yaw < 0 || motion->yaw >= 2048 || motion->pitch < -256 || motion->pitch > 256 ||
            !(motion->eye > 0 && motion->eye <= 100) ||
            !(motion->bob_ms >= 0 && motion->bob_ms < 1000) ||
            !(motion->vy >= -4000 && motion->vy <= 4000) ||
            !(motion->vx >= -10000 && motion->vx <= 10000) ||
            !(motion->vz >= -10000 && motion->vz <= 10000) ||
            !position_valid(motion->top) ||
            motion->airborne < 0 || motion->airborne > 1 ||
            motion->crouched < 0 || motion->crouched > 1 ||
            motion->water < 0 || motion->water > 1 || motion->weight < 0 || motion->weight > 100000 ||
            motion->grounded < 0 || motion->grounded > 1 ||
            motion->have_safe < 0 || motion->have_safe > 1) { return 0; }
        for (i = 0; i < 3; i++) {
            if (!position_valid(motion->feet[i]) || !position_valid(motion->safe[i])) { return 0; }
        }
    }
    for (i = 0; i < state->world_count; i++) {
        row = &state->world[i];
        if ((!row->key.place && !row->key.thing) ||
            (row->kind != KEEP_MOVER && row->kind != KEEP_ENTITY) || row->at_end > 1 ||
            row->dead > 1 || row->moving > 1 || row->placed > 1 || row->hold > 1 ||
            row->made > 2 || row->residence > ENT_HOME_ROAM || (row->made && !kept_row_valid(row)) ||
            row->act >= ENTITY_POSES || !position_valid(row->x) || !position_valid(row->y) ||
            !position_valid(row->z) || !(row->yaw >= 0 && row->yaw < DF_TAU)) { return 0; }
        for (j = 0; j < i; j++) {
            if (row->key.place == state->world[j].key.place && row->key.thing == state->world[j].key.thing) { return 0; }
        }
    }
    return 1;
}

static int magic_references_valid(const GameState *state)
{
    const ObjectInfo *spell, *book;
    df_u32 ids[2];
    int i, slot;
    ids[0] = state->magic.ready; ids[1] = state->magic.last;
    for (i = 0; i < 2; i++) {
        if (!ids[i]) { continue; }
        slot = object_slot(state,ids[i]); if (slot < 0) { return 0; }
        spell = &state->object[slot];
        slot = object_slot(state,spell->parent); if (slot < 0) { return 0; }
        book = &state->object[slot];
        if (spell->type != RECORD_SPELL || book->type != RECORD_ITEM ||
            book->group != 27 || book->index != 0) { return 0; }
    }
    return 1;
}

int state_valid(const GameState *snapshot)
{
    return snapshot && snapshot->version == STATE_VERSION && snapshot->ready == 1 &&
        character_valid(snapshot) && clock_and_view_valid(snapshot) && objects_valid(snapshot) &&
        saved_world_valid(snapshot) && encounters_valid(&snapshot->encounters) &&
        magic_valid(&snapshot->magic) && world_valid(&snapshot->world_state) && magic_references_valid(snapshot) && inventory_valid(snapshot) &&
        (!snapshot->standing.ready || standing_valid(&snapshot->standing)) && bank_valid(&snapshot->bank) && lodging_valid(&snapshot->lodging) && law_valid(&snapshot->law) && guilds_valid(&snapshot->guilds);
}

int state_restore(const GameState *snapshot)
{
    /* A spell the player made is a spell object's definition: the registry holds it before the state is judged. */
    spells_sync_custom(snapshot);
    if (!state_valid(snapshot)) {
        spells_sync_custom(&g_state);
        g_error = "Invalid GFALL session";
        return 0;
    }
    g_state = *snapshot;
    /* The quests come back by reading their files again; bytes that cannot be read leave none running. */
    (void)quest_unpack(g_state.quest_blob, g_state.quest_len);
    spells_sync_custom(&g_state);
    if (g_state.who.klass == 18) {
        class_set(18, &g_state.career);
    }
    state_calendar();
    g_error = "";
    return 1;
}


int state_capture_world(void)
{
    int i, count;
    count = keep_count();
    if (count > WORLD_ROWS) { g_error = "Too many changed world objects to save"; return 0; }
    mem_zero(g_state.world, sizeof(g_state.world));
    for (i = 0; i < count; i++) { g_state.world[i] = *keep_at(i); }
    g_state.world_count = count;
    return 1;
}

int state_restore_world(void)
{
    int i;
    keep_clear();
    for (i = 0; i < g_state.world_count; i++) {
        if (!keep_put(&g_state.world[i])) { g_error = "No memory to restore world changes"; return 0; }
    }
    return 1;
}

void state_capture_motion(void)
{
    move_capture(&g_state.motion);
    g_state.motion_valid = 1;
}

void state_ui(int mode, int weapon_ready, int cursor_active)
{
    g_state.hud_mode = mode;
    g_state.weapon_ready = weapon_ready;
    g_state.cursor_active = cursor_active;
}

void state_save_label(const char *label)
{
    df_copy(g_state.save_label, label, sizeof(g_state.save_label));
}

void state_preview(const df_u16 *pixels)
{
    if (!pixels) { g_state.preview_valid = 0; return; }
    mem_copy(g_state.preview, pixels, sizeof(g_state.preview));
    g_state.preview_valid = 1;
}
