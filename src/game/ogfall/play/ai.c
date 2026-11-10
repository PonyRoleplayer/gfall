#include "game/ogfall/play/ai.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/move.h"
#include "game/ogfall/tables/combatdefs.h"

int og_ai_stealth(int detected, int distance, int encountered)
{
    OgMotion motion;
    float momentum;
    int chance, base;

    if (distance > 1024) { return 0; }
    if (og_state()->minutes == og_state()->stealth_minute) { return detected; }
    og_move_capture(&motion);
    momentum = df_sqrt(motion.vx*motion.vx+motion.vz*motion.vz);
    base = og_character()->attribute[OG_SPD]+150;
    if (encountered && momentum > base/2) { return 1; }
    if (momentum <= base/2 && (og_state()->minutes & 1)) { return detected; }
    og_skill_use(16); og_state_stealth_checked();
    chance = ((og_character()->skill[16]*distance)/1024)*2;
    return og_random_range(1,100) > chance;
}

int og_ai_octant(float dx, float dz)
{
    int octant;
    octant = (df_absf(dx) > df_absf(dz))+(dz < 0 ? 2 : 0);
    return dx < 0 ? 7-octant : octant;
}

static int delta(int from, int to)
{
    return ((to-from+1024) & 2047)-1024;
}
int og_ai_angle(int from, int to)
{
    int difference;
    difference = delta(from,to);
    return difference < 0 ? -difference : difference;
}
int og_ai_turn(int from, int to, int step)
{
    int difference;
    difference = delta(from,to);
    if (difference > step) { difference = step; }
    if (difference < -step) { difference = -step; }
    return (from+difference) & 2047;
}

static int move(Body *body, const Scene *scene, int yaw, int flying,
                 const float *target, float amount, float dt)
{
    float x, z, angle, up, moved;

    x = body->feet[0]; z = body->feet[2]; angle = yaw*(DF_TAU/2048);
    if (flying) {
        up = target[1]-body->height/2-body->feet[1];
        if (up > amount) { up = amount; }
        if (up < -amount) { up = -amount; }
        body_fly_step(body,scene,df_sin(angle)*amount,up,df_cos(angle)*amount);
    } else { body_step(body,scene,df_sin(angle)*amount,df_cos(angle)*amount,dt); }
    moved = df_absf(body->feet[0]-x)+df_absf(body->feet[2]-z);
    return moved > 2*dt*60;
}

void og_ai_walk(OgMind *mind, Body *body, Entity *entity, const Scene *scene,
                 const float *target, int visible, int flying, float speed, float dt)
{
    float dx, dz, distance, amount, before, across, offset;
    int yaw, angle, difference, nav, ticks, quadrant;

    dx = target[0]-body->feet[0]; dz = target[2]-body->feet[2];
    distance = df_sqrt(dx*dx+dz*dz);
    angle = df_iround(df_atan2(dx,dz)*(2048/DF_TAU)) & 2047;
    yaw = df_iround(entity->yaw*(2048/DF_TAU)) & 2047;
    mind->clock += dt*60; ticks = df_ifloor(mind->clock); mind->clock -= ticks;
    difference = og_ai_angle(yaw,angle);
    amount = speed*dt;
    if (amount > distance-60) { amount = distance > 60 ? distance-60 : 0; }
    if (!mind->direction) { mind->direction = 2; }
    if ((visible || mind->detected) && !(mind->blocked & 1)) {
        mind->turns = 12;
        if (difference >= 32) { yaw = og_ai_turn(yaw,angle,64*ticks); amount = 0; }
        else if (!move(body,scene,yaw,flying,target,amount,dt)) {
            if (body->grounded && (mind->stuck += ticks) > 6) { mind->blocked |= 1; mind->stuck = 0; }
        } else { mind->blocked = 0; mind->stuck = 0; }
        entity->yaw = yaw*(DF_TAU/2048);
        if (!amount) { move(body,scene,yaw,flying,target,0,dt); }
        return;
    }
    if (mind->detected && !mind->turns) { mind->blocked &= ~1; }
    nav = og_foe_nav_angle(og_ai_octant(dx,dz));
    if (mind->direction == 4) { nav += 1024; }
    else if (mind->direction == 8) { nav += 1536; }
    nav &= 2047;
    /* Center on the original 64-unit corridor grid before trying another axis. */
    if ((mind->blocked & 1) && difference <= 256 && !flying) {
        quadrant = ((angle+256)/512) & 3;
        across = quadrant & 1 ? body->feet[2] : body->feet[0];
        offset = across-df_ifloor(across/64)*64;
        if (offset != 0) {
            before = across;
            offset = offset < 8 ? -offset : offset < 32 ? -8 : offset > 56 ? 64-offset : 8;
            body_step(body,scene,quadrant & 1 ? 0 : offset,quadrant & 1 ? offset : 0,dt);
            across = quadrant & 1 ? body->feet[2] : body->feet[0];
            if (df_absf(across-before) > 2) { return; }
        }
    }
    yaw = og_ai_turn(yaw,nav,64*ticks); entity->yaw = yaw*(DF_TAU/2048);
    if (og_ai_angle(yaw,nav) >= 64) { move(body,scene,yaw,flying,target,0,dt); return; }
    if (mind->turns > 0) { mind->turns -= ticks; if (mind->turns < 0) { mind->turns = 0; } }
    if (mind->blocked & mind->direction) {
        mind->direction <<= 1;
        if (mind->direction & 16) { mind->direction = 2; mind->blocked &= ~1; }
        if (visible) { mind->turns = 0; mind->blocked &= ~1; }
        move(body,scene,yaw,flying,target,0,dt);
    } else if (!move(body,scene,yaw,flying,target,amount,dt)) { mind->blocked |= mind->direction; }
}
