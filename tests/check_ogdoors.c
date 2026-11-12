#include "check_ogdoors.h"
#include "check_ogactions.h"
#include "check_ogprogress.h"
#include "check_ogsurvival.h"
#include "check_ogencounter.h"
#include "check_ogmagic.h"
#include "check_util.h"
#include "game/dagger/play/activate.h"
#include "game/dagger/play/doors.h"
#include "game/dagger/play/dungeon.h"
#include "game/dagger/play/hud.h"
#include "game/dagger/play/save.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/tables/doordefs.h"
#include "engine/core/mem.h"
#include "engine/platform/fs.h"
#include "engine/scene/interact.h"
#include <string.h>

static OgState g_before;
static Scene g_scene;
static Mover g_door;
static Usable g_use;
static Camera g_camera;

static void fixture(int lock)
{
    mem_zero(&g_scene,sizeof(g_scene));
    mem_zero(&g_door,sizeof(g_door));
    mem_zero(&g_use,sizeof(g_use));
    g_scene.movers = &g_door; g_scene.nmovers = 1;
    g_scene.usables = &g_use; g_scene.nusables = 1;
    g_use.kind = USE_MOVER; g_use.model = g_use.sprite = -1;
    g_use.mover = 0; g_use.label = "door"; g_use.reach = 128;
    g_use.cy = 60; g_use.cz = 60; g_use.nz = 1;
    g_use.hw = 40; g_use.hh = 60; g_use.hd = 2;
    g_door.door = 1; g_door.lock = lock; g_door.next = -1;
    g_door.secs = 0.5f; g_door.ry = DF_TAU/4; g_door.z0 = 60;
    g_door.key.place = 50050; g_door.key.thing = 0x1234;
    mover_init(&g_door,1);
    camera_set(&g_camera,0,60,0,0,0);
    keep_clear();
}

static void choose_roll(int low, int high, int value)
{
    df_u32 seed;
    for (seed = 1; seed < 10000; seed++) {
        og_random_seed(seed);
        if (og_random_range(low,high) == value) { og_random_seed(seed); return; }
    }
    check_expect(0,"a deterministic original-RNG door roll exists");
}

static void pick(void)
{
    og_activate(&g_scene,&g_camera,OG_MODE_STEAL,0,0,1,160,100);
}

static void lockpicking(void)
{
    df_u32 random, uses;
    const Keep *row;

    check_expect(og_lock_chance(50,5,10) == 25 && og_lock_chance(0,1,19) == 5 &&
                 og_lock_chance(100,30,2) == 95 && !og_lock_chance(100,30,20),
                 "original dungeon picking includes level, clamps 5..95 and refuses magical locks");
    check_expect(!strcmp(og_lock_hint(50,10),"This lock has nothing to fear from you...") &&
                 !strcmp(og_lock_hint(100,3),"You see a pathetic excuse for a lock...") &&
                 !strcmp(og_lock_hint(500,1),"This lock is an insult to your abilities...") &&
                 !strcmp(og_lock_hint(100,20),"This is a magically held lock..."),
                 "lock hints use the original text, including safe boosted-skill bounds");
    fixture(10); og_character()->skill[13] = 50; og_character()->level = 5;
    random = og_state()->random; uses = og_state()->skill_uses[13];
    og_activate(&g_scene,&g_camera,OG_MODE_GRAB,0,0,1,160,100);
    check_expect(!mover_at_end(&g_door) && random == og_state()->random &&
                 uses == og_state()->skill_uses[13] &&
                 !strcmp(interact_saying(0),og_lock_hint(50,10)),
                 "Grab reports difficulty without consuming a lockpick roll or training");
    choose_roll(0,100,26); pick();
    check_expect(g_door.lock == 10 && !mover_at_end(&g_door) &&
                 og_state()->skill_uses[13] == uses+1,"failed picking trains once and keeps the lock shut");
    random = og_state()->random; pick();
    check_expect(og_state()->random == random && og_state()->skill_uses[13] == uses+1,
                 "a failed lock cannot be retried at the same Lockpicking skill");
    keep_save(&g_scene,0,1,0,0,0,0,0);
    row = keep_find(g_door.key);
    check_expect(row && row->lock == 10 && row->game[0],"a failed attempt keeps game bytes without unlocking the door");
    check_expect(og_state_capture_world() && og_save_write(6),"save a failed lockpick in an isolated OGFALL save");
    keep_clear();
    check_expect(og_save_read(6) && og_state_restore_world(),"reload failed lockpick state from the save codec");
    mover_init(&g_door,1); keep_restore(&g_scene,0,1,0,0,0,0,0);
    random = og_state()->random; pick();
    check_expect(g_door.lock == 10 && random == og_state()->random,"loading cannot bypass the failed-attempt restriction");
    og_character()->skill[13]++;
    choose_roll(0,100,26); pick();
    check_expect(!g_door.lock && mover_at_end(&g_door) &&
                 !strcmp(interact_saying(0),og_lock_text(OG_LOCK_OPEN)),
                 "a changed skill permits another attempt and a boundary roll unlocks and opens");
    random = og_state()->random; pick();
    check_expect(og_state()->random == random,"using a moving door cannot consume another roll");
    mover_step(&g_scene,1); pick(); mover_step(&g_scene,1);
    check_expect(!mover_at_end(&g_door) && !g_door.lock,"a picked door can be closed without relocking");
    keep_save(&g_scene,0,1,0,0,0,0,0);
    check_expect(og_state_capture_world() && og_save_write(6),"save a picked, closed door");
    fixture(10);
    check_expect(og_save_read(6) && og_state_restore_world(),"load a picked door through the save codec");
    keep_restore(&g_scene,0,1,0,0,0,0,0);
    check_expect(!g_door.lock && !mover_at_end(&g_door),"a picked door stays unlocked even when saved closed");
    g_door.lock = 10; choose_roll(0,100,0); pick();
    check_expect(!g_door.lock && mover_at_end(&g_door),
                 "a successful pick does not count as a failed attempt if a later action relocks the door");
    fixture(20); random = og_state()->random; uses = og_state()->skill_uses[13]; pick();
    check_expect(g_door.lock == 20 && random == og_state()->random && uses == og_state()->skill_uses[13],
                 "a magically held door cannot be picked or used for skill training");
}

