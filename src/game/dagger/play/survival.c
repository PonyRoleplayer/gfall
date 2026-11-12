#include "game/dagger/play/survival.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/dungeon.h"
#include "game/dagger/play/move.h"
#include "game/dagger/play/rest.h"
#include "game/dagger/play/msg.h"

void survival_step(const Scene *scene, float seconds)
{
    Survival *survival;
    CharacterState *who;
    Body *body;
    float surface;
    df_u32 color;
    int ticks, limit;

    if (!scene || seconds <= 0 || character_state()->health <= 0) { return; }
    survival = survival_state(); who = character_state(); body = move_body();
    survival->fraction += seconds*18.2065f;
    ticks = df_ifloor(survival->fraction); survival->fraction -= ticks; survival->ticks += ticks;
    if (!scene_water_at(scene,body->feet[0],body->feet[2],&surface,&color) ||
        body->feet[1]+body->eye_now >= surface) {
        survival->breath = 0; survival->last_tick = survival->ticks; return;
    }
    if (who->conditions & 0x80000) { return; }
    limit = who->attribute[ATTR_END]/2;
    if (limit > 50) { limit = 50; }
    if (god_mode()) { survival->breath = limit; survival->last_tick = survival->ticks; return; }
    if (!survival->breath) { survival->breath = limit; }
    if (survival->ticks-survival->last_tick > 18) {
        survival->last_tick = survival->ticks;
        if (who->race != 7 || !(survival->ticks & 1)) { survival->breath--; }
    }
    if (survival->breath <= 0) { survival->breath = 0; damage_player(who->health,-1); }
}

void survival_minutes(int count, int running, int climbing)
{
    Camera camera;
    int delta;
    if (god_mode() || game_state()->mode != GAME_WORLD || character_state()->health <= 0) { return; }
    while (count-- > 0) {
        delta = 11;
        if (move_in_water() && character_state()->race != 7) {
            if (random_range(1,100) > character_state()->skill[17]) { delta = 44; }
            skill_use(17);
        }
        if (running) { delta = 88; }
        if (climbing) { delta = 22; }
        fatigue_spend(delta);
        if (character_state()->fatigue > 0) { continue; }
        move_camera(&camera);
        if (dungeon_nearby(&camera,1024)) {
            msg_set("You drop to the ground, completely exhausted.");
            damage_player(character_state()->health,-1); return;
        }
        /* Collapse recovery excludes another minute's fatigue and encounters,
         * as original D_001962A4 does while passing this hour. */
        state_pass_minutes(60); rest_recover();
        msg_set("You collapse from exhaustion and recover for an hour.");
        return;
    }
}
