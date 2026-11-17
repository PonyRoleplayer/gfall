#include "game/dagger/play/combat/ai.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/magic.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/input/move.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/ui/hud.h"
#include "game/dagger/tables/combatdefs.h"

int ai_stealth(int detected, int distance, int encountered)
{
    MotionState motion;
    float momentum;
    int chance, base;

    if (distance > STEALTH_RANGE) { return 0; }
    if (game_state()->minutes == game_state()->stealth_minute) { return detected; }
    move_capture(&motion);
    momentum = df_sqrt(motion.vx*motion.vx+motion.vz*motion.vz);
    base = character_state()->attribute[ATTR_SPD]+WALK_SPEED_ADD;
    if (encountered && momentum > base/STEALTH_MOMENTUM_DIV) { return 1; }
    if (momentum <= base/STEALTH_MOMENTUM_DIV && (game_state()->minutes & 1)) { return detected; }
    skill_use(SKILL_STEALTH); state_stealth_checked();
    chance = ((character_state()->skill[SKILL_STEALTH]*distance)/STEALTH_RANGE)*STEALTH_MULTIPLIER;
    return random_range(1,100) > chance;
}

/* The reseed hash of a marker's roll (ours). */
#define PACIFY_SEED_MIX 0x9e3779b9u
#define PACIFY_SEED_MUL 1103515245u
#define PACIFY_SEED_ADD 12345u

/* monster_pacify_check: a drawn weapon sets the base to -25. Only
 * creatures with a language train that skill, even when the roll fails. */
int ai_marker_pacify(int kind, KeepKey key)
{
    const CharacterState *who;
    int chance, language, roll;
    df_u32 saved;

    who = character_state();
    chance = (hud_weapon_ready() ? PACIFY_BASE_ARMED : PACIFY_BASE) + who->attribute[ATTR_PER]/PACIFY_PER_DIV;
    if (ENEMY_IS_CLASS(kind)) { chance += who->skill[SKILL_ETIQUETTE]/PACIFY_ETIQUETTE_DIV; }
    else {
        language = foe_language(kind);
        if (language) {
            chance += who->skill[language];
            skill_add_uses(language,1);
        }
        chance += magic_chance(-1,GROUP_COMPREHEND_LANGUAGES,-1);
    }
    /* The original reseeds for each marker. Our stable identity supplies
     * that seed, independent of the order in which its artwork becomes ready. */
    saved = game_state()->random;
    random_seed((key.thing ^ key.place*PACIFY_SEED_MIX) * PACIFY_SEED_MUL + PACIFY_SEED_ADD);
    roll = random_range(1,PACIFY_ROLL);
    random_seed(saved);
    return roll <= chance;
}

int ai_octant(float dx, float dz)
{
    int octant;
    octant = (df_absf(dx) > df_absf(dz))+(dz < 0 ? 2 : 0);
    return dx < 0 ? OCTANTS-1-octant : octant;
}

static int delta(int from, int to)
{
    return ((to-from+ANGLE_HALF) & ANGLE_MASK)-ANGLE_HALF;
}
int ai_angle(int from, int to)
{
    int difference;
    difference = delta(from,to);
    return difference < 0 ? -difference : difference;
}
int ai_turn(int from, int to, int step)
{
    int difference;
    difference = delta(from,to);
    if (difference > step) { difference = step; }
    if (difference < -step) { difference = -step; }
    return (from+difference) & ANGLE_MASK;
}

static int move(Body *body, const Scene *scene, int yaw, int flying,
                 const float *target, float amount, float dt)
{
    float x, z, angle, up, moved;

    x = body->feet[0]; z = body->feet[2]; angle = yaw*(DF_TAU/ANGLE_FULL);
    if (flying) {
        up = target[1]-body->height/2-body->feet[1];
        if (up > amount) { up = amount; }
        if (up < -amount) { up = -amount; }
        body_fly_step(body,scene,df_sin(angle)*amount,up,df_cos(angle)*amount);
    } else { body_step(body,scene,df_sin(angle)*amount,df_cos(angle)*amount,dt); }
    moved = df_absf(body->feet[0]-x)+df_absf(body->feet[2]-z);
    return moved > STEP_MOVED_MIN*dt*TICKS_PER_SECOND;
}

