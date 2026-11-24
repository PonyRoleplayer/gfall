#include "game/dagger/play/combat/encounter.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/formats/climate.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/tables/combatdefs.h"
#include "engine/scene/keep.h"
#include "engine/scene/spawn.h"
#include "engine/scene/ent.h"
#include "engine/core/mem.h"

/* Ours: what an owner word packs (a kept creature's kind, picture archive and frame rate). */
#define OWNER_KIND_MASK 0xffu
#define OWNER_ARCHIVE_SHIFT 8
#define OWNER_ARCHIVE_MASK 0xfffu
#define OWNER_RATE_SHIFT 20
#define OWNER_RATE_MASK 0xfffu

static const Scene *g_scene;
static int (*g_art)(int, int, Entity *, int *);
static int g_rest_encounters;

typedef struct OutdoorActor {
    SpawnId id;
    int foe;
} OutdoorActor;

static const EncounterOutdoors *g_outdoors;
static OutdoorActor g_outdoor_actor[ENCOUNTERS];

static int archive_for(int kind)
{
    return foe_archive(kind) + (ENEMY_IS_CLASSIC(kind) || kind == ENEMY_CITY_WATCH ? 0 : random_range(0,1));
}

int encounter_pick(int type, int level)
{
    int roll, low, high;
    roll = random_range(1,PERCENT);
    if (roll <= ENC_BAND_NEAR_CHANCE) { low = level-ENC_BAND_NEAR; high = level+ENC_BAND_NEAR; }
    else if (roll <= ENC_BAND_LOW_CHANCE) { low = 0; high = level+ENC_BAND_LOW_HIGH; }
    else { low = 0; high = level > ENC_BAND_WIDE_LEVEL ? ENC_TABLE_LAST : level+ENC_BAND_WIDE_LOW_HIGH; }
    if (low < 0) { low = 0; high = ENC_TABLE_FLOOR_HIGH; }
    if (high > ENC_TABLE_LAST) { low = ENC_TABLE_CEIL_LOW; high = ENC_TABLE_LAST; }
    return foe_encounter(type,random_range(low,high));
}

void encounter_prepare(df_u32 place, int type)
{
    EncounterState *state;
    int i, kind, saved_seed;
    state = encounter_state();
    if (state->place == place) { return; }
    state->place = place;
    saved_seed = random_range(0,RAND_MAX_WATCOM);
    random_seed(place_seed(place));
    /* dungeon_roll_monster_tables rolls the whole land table. Rest takes
     * slots 1..6, not six fresh table draws on every attempted interruption. */
    for (i = 0; i < ENC_TABLE_SIZE; i++) {
        kind = encounter_pick(type,character_state()->level);
        if (i >= ENC_REST_FIRST && i < ENC_REST_FIRST+ENC_REST_CHOICES) { state->choices[i-ENC_REST_FIRST] = kind; }
    }
    /* Original saves rand()'s result, not the generator's internal state.
     * Its unused water table does not affect the restored gameplay stream. */
    random_seed((df_u32)saved_seed);
}

/* What an entity of the dungeon the game made is, packed in its owner: its kind, its picture archive and its frame rate (docs/entities.md, Kept: a row is the whole entity, its look rebuilt from the
 * key and the owner by the revive hook). */
static df_u32 owner_of(int kind, int archive, int rate)
{
    return (df_u32)(kind & OWNER_KIND_MASK) | ((df_u32)(archive & OWNER_ARCHIVE_MASK) << OWNER_ARCHIVE_SHIFT) | ((df_u32)(rate & OWNER_RATE_MASK) << OWNER_RATE_SHIFT);
}

/* How many of the place's kept creatures stand (the original's room for eight at once, now only a cap on them). */
static int kept_here(void)
{
    Entity *pool;
    int n, i, count;

    pool = ent_pool(g_scene, &n);
    count = 0;
    for (i = 0; pool && i < n; i++) {
        if (pool[i].key.thing && (pool[i].flags & ENT_KEPT) && pool[i].key.place == encounter_state()->place) { count++; }
    }
    return count;
}

/* Leave unsearched and nonempty corpses available for looting. Only
 * generated actors have these keys, so quest foes are never reclaimed. */
static int retire_looted(void)
{
    Entity *pool, *corpse;
    df_u32 container;
    int n, i, foe;

    pool = ent_pool(g_scene,&n);
    for (i = 0; pool && i < n; i++) {
        corpse = &pool[i];
        if (!corpse->dead || !(corpse->flags & ENT_KEPT) || corpse->key.place != encounter_state()->place ||
            (corpse->key.thing & 0xff000000u) != 0x80000000u) { continue; }
        container = loot_container_find(corpse->key.place,corpse->key.thing);
        if (!container || inventory_rows(container,-1,0,0)) { continue; }
        foe = dungeon_index(corpse);
        if (foe < 0) { continue; }
        dungeon_release_actor(foe);
        keep_forget(corpse->key);
        object_delete(container);
        ent_remove(ent_id(corpse));
        return 1;
    }
    return 0;
}

