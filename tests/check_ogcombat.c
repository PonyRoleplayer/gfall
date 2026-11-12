#include "check_ogcombat.h"
#include "check_ogdoors.h"
#include "check_ogmagic.h"
#include "check_util.h"
#include "game/dagger/play/anim.h"
#include "game/dagger/play/ai.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/vitals.h"
#include "game/dagger/play/dungeon.h"
#include "game/dagger/play/encounter.h"
#include "game/dagger/play/hud.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/save.h"
#include "game/dagger/play/weapon.h"
#include "game/dagger/play/options.h"
#include "game/dagger/play/chargen.h"
#include "game/dagger/tables/itemdefs.h"
#include "engine/core/mem.h"
#include "engine/scene/walker.h"
#include "engine/scene/keep.h"
#include "engine/ui/pic.h"
#include "engine/ui/cmd.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "engine/sound/ambience.h"
#include "game/dagger/formats/texture.h"
#include "engine/assets/level.h"
#include "engine/assets/vfs.h"
#include "engine/platform/fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int seam_open_hold(float *x, float *y, float *z);
int seam_capture_world(void);
void seam_creatures(const Camera *camera);
void seam_close_hold(void);
int seam_creature_archive(int index);
Level *seam_level(void);
const df_u8 *seam_weapon_screen(const df_u8 **palette);
void check_ogfall_draw_reset(void);
int check_ogfall_draw_count(void);

static OgState g_before;
static Scene g_scene;
static Entity g_entity[2];
static Instance g_model[2];
static Mesh g_mesh[2];
static Tri g_tri[4];
static Camera g_cam;
static int g_arena_version;
static const int g_kind[2] = {0,0}, g_passive[2] = {1,1}, g_rate[2] = {71,71};

static void preview(const char *name)
{
    FILE *file;
    const df_u8 *pixels, *palette;
    char path[100];
    int i, opaque;
    check_ogfall_draw_reset(); og_weapon_draw();
    pixels = seam_weapon_screen(&palette);
    check_expect(pixels && check_ogfall_draw_count(),"the real CIF weapon frame fits beside the HUD on the fake Voodoo");
    if (!pixels) { return; }
    opaque = 0;
    snprintf(path,sizeof(path),"build/ogfall-weapon-%s.ppm",name);
    file = fopen(path,"wb"); if (!file) { return; }
    fprintf(file,"P6\n320 200\n255\n");
    for (i = 0; i < 64000; i++) { fwrite(palette+pixels[i]*3,1,3,file); opaque += pixels[i] != 0; }
    fclose(file);
    check_expect(opaque > 100,"weapon art contains decoded sprite pixels");
}

static void arena(int wall)
{
    int i, k;
    static const float points[4][3][3] = {
        {{-1000,0,-1000},{1000,0,-1000},{1000,0,1000}},
        {{-1000,0,-1000},{1000,0,1000},{-1000,0,1000}},
        {{-100,0,30},{100,0,30},{100,200,30}},
        {{-100,0,30},{100,200,30},{-100,200,30}}
    };
    mem_zero(&g_scene,sizeof(g_scene)); mem_zero(g_mesh,sizeof(g_mesh));
    mem_zero(g_model,sizeof(g_model)); mem_zero(g_tri,sizeof(g_tri));
    for (i = 0; i < 4; i++) {
        for (k = 0; k < 3; k++) {
            g_tri[i].x[k] = points[i][k][0]; g_tri[i].y[k] = points[i][k][1]; g_tri[i].z[k] = points[i][k][2];
        }
    }
    for (i = 0; i < 2; i++) {
        g_mesh[i].tris = &g_tri[i*2]; g_mesh[i].ntris = 2;
        g_model[i].mesh = &g_mesh[i]; g_model[i].m[0] = g_model[i].m[4] = g_model[i].m[8] = 1;
        entity_init(&g_entity[i]);
        g_entity[i].live = 1; g_entity[i].radius = 12;
        g_entity[i].z = i ? 600 : 60; g_entity[i].x = i ? 600 : 0;
        g_entity[i].key.place = 0x40000001u; g_entity[i].key.thing = 0x100u+i;
        g_entity[i].pose[ENTITY_WALK].slot[0] = 0;
        g_entity[i].pose[ENTITY_WALK].w[0] = 40; g_entity[i].pose[ENTITY_WALK].h[0] = 60;
    }
    g_mesh[0].ex = g_mesh[0].ez = 1000;
    g_mesh[1].ex = g_mesh[1].ey = g_mesh[1].cy = 100; g_mesh[1].cz = 30;
    g_scene.models = g_model; g_scene.nmodels = wall ? 2 : 1;
    g_scene.entities = g_entity; g_scene.nentities = 2; g_scene.version = ++g_arena_version;
    keep_clear(); og_dungeon_open(&g_scene,g_kind,g_passive,g_rate);
    for (i = 0; i < 2; i++) { og_dungeon_loaded(i); }
    og_move_init(); og_move_place(0,WALK_EYE,0,0);
    camera_set(&g_cam,0,60,0,0,0);
}

