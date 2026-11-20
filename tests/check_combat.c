#include "check_combat.h"
#include "check_doors.h"
#include "check_magic.h"
#include "check_util.h"
#include "game/dagger/play/combat/anim.h"
#include "game/dagger/play/combat/ai.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/character/vitals.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/combat/foegear.h"
#include "game/dagger/play/character/ailments.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/play/combat/encounter.h"
#include "game/dagger/play/ui/hud.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/items/describe.h"
#include "game/dagger/play/magic/itemmaker.h"
#include "game/dagger/play/magic/magicitem.h"
#include "game/dagger/play/magic/potion.h"
#include "game/dagger/play/save/save.h"
#include "game/dagger/play/combat/weapon.h"
#include "game/dagger/play/ui/options.h"
#include "game/dagger/play/character/chargen.h"
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
void check_game_draw_reset(void);
int check_game_draw_count(void);

static GameState g_before;
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
    check_game_draw_reset(); weapon_draw();
    pixels = seam_weapon_screen(&palette);
    check_expect(pixels && check_game_draw_count(),"the real CIF weapon frame fits beside the HUD on the fake Voodoo");
    if (!pixels) { return; }
    opaque = 0;
    snprintf(path,sizeof(path),"build/gfall-weapon-%s.ppm",name);
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
    keep_clear(); dungeon_open(&g_scene,g_kind,g_passive,g_rate);
    for (i = 0; i < 2; i++) { dungeon_loaded(i); }
    move_init(); move_place(0,WALK_EYE,0,0);
    camera_set(&g_cam,0,60,0,0,0);
}

static void rules(void)
{
    FighterInfo a, t;
    Anim anim;
    int i, strikes;
    mem_zero(&a,sizeof(a)); mem_zero(&t,sizeof(t));
    a.health = a.maximum = t.health = t.maximum = 100;
    a.level = 1; a.race = 0; t.race = 0; t.armor[0] = 30;
    for (i = 0; i < ATTRS; i++) { a.attribute[i] = t.attribute[i] = 50; }
    check_expect(attack_chance(&a,&t,50,5,0,0,0,100) == 70,
                 "original hit chance: skill + armor + 40 - 50");
    a.skill[20] = 40;
    check_expect(attack_chance(&a,&t,50,5,0,0,0,100) == 60,
                 "byte-matched attack uses the attacker's Dodging");
    t.to_hit_against = 5;
    check_expect(attack_chance(&a,&t,50,5,0,0,0,100) == 55, "the biography's TH is taken off the chance to hit the player");
    t.to_hit_against = 0;
    check_expect(attack_chance(&a,&t,1000,5,0,0,0,100) == 97 &&
                 attack_chance(&a,&t,-1000,5,0,0,0,100) == 3,"hit chance clamps to 3..97");
    check_expect(attack_damage(&a,&t,5,1,4,0,5) == 9,"a downward swing adds four damage");
    check_expect(swing_hit(weapon_sector(0,-100)) == 10 &&
                 swing_damage(weapon_sector(0,-100)) == -4 &&
                 swing_hit(weapon_sector(0,100)) == -10 &&
                 swing_damage(weapon_sector(0,100)) == 4,"up/down gestures preserve the original tradeoffs");
    check_expect(weapon_delay(50) == 195 && bow_delay(50) == 1500,"Speed controls the original melee and bow delays");
    check_expect(foe_def(0)->dice == 1 && foe_def(0)->hp_bonus == 8 &&
                 foe_def(0)->minimum[0] == 1 && foe_def(0)->maximum[0] == 4 &&
                 foe_def(2)->armor == -20,"monster data preserves d8 health, WORD damage pairs and signed armor");
    anim_start(&anim,foe_script(0),8); strikes = 0;
    for (i = 0; i < 20 && !anim.ended; i++) {
        if (anim_tick(&anim) & 1) { strikes++; check_expect(anim.frame == 3,"rat strike is attached to original ASCR frame 3"); }
    }
    check_expect(strikes == 1 && anim.ended,"rat attack script strikes once then returns to idle");
    anim_start(&anim,foe_script(9),8); strikes = 0;
    for (i = 0; i < 25 && !anim.ended; i++) { if (anim_tick(&anim) & 1) { strikes++; } }
    check_expect(strikes == 2,"werewolf ASCR retains both attack events");
}

