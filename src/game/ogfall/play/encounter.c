#include "game/ogfall/play/encounter.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/dungeon.h"
#include "game/ogfall/tables/combatdefs.h"
#include "engine/scene/keep.h"
#include "engine/scene/spawn.h"
#include "engine/scene/ent.h"
#include "engine/core/mem.h"

static const Scene *g_scene;
static int (*g_art)(int, int, Entity *, int *);

int og_encounter_pick(int type, int level)
{
    int roll, low, high;
    roll = og_random_range(1,100);
    if (roll <= 80) { low = level-3; high = level+3; }
    else if (roll <= 95) { low = 0; high = level+1; }
    else { low = 0; high = level > 5 ? 19 : level+2; }
    if (low < 0) { low = 0; high = 5; }
    if (high > 19) { low = 14; high = 19; }
    return og_foe_encounter(type,og_random_range(low,high));
}

void og_encounter_prepare(df_u32 place, int type)
{
    OgEncounterState *state;
    int i, kind, saved_seed;
    state = og_encounter_state();
    if (state->place == place) { return; }
    state->place = place;
    saved_seed = og_random_range(0,32767);
    og_random_seed(place & 65535u);
    /* dungeon_roll_monster_tables rolls the whole land table. Rest takes
     * slots 1..6, not six fresh table draws on every attempted interruption. */
    for (i = 0; i < 256; i++) {
        kind = og_encounter_pick(type,og_character()->level);
        if (i >= 1 && i <= 6) { state->choices[i-1] = kind; }
    }
    /* Original saves rand()'s result, not the generator's internal state.
     * Its unused water table does not affect the restored gameplay stream. */
    og_random_seed((df_u32)saved_seed);
}

/* What an entity of the dungeon the game made is, packed in its owner: its kind, its picture archive and its frame rate (docs/entities.md, Kept: a row is the whole entity, its look rebuilt from the
 * key and the owner by the revive hook). */
static df_u32 owner_of(int kind, int archive, int rate)
{
    return (df_u32)(kind & 255) | ((df_u32)(archive & 4095) << 8) | ((df_u32)(rate & 4095) << 20);
}

/* How many of the place's kept creatures stand (the original's room for eight at once, now only a cap on them). */
static int kept_here(void)
{
    Entity *pool;
    int n, i, count;

    pool = ent_pool(g_scene, &n);
    count = 0;
    for (i = 0; pool && i < n; i++) {
        if (pool[i].key.thing && (pool[i].flags & ENT_KEPT) && pool[i].key.place == og_encounter_state()->place) { count++; }
    }
    return count;
}

/* A creature of the place a kept row names, made again: its pictures from the game's art, its key and owner as they were. The engine puts it where it stood, as it was. */
static int revive(const Keep *row, Entity *out)
{
    int kind, archive, rate;

    if (!g_scene || !g_art || !og_encounter_state()->place || row->key.place != og_encounter_state()->place || !(row->key.thing & 0x80000000u)) { return 0; }
    kind = (int)(row->owner & 255u); archive = (int)((row->owner >> 8) & 4095u); rate = (int)((row->owner >> 20) & 4095u);
    if (!g_art(kind,archive,out,&rate)) { return 0; }
    out->key = row->key; out->live = 1; out->homed = 0;
    out->flags |= ENT_KEPT | ENT_MOVES; out->residence = ENT_HOME_PLACE; out->owner = row->owner;
    return 1;
}

/* A kept creature of the place that has no mind yet (just revived, or just made): it joins the dungeon's combat. */
static void adopt(Entity *e)
{
    int kind, archive, rate, foe;

    kind = (int)(e->owner & 255u); archive = (int)((e->owner >> 8) & 4095u); rate = (int)((e->owner >> 20) & 4095u);
    foe = og_dungeon_add(e,kind,0,rate);
    if (foe < 0) { ent_remove(ent_id(e)); return; }
    if (kind >= 128 && kind < 146) { og_dungeon_variant(foe,archive-432); }
    og_dungeon_loaded(foe);
}

static void adopt_all(void)
{
    Entity *pool;
    int n, i;

    pool = ent_pool(g_scene,&n);
    for (i = 0; pool && i < n; i++) {
        if (pool[i].key.thing && (pool[i].flags & ENT_KEPT) && pool[i].key.place == og_encounter_state()->place && !og_dungeon_has(&pool[i])) { adopt(&pool[i]); }
    }
}