static void rules(void)
{
    OgFighter a, t;
    OgAnim anim;
    int i, strikes;
    mem_zero(&a,sizeof(a)); mem_zero(&t,sizeof(t));
    a.health = a.maximum = t.health = t.maximum = 100;
    a.level = 1; a.race = 0; t.race = 0; t.armor[0] = 30;
    for (i = 0; i < OG_ATTRS; i++) { a.attribute[i] = t.attribute[i] = 50; }
    check_expect(og_attack_chance(&a,&t,50,5,0,0,0,100) == 70,
                 "original hit chance: skill + armor + 40 - 50");
    a.skill[20] = 40;
    check_expect(og_attack_chance(&a,&t,50,5,0,0,0,100) == 60,
                 "byte-matched attack uses the attacker's Dodging");
    check_expect(og_attack_chance(&a,&t,1000,5,0,0,0,100) == 97 &&
                 og_attack_chance(&a,&t,-1000,5,0,0,0,100) == 3,"hit chance clamps to 3..97");
    check_expect(og_attack_damage(&a,&t,5,1,4,0,5) == 9,"a downward swing adds four damage");
    check_expect(og_swing_hit(og_weapon_sector(0,-100)) == 10 &&
                 og_swing_damage(og_weapon_sector(0,-100)) == -4 &&
                 og_swing_hit(og_weapon_sector(0,100)) == -10 &&
                 og_swing_damage(og_weapon_sector(0,100)) == 4,"up/down gestures preserve the original tradeoffs");
    check_expect(og_weapon_delay(50) == 195 && og_bow_delay(50) == 1500,"Speed controls the original melee and bow delays");
    check_expect(og_foe_def(0)->dice == 1 && og_foe_def(0)->hp_bonus == 8 &&
                 og_foe_def(0)->minimum[0] == 1 && og_foe_def(0)->maximum[0] == 4 &&
                 og_foe_def(2)->armor == -20,"monster data preserves d8 health, WORD damage pairs and signed armor");
    og_anim_start(&anim,og_foe_script(0),8); strikes = 0;
    for (i = 0; i < 20 && !anim.ended; i++) {
        if (og_anim_tick(&anim) & 1) { strikes++; check_expect(anim.frame == 3,"rat strike is attached to original ASCR frame 3"); }
    }
    check_expect(strikes == 1 && anim.ended,"rat attack script strikes once then returns to idle");
    og_anim_start(&anim,og_foe_script(9),8); strikes = 0;
    for (i = 0; i < 25 && !anim.ended; i++) { if (og_anim_tick(&anim) & 1) { strikes++; } }
    check_expect(strikes == 2,"werewolf ASCR retains both attack events");
}

