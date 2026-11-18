#include "game/dagger/play/character/survival.h"
#include "game/dagger/tables/magic.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/input/move.h"
#include "game/dagger/play/character/rest.h"
#include "game/dagger/play/ui/msg.h"

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
    survival->fraction += seconds*BIOS_HZ;
    ticks = df_ifloor(survival->fraction); survival->fraction -= ticks; survival->ticks += ticks;
    if (!scene_water_at(scene,body->feet[0],body->feet[2],&surface,&color) ||
        body->feet[1]+body->eye_now >= surface) {
        survival->breath = 0; survival->last_tick = survival->ticks; return;
    }
    if (who->conditions & COND_WATER_BREATHING) { return; }
    limit = who->attribute[ATTR_END]/BREATH_END_DIV;
    if (limit > BREATH_MOST) { limit = BREATH_MOST; }
    { const Membership *kynareth = guild_find_kind(TEMPLE_KYNARETH); if (kynareth) { limit += BREATH_PER_TEMPLE_RANK * kynareth->rank; } }
    if (god_mode()) { survival->breath = limit; survival->last_tick = survival->ticks; return; }
    if (!survival->breath) { survival->breath = limit; }
    if (survival->ticks-survival->last_tick > BREATH_TICKS) {
        survival->last_tick = survival->ticks;
        if (curse_race() != PLAYER_RACE_ARGONIAN || !(survival->ticks & 1)) { survival->breath--; }
    }
    if (survival->breath <= 0) { survival->breath = 0; damage_player(who->health,-1); }
}

void survival_minutes(int count, int running, int climbing)
{
    Camera camera;
    int delta;
    if (god_mode() || game_state()->mode != GAME_WORLD || character_state()->health <= 0) { return; }
    while (count-- > 0) {
        delta = FATIGUE_MOVE;
        if (move_in_water() && curse_race() != PLAYER_RACE_ARGONIAN) {
            if (random_range(1,PERCENT) > character_state()->skill[SKILL_SWIMMING]) { delta = FATIGUE_SWIM_FAIL; }
            skill_use(SKILL_SWIMMING);
        }
        if (running) { delta = FATIGUE_RUN; }
        if (climbing) { delta = FATIGUE_CLIMB; }
        fatigue_spend(delta);
        if (character_state()->fatigue > 0) { continue; }
        move_camera(&camera);
        if (dungeon_nearby(&camera,ENEMIES_NEAR)) {
            msg_set("You drop to the ground, completely exhausted.");
            damage_player(character_state()->health,-1); return;
        }
        /* Collapse recovery excludes another minute's fatigue and encounters,
         * as original D_001962A4 does while passing this hour. */
        state_pass_minutes(COLLAPSE_MINUTES); rest_recover();
        msg_set("You collapse from exhaustion and recover for an hour.");
        return;
    }
}