static void ai_rules(void)
{
    static GameState before;
    MotionState motion, before_motion;
    Mind mind;
    Body body;
    Entity entity;
    float target[3];
    df_u32 uses, random;
    int i, blocked;

    before = *game_state(); move_capture(&before_motion); motion = before_motion;
    character_state()->skill[16] = 100;
    uses = game_state()->skill_uses[16];
    check_expect(!ai_stealth(0,1024,0) && game_state()->skill_uses[16] == uses+1,
                 "original stealth chance uses distance and trains once in an even game minute");
    random = game_state()->random;
    check_expect(ai_stealth(1,600,0) && game_state()->random == random &&
                 game_state()->skill_uses[16] == uses+1,"other creatures share the minute's stealth check");
    state_pass_minutes(1); random = game_state()->random;
    check_expect(!ai_stealth(0,600,0) && game_state()->random == random,
                 "quiet movement skips odd-minute stealth rolls");
    motion.vx = 200; motion.vz = 0; move_restore(&motion);
    check_expect(ai_stealth(0,600,1),"fast movement reveals an already encountered player");
    check_expect(ai_octant(1,2) == 0 && ai_octant(2,1) == 1 &&
                 ai_octant(1,-2) == 2 && ai_octant(2,-1) == 3 &&
                 ai_octant(-2,-1) == 4 && ai_octant(-1,-2) == 5 &&
                 ai_octant(-2,1) == 6 && ai_octant(-1,2) == 7,
                 "all original navigation octants preserve dominant-axis and quadrant selection");
    check_expect(ai_turn(2016,32,64) == 32 && ai_turn(0,512,64) == 64 &&
                 ai_angle(2040,8) == 16,"creatures turn gradually across the angle wrap");
    arena(1); g_scene.nentities = 0; g_scene.version = ++g_arena_version;
    mem_zero(&mind,sizeof(mind)); entity_init(&entity);
    entity.yaw = DF_TAU/2; mind.detected = 1;
    body_init(&body,12,60,12,40); body_place(&body,0,0,60);
    target[0] = 0; target[1] = 30; target[2] = -120;
    blocked = 1;
    for (i = 0; i < 120; i++) {
        ai_walk(&mind,&body,&entity,&g_scene,target,0,0,200,1.0f/60);
        if (body.feet[2] < 41 && df_absf(body.feet[0]) < 100) {
            if (blocked) { fprintf(stderr,"AI wall fixture: step %d at %.2f %.2f %.2f\n",i,body.feet[0],body.feet[1],body.feet[2]); }
            blocked = 0;
        }
    }
    check_expect(blocked,"wall-following never moves a creature through the blocking wall");
    check_expect(mind.direction == 2 || mind.direction == 4 || mind.direction == 8,
                 "a blocked creature retains a valid original navigation direction");
    state_restore(&before); state_restore_world(); move_restore(&before_motion);
}

static void swings(df_u32 sword)
{
    PlayIn in;
    StrikeInfo strike;
    int i, hits, fatigue;
    state_equip(19,sword); state_equip(21,0); hud_reset(); hud_toggle_ready(); weapon_reset();
    mem_zero(&in,sizeof(in)); in.held[SWING_BUTTON] = 1; in.dt = 0.01f;
    weapon_frame(&in,0,&strike);
    check_expect(strike.hand < 0 && weapon_view()->group == 0,"holding attack without a gesture does not swing");
    in.mouse_dy = 120; fatigue = character_state()->fatigue;
    weapon_frame(&in,0,&strike); preview("swing"); in.mouse_dy = 0; hits = 0;
    for (i = 0; i < 150; i++) {
        weapon_frame(&in,0,&strike); if (strike.hand >= 0) { hits++; }
    }
    check_expect(hits == 1 && character_state()->fatigue == fatigue-11,"a gesture strikes once at its hit frame and spends 11 fatigue");
    in.mouse_dx = 120; weapon_frame(&in,1,&strike);
    check_expect(weapon_view()->group == 0,"HUD-owned input cannot begin a swing");
    weapon_frame(&in,0,&strike); weapon_cancel(); in.mouse_dx = 0; hits = 0;
    for (i = 0; i < 100; i++) { weapon_frame(&in,0,&strike); hits += strike.hand >= 0; }
    check_expect(!hits,"cancelling an interrupted swing discards its pending hit");
    weapon_switch();
    check_expect(weapon_hand() == 1,"S switches to an empty left hand for unarmed combat");
    weapon_frame(&in,0,&strike); preview("fist");
    in.mouse_dx = -120; in.dt = 0.001f; weapon_frame(&in,0,&strike); in.mouse_dx = 0;
    hits = 0;
    for (i = 0; i < 780; i++) {
        weapon_frame(&in,0,&strike);
        if (strike.hand >= 0) { hits++; check_expect(i >= 777,"the unarmed kick waits for script frame 4"); }
    }
    check_expect(hits == 1,"the kick script strikes once");
    weapon_cancel();
    weapon_switch();
}