static void bashing(void)
{
    OgStrike strike;
    df_u32 random, uses;
    Mesh wall;
    Tri face;
    Instance model;

    check_expect(og_bash_chance(2) == 18 && og_bash_chance(19) == 1 &&
                 !og_bash_chance(20) && !og_bash_chance(0),"original bash chance is 20 minus the lock rating");
    fixture(19); mem_zero(&strike,sizeof(strike)); strike.hand = 0;
    og_dungeon_open(&g_scene,0,0,0);
    uses = og_state()->skill_uses[16]; choose_roll(1,100,2);
    og_dungeon_strike(&g_scene,&g_camera,&strike);
    check_expect(g_door.lock == 19 && og_state()->skill_uses[16] == uses+1,
                 "a missed melee hit reaches a door and trains Stealth on a failed bash");
    choose_roll(1,100,1); og_dungeon_strike(&g_scene,&g_camera,&strike);
    check_expect(!g_door.lock && mover_at_end(&g_door),"the original inclusive bash roll breaks and opens a lock");
    fixture(20); random = og_state()->random;
    og_door_strike(&g_scene,&g_camera);
    check_expect(g_door.lock == 20 && random == og_state()->random,"magical locks cannot be bashed open");
    fixture(2); g_camera.z = -100; random = og_state()->random;
    check_expect(!og_door_strike(&g_scene,&g_camera) && random == og_state()->random,"bashing cannot reach a distant door");
    fixture(2); g_door.door = 0;
    check_expect(!og_door_use(&g_scene,0,OG_MODE_STEAL) && !og_door_strike(&g_scene,&g_camera),
                 "door rules do not turn a lever or lift into a lockpick target");
    fixture(2); mem_zero(&wall,sizeof(wall)); mem_zero(&face,sizeof(face)); mem_zero(&model,sizeof(model));
    face.x[0] = -100; face.y[0] = -100; face.z[0] = 30;
    face.x[1] = 100; face.y[1] = -100; face.z[1] = 30;
    face.x[2] = 0; face.y[2] = 200; face.z[2] = 30;
    wall.tris = &face; wall.ntris = 1; wall.cz = 30; wall.ex = wall.ey = 200;
    model.mesh = &wall; model.m[0] = model.m[4] = model.m[8] = 1;
    g_scene.models = &model; g_scene.nmodels = 1; random = og_state()->random;
    check_expect(!og_door_strike(&g_scene,&g_camera) && random == og_state()->random,
                 "a nearer wall blocks bashing without consuming a roll");
    og_dungeon_close();
}

static void keys_and_relocking(void)
{
    df_u32 key, bag, random, uses;
    OgObject *item;

    fixture(10);
    check_expect(og_door_lock(&g_door,17) && og_door_unlock(&g_door) && !g_door.lock,
                 "an action can temporarily unlock a changed lock rating");
    check_expect(og_state_capture_world() && og_save_write(6) && og_save_read(6) && og_state_restore_world(),
                 "a door's underlying rating survives the save codec while unlocked");
    check_expect(og_door_relock(&g_door) && g_door.lock == 17,
                 "relocking restores the last action rating rather than the file's starting lock");
    fixture(10); uses = og_state()->skill_uses[13];
    bag = og_object_new(OG_RECORD_CONTAINER,og_state()->inventory_id,0,100);
    key = og_item_create(bag,27,3,0); item = og_object_edit(key);
    check_expect(item != 0,"create the original unshipped key-slot fixture inside a carried container");
    if (item) {
        random = og_state()->random;
        item->value = 50050; item->message = 10;
        og_door_use(&g_scene,0,OG_MODE_INFO);
        check_expect(!mover_at_end(&g_door) && og_object(key),
                     "matching a key's message to the lock level does not satisfy the original location check");
        item->message = 50050;
        og_door_use(&g_scene,0,OG_MODE_TALK);
        check_expect(mover_at_end(&g_door) && !g_door.lock && !og_object(key) &&
                     og_state()->random == random && og_state()->skill_uses[13] == uses,
                     "any non-Steal mode consumes a matching carried key without a lockpick roll");
    }
    og_object_delete(bag);
}