/* A creature of the place a kept row names, made again: its pictures from the game's art, its key and owner as they were. The engine puts it where it stood, as it was. */
static int revive(const Keep *row, Entity *out)
{
    int kind, archive, rate;

    if (!g_scene || !g_art || !encounter_state()->place || row->key.place != encounter_state()->place || !(row->key.thing & 0x80000000u)) { return 0; }
    kind = (int)(row->owner & OWNER_KIND_MASK); archive = (int)((row->owner >> OWNER_ARCHIVE_SHIFT) & OWNER_ARCHIVE_MASK); rate = (int)((row->owner >> OWNER_RATE_SHIFT) & OWNER_RATE_MASK);
    if (!g_art(kind,archive,out,&rate)) { return 0; }
    out->key = row->key; out->live = 1; out->homed = 0;
    out->flags |= ENT_KEPT | ENT_MOVES; out->residence = ENT_HOME_PLACE; out->owner = row->owner;
    return 1;
}

/* A kept creature of the place that has no mind yet (just revived, or just made): it joins the dungeon's combat. */
static int adopt(Entity *e)
{
    int kind, archive, rate, foe;

    kind = (int)(e->owner & OWNER_KIND_MASK); archive = (int)((e->owner >> OWNER_ARCHIVE_SHIFT) & OWNER_ARCHIVE_MASK); rate = (int)((e->owner >> OWNER_RATE_SHIFT) & OWNER_RATE_MASK);
    foe = dungeon_add(e,kind,0,rate);
    if (foe < 0) { ent_remove(ent_id(e)); return -1; }
    if (ENEMY_IS_CLASS_ONLY(kind)) { dungeon_variant(foe,archive-ENEMY_ARCHIVE_CLASS); }
    dungeon_loaded(foe);
    return foe;
}

static void adopt_all(void)
{
    Entity *pool;
    int n, i;

    pool = ent_pool(g_scene,&n);
    for (i = 0; pool && i < n; i++) {
        if (pool[i].key.thing && (pool[i].flags & ENT_KEPT) && pool[i].key.place == encounter_state()->place && !dungeon_has(&pool[i])) { adopt(&pool[i]); }
    }
}

static void outdoor_retire(OutdoorActor *actor)
{
    Entity *entity;

    entity = spawn_get(actor->id);
    if (entity) {
        if (g_outdoors->release_art) { g_outdoors->release_art(entity); }
        dungeon_release_actor(actor->foe);
        spawn_remove(actor->id);
    }
    actor->id = 0;
}

void encounter_update(const Scene *scene, const Camera *camera)
{
    const Entity *entity;
    float dx, dz;
    int i;

    if (!g_outdoors || scene != g_scene || !camera) { return; }
    for (i = 0; i < ENCOUNTERS; i++) {
        if (!g_outdoor_actor[i].id) { continue; }
        entity = spawn_get(g_outdoor_actor[i].id);
        if (entity) {
            dx = entity->x-camera->x; dz = entity->z-camera->z;
            if (dx*dx+dz*dz > (float)ENC_OUTDOOR_RETIRE*ENC_OUTDOOR_RETIRE) { outdoor_retire(&g_outdoor_actor[i]); }
        }
    }
}

int encounter_outdoor_actor(const Entity *entity)
{
    int i;

    if (entity && g_outdoors) {
        for (i = 0; i < ENCOUNTERS; i++) {
            if (g_outdoor_actor[i].id && spawn_get(g_outdoor_actor[i].id) == entity) { return 1; }
        }
    }
    return 0;
}

void encounter_close(void)
{
    Entity *pool;
    int n, i;

    if (g_outdoors) {
        for (i = 0; i < ENCOUNTERS; i++) { if (g_outdoor_actor[i].id) { outdoor_retire(&g_outdoor_actor[i]); } }
        g_outdoors = 0;
    }
    pool = g_scene && g_art ? ent_pool(g_scene,&n) : 0;
    for (i = 0; pool && i < n; i++) {
        if (pool[i].key.thing && (pool[i].flags & ENT_KEPT)) { ent_remove(ent_id(&pool[i])); }
    }
    g_scene = 0; g_art = 0;
}

int encounter_open(const Scene *scene, int (*art)(int, int, Entity *, int *), int rest_encounters)
{
    encounter_close(); g_scene = scene; g_art = art;
    g_rest_encounters = rest_encounters;
    if (scene) {
        ent_set_revive_hook(revive);
        ent_place(scene);
        ent_revive_place(scene,0,0,0);
        adopt_all();
    }
    return 1;
}