static void ai_rules(void)
{
    static OgState before;
    OgMotion motion, before_motion;
    OgMind mind;
    Body body;
    Entity entity;
    float target[3];
    df_u32 uses, random;
    int i, blocked;

    before = *og_state(); og_move_capture(&before_motion); motion = before_motion;
    og_character()->skill[16] = 100;
    uses = og_state()->skill_uses[16];
    check_expect(!og_ai_stealth(0,1024,0) && og_state()->skill_uses[16] == uses+1,
                 "original stealth chance uses distance and trains once in an even game minute");
    random = og_state()->random;
    check_expect(og_ai_stealth(1,600,0) && og_state()->random == random &&
                 og_state()->skill_uses[16] == uses+1,"other creatures share the minute's stealth check");
    og_state_pass_minutes(1); random = og_state()->random;
    check_expect(!og_ai_stealth(0,600,0) && og_state()->random == random,
                 "quiet movement skips odd-minute stealth rolls");
    motion.vx = 200; motion.vz = 0; og_move_restore(&motion);
    check_expect(og_ai_stealth(0,600,1),"fast movement reveals an already encountered player");
    check_expect(og_ai_octant(1,2) == 0 && og_ai_octant(2,1) == 1 &&
                 og_ai_octant(1,-2) == 2 && og_ai_octant(2,-1) == 3 &&
                 og_ai_octant(-2,-1) == 4 && og_ai_octant(-1,-2) == 5 &&
                 og_ai_octant(-2,1) == 6 && og_ai_octant(-1,2) == 7,
                 "all original navigation octants preserve dominant-axis and quadrant selection");
    check_expect(og_ai_turn(2016,32,64) == 32 && og_ai_turn(0,512,64) == 64 &&
                 og_ai_angle(2040,8) == 16,"creatures turn gradually across the angle wrap");
    arena(1); g_scene.nentities = 0; g_scene.version = ++g_arena_version;
    mem_zero(&mind,sizeof(mind)); entity_init(&entity);
    entity.yaw = DF_TAU/2; mind.detected = 1;
    body_init(&body,12,60,12,40); body_place(&body,0,0,60);
    target[0] = 0; target[1] = 30; target[2] = -120;
    blocked = 1;
    for (i = 0; i < 120; i++) {
        og_ai_walk(&mind,&body,&entity,&g_scene,target,0,0,200,1.0f/60);
        if (body.feet[2] < 41 && df_absf(body.feet[0]) < 100) {
            if (blocked) { fprintf(stderr,"AI wall fixture: step %d at %.2f %.2f %.2f\n",i,body.feet[0],body.feet[1],body.feet[2]); }
            blocked = 0;
        }
    }
    check_expect(blocked,"wall-following never moves a creature through the blocking wall");
    check_expect(mind.direction == 2 || mind.direction == 4 || mind.direction == 8,
                 "a blocked creature retains a valid original navigation direction");
    og_state_restore(&before); og_state_restore_world(); og_move_restore(&before_motion);
}

static void swings(df_u32 sword)
{
    OgIn in;
    OgStrike strike;
    int i, hits, fatigue;
    og_state_equip(19,sword); og_state_equip(21,0); og_hud_reset(); og_hud_toggle_ready(); og_weapon_reset();
    mem_zero(&in,sizeof(in)); in.held[OG_SWING_BUTTON] = 1; in.dt = 0.01f;
    og_weapon_frame(&in,0,&strike);
    check_expect(strike.hand < 0 && og_weapon_view()->group == 0,"holding attack without a gesture does not swing");
    in.mouse_dy = 120; fatigue = og_character()->fatigue;
    og_weapon_frame(&in,0,&strike); preview("swing"); in.mouse_dy = 0; hits = 0;
    for (i = 0; i < 150; i++) {
        og_weapon_frame(&in,0,&strike); if (strike.hand >= 0) { hits++; }
    }
    check_expect(hits == 1 && og_character()->fatigue == fatigue-11,"a gesture strikes once at its hit frame and spends 11 fatigue");
    in.mouse_dx = 120; og_weapon_frame(&in,1,&strike);
    check_expect(og_weapon_view()->group == 0,"HUD-owned input cannot begin a swing");
    og_weapon_frame(&in,0,&strike); og_weapon_cancel(); in.mouse_dx = 0; hits = 0;
    for (i = 0; i < 100; i++) { og_weapon_frame(&in,0,&strike); hits += strike.hand >= 0; }
    check_expect(!hits,"cancelling an interrupted swing discards its pending hit");
    og_weapon_switch();
    check_expect(og_weapon_hand() == 1,"S switches to an empty left hand for unarmed combat");
    og_weapon_frame(&in,0,&strike); preview("fist");
    in.mouse_dx = -120; in.dt = 0.001f; og_weapon_frame(&in,0,&strike); in.mouse_dx = 0;
    hits = 0;
    for (i = 0; i < 780; i++) {
        og_weapon_frame(&in,0,&strike);
        if (strike.hand >= 0) { hits++; check_expect(i >= 777,"the unarmed kick waits for script frame 4"); }
    }
    check_expect(hits == 1,"the kick script strikes once");
    og_weapon_cancel();
    og_weapon_switch();
}

