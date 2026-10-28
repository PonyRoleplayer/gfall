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

void player_init(Player *p)
{
    body_init(&p->body, WALK_RADIUS, WALK_HEIGHT, WALK_STEP, WALK_EYE);
    p->yaw = 0.0f;
    p->pitch = 0.0f;
    p->speed = PLAYER_SPEED;
    p->running = PLAYER_RUNNING;
    p->jumping = PLAYER_JUMPING;
}

void player_place(Player *p, float x, float eye_y, float z, float yaw, float pitch)
{
    body_place(&p->body, x, eye_y - p->body.eye, z);
    p->yaw = df_wrap_tau(yaw);
    p->pitch = pitch;
}

void player_step(Player *p, const Input *in, const Scene *scene, float dt)
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
    mps = (in->held & INPUT_BIT(INPUT_RUN)) ? rules_run_speed(p->speed, p->running)
                                             : rules_walk_speed(p->speed);
    step = mps * RULES_UNITS_PER_M * dt;
    /* The camera's basis: forward is (sin yaw, cos yaw), right is (cos yaw, -sin yaw). */
    sy = df_sin(p->yaw);
    cy = df_cos(p->yaw);
    if (in->pressed & INPUT_BIT(INPUT_JUMP)) {
        float h;

        /*
         * Classic's height, under the engine's gravity: DFU falls at 20
         * m/s^2, the body at 9.8, so the jump starts slower to rise as
         * high (it then hangs a little longer).
         */
        h = rules_jump_height(p->jumping) * RULES_UNITS_PER_M;
        body_jump(&p->body, df_sqrt(2.0f * WALK_GRAVITY * h));
    }
    body_step(&p->body, scene, (sy * f + cy * r) * step, (cy * f - sy * r) * step, dt);
}

void player_camera(const Player *p, Camera *cam)
{
    cam->yaw = p->yaw;
    cam->pitch = p->pitch;
    rig_first(cam, &p->body);
}
