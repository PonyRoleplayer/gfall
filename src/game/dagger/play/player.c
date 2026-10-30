#include "game/dagger/play/player.h"
#include "game/dagger/play/rules.h"
#include "engine/core/df.h"
#include "engine/gfx/rig.h"
#include "engine/scene/walker.h"

/* See player.h. */

/* Radians a mouse count turns; looking up or down stops short of straight. */
#define LOOK_SENS 0.003f
#define PITCH_LIMIT 1.55334303f
/* Both keys at once go no faster than one: DFU's limitDiagonalSpeed. */
#define DIAGONAL 0.70710678f

void player_init(Player *p, const Character *sheet)
{
    body_init(&p->body, WALK_RADIUS, WALK_HEIGHT, WALK_STEP, WALK_EYE);
    /* Classic falls faster than the engine's 9.8: DFU's 20 m/s^2, so a jump is classic's in height and in time. */
    p->body.gravity = rules_gravity() * RULES_UNITS_PER_M;
    p->yaw = 0.0f;
    p->pitch = 0.0f;
    p->sheet = *sheet;
    inventory_clear(&p->pack);
    p->flying = 0;
    p->god = 0;
}

int player_hurt(Player *p, int amount)
{
    if (amount <= 0 || p->god) {
        return -1;
    }
    return character_hurt(&p->sheet, amount);
}

void player_place(Player *p, float x, float eye_y, float z, float yaw, float pitch)
{
    body_place(&p->body, x, eye_y - p->body.eye, z);
    p->yaw = df_wrap_tau(yaw);
    p->pitch = pitch;
}

void player_step(Player *p, const Input *in, const Scene *scene, float dt, int collide)
{
    float pitch;
    float mps;
    float step;
    float f;
    float r;
    float sy;
    float cy;

    p->yaw = df_wrap_tau(p->yaw + (float)in->look_dx * LOOK_SENS);
    pitch = p->pitch - (float)in->look_dy * LOOK_SENS;
    p->pitch = pitch > PITCH_LIMIT ? PITCH_LIMIT : (pitch < -PITCH_LIMIT ? -PITCH_LIMIT : pitch);

    f = (float)in->move_f;
    r = (float)in->move_r;
    if (f != 0.0f && r != 0.0f) {
        f *= DIAGONAL;
        r *= DIAGONAL;
    }
    mps = (in->held & INPUT_BIT(INPUT_RUN))
              ? rules_run_speed(p->sheet.attribute[ATTR_SPEED], p->sheet.skill[SKILL_RUNNING])
              : rules_walk_speed(p->sheet.attribute[ATTR_SPEED]);
    step = mps * RULES_UNITS_PER_M * dt;
    /* The camera's basis: forward is (sin yaw, cos yaw), right is (cos yaw, -sin yaw). */
    sy = df_sin(p->yaw);
    cy = df_cos(p->yaw);
    if (!collide) {
        float x;
        float y;
        float z;
        float sp;
        float cp;

        /*
         * Collision off (tcl 0): the eye flies along the view, pitch and
         * all, through anything, Q up and E down; no gravity, no floor.
         * The body is set at it, not walked: back on, it starts there,
         * out of whatever it is caught in.
         */
        sp = df_sin(p->pitch);
        cp = df_cos(p->pitch);
        body_eye(&p->body, &x, &y, &z);
        x += (sy * cp * f + cy * r) * step;
        y += (sp * f + (float)in->move_y) * step;
        z += (cy * cp * f - sy * r) * step;
        body_place(&p->body, x, y - p->body.eye, z);
        p->body.vy = 0.0f;
        p->flying = 1;
        return;
    }
    /* Back from flying: the body starts where the eye is, out of any wall, its fall from there. */
    if (p->flying) {
        body_unstick(&p->body);
        p->flying = 0;
    }
    if (in->pressed & INPUT_BIT(INPUT_JUMP)) {
        body_jump(&p->body, rules_jump_speed(p->sheet.skill[SKILL_JUMPING]) * RULES_UNITS_PER_M);
    }
    body_step(&p->body, scene, (sy * f + cy * r) * step, (cy * f - sy * r) * step, dt);
}

void player_camera(const Player *p, Camera *cam)
{
    cam->yaw = p->yaw;
    cam->pitch = p->pitch;
    rig_first(cam, &p->body);
}