static void melee(df_u32 sword)
{
    OgStrike s;
    int before, i;
    mem_zero(&s,sizeof(s)); s.hand = 0; s.item = sword; s.damage = 4;
    arena(1); before = og_dungeon_health(0);
    for (i = 0; i < 10; i++) { og_dungeon_strike(&g_scene,&g_cam,&s); }
    check_expect(og_dungeon_health(0) == before,"melee cannot damage a creature through a wall");
    {
        float from[3] = {0,40,0}, to[3] = {0,40,100};
        int visible_models[2], count, visual;
        g_scene.solid_models = 1; g_scene.version++;
        check_expect(!scene_line_blocked(&g_scene,from,to,0),"visual models do not obstruct sight rays");
        count = scene_visible_models(&g_scene,&g_cam,visible_models,2); visual = 0;
        for (i = 0; i < count; i++) { if (visible_models[i] == 1) { visual = 1; } }
        check_expect(visual,"a nonsolid model remains in the renderer's visible model list");
        for (i = 0; i < 20 && !g_entity[0].dead; i++) { og_dungeon_strike(&g_scene,&g_cam,&s); }
        check_expect(g_entity[0].dead,"the collision ray also ignores a visual model");
    }
    arena(0); before = og_dungeon_health(1);
    for (i = 0; i < 30 && !g_entity[0].dead; i++) { og_dungeon_strike(&g_scene,&g_cam,&s); }
    check_expect(g_entity[0].dead && og_dungeon_health(0) == 0 && g_entity[0].act == ENTITY_DEAD && !g_entity[0].radius,
                 "lethal melee makes a nonsolid corpse");
    check_expect(og_dungeon_health(1) == before,"melee respects its 90-unit reach");
    /* Crouched, the eye is 32 above the floor: the blow's ray must start in the body, not 35 under the eye (in the floor). */
    {
        float standing_y;

        standing_y = g_cam.y;
        arena(0);
        g_cam.y = og_move_body()->feet[1] + WALK_EYE / 2;
        for (i = 0; i < 40 && !g_entity[0].dead; i++) { og_dungeon_strike(&g_scene,&g_cam,&s); }
        check_expect(g_entity[0].dead,"a crouched player's blows connect");
        g_cam.y = standing_y;
    }
    keep_save(&g_scene,0,0,g_entity,2,0,0,0);
    check_expect(keep_find(g_entity[0].key)->dead && keep_find(g_entity[0].key)->game[0] == 0xc1,
                 "health and corpse pose share the saved world record");
    before = og_dungeon_health(1);
    og_dungeon_close(); keep_restore(&g_scene,0,0,g_entity,2,0,0,0);
    og_dungeon_open(&g_scene,g_kind,g_passive,g_rate);
    check_expect(g_entity[0].dead && og_dungeon_health(0) == 0 && og_dungeon_health(1) == before,
                 "opening the dungeon again restores corpses and previously rolled health");
    check_expect(og_dungeon_container(0) != 0,"a dead creature has one persistent loot container");
}