void ai_walk(Mind *mind, Body *body, Entity *entity, const Scene *scene,
                 const float *target, int visible, int flying, float speed, float dt)
{
    float dx, dz, distance, amount, before, across, offset;
    int yaw, angle, difference, nav, ticks, quadrant;

    dx = target[0]-body->feet[0]; dz = target[2]-body->feet[2];
    distance = df_sqrt(dx*dx+dz*dz);
    angle = df_iround(df_atan2(dx,dz)*(ANGLE_FULL/DF_TAU)) & ANGLE_MASK;
    yaw = df_iround(entity->yaw*(ANGLE_FULL/DF_TAU)) & ANGLE_MASK;
    mind->clock += dt*TICKS_PER_SECOND; ticks = df_ifloor(mind->clock); mind->clock -= ticks;
    difference = ai_angle(yaw,angle);
    amount = speed*dt;
    if (amount > distance-WALK_STOP_SHORT) { amount = distance > WALK_STOP_SHORT ? distance-WALK_STOP_SHORT : 0; }
    if (!mind->direction) { mind->direction = NAV_DIR_AHEAD; }
    if ((visible || mind->detected) && !(mind->blocked & 1)) {
        mind->turns = NAV_TURNS;
        if (difference >= NAV_TURN_FIRST_ARC) { yaw = ai_turn(yaw,angle,FOE_TURN_PER_TICK*ticks); amount = 0; }
        else if (!move(body,scene,yaw,flying,target,amount,dt)) {
            if (body->grounded && (mind->stuck += ticks) > NAV_STUCK_STEPS) { mind->blocked |= 1; mind->stuck = 0; }
        } else { mind->blocked = 0; mind->stuck = 0; }
        entity->yaw = yaw*(DF_TAU/ANGLE_FULL);
        if (!amount) { move(body,scene,yaw,flying,target,0,dt); }
        return;
    }
    if (mind->detected && !mind->turns) { mind->blocked &= ~1; }
    nav = foe_nav_angle(ai_octant(dx,dz));
    if (mind->direction == NAV_DIR_BACK) { nav += NAV_BACK_OFFSET; }
    else if (mind->direction == NAV_DIR_SIDE) { nav += NAV_SIDE_OFFSET; }
    nav &= ANGLE_MASK;
    /* Center on the original 64-unit corridor grid before trying another axis. */
    if ((mind->blocked & 1) && difference <= NAV_CENTRE_ARC && !flying) {
        quadrant = ((angle+NAV_CENTRE_ARC)/ANGLE_QUARTER) & 3;
        across = quadrant & 1 ? body->feet[2] : body->feet[0];
        offset = across-df_ifloor(across/NAV_GRID)*NAV_GRID;
        if (offset != 0) {
            before = across;
            offset = offset < NAV_SNAP ? -offset : offset < NAV_GRID_HALF ? -NAV_SNAP : offset > NAV_SNAP_FAR ? NAV_GRID-offset : NAV_SNAP;
            body_step(body,scene,quadrant & 1 ? 0 : offset,quadrant & 1 ? offset : 0,dt);
            across = quadrant & 1 ? body->feet[2] : body->feet[0];
            if (df_absf(across-before) > 2) { return; }
        }
    }
    yaw = ai_turn(yaw,nav,FOE_TURN_PER_TICK*ticks); entity->yaw = yaw*(DF_TAU/ANGLE_FULL);
    if (ai_angle(yaw,nav) >= FOE_TURN_PER_TICK) { move(body,scene,yaw,flying,target,0,dt); return; }
    if (mind->turns > 0) { mind->turns -= ticks; if (mind->turns < 0) { mind->turns = 0; } }
    if (mind->blocked & mind->direction) {
        mind->direction <<= 1;
        if (mind->direction & NAV_DIR_WRAP) { mind->direction = NAV_DIR_AHEAD; mind->blocked &= ~1; }
        if (visible) { mind->turns = 0; mind->blocked &= ~1; }
        move(body,scene,yaw,flying,target,0,dt);
    } else if (!move(body,scene,yaw,flying,target,amount,dt)) { mind->blocked |= mind->direction; }
}