void og_encounter_close(void)
{
    Entity *pool;
    int n, i;

    pool = g_scene ? ent_pool(g_scene,&n) : 0;
    for (i = 0; pool && i < n; i++) {
        if (pool[i].key.thing && (pool[i].flags & ENT_KEPT)) { ent_remove(ent_id(&pool[i])); }
    }
    g_scene = 0; g_art = 0;
}

int og_encounter_open(const Scene *scene, int (*art)(int, int, Entity *, int *))
{
    og_encounter_close(); g_scene = scene; g_art = art;
    if (scene) {
        ent_set_revive_hook(revive);
        ent_place(scene);
        ent_revive_place(scene,0,0,0);
        adopt_all();
    }
    return 1;
}

/* A creature put in the dungeon by something other than a rest's table (a quest's foe) or by it: near the player and unseen, or (at != 0) on the floor near a point, where a quest's marker is. It is a kept
 * entity of the place (ENT_KEPT, its key and owner say what it is), a mind joins it to the dungeon's combat. 0 when there is no room or no floor there yet (the chunk is not loaded): the caller tries again. */
static int place_actor(const Camera *camera, int kind, df_u32 thing, const float *at, int rest)
{
    Entity entity, *live;
    SpawnSpot want;
    Body *player;
    int archive, rate;
    float x, y, z;

    if (!g_scene || !g_art || !camera || kept_here() >= OG_ENCOUNTERS) { return 0; }
    if (rest && og_encounter_state()->serial >= 0xffffffu) { return 0; }
    archive = kind < 43 ? 255+kind : kind == 146 ? 399 : 432+og_foe_texture_kind(kind)+og_random_range(0,1);
    if (!g_art(kind,archive,&entity,&rate)) { return 0; }
    player = og_move_body(); mem_zero(&want,sizeof(want));
    want.x = at ? at[0] : player->feet[0]; want.y = at ? at[1] : player->feet[1]; want.z = at ? at[2] : player->feet[2];
    if (at) { want.inner = 0; want.outer = 192; want.climb = 256; } else { want.inner = 384; want.outer = 768; want.climb = 384; }
    want.radius = 12; want.height = entity.pose[ENTITY_WALK].h[0];
    if (want.height < 24) { want.height = 24; }
    if (want.height > 128) { want.height = 128; }
    want.flags = (at ? 0 : SPAWN_UNSEEN) | ((og_foe_flags(kind) & 65) ? 0 : SPAWN_DRY);
    if (!spawn_find(g_scene,camera,&want,&x,&y,&z)) { return 0; }
    if (rest) { thing = 0x80000000u | ++og_encounter_state()->serial; }
    entity.key.place = og_encounter_state()->place; entity.key.thing = thing;
    entity.x = x; entity.y = y; entity.z = z;
    entity.live = 1; entity.homed = 0;
    entity.flags |= ENT_KEPT | ENT_MOVES; entity.residence = ENT_HOME_PLACE; entity.owner = owner_of(kind,archive,rate);
    keep_restore((Scene *)g_scene,0,0,&entity,1,0,0,0);
    live = spawn_get(spawn_add(g_scene,&entity,0));
    if (!live) { return 0; }
    adopt(live);
    return 1;
}

int og_encounter_try(const Camera *camera)
{
    if (!g_scene || !g_art || !camera) { return 0; }
    return place_actor(camera, og_encounter_state()->choices[og_random_range(0,5)], 0, 0, 1);
}

int og_encounter_place(const Camera *camera, int kind, df_u32 thing) { return place_actor(camera, kind, thing, 0, 0); }

int og_encounter_place_at(const Camera *camera, int kind, df_u32 thing, float x, float y, float z)
{
    float at[3];

    at[0] = x; at[1] = y; at[2] = z;
    return place_actor(camera, kind, thing, at, 0);
}

int og_encounter_minute(const Scene *scene, const Camera *camera, df_u32 minute, int loitering)
{
    return g_scene && scene == g_scene && !loitering && !(minute%12) && og_random_range(0,35) == 0 && og_encounter_try(camera);
}

/* Before the place is left or a save is written: every kept creature into the keep table (rows made 2, filed under the place), which a save holds. */
void og_encounter_capture(void)
{
    if (g_scene) { ent_keep_now((Scene *)g_scene,0,0,0,0); }
}