static void archery(void)
{
    OgIn in;
    OgStrike strike;
    df_u32 bow, arrows;
    int i, hits, before;
    bow = og_item_create(og_state()->inventory_id,3,16,1);
    arrows = og_item_create(og_state()->inventory_id,3,18,0);
    og_object_edit(arrows)->count = 2; og_state_equip(19,bow); og_weapon_reset();
    mem_zero(&in,sizeof(in)); in.dt = 1.0f/60;
    for (i = 0; i < 10; i++) { og_weapon_frame(&in,0,&strike); }
    check_expect(og_weapon_view()->frame == 3 && og_arrow_count() == 2,"the drawn bow waits at frame 3 without consuming ammo");
    preview("bow");
    in.pressed[OG_SWING_BUTTON] = 1; og_weapon_frame(&in,0,&strike); in.pressed[OG_SWING_BUTTON] = 0;
    hits = 0;
    for (i = 0; i < 20; i++) { og_weapon_frame(&in,0,&strike); if (strike.hand >= 0) { hits++; } }
    check_expect(hits == 1 && og_arrow_count() == 1,"bow release fires once at frame 5 and consumes one arrow");
    mem_zero(&strike,sizeof(strike)); strike.hand = 0; strike.bow = 1; strike.item = bow;
    arena(1); before = og_dungeon_health(0); og_dungeon_strike(&g_scene,&g_cam,&strike);
    og_dungeon_step(&g_scene,&g_cam,0.05f);
    check_expect(og_dungeon_health(0) == before && !og_dungeon_arrow(0)->live,"an arrow stops at a nearer wall before its target");
    arena(0); g_entity[0].z = 300; before = og_dungeon_health(0);
    og_dungeon_strike(&g_scene,&g_cam,&strike);
    og_dungeon_step(&g_scene,&g_cam,0.05f);
    check_expect(og_dungeon_health(0) == before && og_dungeon_arrow(0)->live,"arrows travel through the dungeon over multiple frames");
    for (i = 0; i < 3; i++) { og_dungeon_step(&g_scene,&g_cam,0.05f); }
    check_expect(!og_dungeon_arrow(0)->live && og_inventory_rows(og_dungeon_container(0),0,0,0) > 0,
                 "a struck creature retains the recoverable arrow in its loot");
    og_weapon_cancel(); og_weapon_reset(); og_object_edit(arrows)->count = 0;
    og_object_delete(arrows); og_weapon_frame(&in,0,&strike);
    check_expect(!og_hud_weapon_ready() && strike.hand < 0,"a bow without ammunition lowers without firing");
}

static void injuries(void)
{
    int before, i;
    arena(0); g_entity[0].z = 60;
    /* Attacking alerts the rat; it must then use its animation event. */
    keep_find(g_entity[0].key)->game[1] = 1;
    og_dungeon_open(&g_scene,g_kind,g_passive,g_rate); og_dungeon_loaded(0);
    before = og_character()->health;
    check_expect(og_dungeon_nearby(&g_cam,1024),"an alerted passive creature also prevents resting nearby");
    for (i = 0; i < 200 && og_character()->health == before; i++) { og_dungeon_step(&g_scene,&g_cam,0.05f); }
    check_expect(og_character()->health < before,"an alerted creature's animated attack can injure the player");
    og_damage_player(10000,0);
    check_expect(!og_character()->health && og_combat_flash() > 0,"lethal damage clamps health to zero and produces hit feedback");
    before = og_dungeon_health(0); og_dungeon_step(&g_scene,&g_cam,1);
    check_expect(og_dungeon_health(0) == before,"combat stops after player death");
}

static void god_mode(df_u32 sword)
{
    OgStrike strike;
    int i, health, fatigue, magicka;
    /* The focused executable has OGFALL's command table. The combined
     * hostcheck has already installed GFALL's independent tgm command. */
#ifdef OGFALL_HOST_MAIN
    cmd_run("tgm");
#else
    og_god_set(1);
#endif
    check_expect(og_god_mode() && og_character()->health == og_character()->max_health,
                 "tgm toggles on and recovers a dead player");
    health = og_character()->health; fatigue = og_character()->fatigue;
    og_character()->max_magicka = og_character()->magicka = 40; magicka = 40;
    og_damage_player(100000,0);
    check_expect(og_character()->health == health &&
                 og_fatigue_add(fatigue,-100000,50,50) == fatigue &&
                 og_magicka_cost(100000) == 0 && og_magicka_spend(100000) && og_character()->magicka == magicka,
                 "god mode blocks lethal damage and fatigue and magicka debits");
    mem_zero(&strike,sizeof(strike)); strike.hand = 0; strike.item = sword;
    for (i = 0; i < 128; i++) {
        arena(0); health = og_dungeon_health(0);
        og_character()->skill[og_weapon_skill(5)] = 0;
        og_dungeon_strike(&g_scene,&g_cam,&strike);
        check_expect(og_dungeon_health(0) < health,"god mode connects every unobstructed melee attack");
    }
    arena(1); health = og_dungeon_health(0);
    og_dungeon_strike(&g_scene,&g_cam,&strike);
    check_expect(og_dungeon_health(0) == health,"god mode preserves walls and weapon reach");
#ifdef OGFALL_HOST_MAIN
    cmd_run("tgm");
#else
    og_god_set(0);
#endif
    check_expect(!og_god_mode() && og_magicka_cost(9) == 9 && og_magicka_spend(9) &&
                 og_character()->magicka == magicka-9,"tgm toggles off and restores ordinary spell costs");
}

