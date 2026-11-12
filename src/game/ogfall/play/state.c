#include "game/ogfall/play/state.h"
#include "engine/core/mem.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/enchant.h"
#include "game/ogfall/play/quest.h"
#include "game/ogfall/tables/combatdefs.h"
#include "engine/scene/clock.h"

static OgState g_state;
OgWorldState *og_world_state(void) { return &g_state.world_state; }
static void (*g_observer)(const OgEvent *event);
static const char *g_error = "";
OgMagicState *og_magic_state(void) { return &g_state.magic; }

void og_state_stealth_checked(void) { g_state.stealth_minute = g_state.minutes; }
OgProgress *og_progress(void) { return &g_state.progress; }
OgSurvival *og_survival(void) { return &g_state.survival; }
OgEncounterState *og_encounter_state(void) { return &g_state.encounters; }
void og_state_clear_skill_uses(int skill)
{
    if (skill >= 0 && skill < OG_SKILLS) { g_state.skill_uses[skill] = 0; }
}
void og_state_base_attribute(int attribute, int value)
{
    if (attribute >= 0 && attribute < OG_ATTRS && value >= 0 && value <= 100) { g_state.base_attribute[attribute] = value; }
}

OgCharacter *og_character(void)
{
    return &g_state.who;
}

const OgState *og_state(void)
{
    return &g_state;
}

OgStanding *og_standing(void)
{
    return &g_state.standing;
}

OgBankState *og_bank(void)
{
    return &g_state.bank;
}

OgCustomSpell *og_custom_spell_edit(int slot)
{
    return slot >= 0 && slot < OG_CUSTOM_SPELLS ? &g_state.custom_spell[slot] : 0;
}

OgItemEnchant *og_item_enchant_edit(int slot)
{
    return slot >= 0 && slot < OG_ENCHANTED_ITEMS ? &g_state.enchant[slot] : 0;
}

OgGuilds *og_guilds(void)
{
    return &g_state.guilds;
}

OgLodging *og_lodging(void)
{
    return &g_state.lodging;
}

OgLaw *og_law(void)
{
    return &g_state.law;
}

void og_skill_add_uses(int skill, int n)
{
    long uses;

    if (skill < 0 || skill >= OG_SKILLS) { return; }
    uses = (long)g_state.skill_uses[skill] + n;
    g_state.skill_uses[skill] = uses < 0 ? 0 : uses > 20000 ? 20000 : (df_u32)uses;
}

void og_skill_use(int skill)
{
    if (skill >= 0 && skill < OG_SKILLS && g_state.skill_uses[skill] != 0xffffffffu) {
        g_state.skill_uses[skill]++;
    }
}

const char *og_state_error(void)
{
    return g_error;
}

void og_character_clear(void)
{
    mem_zero(&g_state.who, sizeof(g_state.who));
    g_state.who.reflexes = 2;
    g_state.who.level = 1;
    g_state.who.klass = -1;
    g_state.who.race = -1;
}

void og_state_clear(void)
{
    df_u32 seed;
    int flags, mouse;
    seed = g_state.random;
    flags = g_state.settings_flags;
    mouse = g_state.mouse_mode;
    mem_zero(&g_state, sizeof(g_state));
    g_state.version = OG_STATE_VERSION;
    g_state.random = seed;
    g_state.settings_flags = flags;
    g_state.mouse_mode = mouse;
    g_state.next_low = 10000;
    g_state.region = g_state.location = -1;
    og_spells_sync_custom(&g_state);
    og_quest_reset();
    og_character_clear();
}

void og_random_seed(df_u32 seed)
{
    g_state.random = seed;
}

int og_random_range(int low, int high)
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

static int object_slot(const OgState *s, df_u32 id)
{
    int i;

    if (!id) {
        return -1;
    }
    for (i = 0; i < OG_OBJECTS; i++) {
        if (s->object[i].id == id) {
            return i;
        }
    }
    return -1;
}