static void melee(df_u32 sword)
{
    StrikeInfo s;
    int before, i;
    mem_zero(&s,sizeof(s)); s.hand = 0; s.item = sword; s.damage = 4;
    arena(1); before = dungeon_health(0);
    for (i = 0; i < 10; i++) { dungeon_strike(&g_scene,&g_cam,&s); }
    check_expect(dungeon_health(0) == before,"melee cannot damage a creature through a wall");
    {
        float from[3] = {0,40,0}, to[3] = {0,40,100};
        int visible_models[2], count, visual;
        g_scene.solid_models = 1; g_scene.version++;
        check_expect(!scene_line_blocked(&g_scene,from,to,0),"visual models do not obstruct sight rays");
        count = scene_visible_models(&g_scene,&g_cam,visible_models,2); visual = 0;
        for (i = 0; i < count; i++) { if (visible_models[i] == 1) { visual = 1; } }
        check_expect(visual,"a nonsolid model remains in the renderer's visible model list");
        for (i = 0; i < 20 && !g_entity[0].dead; i++) { dungeon_strike(&g_scene,&g_cam,&s); }
        check_expect(g_entity[0].dead,"the collision ray also ignores a visual model");
    }
    arena(0); before = dungeon_health(1);
    for (i = 0; i < 30 && !g_entity[0].dead; i++) { dungeon_strike(&g_scene,&g_cam,&s); }
    check_expect(g_entity[0].dead && dungeon_health(0) == 0 && g_entity[0].act == ENTITY_DEAD && !g_entity[0].radius,
                 "lethal melee makes a nonsolid corpse");
    check_expect(dungeon_health(1) == before,"melee respects its 90-unit reach");
    /* Crouched, the eye is 32 above the floor: the blow's ray must start in the body, not 35 under the eye (in the floor). */
    {
        float standing_y;

        standing_y = g_cam.y;
        arena(0);
        g_cam.y = move_body()->feet[1] + WALK_EYE / 2;
        for (i = 0; i < 40 && !g_entity[0].dead; i++) { dungeon_strike(&g_scene,&g_cam,&s); }
        check_expect(g_entity[0].dead,"a crouched player's blows connect");
        g_cam.y = standing_y;
    }
    keep_save(&g_scene,0,0,g_entity,2,0,0,0);
    check_expect(keep_find(g_entity[0].key)->dead && keep_find(g_entity[0].key)->game[0] == 0xc1,
                 "health and corpse pose share the saved world record");
    before = dungeon_health(1);
    dungeon_close(); keep_restore(&g_scene,0,0,g_entity,2,0,0,0);
    dungeon_open(&g_scene,g_kind,g_passive,g_rate);
    check_expect(g_entity[0].dead && dungeon_health(0) == 0 && dungeon_health(1) == before,
                 "opening the dungeon again restores corpses and previously rolled health");
    check_expect(dungeon_container(0) != 0,"a dead creature has one persistent loot container");
}

static void archery(void)
{
    PlayIn in;
    StrikeInfo strike;
    df_u32 bow, arrows;
    int i, hits, before;
    bow = item_create(game_state()->inventory_id,3,16,1);
    arrows = item_create(game_state()->inventory_id,3,18,0);
    object_edit(arrows)->count = 2; state_equip(19,bow); weapon_reset();
    mem_zero(&in,sizeof(in)); in.dt = 1.0f/60;
    for (i = 0; i < 10; i++) { weapon_frame(&in,0,&strike); }
    check_expect(weapon_view()->frame == 3 && arrow_count() == 2,"the drawn bow waits at frame 3 without consuming ammo");
    preview("bow");
    in.pressed[SWING_BUTTON] = 1; weapon_frame(&in,0,&strike); in.pressed[SWING_BUTTON] = 0;
    hits = 0;
    for (i = 0; i < 20; i++) { weapon_frame(&in,0,&strike); if (strike.hand >= 0) { hits++; } }
    check_expect(hits == 1 && arrow_count() == 1,"bow release fires once at frame 5 and consumes one arrow");
    mem_zero(&strike,sizeof(strike)); strike.hand = 0; strike.bow = 1; strike.item = bow;
    arena(1); before = dungeon_health(0); dungeon_strike(&g_scene,&g_cam,&strike);
    dungeon_step(&g_scene,&g_cam,0.05f);
    check_expect(dungeon_health(0) == before && !dungeon_arrow(0)->live,"an arrow stops at a nearer wall before its target");
    arena(0); g_entity[0].z = 300; before = dungeon_health(0);
    dungeon_strike(&g_scene,&g_cam,&strike);
    dungeon_step(&g_scene,&g_cam,0.05f);
    check_expect(dungeon_health(0) == before && dungeon_arrow(0)->live,"arrows travel through the dungeon over multiple frames");
    for (i = 0; i < 3; i++) { dungeon_step(&g_scene,&g_cam,0.05f); }
    check_expect(!dungeon_arrow(0)->live && inventory_rows(dungeon_container(0),0,0,0) > 0,
                 "a struck creature retains the recoverable arrow in its loot");
    weapon_cancel(); weapon_reset(); object_edit(arrows)->count = 0;
    object_delete(arrows); weapon_frame(&in,0,&strike);
    check_expect(!hud_weapon_ready() && strike.hand < 0,"a bow without ammunition lowers without firing");
}