static void magical_locks(void)
{
    int level;
    level = og_character()->level;
    og_character()->level = 40; fixture(10);
    og_magic_apply(18,-1,-1,40,1); choose_roll(1,100,1);
    og_door_use(&g_scene,0,OG_MODE_GRAB);
    check_expect(!g_door.lock && mover_at_end(&g_door) && !(og_character()->conditions & 64),
                 "Open unlocks a pointed door and consumes its one-use condition");
    mover_step(&g_scene,1);
    og_magic_apply(19,-1,-1,40,1); choose_roll(1,100,1);
    og_door_use(&g_scene,0,OG_MODE_GRAB);
    check_expect(g_door.lock == 40 && !mover_at_end(&g_door),"Wizard Lock closes an open door and locks it at the caster's level");
    check_expect(og_state_capture_world() && og_save_write(6) && og_save_read(6),"magical door ratings and the remaining Lock condition survive save/load");
    og_magic_remove(-1,-1); og_character()->level = level;
}

void check_ogdoors(const char *root)
{
    og_state_capture_world(); g_before = *og_state();
    check_ogmagic_prepare();
    fs_set_root("build/ARENA2");
    check_expect(fs_make_dirs("ogfall-door-data"),"create an isolated door-save fixture");
    fs_set_root("build/ogfall-door-data/ARENA2");
    lockpicking(); bashing(); keys_and_relocking(); magical_locks();
    check_ogactions(); check_ogprogress(); check_ogsurvival(); check_ogencounter(); check_ogmagic();
    fs_delete("ogfall\\saves\\S0006.SAV");
    fs_remove_dir("ogfall\\saves"); fs_remove_dir("ogfall");
    fs_set_root("build/ARENA2"); fs_remove_dir("ogfall-door-data"); fs_set_root(root);
    og_state_restore(&g_before); og_state_restore_world();
}

/* These two checks surround the existing fresh-process Hold combat save.
 * Exercise actual builder keys, not only a synthetic mover's codec row. */
void check_ogdoors_hold(const Scene *scene)
{
    Mover *door;
    int i, count, skill;

    count = 0; skill = og_character()->skill[13];
    for (i = 0; i < scene->nusables && count < 2; i++) {
        if (scene->usables[i].kind != USE_MOVER) { continue; }
        door = &scene->movers[scene->usables[i].mover];
        if (!door->door || door->teleport || mover_at_end(door)) { continue; }
        /* The Hold need not ship with locked doors. Set a runtime lock, as a
         * dungeon action would, to test its real model/key through rebuilding. */
        door->lock = 2;
        og_character()->skill[13] = 50;
        choose_roll(0,100,count ? 100 : 0);
        og_door_use(scene,i,OG_MODE_STEAL);
        if (!count) {
            check_expect(!door->lock && mover_at_end(door),"a lock on an actual Hold door can be picked and opened");
        } else {
            check_expect(door->lock > 0 && !mover_at_end(door),"an actual Hold lock remembers a failed attempt");
        }
        count++;
    }
    og_character()->skill[13] = skill;
    check_expect(count == 2,"the Hold fixture exercises both a picked and a failed lock");
}

void check_ogdoors_loaded(const Scene *scene)
{
    const Keep *row;
    const Mover *door;
    df_u32 random;
    int i, picked, failed, skill;

    picked = failed = 0; skill = og_character()->skill[13];
    for (i = 0; i < scene->nusables; i++) {
        if (scene->usables[i].kind != USE_MOVER) { continue; }
        door = &scene->movers[scene->usables[i].mover]; row = keep_find(door->key);
        if (!door->door || !row || !row->game[0]) { continue; }
        if (!row->lock) {
            picked++;
            check_expect(!door->lock && mover_at_end(door),"a fresh process restores the Hold's picked-open door");
        } else {
            failed++; og_character()->skill[13] = 50; random = og_state()->random;
            og_door_use(scene,i,OG_MODE_STEAL);
            check_expect(door->lock == row->lock && og_state()->random == random,
                         "a fresh process retains the Hold door's failed-attempt restriction");
        }
    }
    og_character()->skill[13] = skill;
    check_expect(picked == 1 && failed == 1,"fresh-process Hold load finds both persistent lock outcomes");
}
