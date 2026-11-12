#include "game/dagger/play/doors.h"
#include "game/dagger/play/hud.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/dungeon.h"
#include "game/dagger/play/actions.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/tables/doordefs.h"
#include "engine/scene/interact.h"
#include "engine/scene/keep.h"

#define LOCK_ATTEMPT 0xd1

int lock_chance(int skill, int level, int lock)
{
    int chance;

    if (lock <= 0 || lock >= 20) { return 0; }
    chance = skill+(level-lock)*5;
    return chance < 5 ? 5 : chance > 95 ? 95 : chance;
}

int bash_chance(int lock)
{
    return lock > 0 && lock < 20 ? 20-lock : 0;
}

static Mover *door_at(const Scene *scene, int usable)
{
    const Usable *u;

    if (!scene || usable < 0 || usable >= scene->nusables) { return 0; }
    u = &scene->usables[usable];
    if (u->kind != USE_MOVER || u->mover < 0 || u->mover >= scene->nmovers) { return 0; }
    return scene->movers[u->mover].door ? &scene->movers[u->mover] : 0;
}

static int tried(const Mover *door, int skill)
{
    const Keep *row;
    df_u32 previous;
    int i;

    /* Original records begin with lockpick_skill_tried == 0. */
    if (!skill) { return 1; }
    row = keep_find(door->key);
    if (!row || row->kind != KEEP_MOVER || row->game[0] != LOCK_ATTEMPT || !row->game[1]) { return 0; }
    previous = 0;
    for (i = 0; i < 4; i++) { previous |= (df_u32)row->game[i+4] << (i*8); }
    return previous == (df_u32)skill;
}

static Keep *door_state(const Mover *door)
{
    Keep *row;

    if (!door->key.place && !door->key.thing) { return 0; }
    row = keep_get(door->key,KEEP_MOVER);
    if (!row) { return 0; }
    row->lock = door->lock;
    row->at_end = (df_u8)(!door->teleport && mover_at_end(door));
    row->game[0] = LOCK_ATTEMPT;
    return row;
}

int door_lock(Mover *door, int rating)
{
    Keep *row;
    if (!door || rating < 0 || rating > 65535) { return 0; }
    row = door_state(door);
    if (!row) { return 0; }
    row->game[2] = (df_u8)rating; row->game[3] = (df_u8)(rating >> 8);
    row->lock = door->lock = rating;
    return 1;
}

int door_unlock(Mover *door)
{
    Keep *row;
    if (!door) { return 0; }
    row = door_state(door);
    if (!row) { return 0; }
    if (door->lock) {
        row->game[2] = (df_u8)door->lock; row->game[3] = (df_u8)(door->lock >> 8);
    }
    row->lock = door->lock = 0;
    return 1;
}

int door_relock(Mover *door)
{
    const Keep *row;
    int rating;
    if (!door) { return 0; }
    row = keep_find(door->key);
    rating = row && row->game[0] == LOCK_ATTEMPT ? df_ru16(row->game+2) : 0;
    if (!rating) { rating = door->lock ? door->lock : door->lock0; }
    return door_lock(door,rating);
}

static df_u32 find_key(const Mover *door)
{
    const ObjectInfo *item, *owner;
    int i, depth;
    df_u32 location;
    location = door->key.place & 65535u;
    for (i = 0; i < OBJECTS; i++) {
        item = &game_state()->object[i];
        /* door_key_match_cb compares both fields with the location ID.
         * Its lock-level argument is unused; 27/3 is an unshipped item slot. */
        if (!item->id || item->type != RECORD_ITEM || item->group != 27 || item->index != 3 ||
            item->value != (int)location || (df_u16)item->message != location) { continue; }
        owner = item;
        for (depth = 0; owner && depth < OBJECTS; depth++) {
            if (owner->parent == game_state()->entity_id) { return item->id; }
            owner = object_at(owner->parent);
        }
    }
    return 0;
}

