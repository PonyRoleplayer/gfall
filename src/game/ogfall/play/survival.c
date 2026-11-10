#include "game/ogfall/play/survival.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/cheats.h"
#include "game/ogfall/play/combat.h"
#include "game/ogfall/play/dungeon.h"
#include "game/ogfall/play/move.h"
#include "game/ogfall/play/rest.h"
#include "game/ogfall/play/msg.h"

void og_survival_step(const Scene *scene, float seconds)
{
    OgSurvival *survival;
    OgCharacter *who;
    Body *body;
    float surface;
    df_u32 color;
    int ticks, limit;

    if (!scene || seconds <= 0 || og_character()->health <= 0) { return; }
    survival = og_survival(); who = og_character(); body = og_move_body();
    survival->fraction += seconds*18.2065f;
    ticks = df_ifloor(survival->fraction); survival->fraction -= ticks; survival->ticks += ticks;
    if (!scene_water_at(scene,body->feet[0],body->feet[2],&surface,&color) ||
        body->feet[1]+body->eye_now >= surface) {
        survival->breath = 0; survival->last_tick = survival->ticks; return;
    }
    if (who->conditions & 0x80000) { return; }
    limit = who->attribute[OG_END]/2;
    if (limit > 50) { limit = 50; }
    if (og_god_mode()) { survival->breath = limit; survival->last_tick = survival->ticks; return; }
    if (!survival->breath) { survival->breath = limit; }
    if (survival->ticks-survival->last_tick > 18) {
        survival->last_tick = survival->ticks;
        if (who->race != 7 || !(survival->ticks & 1)) { survival->breath--; }
    }
    if (survival->breath <= 0) { survival->breath = 0; og_damage_player(who->health,-1); }
}

void og_survival_minutes(int count, int running, int climbing)
{
    Camera camera;
    int delta;
    if (og_god_mode() || og_state()->mode != OG_GAME_WORLD || og_character()->health <= 0) { return; }
    while (count-- > 0) {
        delta = 11;
        if (og_move_in_water() && og_character()->race != 7) {
            if (og_random_range(1,100) > og_character()->skill[17]) { delta = 44; }
            og_skill_use(17);
        }
        if (running) { delta = 88; }
        if (climbing) { delta = 22; }
        og_fatigue_spend(delta);
        if (og_character()->fatigue > 0) { continue; }
        og_move_camera(&camera);
        if (og_dungeon_nearby(&camera,1024)) {
            og_msg_set("You drop to the ground, completely exhausted.");
            og_damage_player(og_character()->health,-1); return;
        }
        /* Collapse recovery excludes another minute's fatigue and encounters,
         * as original D_001962A4 does while passing this hour. */
        og_state_pass_minutes(60); og_rest_recover();
        og_msg_set("You collapse from exhaustion and recover for an hour.");
        return;
    }
}