/* A flyer killed in the air falls to the floor and lies there; flying bodies have no gravity, a corpse must. */
static void flyer_corpse(void)
{
    int kind[2], i;
    const FoeDef *def;

    for (i = 0; i < 43; i++) { def = foe_def(i); if (def && (def->flags & 1)) { break; } }
    if (i == 43) { return; }
    kind[0] = kind[1] = i;
    character_state()->health = character_state()->max_health = 100;
    arena(0); g_entity[0].z = 60; g_entity[0].y = 150;
    dungeon_open(&g_scene,kind,g_passive,g_rate); dungeon_loaded(0);
    for (i = 0; i < 4 && dungeon_health(0) > 0; i++) { dungeon_hurt_nearest(&g_cam,1000,100000); }
    check_expect(dungeon_health(0) <= 0 && g_entity[0].dead,"a flyer is killed in the air");
    for (i = 0; i < 100; i++) { dungeon_step(&g_scene,&g_cam,0.05f); }
    check_expect(g_entity[0].y < 5,"a flyer killed in the air falls to the floor");
}

static void marker_pacification(void)
{
    const int kinds[2] = {1,0};
    const int hostile[2] = {0,0};
    int drawn, impish;
    df_u32 uses, random;

    arena(0);
    keep_clear();
    drawn = hud_weapon_ready();
    if (drawn) { hud_toggle_ready(); }
    impish = character_state()->skill[12];
    character_state()->skill[12] = 50;
    dungeon_open(&g_scene,kinds,hostile,g_rate);
    uses = game_state()->skill_uses[12];
    random = game_state()->random;
    check_expect(dungeon_loaded_marker(0) && dungeon_is_pacified(0) &&
                 game_state()->skill_uses[12] == uses+1 && game_state()->random == random,
                 "an Imp uses the shipped Impish skill and marker seed to pacify once");
    check_expect(!dungeon_loaded_marker(0) && game_state()->skill_uses[12] == uses+1,
                 "making the same marker active again does not train Impish again");
    dungeon_open(&g_scene,kinds,hostile,g_rate);
    check_expect(dungeon_loaded_marker(0) && dungeon_is_pacified(0) &&
                 game_state()->skill_uses[12] == uses+1,"the keep row preserves the roll and its outcome");
    dungeon_enchant_damage(0,1,TARGET_PLAYER);
    check_expect(!dungeon_is_pacified(0),"a damaging hit clears marker pacification");
    dungeon_open(&g_scene,kinds,hostile,g_rate);
    dungeon_loaded_marker(0);
    check_expect(!dungeon_is_pacified(0) && game_state()->skill_uses[12] == uses+1,
                 "a revisit preserves a pacified creature's broken truce");
    keep_clear();
    hud_toggle_ready();
    dungeon_open(&g_scene,kinds,hostile,g_rate);
    check_expect(dungeon_loaded_marker(0) && !dungeon_is_pacified(0) &&
                 game_state()->skill_uses[12] == uses+2,
                 "a drawn weapon lowers the chance but a failed check still trains Impish");
    if (!drawn) { hud_toggle_ready(); }
    character_state()->skill[12] = impish;
}

static void creature_teams(void)
{
    int i, health, first, second;
    df_u32 kill_minute;

    arena(0);
    g_entity[0].x = g_entity[1].x = 0;
    g_entity[0].z = 160; g_entity[1].z = 204;
    g_entity[0].yaw = 0; g_entity[1].yaw = DF_TAU/2;
    dungeon_open(&g_scene,g_kind,g_passive,g_rate);
    for (i = 0; i < 2; i++) {
        dungeon_loaded(i);
        dungeon_set_pacified(i,0);
        dungeon_wake(i);
    }
    move_place(0,WALK_EYE,-700,0);
    camera_set(&g_cam,0,WALK_EYE,-700,0,0);
    dungeon_step(&g_scene,&g_cam,0.05f);
    check_expect(dungeon_target(0) == TARGET_PLAYER && dungeon_target(1) == TARGET_PLAYER,
                 "hostile creatures do not select their teammates");
    dungeon_set_team(1,TEAM_PLAYER);
    dungeon_step(&g_scene,&g_cam,0.05f);
    for (i = 0; i < 12 && dungeon_target(0) != 1; i++) { dungeon_step(&g_scene,&g_cam,0.05f); }
    check_expect(dungeon_target(0) == 1 && dungeon_target(1) == 0,
                 "opposing creatures select each other, and an ally ignores the player");
    health = character_state()->health;
    first = dungeon_health(0); second = dungeon_health(1);
    kill_minute = game_state()->last_kill;
    for (i = 0; i < 190 && dungeon_health(0) == first && dungeon_health(1) == second; i++) {
        dungeon_step(&g_scene,&g_cam,0.05f);
    }
    check_expect((dungeon_health(0) < first || dungeon_health(1) < second) &&
                 character_state()->health == health && game_state()->last_kill == kill_minute,
                 "the attack animation deals damage to an opposing creature");
    dungeon_open(&g_scene,g_kind,g_passive,g_rate);
    check_expect(dungeon_team(1) == TEAM_PLAYER,"the keep row restores the ally's team");
}