int door_use(const Scene *scene, int usable, int mode)
{
    Mover *door;
    Keep *row;
    df_u32 key;
    int mover, skill, chance, i;

    if (mode < MODE_GRAB || mode > MODE_TALK) { return 0; }
    door = door_at(scene,usable);
    if (!door) { return 0; }
    mover = scene->usables[usable].mover;
    if (mover_busy(scene,mover)) { return 1; }
    if ((character_state()->conditions & 32) && (door->lock <= 0 || mover_at_end(door))) {
        if (mover_at_end(door)) {
            if (mover_start(scene,mover) && random_range(1,100) <= magic_chance(-1,16,-1)) {
                door_lock(door,character_state()->level);
            }
            return 1;
        }
        if (random_range(1,100) <= magic_chance(-1,16,-1)) {
            door_lock(door,character_state()->level); return 1;
        }
    }
    if (door->lock <= 0 || mover_at_end(door) || !interact_locks()) {
        if (interact_use(scene,usable)) {
            dungeon_alert(-1);
            actions_trigger(scene,scene->usables[usable].model,2);
        }
        return 1;
    }
    key = find_key(door);
    if (key) {
        if (mover_start(scene,mover) && door_unlock(door)) { object_delete(key); }
        return 1;
    }
    skill = character_state()->skill[13];
    if (mode != MODE_STEAL) {
        if (character_state()->conditions & 64) {
            chance = magic_chance(-1,17,-1); magic_remove(-1,17);
            if (random_range(1,100) <= chance && mover_start(scene,mover)) {
                door_unlock(door); dungeon_alert(-1);
            } else { interact_say("The spell failed."); }
            return 1;
        }
        interact_say(lock_hint(skill,door->lock));
        return 1;
    }
    chance = lock_chance(skill,character_state()->level,door->lock);
    if (!chance || tried(door,skill)) {
        interact_say(lock_text(LOCK_FAILED));
        if (!chance) { actions_trigger(scene,scene->usables[usable].model,4); }
        return 1;
    }
    /* Secure the attempt before rolling, so a failed allocation cannot give
     * unlimited retries or leave a lock-changing row with an uninitialized lock. */
    row = door_state(door);
    if (!row) {
        interact_say("Cannot remember this lock.");
        return 1;
    }
    skill_use(13);
    if (random_range(0,100) <= chance) {
        door_unlock(door);
        mover_start(scene,mover);
        sound_at(60,door->x0,door->y0,door->z0);
        interact_say(lock_text(LOCK_OPEN));
    } else {
        /* Only failure updates lockpick_skill_tried. A successful pick must
         * not become a failed attempt if a later action relocks the door. */
        row->game[1] = 1;
        for (i = 0; i < 4; i++) { row->game[i+4] = (df_u8)((df_u32)skill >> (i*8)); }
        interact_say(lock_text(LOCK_FAILED));
    }
    row->lock = door->lock;
    row->at_end = (df_u8)(!door->teleport && mover_at_end(door));
    /* Trigger callbacks can grow Keep: do not retain row across this call. */
    if (!door->lock) { dungeon_alert(-1); actions_trigger(scene,scene->usables[usable].model,7); }
    else { actions_trigger(scene,scene->usables[usable].model,4); }
    return 1;
}

int door_strike(const Scene *scene, const Camera *camera)
{
    UseHit hit;
    Mover *door;
    float dx, dz;
    int mover, chance;

    if (!camera) { return 0; }
    if (!interact_aim(scene,camera,&hit)) { return 0; }
    door = door_at(scene,hit.usable);
    if (!door) { return 0; }
    dx = door->x0-camera->x; dz = door->z0-camera->z;
    if (dx*dx+dz*dz > 100*100) { return 0; }
    mover = scene->usables[hit.usable].mover;
    if (mover_busy(scene,mover)) { return 1; }
    if (door->lock <= 0 || mover_at_end(door)) {
        interact_use(scene,hit.usable);
        return 1;
    }
    /* weapon_melee_strike trains Stealth, even at a magically held door. */
    skill_use(16);
    dungeon_alert(-1);
    chance = bash_chance(door->lock);
    if (chance && random_range(1,100) <= chance && mover_start(scene,mover)) {
        door_unlock(door);
    }
    sound_at(9,door->x0,door->y0,door->z0);
    return 1;
}