static void small_creature_floor(void)
{
    int i;
    arena(0);
    g_entity[0].x = 180; g_entity[0].z = 180;
    g_entity[0].pose[ENTITY_WALK].h[0] = 24;
    keep_find(g_entity[0].key)->game[1] = 1;
    og_dungeon_open(&g_scene,g_kind,g_passive,g_rate); og_dungeon_loaded(0);
    og_god_set(1); sound_stop_all();
    for (i = 0; i < 600; i++) {
        og_dungeon_step(&g_scene,&g_cam,0.02f);
        check_expect(g_entity[0].y >= -0.5f && g_entity[0].y <= 0.5f,
                     "a small creature walks, attacks and idles on its floor");
    }
    check_expect(sound_busy() > 0,"a nearby creature plays its original DAGGER.SND voice");
    og_god_set(0); sound_stop_all();
}

static void flying_creature_floor(void)
{
    static const int kinds[2] = {3,0};
    int i;
    arena(0); g_entity[0].z = 180; g_entity[0].pose[ENTITY_WALK].h[0] = 24;
    keep_find(g_entity[0].key)->game[1] = 1;
    og_dungeon_open(&g_scene,kinds,g_passive,g_rate); og_dungeon_loaded(0);
    g_cam.y = -100; og_god_set(1);
    for (i = 0; i < 200; i++) {
        og_dungeon_step(&g_scene,&g_cam,0.02f);
        check_expect(g_entity[0].y >= -0.5f,"a flying creature's vertical pursuit cannot pass through the floor");
    }
    og_god_set(0);
}

static void flight_stairwell(void)
{
    Body bat;
    int i;
    arena(1);
    /* Turn the second model's wall into a low ceiling. */
    g_model[1].m[4] = 0; g_model[1].m[5] = -1;
    g_model[1].m[7] = 1; g_model[1].m[8] = 0; g_model[1].y = 80;
    g_scene.version++;
    body_init(&bat,8,24,6,12); body_place(&bat,0,20,0);
    for (i = 0; i < 100; i++) { body_fly_step(&bat,&g_scene,0,2,0); }
    check_expect(bat.feet[1] < 27 && bat.feet[1] >= 20,"free flight stops below a stairwell ceiling without snapping to the floor above");
    for (i = 0; i < 100; i++) { body_fly_step(&bat,&g_scene,0,-2,0); }
    check_expect(bat.feet[1] >= -0.1f,"free flight also stops on the floor below");
}

static void creature_poses(const Level *level)
{
    static const int acts[4] = {ENTITY_ATTACK,ENTITY_HURT,ENTITY_CAST,ENTITY_SHOOT};
    static const int records[4] = {5,10,20,25};
    const Entity *e;
    const Image *actual;
    Image *expected;
    int i, p, archive, slot, checked;
    checked = 0;
    for (i = 0; i < level->scene.nentities; i++) {
        archive = seam_creature_archive(i);
        if (archive < 475 || archive > 491) { continue; }
        e = &level->scene.entities[i];
        for (p = 0; p < 4; p++) {
            slot = e->pose[acts[p]].slot[0];
            if (slot == 0xffff) { continue; }
            expected = texture_load(archive,records[p]);
            if (expected && expected->frames > 1) { texture_sheet(expected,archive,records[p]); }
            actual = slot < level->nimages ? level->images[slot] : 0;
            check_expect(expected && actual && expected->width == actual->width && expected->height == actual->height &&
                         !memcmp(expected->index,actual->index,actual->width*actual->height),
                         "every human hurt, attack and ranged pose uses its walking archive and sex");
            check_expect(e->pose[acts[p]].h[0] > 0,"a human combat pose has drawable dimensions");
            checked++;
        }
    }
    check_expect(checked > 0,"actual Hold humanoids exercise the combat archive checks");
}

