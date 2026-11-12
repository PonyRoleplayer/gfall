#include "check_ogsurvival.h"
#include "check_util.h"
#include "game/dagger/play/survival.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/dungeon.h"
#include "game/dagger/play/save.h"
#include "engine/core/mem.h"
#include "engine/scene/walker.h"

static OgState g_before;
static Scene g_scene;
static Instance g_model[2];
static Mesh g_mesh[2];
static Tri g_tri[4];
static Water g_water;

static void fixture(void)
{
    static const float point[4][3][3] = {
        {{-1000,0,-1000},{1000,0,-1000},{1000,0,1000}},
        {{-1000,0,-1000},{1000,0,1000},{-1000,0,1000}},
        {{-100,0,30},{100,0,30},{100,200,30}},
        {{-100,0,30},{100,200,30},{-100,200,30}}
    };
    int i, k;
    mem_zero(&g_scene,sizeof(g_scene)); mem_zero(g_model,sizeof(g_model));
    mem_zero(g_mesh,sizeof(g_mesh)); mem_zero(g_tri,sizeof(g_tri));
    for (i = 0; i < 4; i++) {
        for (k = 0; k < 3; k++) { g_tri[i].x[k] = point[i][k][0]; g_tri[i].y[k] = point[i][k][1]; g_tri[i].z[k] = point[i][k][2]; }
    }
    for (i = 0; i < 2; i++) {
        g_mesh[i].tris = g_tri+i*2; g_mesh[i].ntris = 2; g_model[i].mesh = &g_mesh[i];
        g_model[i].m[0] = g_model[i].m[4] = g_model[i].m[8] = 1;
    }
    g_mesh[0].ex = g_mesh[0].ez = 1000;
    g_mesh[1].ex = g_mesh[1].ey = g_mesh[1].cy = 100; g_mesh[1].cz = 30;
    g_scene.models = g_model; g_scene.nmodels = 1;
    mem_zero(&g_water,sizeof(g_water));
    g_water.x0 = g_water.z0 = -1000; g_water.x1 = g_water.z1 = 1000; g_water.y = 300;
    og_move_init(); og_move_place(0,WALK_EYE,0,0);
    mem_zero(og_survival(),sizeof(OgSurvival)); og_dungeon_close();
}

static void movement(void)
{
    OgMoveIn move;
    OgMotion motion;
    unsigned char held[OG_ACTS], pressed[OG_ACTS];
    float height;
    int i, wall;
    mem_zero(&move,sizeof(move)); mem_zero(held,sizeof(held)); mem_zero(pressed,sizeof(pressed));
    move.held = held; move.pressed = pressed; move.dt = 1.0f/60; move.region = 1;
    for (i = 0; i < 10; i++) { og_move_frame(&g_scene,&move); }
    check_expect(!og_move_climbing() && !og_move_body()->hit_wall,"a short ordinary walking frame cannot start climbing");
    g_scene.nmodels = 2; g_scene.version++;
    og_move_place(0,WALK_EYE,0,0); wall = 0;
    for (i = 0; i < 100; i++) {
        og_move_frame(&g_scene,&move);
        if (og_move_body()->hit_wall && og_move_body()->wall_normal[2] < -0.9f) { wall = 1; }
    }
    check_expect(wall && og_move_climbing() && og_move_body()->feet[1] > 1,
                 "the actual wall normal and original hold delay start a collision-constrained climb");
    og_state_capture_motion(); height = og_move_body()->feet[1];
    check_expect(og_state_capture_world() && og_save_write(6) && og_save_read(6),"save and reload an active climb");
    og_move_restore(&og_state()->motion);
    check_expect(og_move_climbing() && og_move_body()->feet[1] == height,"a resumed climb retains its physical height and mode");
    og_move_capture(&motion); motion.yaw = 1024; og_move_restore(&motion); move.region = 4;
    og_move_frame(&g_scene,&move);
    check_expect(!og_move_climbing(),"turning away from the wall ends a climb");
    g_scene.nmodels = 1; g_scene.version++; g_scene.water = &g_water; g_scene.nwater = 1;
    og_move_place(0,WALK_EYE,0,0); held[OG_FLOAT_UP] = 1;
    for (i = 0; i < 60; i++) { og_move_frame(&g_scene,&move); }
    check_expect(og_move_body()->feet[1] > 78 && og_move_body()->feet[1] < 82,
                 "swimming float-up moves at the original 80 units per second");
    held[OG_FLOAT_UP] = 0; held[OG_FLOAT_DOWN] = 1;
    for (i = 0; i < 60; i++) { og_move_frame(&g_scene,&move); }
    check_expect(og_move_body()->feet[1] >= -0.1f && og_move_body()->feet[1] < 2,
                 "swimming downward stops on the floor rather than snapping to another storey");
    held[OG_FLOAT_DOWN] = 0; og_move_place(0,100+WALK_EYE,0,0); og_move_set_weight(251);
    og_move_frame(&g_scene,&move);
    check_expect(og_move_body()->feet[1] < 100,"a heavily laden swimmer sinks");
}

