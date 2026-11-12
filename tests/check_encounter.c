#include "check_encounter.h"
#include "check_util.h"
#include "game/dagger/play/encounter.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/dungeon.h"
#include "game/dagger/play/save.h"
#include "game/dagger/play/cheats.h"
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
    check_expect(encounter_open(&g_scene,art),"open a dungeon encounter pool");
    random = game_state()->random;
    check_expect(!encounter_minute(&g_scene,&camera,12,1) &&
                 !encounter_minute(&g_scene,&camera,13,0) && random == game_state()->random,
                 "loitering and minutes outside the twelve-minute boundary consume no encounter rolls");
    g_scene.nmodels = 0; g_scene.version++;
    check_expect(!encounter_try(&camera) && !spawn_count() && !encounter_state()->serial,
                 "a failed floor search leaves no persistent or live encounter");
    g_scene.nmodels = 1; g_scene.version++;
    check_expect(encounter_try(&camera) && spawn_count() == 1 && dungeon_count() == 1,
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
        check_expect(encounter_open(&g_scene,art) && spawn_count() == 1 && dungeon_count() == 1,
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

        kept = -1;
        for (r = 0; r < game_state()->world_count; r++) { if (game_state()->world[r].made) { kept = r; break; } }
        check_expect(kept >= 0, "a rest encounter is a kept row of the saved session");
        if (kept >= 0) {
            g_bad = *game_state(); g_bad.world[kept].owner = (g_bad.world[kept].owner & ~(4095u << 8)) | (483u << 8);
            check_expect(!state_valid(&g_bad),"save validation refuses a kept creature with a different archive");
            g_bad = *game_state(); g_bad.world[kept].flags &= ~(unsigned)ENT_KEPT;
            check_expect(!state_valid(&g_bad),"save validation refuses a made row that is not flagged kept");
            g_bad = *game_state(); g_bad.world[kept].owner = (g_bad.world[kept].owner & ~255u) | 127u;
            check_expect(!state_valid(&g_bad),"save validation refuses a kept creature of no kind");
        }
    }
    encounter_close(); dungeon_close(); spawn_clear();
    state_restore(&g_before); state_restore_world(); move_restore(&motion);
}
