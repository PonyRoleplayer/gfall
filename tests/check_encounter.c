#include "check_encounter.h"
#include "check_util.h"
#include "game/dagger/play/combat/encounter.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/save/save.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/formats/climate.h"
#include "engine/core/mem.h"
#include "engine/scene/spawn.h"
#include "engine/scene/walker.h"

static GameState g_before, g_bad;
static Scene g_scene;
static Mesh g_mesh;
static Instance g_model;
static Tri g_tri[2];

static int art(int kind, int archive, Entity *entity, int *rate)
{
    (void)archive;
    if (kind != 0) { return 0; }
    entity_init(entity); *rate = 71;
    entity->pose[ENTITY_WALK].w[0] = 40; entity->pose[ENTITY_WALK].h[0] = 60;
    entity->pose[ENTITY_DEAD].w[0] = 40; entity->pose[ENTITY_DEAD].h[0] = 10;
    return 1;
}

static void floor_fixture(void)
{
    static const float point[2][3][3] = {
        {{-2000,0,-2000},{2000,0,-2000},{2000,0,2000}},
        {{-2000,0,-2000},{2000,0,2000},{-2000,0,2000}}
    };
    int i, k;
    mem_zero(&g_scene,sizeof(g_scene)); mem_zero(&g_mesh,sizeof(g_mesh));
    mem_zero(&g_model,sizeof(g_model)); mem_zero(g_tri,sizeof(g_tri));
    for (i = 0; i < 2; i++) {
        for (k = 0; k < 3; k++) {
            g_tri[i].x[k] = point[i][k][0]; g_tri[i].y[k] = point[i][k][1]; g_tri[i].z[k] = point[i][k][2];
        }
    }
    g_mesh.tris = g_tri; g_mesh.ntris = 2; g_mesh.ex = g_mesh.ez = 2000;
    g_model.mesh = &g_mesh; g_model.m[0] = g_model.m[4] = g_model.m[8] = 1;
    g_scene.models = &g_model; g_scene.nmodels = 1; g_scene.version = 20000;
}

static int g_climate, g_settlement, g_loaded, g_released;

static int outdoor_site(const Camera *camera, int *settlement)
{
    (void)camera;
    *settlement = g_settlement;
    return g_climate;
}

static int outdoor_art(int kind, int archive, Entity *entity, int *rate)
{
    (void)kind;
    g_loaded++;
    return art(0,archive,entity,rate);
}

static void outdoor_release(const Entity *entity)
{
    (void)entity;
    g_released++;
}

static void check_outdoor_encounters(void)
{
    static const EncounterOutdoors world = {outdoor_site,outdoor_art,outdoor_release};
    Camera camera, far;
    Entity *actor;
    int i;

    dungeon_open_street(&g_scene);
    encounter_open_outdoors(&g_scene,&world);
    camera_set(&camera,0,WALK_EYE,0,0,0);
    move_place(0,WALK_EYE,0,0);
    g_climate = CLIMATE_DESERT; g_settlement = 1; g_loaded = g_released = 0;
    random_seed(0);
    check_expect(!encounter_minute(&g_scene,&camera,720,ENCOUNTER_WALK) && !game_state()->random,
                 "settlements skip daytime rolls while walking");
    check_expect(!encounter_minute(&g_scene,&camera,0,ENCOUNTER_LOITER) && !game_state()->random,
                 "loitering outdoors consumes no encounter roll");
    g_climate = CLIMATE_OCEAN;
    check_expect(!encounter_minute(&g_scene,&camera,0,ENCOUNTER_REST) && !game_state()->random,
                 "the ocean consumes no encounter roll");

    g_climate = CLIMATE_DESERT;
    check_expect(encounter_minute(&g_scene,&camera,0,ENCOUNTER_REST),
                 "a successful nighttime settlement roll can interrupt rest");
    actor = dungeon_entity(0);
    check_expect(actor && encounter_outdoor_actor(actor) && actor->z < 0,
                 "a settlement encounter appears unseen rather than in front of the player");

    camera_set(&far,4000,WALK_EYE,0,0,0);
    encounter_update(&g_scene,&far);
    check_expect(!dungeon_count() && g_released == 1,"moving away retires a field actor and releases its art");
    g_settlement = 0;
    for (i = 0; i < 10; i++) {
        random_seed(0);
        if (!encounter_minute(&g_scene,&camera,720,ENCOUNTER_WALK)) { break; }
        actor = dungeon_entity(0);
        if (!actor || actor->z < 896 || actor->z > 1152 || dungeon_count() != 1) { break; }
        encounter_update(&g_scene,&far);
    }
    check_expect(i == 10 && g_loaded == g_released && !dungeon_count(),
                 "wilderness encounters spawn ahead and reuse released actor slots");
    encounter_close();
}