static void injuries(void)
{
    int before, i;
    arena(0); g_entity[0].z = 60;
    /* Attacking alerts the rat; it must then use its animation event. */
    keep_find(g_entity[0].key)->game[1] = 1;
    dungeon_open(&g_scene,g_kind,g_passive,g_rate); dungeon_loaded(0);
    /* The row's flag is ignored without the dungeon's own tag, so the rat is alerted as a noise would (a sleeper that wakes by chance made this test depend on the dice). */
    dungeon_alert(0);
    /* A pickpocket's aim: the ray at the rat finds it, the opposite ray or a short reach does not. */
    {
        float vx = g_entity[0].x-g_cam.x, vy = g_entity[0].y+BODY_MIDDLE-g_cam.y, vz = g_entity[0].z-g_cam.z, d = df_sqrt(vx*vx+vy*vy+vz*vz);

        vx /= d; vy /= d; vz /= d;
        check_expect(dungeon_aim(g_cam.x,g_cam.y,g_cam.z,vx,vy,vz,d+10.0f) == 0 && dungeon_aim(g_cam.x,g_cam.y,g_cam.z,-vx,-vy,-vz,d+10.0f) < 0 &&
                     dungeon_aim(g_cam.x,g_cam.y,g_cam.z,vx,vy,vz,d*0.5f) < 0 && dungeon_level(0) >= 0, "dungeon aim: a creature in the ray was not found, or one behind or past reach was");
    }
    before = character_state()->health;
    check_expect(dungeon_nearby(&g_cam,1024),"an alerted hostile creature prevents resting nearby");
    for (i = 0; i < 200 && character_state()->health == before; i++) { dungeon_step(&g_scene,&g_cam,0.05f); }
    check_expect(character_state()->health < before,"an alerted creature's animated attack can injure the player");
    damage_player(10000,0);
    check_expect(!character_state()->health && combat_flash() > 0,"lethal damage clamps health to zero and produces hit feedback");
    before = dungeon_health(0); dungeon_step(&g_scene,&g_cam,1);
    check_expect(dungeon_health(0) == before,"combat stops after player death");
}

static void god_swing(df_u32 sword)
{
    StrikeInfo strike;
    int i, health, fatigue, magicka;
    /* The focused executable has GFALL's command table. The combined
     * hostcheck has already installed GFALL's independent tgm command. */
#ifdef GFALL_HOST_MAIN
    cmd_run("tgm");
#else
    god_set(1);
#endif
    check_expect(god_mode() && character_state()->health == character_state()->max_health,
                 "tgm toggles on and recovers a dead player");
    health = character_state()->health; fatigue = character_state()->fatigue;
    character_state()->max_magicka = character_state()->magicka = 40; magicka = 40;
    damage_player(100000,0);
    check_expect(character_state()->health == health &&
                 fatigue_add(fatigue,-100000,50,50) == fatigue &&
                 magicka_cost(100000) == 0 && magicka_spend(100000) && character_state()->magicka == magicka,
                 "god mode blocks lethal damage and fatigue and magicka debits");
    mem_zero(&strike,sizeof(strike)); strike.hand = 0; strike.item = sword;
    for (i = 0; i < 128; i++) {
        arena(0); health = dungeon_health(0);
        character_state()->skill[weapon_skill(5)] = 0;
        dungeon_strike(&g_scene,&g_cam,&strike);
        check_expect(dungeon_health(0) < health,"god mode connects every unobstructed melee attack");
    }
    arena(1); health = dungeon_health(0);
    dungeon_strike(&g_scene,&g_cam,&strike);
    check_expect(dungeon_health(0) == health,"god mode preserves walls and weapon reach");
#ifdef GFALL_HOST_MAIN
    cmd_run("tgm");
#else
    god_set(0);
#endif
    check_expect(!god_mode() && magicka_cost(9) == 9 && magicka_spend(9) &&
                 character_state()->magicka == magicka-9,"tgm toggles off and restores ordinary spell costs");
}