void check_ogcombat_loaded(void)
{
    const Scene *scene;
    const Entity *corpse;
    Camera cam;
    int i, j, dead, usable;
    scene = &seam_level()->scene; dead = usable = 0;
    check_ogdoors_loaded(scene);
    check_ogmagic_loaded();
    for (i = 0; i < og_dungeon_count(); i++) {
        corpse = og_dungeon_entity(i);
        if (!corpse->dead) { continue; }
        dead++;
        camera_set(&cam,corpse->x,corpse->y+40,corpse->z-20,0,0);
        for (j = 0; j < 80 && !corpse->live; j++) { seam_creatures(&cam); }
        seam_creatures(&cam);
        check_expect(corpse->live && corpse->act == ENTITY_DEAD && corpse->pose[ENTITY_DEAD].w[0] > 0,
                     "reloaded Privateer's Hold actually draws its saved corpse sprite");
        check_expect(!og_dungeon_health(i),"reloaded corpse health remains zero");
        for (j = 0; j < scene->nusables; j++) {
            if (scene->usables[j].sprite < 0 && (df_u32)scene->usables[j].tag == corpse->key.thing &&
                scene->usables[j].label && !strcmp(scene->usables[j].label,"Corpse")) { usable++; }
        }
    }
    check_expect(dead && usable,"saved corpses remain lootable after a fresh-process load");
    for (i = 0; i < keep_count(); i++) {
        const Keep *row;
        const Entity *actor;
        row = keep_at(i);
        /* A rest encounter is a kept row (made 2, a key with the high bit): its creature is rebuilt from the key and the owner. */
        if (!row || row->made != 2 || !(row->key.thing & 0x80000000u)) { continue; }
        actor = 0;
        for (j = 0; j < og_dungeon_count(); j++) {
            if (og_dungeon_entity(j)->key.thing == row->key.thing) { actor = og_dungeon_entity(j); break; }
        }
        check_expect(actor && actor->live && actor->dead && actor->x == row->x &&
                     actor->y == row->y && actor->z == row->z && actor->pose[ENTITY_DEAD].w[0] > 0,
                     "a fresh process rebuilds the saved rest encounter with its corpse art and physical position");
    }
    check_expect(scene->nmodels == scene_solid_models(scene)+OG_ARROWS,"arrow meshes have reserved nonsolid draw slots");
    check_expect(seam_capture_world(),"the reloaded corpse can be saved again");
}