void check_encounter(void)
{
    Camera camera;
    MotionState motion;
    StrikeInfo strike;
    Entity *entity;
    KeepKey key;
    df_u32 random;
    float x, z;
    int i;

    g_before = *game_state(); move_capture(&motion); floor_fixture();
    encounter_close(); spawn_clear(); keep_clear();
    mem_zero(encounter_state(),sizeof(EncounterState));
    encounter_prepare(0x40000001u,0);
    random = game_state()->random; encounter_prepare(0x40000001u,0);
    check_expect(random == game_state()->random,"reopening the same dungeon retains its rolled encounter table");
    for (i = 0; i < 6; i++) { encounter_state()->choices[i] = 0; }
    move_init(); move_place(0,WALK_EYE,0,0); camera_set(&camera,0,WALK_EYE,0,0,0);
    dungeon_open(&g_scene,0,0,0);
    check_expect(encounter_open(&g_scene,art,1),"open a dungeon encounter pool");
    random = game_state()->random;
    check_expect(!encounter_minute(&g_scene,&camera,12,ENCOUNTER_LOITER) &&
                 !encounter_minute(&g_scene,&camera,12,ENCOUNTER_WALK) &&
                 !encounter_minute(&g_scene,&camera,13,ENCOUNTER_REST) && random == game_state()->random,
                 "loitering and minutes outside the twelve-minute boundary consume no encounter rolls");
    g_scene.nmodels = 0; g_scene.version++;
    check_expect(!encounter_try(&camera) && !spawn_count() && !encounter_state()->serial,
                 "a failed floor search leaves no persistent or live encounter");
    g_scene.nmodels = 1; g_scene.version++;
    random_seed(0);
    check_expect(encounter_minute(&g_scene,&camera,12,ENCOUNTER_REST) && spawn_count() == 1 && dungeon_count() == 1,
                 "a rest encounter joins the same dungeon combat and spawn pools");
    entity = dungeon_entity(0);
    if (entity) {
        key = entity->key; x = entity->x+8; z = entity->z; entity->x = x;
        check_expect(x*x+z*z >= 370*370 && x*x+z*z <= 780*780,
                     "the unseen rest enemy stands in the original 384..768-unit ring");
        move_place(x,WALK_EYE,z-30,0); camera_set(&camera,x,WALK_EYE,z-30,0,0);
        mem_zero(&strike,sizeof(strike)); strike.hand = 0; god_set(1);
        for (i = 0; i < 40 && !entity->dead; i++) { dungeon_strike(&g_scene,&camera,&strike); }
        god_set(0);
        check_expect(entity->dead,"an encounter can be struck and killed through normal combat");
        encounter_capture();
        check_expect(state_capture_world() && save_write(6) && save_read(6) && state_restore_world(),
                     "encounter identity, corpse, health and position pass through the save codec");
        encounter_close(); dungeon_open(&g_scene,0,0,0);
        random = game_state()->random;
        check_expect(encounter_open(&g_scene,art,1) && spawn_count() == 1 && dungeon_count() == 1,
                     "reloading recreates an encounter exactly once");
        entity = dungeon_entity(0);
        check_expect(entity && entity->key.thing == key.thing && entity->dead &&
                     entity->x == x && entity->z == z && !dungeon_health(0) && random == game_state()->random,
                     "a restored encounter keeps its corpse and position without rolling new health");
    }
    g_bad = *game_state(); g_bad.encounters.choices[0] = 127;
    check_expect(!state_valid(&g_bad),"save validation refuses an invalid encounter kind");
    /* A kept creature's row (the actor table's successor) is judged: its archive must be its kind's, and the same key twice is refused. */
    {
        int r, kept;

        Keep row;

        kept = -1;
        for (r = 0; r < keep_count(); r++) { if (keep_at(r)->made) { kept = r; break; } }
        check_expect(kept >= 0, "a rest encounter is a kept row of the saved session");
        if (kept >= 0) {
            row = *keep_at(kept);
            check_expect(state_keep_row_valid(&row), "a kept creature's own row is valid");
            row = *keep_at(kept); row.owner = (row.owner & ~(4095u << 8)) | (483u << 8);
            check_expect(!state_keep_row_valid(&row),"save validation refuses a kept creature with a different archive");
            row = *keep_at(kept); row.flags &= ~(unsigned)ENT_KEPT;
            check_expect(!state_keep_row_valid(&row),"save validation refuses a made row that is not flagged kept");
            row = *keep_at(kept); row.owner = (row.owner & ~255u) | 127u;
            check_expect(!state_keep_row_valid(&row),"save validation refuses a kept creature of no kind");
        }
    }
    check_expect(encounter_release_soul(&camera,0) && dungeon_count() == 2 && spawn_count() == 2 &&
                 dungeon_kind(1) == 0 && dungeon_health(1) > 0,
                 "the released soul joins the current dungeon as a living creature");
    if (entity && entity->dead) {
        df_u32 box;
        int original;

        original = spawn_count();
        for (i = original; i < ENCOUNTERS && encounter_try(&camera); i++) { }
        check_expect(i == ENCOUNTERS && !encounter_try(&camera),
                     "a full dungeon pool retains an unsearched corpse");
        box = loot_container(entity->key.place,entity->key.thing,entity->x,entity->y,entity->z,-1);
        check_expect(box && encounter_try(&camera) && spawn_count() == ENCOUNTERS &&
                     !loot_container_find(key.place,key.thing) && !keep_find(key),
                     "a looted dungeon corpse gives its slot to a new encounter");
    }
    encounter_close(); dungeon_close(); spawn_clear();
    check_outdoor_encounters();
    dungeon_close(); spawn_clear();
    state_restore(&g_before); state_restore_world(); move_restore(&motion);
}