void encounter_open_outdoors(const Scene *scene, const EncounterOutdoors *world)
{
    encounter_close();
    g_scene = scene; g_outdoors = world;
}

/* A creature put in the dungeon by something other than a rest's table (a quest's foe) or by it: near the player and unseen, or (at != 0) on the floor near a point, where a quest's marker is. It is a kept
 * entity of the place (ENT_KEPT, its key and owner say what it is), a mind joins it to the dungeon's combat. 0 when there is no room or no floor there yet (the chunk is not loaded): the caller tries again. */
static int place_actor(const Camera *camera, int kind, df_u32 thing, const float *at, float at_climb, int generated, int summoned, int team)
{
    Entity entity, *live;
    SpawnSpot want;
    Body *player;
    int archive, rate;
    float x, y, z;

    if (!g_scene || !g_art || !camera) { return 0; }
    if (generated && encounter_state()->serial >= 0xffffffu) { return 0; }
    archive = archive_for(kind);
    if (!g_art(kind,archive,&entity,&rate)) { return 0; }
    player = move_body(); mem_zero(&want,sizeof(want));
    want.x = at ? at[0] : player->feet[0]; want.y = at ? at[1] : player->feet[1]; want.z = at ? at[2] : player->feet[2];
    if (at) { want.inner = 0; want.outer = ENC_AT_OUTER; want.climb = at_climb; }
    else if (summoned) { want.inner = SUMMON_INNER; want.outer = SUMMON_OUTER; want.climb = SUMMON_CLIMB; }
    else { want.inner = ENC_HIDDEN_INNER; want.outer = ENC_HIDDEN_OUTER; want.climb = ENC_HIDDEN_CLIMB; }
    want.radius = FOE_BODY_RADIUS; want.height = entity.pose[ENTITY_WALK].h[0];
    if (want.height < FOE_BODY_HEIGHT_MIN) { want.height = FOE_BODY_HEIGHT_MIN; }
    if (want.height > FOE_BODY_HEIGHT_MAX) { want.height = FOE_BODY_HEIGHT_MAX; }
    want.flags = (at || summoned ? 0 : SPAWN_UNSEEN) | ((foe_flags(kind) & FOE_FLAGS_NO_FLOOR) ? 0 : SPAWN_DRY);
    if (!spawn_find(g_scene,camera,&want,&x,&y,&z)) { return 0; }
    if (kept_here() >= ENCOUNTERS && !retire_looted()) { return 0; }
    if (generated) { thing = 0x80000000u | ++encounter_state()->serial; }
    entity.key.place = encounter_state()->place; entity.key.thing = thing;
    entity.x = x; entity.y = y; entity.z = z;
    entity.live = 1; entity.homed = 0;
    entity.flags |= ENT_KEPT | ENT_MOVES; entity.residence = ENT_HOME_PLACE; entity.owner = owner_of(kind,archive,rate);
    keep_restore((Scene *)g_scene,0,0,&entity,1,0,0,0);
    live = spawn_get(spawn_add(g_scene,&entity,0));
    if (!live) { return 0; }
    rate = adopt(live);
    if (rate < 0) { return 0; }
    if (summoned) { dungeon_set_team(rate,team); dungeon_wake(rate); }
    else if (QUEST_FOE_IS(thing) &&
        quest_foe_pacified(QUEST_FOE_NUMBER(thing))) { dungeon_set_pacified(rate,1); }
    return 1;
}

int encounter_try(const Camera *camera)
{
    if (!g_scene || !g_art || !camera) { return 0; }
    return place_actor(camera, encounter_state()->choices[random_range(0,ENC_REST_CHOICES-1)], 0, 0, 0, 1, 0, TEAM_HOSTILE);
}

int encounter_place(const Camera *camera, int kind, df_u32 thing) { return place_actor(camera, kind, thing, 0, 0, 0, 0, TEAM_HOSTILE); }

int encounter_place_at(const Camera *camera, int kind, df_u32 thing, float x, float y, float z, float climb)
{
    float at[3];

    at[0] = x; at[1] = y; at[2] = z;
    return place_actor(camera, kind, thing, at, climb, 0, 0, TEAM_HOSTILE);
}

int encounter_summon(const Camera *camera, int kind, int team)
{
    return place_actor(camera,kind,0,0,0,1,1,team);
}

int encounter_release_soul(const Camera *camera, int kind)
{
    return encounter_summon(camera,kind,TEAM_HOSTILE);
}

/* FALL.EXE climate_categories at 0x179D74 for climates 224–232.
 * The original switch leaves swamp category 3 as the table index. */