static void hold_round_trip(const char *root)
{
    const char *test_root = "build/ogfall-combat-data/ARENA2";
    const Scene *scene;
    const Entity *target;
    OgStrike strike;
    Weather weather;
    df_u32 sword;
    float x, y, z;
    int i, j, killed, status, original, spawned;
    char resolved[4096];
    pid_t child;
    og_state_restore(&g_before); og_state_restore_world();
    check_expect(realpath(root,resolved) != 0,"resolve original assets for an isolated combat save");
    fs_set_root("build/ARENA2"); fs_make_dirs("ogfall-combat-data");
    unlink(test_root); check_expect(!symlink(resolved,test_root),"mount read-only assets beside the combat fixtures");
    vfs_mount(test_root);
    if (!seam_open_hold(&x,&y,&z)) { check_expect(0,"combat scenario opens the actual Hold"); vfs_mount(root); return; }
    scene = &seam_level()->scene;
    creature_poses(seam_level());
    mem_zero(&weather,sizeof(weather));
    camera_set(&g_cam,x,y,z,0,0);
    ambience_frame(seam_level(),&g_cam,&weather,0,0,0.02f);
    check_expect(music_name()[0] != 0,"the actual Hold soundscape loads a dungeon song from MIDI.BSA");
    sword = og_item_create(og_state()->inventory_id,3,9,9);
    og_character()->health = og_character()->max_health;
    for (i = 0; i < OG_SKILLS; i++) { og_character()->skill[i] = 100; }
    mem_zero(&strike,sizeof(strike)); strike.hand = 0; strike.item = sword;
    killed = 0;
    for (i = 0; i < scene->nentities && !killed; i++) {
        target = &scene->entities[i];
        camera_set(&g_cam,target->x,target->y+40,target->z-20,0,0);
        for (j = 0; j < 80 && !target->live; j++) { seam_creatures(&g_cam); }
        og_move_place(g_cam.x,target->y+WALK_EYE,g_cam.z,0);
        for (j = 0; j < 30 && !target->dead; j++) { og_dungeon_strike(scene,&g_cam,&strike); }
        killed = target->dead;
    }
    check_expect(killed,"actual Hold combat can kill a loaded creature");
    original = og_dungeon_count(); spawned = 0;
    for (i = 0; i < original && !spawned; i++) {
        target = og_dungeon_entity(i);
        og_move_place(target->x,target->y+WALK_EYE,target->z,0);
        og_move_camera(&g_cam);
        spawned = og_encounter_try(&g_cam);
    }
    check_expect(spawned && og_dungeon_count() == original+1,
                 "the actual Hold can place a rest encounter with preloaded drawable resources");
    if (spawned) {
        int slot;
        target = og_dungeon_entity(original); slot = target->pose[ENTITY_WALK].slot[0];
        check_expect(slot < seam_level()->nimages && seam_level()->images[slot] &&
                     target->pose[ENTITY_WALK].w[0],"a real rest creature has decoded walking art before its first frame");
        og_move_place(target->x,target->y+WALK_EYE,target->z-30,0); og_move_camera(&g_cam);
        og_god_set(1);
        for (j = 0; j < 60 && !target->dead; j++) { og_dungeon_strike(scene,&g_cam,&strike); }
        og_god_set(0);
        check_expect(target->dead,"the actual rest encounter leaves a persistent combat corpse");
        seam_creatures(&g_cam);
    }
    check_ogdoors_hold(scene);
    og_state_capture_motion(); og_move_camera(&g_cam);
    og_state_pose(g_cam.x,g_cam.y,g_cam.z,og_move_yaw(),0);
    og_state_save_label("Combat round trip");
    check_ogmagic_hold();
    check_expect(seam_capture_world() && og_save_write(5),"write actual combat health and corpse state to an isolated save");
    child = fork();
    if (!child) { execl("/proc/self/exe","ogfall-test","--ogfall-resume-combat",test_root,(char *)0); _exit(127); }
    status = -1;
    if (child > 0) { waitpid(child,&status,0); }
    check_expect(child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,"a fresh process loads the combat save and its corpse art");
    seam_close_hold();
    fs_delete("ogfall\\saves\\S0005.SAV"); fs_remove_dir("ogfall\\saves"); fs_remove_dir("ogfall");
    fs_set_root("build/ARENA2"); unlink(test_root); fs_remove_dir("ogfall-combat-data"); vfs_mount(root);
    og_state_restore(&g_before); og_state_restore_world();
    check_expect(seam_open_hold(&x,&y,&z),"restore the original test session after combat");
}

void check_ogcombat(const char *root)
{
    df_u32 sword;
    int i;
    g_before = *og_state();
    og_options_close(); og_state_clear();
    og_character()->klass = 17; og_character()->race = 0; og_character()->level = 1;
    og_character()->health = og_character()->max_health = 100;
    og_character()->fatigue = 6400; og_character()->reflexes = 2;
    df_copy(og_character()->name,"Combat test",OG_NAME);
    for (i = 0; i < OG_ATTRS; i++) { og_character()->attribute[i] = 50; }
    for (i = 0; i < OG_SKILLS; i++) { og_character()->skill[i] = 100; }
    check_expect(og_state_begin(),"combat test creates an independent player");
    og_object_edit(OG_WORLD_ROOT)->place_key = 0x40000001u;
    sword = og_item_create(og_state()->inventory_id,3,5,1);
    og_dungeon_blood(-1,-1);
    check_ogdoors(root);
    rules(); ai_rules(); swings(sword); melee(sword); archery(); injuries(); god_mode(sword);
    small_creature_floor(); flying_creature_floor(); flight_stairwell();
    og_dungeon_close(); og_weapon_cancel(); og_state_restore(&g_before); og_state_restore_world();
    hold_round_trip(root);
}