static void breath(void)
{
    int i;
    g_scene.water = &g_water; g_scene.nwater = 1;
    og_move_place(0,WALK_EYE,0,0); og_character()->attribute[OG_END] = 2;
    og_character()->health = og_character()->max_health; og_character()->race = 0;
    mem_zero(og_survival(),sizeof(OgSurvival));
    og_survival_step(&g_scene,0.05f);
    check_expect(og_survival()->breath == 1,"underwater breath begins at half Endurance");
    check_expect(og_state_capture_world() && og_save_write(6) && og_save_read(6) && og_survival()->breath == 1,
                 "remaining breath and its clock phase survive save/load");
    for (i = 0; i < 24; i++) { og_survival_step(&g_scene,0.05f); }
    check_expect(!og_character()->health,"running out of breath causes drowning");
    og_character()->health = og_character()->max_health; og_character()->race = 7;
    mem_zero(og_survival(),sizeof(OgSurvival));
    for (i = 0; i < 24; i++) { og_survival_step(&g_scene,0.05f); }
    check_expect(og_character()->health && og_survival()->breath == 1,"Argonians skip the original alternating breath decrement");
    og_god_set(1);
    for (i = 0; i < 200; i++) { og_survival_step(&g_scene,0.05f); }
    check_expect(og_character()->health && og_survival()->breath == 1,"god mode cannot drown");
    og_god_set(0); g_scene.nwater = 0; og_survival_step(&g_scene,0.05f);
    check_expect(!og_survival()->breath,"surfacing clears the breath bar and resets its clock");
}

static void fatigue(void)
{
    int fatigue;
    df_u32 minute;
    og_move_init(); og_move_place(0,WALK_EYE,0,0); g_scene.nwater = 0;
    og_character()->fatigue = fatigue = 2000;
    og_survival_minutes(1,0,0);
    check_expect(og_character()->fatigue == fatigue-11,"ordinary game minutes spend the original 11 fatigue");
    og_survival_minutes(1,1,0); og_survival_minutes(1,0,1);
    check_expect(og_character()->fatigue == fatigue-11-88-22,"running and climbing use the original minute costs");
    og_god_set(1); fatigue = og_character()->fatigue; og_fatigue_spend(11); og_survival_minutes(1,1,0);
    check_expect(og_character()->fatigue == fatigue,"god mode protects both action and minute fatigue costs");
    og_god_set(0); og_character()->fatigue = 1; minute = og_state()->minutes;
    og_survival_minutes(1,0,0);
    check_expect(og_character()->fatigue > 0 && og_state()->minutes == minute+60,
                 "exhaustion without enemies passes an hour and applies original rest recovery");
}

void check_ogsurvival(void)
{
    OgMotion before;
    g_before = *og_state(); og_move_capture(&before);
    fixture(); movement(); breath(); fatigue();
    og_god_set(0); og_state_restore(&g_before); og_state_restore_world(); og_move_restore(&before);
}
