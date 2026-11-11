#include "game/ogfall/play/encounter.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/dungeon.h"
#include "game/ogfall/tables/combatdefs.h"
#include "engine/scene/keep.h"
#include "engine/scene/spawn.h"
#include "engine/core/mem.h"

static const Scene *g_scene;
static int (*g_art)(int, int, Entity *, int *);
static SpawnId g_spawn[OG_ENCOUNTERS];

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

static int create(int index)
{
    const OgEncounter *saved;
    Entity entity;
    int rate, foe;

    saved = &og_encounter_state()->actor[index];
    if (!g_art || !g_art(saved->kind,saved->archive,&entity,&rate)) { return 0; }
    entity.key = saved->key; entity.x = saved->x; entity.y = saved->y; entity.z = saved->z;
    entity.live = 1; entity.homed = 0;
    keep_restore((Scene *)g_scene,0,0,&entity,1,0,0,0);
    g_spawn[index] = spawn_add(g_scene,&entity,0);
    if (!g_spawn[index]) { return 0; }
    foe = og_dungeon_add(spawn_get(g_spawn[index]),saved->kind,0,rate);
    if (foe < 0) { spawn_remove(g_spawn[index]); g_spawn[index] = 0; return 0; }
    if (saved->kind >= 128 && saved->kind < 146) { og_dungeon_variant(foe,saved->archive-432); }
    og_dungeon_loaded(foe);
    return 1;
}

void og_encounter_close(void)
{
    int i;
    for (i = 0; i < OG_ENCOUNTERS; i++) { if (g_spawn[i]) { spawn_remove(g_spawn[i]); } }
    mem_zero(g_spawn,sizeof(g_spawn)); g_scene = 0; g_art = 0;
}

int og_encounter_open(const Scene *scene, int (*art)(int, int, Entity *, int *))
{
    int i;
    og_encounter_close(); g_scene = scene; g_art = art;
    for (i = 0; scene && i < OG_ENCOUNTERS; i++) {
        if (og_encounter_state()->actor[i].key.place == og_encounter_state()->place &&
            og_encounter_state()->actor[i].key.thing && !create(i)) { return 0; }
    }
    return 1;
}

int og_encounter_try(const Camera *camera)
{
    OgEncounter *saved;
    Entity entity;
    SpawnSpot want;
    Body *player;
    int slot, kind, archive, rate;
    float x, y, z;

    if (!g_scene || !g_art || !camera) { return 0; }
    for (slot = 0; slot < OG_ENCOUNTERS; slot++) { if (!og_encounter_state()->actor[slot].key.thing) { break; } }
    if (slot == OG_ENCOUNTERS || og_encounter_state()->serial >= 0xffffffu) { return 0; }
    kind = og_encounter_state()->choices[og_random_range(0,5)];
    archive = kind < 43 ? 255+kind : kind == 146 ? 399 : 432+og_foe_texture_kind(kind)+og_random_range(0,1);
    if (!g_art(kind,archive,&entity,&rate)) { return 0; }
    player = og_move_body(); mem_zero(&want,sizeof(want));
    want.x = player->feet[0]; want.y = player->feet[1]; want.z = player->feet[2];
    want.inner = 384; want.outer = 768; want.climb = 384;
    want.radius = 12; want.height = entity.pose[ENTITY_WALK].h[0];
    if (want.height < 24) { want.height = 24; }
    if (want.height > 128) { want.height = 128; }
    want.flags = SPAWN_UNSEEN | ((og_foe_flags(kind) & 65) ? 0 : SPAWN_DRY);
    if (!spawn_find(g_scene,camera,&want,&x,&y,&z)) { return 0; }
    saved = &og_encounter_state()->actor[slot];
    saved->key.place = og_encounter_state()->place;
    saved->key.thing = 0x80000000u | ++og_encounter_state()->serial;
    saved->kind = kind; saved->archive = archive; saved->x = x; saved->y = y; saved->z = z;
    if (!create(slot)) { mem_zero(saved,sizeof(*saved)); return 0; }
    return 1;
}

int og_encounter_place(const Camera *camera, int kind, df_u32 thing)
{
    OgEncounter *saved;
    Entity entity;
    SpawnSpot want;
    Body *player;
    int slot, archive, rate;
    float x, y, z;

    if (!g_scene || !g_art || !camera) { return 0; }
    for (slot = 0; slot < OG_ENCOUNTERS; slot++) { if (!og_encounter_state()->actor[slot].key.thing) { break; } }
    if (slot == OG_ENCOUNTERS) { return 0; }
    archive = kind < 43 ? 255+kind : kind == 146 ? 399 : 432+og_foe_texture_kind(kind)+og_random_range(0,1);
    if (!g_art(kind,archive,&entity,&rate)) { return 0; }
    player = og_move_body(); mem_zero(&want,sizeof(want));
    want.x = player->feet[0]; want.y = player->feet[1]; want.z = player->feet[2];
    want.inner = 384; want.outer = 768; want.climb = 384;
    want.radius = 12; want.height = entity.pose[ENTITY_WALK].h[0];
    if (want.height < 24) { want.height = 24; }
    if (want.height > 128) { want.height = 128; }
    want.flags = SPAWN_UNSEEN | ((og_foe_flags(kind) & 65) ? 0 : SPAWN_DRY);
    if (!spawn_find(g_scene,camera,&want,&x,&y,&z)) { return 0; }
    saved = &og_encounter_state()->actor[slot];
    saved->key.place = og_encounter_state()->place;
    saved->key.thing = thing;
    saved->kind = kind; saved->archive = archive; saved->x = x; saved->y = y; saved->z = z;
    if (!create(slot)) { mem_zero(saved,sizeof(*saved)); return 0; }
    return 1;
}

int og_encounter_minute(const Scene *scene, const Camera *camera, df_u32 minute, int loitering)
{
    return g_scene && scene == g_scene && !loitering && !(minute%12) && og_random_range(0,35) == 0 && og_encounter_try(camera);
}

void og_encounter_capture(void)
{
    Entity *entity;
    int i;
    for (i = 0; g_scene && i < OG_ENCOUNTERS; i++) {
        entity = spawn_get(g_spawn[i]);
        if (entity) { keep_save((Scene *)g_scene,0,0,entity,1,0,0,0); }
    }
}