static void small_creature_floor(void)
{
    int i;
    arena(0);
    g_entity[0].x = 180; g_entity[0].z = 180;
    g_entity[0].pose[ENTITY_WALK].h[0] = 24;
    keep_find(g_entity[0].key)->game[1] = 1;
    dungeon_open(&g_scene,g_kind,g_passive,g_rate); dungeon_loaded(0);
    god_set(1); sound_stop_all();
    for (i = 0; i < 600; i++) {
        dungeon_step(&g_scene,&g_cam,0.02f);
        check_expect(g_entity[0].y >= -0.5f && g_entity[0].y <= 0.5f,
                     "a small creature walks, attacks and idles on its floor");
    }
    check_expect(sound_busy() > 0,"a nearby creature plays its original DAGGER.SND voice");
    god_set(0); sound_stop_all();
}

static void flying_creature_floor(void)
{
    static const int kinds[2] = {3,0};
    int i;
    arena(0); g_entity[0].z = 180; g_entity[0].pose[ENTITY_WALK].h[0] = 24;
    keep_find(g_entity[0].key)->game[1] = 1;
    dungeon_open(&g_scene,kinds,g_passive,g_rate); dungeon_loaded(0);
    g_cam.y = -100; god_set(1);
    for (i = 0; i < 200; i++) {
        dungeon_step(&g_scene,&g_cam,0.02f);
        check_expect(g_entity[0].y >= -0.5f,"a flying creature's vertical pursuit cannot pass through the floor");
    }
    god_set(0);
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

void check_combat_loaded(void)
{
    const Scene *scene;
    const Entity *corpse;
    Camera cam;
    int i, j, dead, usable;
    scene = &seam_level()->scene; dead = usable = 0;
    check_doors_loaded(scene);
    check_magic_loaded();
    for (i = 0; i < dungeon_count(); i++) {
        corpse = dungeon_entity(i);
        if (!corpse->dead) { continue; }
        dead++;
        camera_set(&cam,corpse->x,corpse->y+40,corpse->z-20,0,0);
        for (j = 0; j < 80 && !corpse->live; j++) { seam_creatures(&cam); }
        seam_creatures(&cam);
        check_expect(corpse->live && corpse->act == ENTITY_DEAD && corpse->pose[ENTITY_DEAD].w[0] > 0,
                     "reloaded Privateer's Hold actually draws its saved corpse sprite");
        check_expect(!dungeon_health(i),"reloaded corpse health remains zero");
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
        for (j = 0; j < dungeon_count(); j++) {
            if (dungeon_entity(j)->key.thing == row->key.thing) { actor = dungeon_entity(j); break; }
        }
        check_expect(actor && actor->live && actor->dead && actor->x == row->x &&
                     actor->y == row->y && actor->z == row->z && actor->pose[ENTITY_DEAD].w[0] > 0,
                     "a fresh process rebuilds the saved rest encounter with its corpse art and physical position");
    }
    check_expect(scene->nmodels == scene_solid_models(scene)+ARROWS,"arrow meshes have reserved nonsolid draw slots");
    check_expect(seam_capture_world(),"the reloaded corpse can be saved again");
}

static void hold_round_trip(const char *root)
{
    const char *test_root = "build/gfall-combat-data/ARENA2";
    const Scene *scene;
    const Entity *target;
    StrikeInfo strike;
    Weather weather;
    df_u32 sword;
    float x, y, z;
    int i, j, killed, status, original, spawned;
    char resolved[4096];
    pid_t child;
    state_restore(&g_before); state_restore_world();
    check_expect(realpath(root,resolved) != 0,"resolve original assets for an isolated combat save");
    fs_set_root("build/ARENA2"); fs_make_dirs("gfall-combat-data");
    unlink(test_root); check_expect(!symlink(resolved,test_root),"mount read-only assets beside the combat fixtures");
    vfs_mount(test_root);
    if (!seam_open_hold(&x,&y,&z)) { check_expect(0,"combat scenario opens the actual Hold"); vfs_mount(root); return; }
    scene = &seam_level()->scene;
    creature_poses(seam_level());
    mem_zero(&weather,sizeof(weather));
    camera_set(&g_cam,x,y,z,0,0);
    ambience_frame(seam_level(),&g_cam,&weather,0,0,0.02f);
    check_expect(music_name()[0] != 0,"the actual Hold soundscape loads a dungeon song from MIDI.BSA");
    sword = item_create(game_state()->inventory_id,3,9,9);
    character_state()->health = character_state()->max_health;
    for (i = 0; i < SKILLS; i++) { character_state()->skill[i] = 100; }
    mem_zero(&strike,sizeof(strike)); strike.hand = 0; strike.item = sword;
    killed = 0;
    for (i = 0; i < scene->nentities && !killed; i++) {
        target = &scene->entities[i];
        camera_set(&g_cam,target->x,target->y+40,target->z-20,0,0);
        for (j = 0; j < 80 && !target->live; j++) { seam_creatures(&g_cam); }
        move_place(g_cam.x,target->y+WALK_EYE,g_cam.z,0);
        for (j = 0; j < 30 && !target->dead; j++) { dungeon_strike(scene,&g_cam,&strike); }
        killed = target->dead;
    }
    check_expect(killed,"actual Hold combat can kill a loaded creature");
    original = dungeon_count(); spawned = 0;
    for (i = 0; i < original && !spawned; i++) {
        target = dungeon_entity(i);
        move_place(target->x,target->y+WALK_EYE,target->z,0);
        move_camera(&g_cam);
        spawned = encounter_try(&g_cam);
    }
    check_expect(spawned && dungeon_count() == original+1,
                 "the actual Hold can place a rest encounter with preloaded drawable resources");
    if (spawned) {
        int slot, side, health;
        target = dungeon_entity(original); slot = target->pose[ENTITY_WALK].slot[0];
        check_expect(slot < seam_level()->nimages && seam_level()->images[slot] &&
                     target->pose[ENTITY_WALK].w[0],"a real rest creature has decoded walking art before its first frame");
        /* The hidden placement can have a wall immediately on one side.
         * Find a clear strike before hammering in one direction. */
        health = dungeon_health(original);
        god_set(1);
        for (side = 0; side < 8 && dungeon_health(original) == health; side++) {
            float yaw;

            yaw = side*(DF_TAU/8.0f);
            move_place(target->x-18*df_sin(yaw),target->y+WALK_EYE,target->z-18*df_cos(yaw),side*256);
            move_camera(&g_cam);
            dungeon_strike(scene,&g_cam,&strike);
        }
        for (j = 0; j < 200 && !target->dead; j++) { dungeon_strike(scene,&g_cam,&strike); }
        god_set(0);
        check_expect(target->dead,"the actual rest encounter leaves a persistent combat corpse");
        seam_creatures(&g_cam);
    }
    check_doors_hold(scene);
    state_capture_motion(); move_camera(&g_cam);
    state_pose(g_cam.x,g_cam.y,g_cam.z,move_yaw(),0);
    state_save_label("Combat round trip");
    check_magic_hold();
    check_expect(seam_capture_world() && save_write(5),"write actual combat health and corpse state to an isolated save");
    child = fork();
    if (!child) { execl("/proc/self/exe","gfall-test","--gfall-resume-combat",test_root,(char *)0); _exit(127); }
    status = -1;
    if (child > 0) { waitpid(child,&status,0); }
    check_expect(child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,"a fresh process loads the combat save and its corpse art");
    seam_close_hold();
    fs_delete("gfall\\saves\\S0005.SAV"); fs_remove_dir("gfall\\saves"); fs_remove_dir("gfall");
    fs_set_root("build/ARENA2"); unlink(test_root); fs_remove_dir("gfall-combat-data"); vfs_mount(root);
    state_restore(&g_before); state_restore_world();
    check_expect(seam_open_hold(&x,&y,&z),"restore the original test session after combat");
}

/* A container and what is in it. */
static void drop_bag(df_u32 bag)
{
    int i;

    for (i = 0; i < OBJECTS; i++) {
        if (game_state()->object[i].id && game_state()->object[i].parent == bag) { object_delete(game_state()->object[i].id); }
    }
    object_delete(bag);
}

/* Poisoned weapons (monster_give_equipment): none while the player is level 1; from level 2 an assassin's weapon in about 59 in a hundred, the others' in 4, the poison one of the eight, spent by weapon_poison_spend. */
static void poisoned_weapons(void)
{
    int n, poisoned, level_one, poisons[8], i, bad;
    df_u32 bag, slots[EQUIP_SLOTS];
    FighterInfo fighter;
    int kinds[2];

    kinds[0] = ENEMY_ASSASSIN; kinds[1] = ENEMY_FIRST_CLASS;
    for (i = 0; i < 8; i++) { poisons[i] = 0; }
    level_one = 0; bad = 0;
    for (n = 0; n < 100; n++) {
        bag = object_new(RECORD_CONTAINER,WORLD_ROOT,0,1);
        foe_equipment(ENEMY_ASSASSIN,bag,slots,&fighter);
        level_one += weapon_poison(slots[EQUIP_RIGHT_HAND]) != 0;
        drop_bag(bag);
    }
    check_expect(level_one == 0, "foe gear: a weapon was poisoned while the player was level 1");
    character_state()->level = POISON_WEAPON_LEVEL;
    for (i = 0; i < 2; i++) {
        poisoned = 0;
        for (n = 0; n < 1000; n++) {
            int p;

            bag = object_new(RECORD_CONTAINER,WORLD_ROOT,0,1);
            if (!bag) { bad = 1; break; }
            foe_equipment(kinds[i],bag,slots,&fighter);
            p = weapon_poison(slots[EQUIP_RIGHT_HAND]);
            if (p) { poisoned++; if (p >= POISON_FIRST && p < POISON_FIRST+POISON_WEAPON_KINDS) { poisons[p-POISON_FIRST]++; } else { bad = 1; } weapon_poison_spend(slots[EQUIP_RIGHT_HAND]); if (weapon_poison(slots[EQUIP_RIGHT_HAND])) { bad = 1; } }
            drop_bag(bag);
        }
        check_expect(!bad && (i == 0 ? poisoned > 520 && poisoned < 660 : poisoned > 15 && poisoned < 75), i == 0 ? "foe gear: an assassin's weapon is not poisoned about 59 times in a hundred" : "foe gear: another class's weapon is not poisoned about 4 times in a hundred");
    }
    bad = 0;
    for (i = 0; i < 8; i++) { bad += poisons[i] == 0; }
    check_expect(!bad, "foe gear: a weapon's poison is not each of the eight");
    {
        char text[300];
        df_u32 sword = item_create(game_state()->inventory_id,3,5,1);

        item_info(sword,text,sizeof(text));
        check_expect(!strstr(text,"Poisoned"), "item info: a clean sword says it is poisoned");
        object_edit(sword)->flags |= WEAPON_POISONED | ((df_u32)POISON_ARSENIC << WEAPON_POISON_SHIFT);
        item_info(sword,text,sizeof(text));
        check_expect(strstr(text,"Poisoned with Arsenic") != 0, "item info: a poisoned sword (1005) does not say with what");
        weapon_poison_spend(sword);
        item_info(sword,text,sizeof(text));
        check_expect(!strstr(text,"Poisoned"), "item info: a spent poison still shows");
        /* An enchanted sword that is no artifact is still told by 1001 (1012 is the artifact's, with no material). */
        {
            char plain[300];
            ItemEnchant *row = item_enchant_edit(0);
            int k;

            item_info(sword,plain,sizeof(plain));
            row->item = sword; row->identified = 1;
            for (k = 0; k < 10; k++) { row->type[k] = -1; row->param[k] = -1; }
            row->type[0] = 5; row->param[0] = 0;
            item_info(sword,text,sizeof(text));
            check_expect(item_enchanted(sword) && !magic_item_artifact(sword) && strlen(text) == strlen(plain), "item info: an enchanted sword that is no artifact is not told by record 1001");
            row->item = 0;
        }
        /* A bottle with a recipe is told by 1008 with the potion's name. */
        {
            df_u32 bottle = item_create(game_state()->inventory_id,POTION_GROUP,POTION_INDEX,-1);

            if (bottle) {
                object_edit(bottle)->message = 1;
                item_info(bottle,text,sizeof(text));
                check_expect(potion_name(0)[0] && strstr(text,potion_name(0)) != 0, "item info: a potion's bottle (1008) does not name the potion");
                drop_bag(bottle);
            }
        }
        drop_bag(sword);
    }
    character_state()->level = 1;
}

/* A creature an artifact summoned, left alone, vanishes (ai_update_creatures, flag 2). */
static void summoned_alone(void)
{
    character_state()->health = character_state()->max_health;
    arena(0);
    dungeon_next_summoned(1);
    dungeon_open(&g_scene,g_kind,g_passive,g_rate); dungeon_loaded(0);
    dungeon_next_summoned(0);
    g_entity[1].dead = 1;
    dungeon_step(&g_scene,&g_cam,0.05f);
    check_expect(!g_entity[0].live, "a summoned creature left alone did not vanish");
}

void check_combat(const char *root)
{
    df_u32 sword;
    int i;
    g_before = *game_state();
    options_close(); state_clear();
    character_state()->klass = 17; character_state()->race = 0; character_state()->level = 1;
    character_state()->health = character_state()->max_health = 100;
    character_state()->fatigue = 6400; character_state()->reflexes = 2;
    df_copy(character_state()->name,"Combat test",NAME_LEN);
    for (i = 0; i < ATTRS; i++) { character_state()->attribute[i] = 50; }
    for (i = 0; i < SKILLS; i++) { character_state()->skill[i] = 100; }
    check_expect(state_begin(),"combat test creates an independent player");
    object_edit(WORLD_ROOT)->place_key = 0x40000001u;
    sword = item_create(game_state()->inventory_id,3,5,1);
    dungeon_blood(-1,-1);
    check_doors(root);
    poisoned_weapons();
    rules(); ai_rules(); swings(sword); melee(sword); archery(); marker_pacification(); creature_teams(); injuries(); summoned_alone(); flyer_corpse(); god_swing(sword);
    small_creature_floor(); flying_creature_floor(); flight_stairwell();
    dungeon_close(); weapon_cancel(); state_restore(&g_before); state_restore_world();
    hold_round_trip(root);
}