const OgObject *og_object(df_u32 id)
{
    int i;
    i = object_slot(&g_state, id);
    return i < 0 ? 0 : &g_state.object[i];
}

OgObject *og_object_edit(df_u32 id)
{
    int slot;
    slot = object_slot(&g_state, id);
    return slot < 0 ? 0 : &g_state.object[slot];
}

int og_state_equip(int slot, df_u32 id)
{
    const OgObject *item;
    if (slot < 0 || slot >= 27) { return 0; }
    item = og_object(id);
    if (id && (!item || item->type != OG_RECORD_ITEM || item->parent != g_state.inventory_id)) { return 0; }
    /* item_remove_equip_effects, then the slot, then item_apply_equip_effects: the one place an item goes into or out of a slot, so every path of the original (and the ones it forgot) applies and removes. */
    if (g_state.equipped[slot] == id) { return 1; }
    if (g_state.equipped[slot]) { og_enchant_unequip(g_state.equipped[slot], slot); }
    g_state.equipped[slot] = id;
    if (id) { og_enchant_equip(id, slot); }
    return 1;
}

static OgObject *make_object(df_u32 id, int type, df_u32 parent, int index)
{
    int i;

    for (i = 0; i < OG_OBJECTS; i++) {
        if (!g_state.object[i].id) {
            OgObject *o;
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
    g_error = "OGFALL object capacity reached";
    return 0;
}

df_u32 og_object_new(int type, df_u32 parent, int index, unsigned high)
{
    df_u32 id;
    int i;

    if (!og_object(parent) || type <= 0 || high > 65535u) {
        return 0;
    }
    /* object_new_id: low IDs 10000..62999, then wrap, checking both roots. */
    for (i = 0; i < 53000; i++) {
        if (g_state.next_low >= 63000u) {
            g_state.next_low = 10000u;
        }
        id = ((df_u32)high << 16) | g_state.next_low++;
        if (!og_object(id)) {
            return make_object(id, type, parent, index) ? id : 0;
        }
    }
    return 0;
}

static int protected_id(df_u32 id)
{
    const OgObject *o;
    o = og_object(id);
    if (!o) {
        return 0;
    }
    return id == OG_WORLD_ROOT || id == OG_PLAYER_ROOT ||
        id == g_state.entity_id || id == g_state.move_id || id == g_state.eye_id ||
        (o->parent == g_state.entity_id &&
         (o->type == OG_RECORD_CONTAINER || o->type == OG_RECORD_QUESTS ||
          o->type == OG_RECORD_OPTIONS || o->type == OG_RECORD_LOGBOOK || o->type == OG_RECORD_BANK));
}

int og_object_reparent(df_u32 id, df_u32 parent)
{
    int slot, i;
    df_u32 p;
    slot = object_slot(&g_state, id);
    if (slot < 0 || !og_object(parent) || protected_id(id)) {
        return 0;
    }
    for (i = 0; i < 27; i++) { if (g_state.equipped[i] == id) { return 0; } }
    p = parent;
    for (i = 0; p && i < OG_OBJECTS; i++) {
        if (p == id) {
            return 0;
        }
        p = og_object(p)->parent;
    }
    if (p) {
        return 0;
    }
    g_state.object[slot].parent = parent;
    return 1;
}

int og_object_delete(df_u32 id)
{
    int i, slot;
    slot = object_slot(&g_state, id);
    if (slot < 0 || protected_id(id)) {
        return 0;
    }
    for (i = 0; i < OG_OBJECTS; i++) {
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
    mem_zero(&g_state.object[slot], sizeof(OgObject));
    return 1;
}

void og_state_observe(void (*fn)(const OgEvent *event))
{
    g_observer = fn;
}

void og_state_event(int kind, df_u32 subject, df_u32 other, int value)
{
    OgEvent e;
    e.kind = kind;
    e.subject = subject;
    e.other = other;
    e.value = value;
    g_state.last_event = e;
    g_state.event_serial++;
    /* A kill by the player starts Health leech's clock again. */
    if (kind == OG_EVENT_KILL && other && other == g_state.entity_id) { g_state.last_kill = g_state.minutes; }
    og_quest_event(kind, subject, other);
    if (g_observer) {
        g_observer(&e);
    }
}

int og_state_begin(void)
{
    OgCharacter who;
    const OgClass *career;
    df_u32 id;
    int i;
    who = g_state.who;
    career = og_class(who.klass);
    if (!career || who.race < 0 || who.race > 7) {
        return 0;
    }
    og_state_clear();
    og_standing_defaults(&g_state.standing);
    g_state.who = who;
    g_state.who.weapon = g_state.who.weapon_sound = 0;
    g_state.career = *career;
    for (i = 0; i < OG_ATTRS; i++) {
        g_state.base_attribute[i] = who.attribute[i];
    }
    /* init_player_records, original type numbers; no engine slot identities. */
    make_object(OG_WORLD_ROOT, OG_RECORD_WORLD, 0, 0);
    make_object(OG_PLAYER_ROOT, OG_RECORD_NONWORLD, 0, 0);
    g_state.move_id = og_object_new(OG_RECORD_MOVE, OG_PLAYER_ROOT, 0, 100);
    g_state.eye_id = og_object_new(OG_RECORD_EYE, g_state.move_id, 0, 100);
    g_state.entity_id = og_object_new(OG_RECORD_ENTITY, g_state.move_id, 0, 100);
    for (i = 0; i <= 8; i++) {
        /* Wagon exists only when owned. */
        if (i == 4) {
            continue;
        }
        id = og_object_new(OG_RECORD_CONTAINER, g_state.entity_id, i, 100);
        if (i == 0) {
            g_state.inventory_id = id;
        }
    }
    og_object_new(OG_RECORD_QUESTS, g_state.entity_id, 0, 100);
    og_object_new(OG_RECORD_OPTIONS, g_state.entity_id, 0, 100);
    og_object_new(OG_RECORD_LOGBOOK, g_state.entity_id, 0, 100);
    og_object_new(OG_RECORD_BANK, g_state.entity_id, 0, 100);
    for (i = 0; i < who.gifts && i < 32; i++) {
        id = og_item_create(g_state.inventory_id, who.gift[i].group,
                            who.gift[i].index, who.gift[i].material);
        if (!id) { return 0; }
        if (who.gift[i].group == 3 && who.gift[i].index == 18) {
            og_object_edit(id)->count = who.arrows ? who.arrows : 24;
        }
        if (who.gift[i].group == 27 && who.gift[i].index == 0) { og_inventory_start_spells(id); }
        if (i == who.gifts-2 && who.klass != 13 && who.gift[i].group == 3) {
            /* starting_equipment_give fixes a standard class weapon's value. */
            og_object_edit(id)->value = 15;
        }
        if (who.gift[i].group == 6 || who.gift[i].group == 12) {
            og_inventory_equip(id);
        }
    }
    g_state.minutes = 523530u; /* Initial value, func_0004ED36; game_reset differs. */
    g_state.ready = 1;
    og_progress_begin();
    automap_clear();
    og_state_calendar();
    return 1;
}

void og_state_settings(int flags, int mouse_mode)
{
    g_state.settings_flags = flags & 3;
    g_state.mouse_mode = mouse_mode ? 1 : 0;
}

int og_state_mode_push(int mode)
{
    if (!g_state.ready || mode < 0 || mode > 19 || g_state.depth >= OG_MODES) {
        return 0;
    }
    if (g_state.mode == OG_GAME_WORLD) {
        g_state.ticks = 0;
        g_state.tick_fraction = 0;
        g_state.frame_fraction = 0;
    }
    g_state.mode_stack[g_state.depth++] = g_state.mode;
    g_state.mode = mode;
    return 1;
}

int og_state_mode_pop(void)
{
    if (!g_state.depth) {
        return 0;
    }
    g_state.mode = g_state.mode_stack[--g_state.depth];
    g_state.mode_stack[g_state.depth] = 0;
    return 1;
}

void og_state_set_place(int region, int location, int environment)
{
    if (!g_state.ready) {
        return;
    }
    g_state.region = region;
    g_state.location = location;
    g_state.environment = environment;
    og_state_event(OG_EVENT_PLACE, 0, 0, location);
}

void og_state_pose(float x, float eye_y, float z, int yaw, int pitch)
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

void og_state_date(int *year, int *month, int *day, int *weekday)
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

void og_state_calendar(void)
{
    Calendar c;
    clock_calendar_default(&c);
    c.week_restarts = 0;
    c.start_year = 400;
    c.start_month = c.start_day = 0;
    clock_set_calendar(&c);
    clock_set_scale(0); /* OGFALL owns time; rendering cannot advance it again. */
    clock_set_day(400 * 360 + (int)(g_state.minutes / 1440u));
    clock_set_minute((float)(g_state.minutes % 1440u));
}

void og_state_pass_minutes(df_u32 minutes)
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
    og_magic_minutes(start,minutes);
    og_enchant_minutes();
    /* quests_run_all for each passing minute, unless a sentence is being served. */
    if (!og_law_is_serving()) { og_quest_run_minute(); }
    for (i = 0; i < minutes; i++) {
        og_state_event(OG_EVENT_MINUTE, 0, 0, (int)(start + i + 1));
        /* time_pass: each 161280 minutes the standings drift one point toward nothing (reputation_decay). */
        if ((start + i + 1) % OG_DECAY_MINUTES == 0) {
            og_standing_decay(og_law_is_serving());
        }
    }
    og_blessing_expire(g_state.minutes);
    og_state_calendar();
}

void og_state_advance_ms(df_u32 milliseconds)
{
    df_u32 step, fraction, ticks;
    if (!g_state.ready || g_state.mode != OG_GAME_WORLD || g_state.depth) {
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
            og_state_pass_minutes(ticks / 90u);
        }
    }
}

void og_state_frame(float seconds)
{
    float total;
    df_u32 ms;
    if (!g_state.ready || g_state.mode != OG_GAME_WORLD || g_state.depth ||
        !(seconds > 0.0f && seconds <= 1.0f)) { return; }
    total = g_state.frame_fraction + seconds * 1000.0f;
    ms = (df_u32)df_ifloor(total);
    g_state.frame_fraction = total - (float)ms;
    og_state_advance_ms(ms);
    og_enchant_frame();
    /* quests_run_all, every frame of play (the state's frame runs in the world only: the inventory suspends them). */
    og_quest_run();
}

static int character_valid(const OgState *state)
{
    const OgCharacter *who;
    const OgClass *career;
    int i;

    who = &state->who;
    career = &state->career;
    if (who->race < 0 || who->race > 7 || who->female < 0 || who->female > 1 ||
        who->klass < 0 || who->klass >= OG_CLASSES || who->name[OG_NAME - 1] ||
        who->gifts < 0 || who->gifts > 32 || who->reflexes < 0 || who->reflexes > 4 ||
        who->gold < 0 || who->gold > 100000000 || who->level < 1 || who->max_health < 1 || who->max_health > 1000000 ||
        who->health < 0 || who->health > who->max_health ||
        who->max_magicka < 0 || who->max_magicka > 1000000 ||
        who->magicka < 0 || who->magicka > who->max_magicka ||
        who->fatigue < 0 || who->fatigue > 1000000 || career->name[15]) {
        return 0;
    }
    /* Generous bounds allow later buffs without permitting arithmetic overflow. */
    for (i = 0; i < OG_ATTRS; i++) {
        if (who->attribute[i] < 0 || who->attribute[i] > 1000 ||
            state->base_attribute[i] < 0 || state->base_attribute[i] > 1000) {
            return 0;
        }
    }
    for (i = 0; i < OG_SKILLS; i++) {
        if (who->skill[i] < 0 || who->skill[i] > 1000) {
            return 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if (career->primary[i] < 0 || career->primary[i] >= OG_SKILLS ||
            career->major[i] < 0 || career->major[i] >= OG_SKILLS) {
            return 0;
        }
    }
    for (i = 0; i < 6; i++) {
        if (career->minor[i] < 0 || career->minor[i] >= OG_SKILLS) {
            return 0;
        }
    }
    return 1;
}

static int record_matches(const OgState *state, df_u32 id, int type, df_u32 parent)
{
    int slot;

    slot = object_slot(state, id);
    return slot >= 0 && state->object[slot].type == type &&
        state->object[slot].parent == parent;
}

static int objects_valid(const OgState *state)
{
    const OgObject *object;
    df_u32 parent;
    int i, j, slot, depth;

    if (!record_matches(state, OG_WORLD_ROOT, OG_RECORD_WORLD, 0) ||
        !record_matches(state, OG_PLAYER_ROOT, OG_RECORD_NONWORLD, 0) ||
        !record_matches(state, state->move_id, OG_RECORD_MOVE, OG_PLAYER_ROOT) ||
        !record_matches(state, state->eye_id, OG_RECORD_EYE, state->move_id) ||
        !record_matches(state, state->entity_id, OG_RECORD_ENTITY, state->move_id) ||
        !record_matches(state, state->inventory_id, OG_RECORD_CONTAINER, state->entity_id)) {
        return 0;
    }
    for (i = 0; i < OG_OBJECTS; i++) {
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
        for (j = i + 1; j < OG_OBJECTS; j++) {
            if (state->object[j].id == object->id) {
                return 0;
            }
        }
        if (!object->parent && object->id != OG_WORLD_ROOT && object->id != OG_PLAYER_ROOT) {
            return 0;
        }
        parent = object->parent;
        for (depth = 0; parent && depth < OG_OBJECTS; depth++) {
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
        if (state->equipped[i] && (slot < 0 || state->object[slot].type != OG_RECORD_ITEM)) {
            return 0;
        }
    }
    return 1;
}

static int clock_and_view_valid(const OgState *state)
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
    for (i = 0; i < OG_ATTRS; i++) {
        if (state->progress.floor[i] < 0 || state->progress.floor[i] > 100) { return 0; }
    }

    if (state->next_low < 10000u || state->next_low > 63000u ||
        state->minutes > 0x7fffffffu || state->stealth_minute > state->minutes ||
        state->ticks >= 90u || state->tick_fraction >= 65536000u ||
        !(state->frame_fraction >= 0 && state->frame_fraction < 1) ||
        state->settings_flags < 0 || state->settings_flags > 3 ||
        state->mouse_mode < 0 || state->mouse_mode > 1 ||
        state->depth < 0 || state->depth > OG_MODES || state->mode < 0 || state->mode > 19 ||
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

static int encounters_valid(const OgEncounterState *state)
{
    const OgEncounter *actor;
    int i, j, archive;
    df_u32 serial;

    if (state->serial > 0xffffffu) { return 0; }
    for (i = 0; i < 6; i++) { if (!encounter_kind_valid(state->choices[i])) { return 0; } }
    for (i = 0; i < OG_ENCOUNTERS; i++) {
        actor = &state->actor[i];
        if (!actor->key.thing) { continue; }
        serial = actor->key.thing & 0xffffffu;
        if (!state->place || !actor->key.place || !serial || serial > state->serial ||
            actor->key.thing != (0x80000000u | serial) || !encounter_kind_valid(actor->kind) ||
            !position_valid(actor->x) || !position_valid(actor->y) || !position_valid(actor->z)) { return 0; }
        archive = actor->kind < 43 ? 255+actor->kind : actor->kind == 146 ? 399 :
                  432+og_foe_texture_kind(actor->kind);
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
    expect = kind < 43 ? 255+kind : kind == 146 ? 399 : 432+og_foe_texture_kind(kind);
    return (row->flags & ENT_KEPT) && (row->key.thing & 0x80000000u) && row->made == 2 && encounter_kind_valid(kind) &&
           (archive == expect || (kind >= 128 && kind < 146 && archive == expect+1));
}

static int saved_world_valid(const OgState *state)
{
    const Keep *row;
    const OgMotion *motion;
    int i, j;

    if (state->world_count < 0 || state->world_count > OG_WORLD_ROWS ||
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

static int magic_references_valid(const OgState *state)
{
    const OgObject *spell, *book;
    df_u32 ids[2];
    int i, slot;
    ids[0] = state->magic.ready; ids[1] = state->magic.last;
    for (i = 0; i < 2; i++) {
        if (!ids[i]) { continue; }
        slot = object_slot(state,ids[i]); if (slot < 0) { return 0; }
        spell = &state->object[slot];
        slot = object_slot(state,spell->parent); if (slot < 0) { return 0; }
        book = &state->object[slot];
        if (spell->type != OG_RECORD_SPELL || book->type != OG_RECORD_ITEM ||
            book->group != 27 || book->index != 0) { return 0; }
    }
    return 1;
}

int og_state_valid(const OgState *snapshot)
{
    return snapshot && snapshot->version == OG_STATE_VERSION && snapshot->ready == 1 &&
        character_valid(snapshot) && clock_and_view_valid(snapshot) && objects_valid(snapshot) &&
        saved_world_valid(snapshot) && encounters_valid(&snapshot->encounters) &&
        og_magic_valid(&snapshot->magic) && og_world_valid(&snapshot->world_state) && magic_references_valid(snapshot) && og_inventory_valid(snapshot) &&
        (!snapshot->standing.ready || og_standing_valid(&snapshot->standing)) && og_bank_valid(&snapshot->bank) && og_lodging_valid(&snapshot->lodging) && og_law_valid(&snapshot->law) && og_guilds_valid(&snapshot->guilds);
}

int og_state_restore(const OgState *snapshot)
{
    /* A spell the player made is a spell object's definition: the registry holds it before the state is judged. */
    og_spells_sync_custom(snapshot);
    if (!og_state_valid(snapshot)) {
        og_spells_sync_custom(&g_state);
        g_error = "Invalid OGFALL session";
        return 0;
    }
    g_state = *snapshot;
    /* The quests come back by reading their files again; bytes that cannot be read leave none running. */
    (void)og_quest_unpack(g_state.quest_blob, g_state.quest_len);
    og_spells_sync_custom(&g_state);
    if (g_state.who.klass == 18) {
        og_class_set(18, &g_state.career);
    }
    og_state_calendar();
    g_error = "";
    return 1;
}


int og_state_capture_world(void)
{
    int i, count;
    count = keep_count();
    if (count > OG_WORLD_ROWS) { g_error = "Too many changed world objects to save"; return 0; }
    mem_zero(g_state.world, sizeof(g_state.world));
    for (i = 0; i < count; i++) { g_state.world[i] = *keep_at(i); }
    g_state.world_count = count;
    return 1;
}

int og_state_restore_world(void)
{
    int i;
    keep_clear();
    for (i = 0; i < g_state.world_count; i++) {
        if (!keep_put(&g_state.world[i])) { g_error = "No memory to restore world changes"; return 0; }
    }
    return 1;
}

void og_state_capture_motion(void)
{
    og_move_capture(&g_state.motion);
    g_state.motion_valid = 1;
}

void og_state_ui(int mode, int weapon_ready, int cursor_active)
{
    g_state.hud_mode = mode;
    g_state.weapon_ready = weapon_ready;
    g_state.cursor_active = cursor_active;
}

void og_state_save_label(const char *label)
{
    df_copy(g_state.save_label, label, sizeof(g_state.save_label));
}

void og_state_preview(const df_u16 *pixels)
{
    if (!pixels) { g_state.preview_valid = 0; return; }
    mem_copy(g_state.preview, pixels, sizeof(g_state.preview));
    g_state.preview_valid = 1;
}