static int outdoor_table(int climate, int settlement, int night)
{
    static const df_u8 base[] = {ENC_TABLE_DESERT,ENC_TABLE_DESERT,ENC_TABLE_MOUNTAIN,ENC_TABLE_RAINFOREST,ENC_TABLE_SWAMP,ENC_TABLE_SUBTROPICAL,ENC_TABLE_WOODS,ENC_TABLE_WOODS,ENC_TABLE_HAUNTED};
    int table;

    if (climate < CLIMATE_DESERT || climate > CLIMATE_HAUNTED) { return -1; }
    table = base[climate-CLIMATE_DESERT];
    if (climate == CLIMATE_SWAMP || settlement) { return table; }
    return table + (night ? ENC_TABLE_NIGHT : ENC_TABLE_DAY);
}

static int outdoor_spawn(const Camera *camera, int kind, int settlement)
{
    OutdoorActor *actor;
    Entity entity, *live;
    SpawnSpot spot;
    Body *player;
    int i, archive, foe, rate;

    for (i = 0; i < ENCOUNTERS && g_outdoor_actor[i].id; i++) { }
    if (i == ENCOUNTERS) { return 0; }
    actor = &g_outdoor_actor[i];
    archive = archive_for(kind);
    if (!g_outdoors->art(kind,archive,&entity,&rate)) { return 0; }

    player = move_body(); mem_zero(&spot,sizeof(spot));
    spot.x = player->feet[0]; spot.y = player->feet[1]; spot.z = player->feet[2];
    spot.radius = FOE_BODY_RADIUS; spot.height = entity.pose[ENTITY_WALK].h[0];
    if (spot.height < FOE_BODY_HEIGHT_MIN) { spot.height = FOE_BODY_HEIGHT_MIN; }
    if (spot.height > FOE_BODY_HEIGHT_MAX) { spot.height = FOE_BODY_HEIGHT_MAX; }
    spot.flags = (foe_flags(kind) & FOE_FLAGS_NO_FLOOR) ? 0 : SPAWN_DRY;
    if (settlement) {
        spot.inner = ENC_HIDDEN_INNER; spot.outer = ENC_HIDDEN_OUTER; spot.climb = ENC_HIDDEN_CLIMB;
        spot.flags |= SPAWN_UNSEEN;
    } else {
        spot.x += ENC_AHEAD*df_sin(camera->yaw); spot.z += ENC_AHEAD*df_cos(camera->yaw);
        spot.outer = ENC_AHEAD_RING; spot.climb = ENC_AHEAD_CLIMB;
    }
    if (spawn_find(g_scene,camera,&spot,&entity.x,&entity.y,&entity.z)) {
        entity.live = 1; entity.flags |= ENT_MOVES;
        actor->id = spawn_add(g_scene,&entity,0);
        live = spawn_get(actor->id);
        if (live) {
            foe = dungeon_add(live,kind,0,rate);
            if (foe >= 0) {
                actor->foe = foe;
                if (ENEMY_IS_CLASS_ONLY(kind)) { dungeon_variant(foe,archive-ENEMY_ARCHIVE_CLASS); }
                dungeon_loaded(foe);
                return 1;
            }
            spawn_remove(actor->id);
            actor->id = 0;
        }
    }
    if (g_outdoors->release_art) { g_outdoors->release_art(&entity); }
    return 0;
}

int encounter_minute(const Scene *scene, const Camera *camera, df_u32 minute, int activity)
{
    int climate, night, settlement, table, kind, tries;

    if (!g_scene || scene != g_scene || !camera || activity == ENCOUNTER_LOITER || minute%ENC_ROLL_MINUTES) { return 0; }
    if (!g_outdoors) {
        return g_rest_encounters && activity == ENCOUNTER_REST && random_range(0,ENC_CHANCE_DAY) == 0 && encounter_try(camera);
    }
    if (!g_outdoors->site || !g_outdoors->art) { return 0; }
    climate = g_outdoors->site(camera,&settlement);
    minute %= MINUTES_PER_DAY;
    night = minute < DAWN_MINUTE || minute > DUSK_MINUTE;
    if (climate == CLIMATE_OCEAN || (settlement && !night)) { return 0; }
    table = outdoor_table(climate,settlement,night);
    if (table < 0 || random_range(0,night ? ENC_CHANCE_NIGHT : ENC_CHANCE_DAY) != 0) { return 0; }
    for (tries = 0; tries < ENC_DAY_REROLLS; tries++) {
        kind = encounter_pick(table,character_state()->level);
        if (night || (kind != ENEMY_GHOST && kind != ENEMY_WRAITH)) { return outdoor_spawn(camera,kind,settlement); }
    }
    return 0;
}

/* Before the place is left or a save is written: every kept creature into the keep table (rows made 2, filed under the place), which a save holds. */
void encounter_capture(void)
{
    if (g_scene && g_art) { ent_keep_now((Scene *)g_scene,0,0,0,0); }
}
