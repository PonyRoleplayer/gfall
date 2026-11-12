#include "game/dagger/play/state.h"
#include "game/dagger/play/move.h"

#include "game/dagger/play/vitals.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/options.h"
#include "game/dagger/tables/tables.h"

#include "engine/core/df.h"
#include "engine/scene/scene.h"
#include "engine/scene/walker.h"
#include "engine/scene/interact.h"
#include "game/dagger/play/msg.h"

/* D_00187CA9's initial value in FALL.EXE: walk speed is SPD + 150. */
#define WALK_ADD 200
/* Default mouse_turn_rate 0 makes the turn cap 4. At weight 255 that is 255 steps a second. */
#define TURN_PER_SEC 255
/* Classic units in a circle. Compass and pitch use this. */
#define CIRCLE 2048

static Body g_body;
static int g_ready;
static int g_yaw;
static int g_pitch;
static int g_air;
static int g_crouch;
static int g_water;
static int g_splash;
static int g_climb;
static int g_weight;
static float g_vx;
static float g_vz;
static float g_prev_x;
static float g_prev_z;
static float g_bob_ms;
static float g_bob_offset;

int ground_speed(int spd, int crouched, int running, int run_skill, int in_water, int swim_skill)
{
    int speed;

    if (crouched) {
        speed = spd + 50;
    } else {
        speed = WALK_ADD + (spd - 50);
    }
    if (running && !in_water) {
        speed = speed * (((run_skill << 8) / 200) + 320) / 256;
    } else if (in_water) {
        speed >>= 2;
        speed = speed + speed * ((swim_skill << 8) / 200) / 256;
    }
    return speed;
}

int region_speed(int region, int base)
{
    if (region < 0 || region == 3 || region == 4 || region == 5) {
        return 0;
    }
    if (region == 1) {
        return base * 255 / 220;
    }
    return base * 255 / 256;
}

void move_init(void)
{
    body_init(&g_body, WALK_RADIUS, WALK_HEIGHT, WALK_STEP, WALK_EYE);
    g_ready = 1;
    g_yaw = 0;
    g_pitch = 0;
    g_air = 0;
    g_crouch = 0;
    g_water = 0;
    g_splash = 0;
    g_climb = 0;
    g_weight = 0;
    g_vx = 0.0f;
    g_vz = 0.0f;
    g_bob_ms = g_bob_offset = 0;
    walker_set_game(&g_body);
    walker_set_on(1);
    interact_set_body(&g_body);
}

Body *move_body(void)
{
    return &g_body;
}

void move_place(float x, float eye_y, float z, int classic_yaw)
{
    if (!g_ready) {
        move_init();
    }
    g_yaw = classic_yaw & 0x7ff;
    g_pitch = 0;
    g_air = 0;
    survival_state()->climbing = survival_state()->climb_failed = 0;
    survival_state()->climb_time = 0;
    body_place(&g_body, x, eye_y - WALK_EYE, z);
    g_prev_x = x;
    g_prev_z = z;
    state_pose(x, eye_y, z, g_yaw, g_pitch);
}

int move_take_teleport(void)
{
    float x, y, z, yaw;

    if (!g_ready || !interact_teleport(&x,&y,&z,&yaw)) { return 0; }
    /* Original link action 14 changes position and retains the player's
     * angles. The borrowed builder supplies the standing eye above its floor.
     * Keep crouch/eye size, and restart collision/fall tracking at that floor. */
    body_place(&g_body,x,y-WALK_EYE,z);
    survival_state()->climbing = survival_state()->climb_failed = 0;
    survival_state()->climb_time = 0;
    g_air = g_climb = g_water = g_splash = 0;
    g_vx = g_vz = g_bob_ms = g_bob_offset = 0;
    g_prev_x = x; g_prev_z = z;
    state_pose(x,g_body.feet[1]+g_body.eye_now,z,g_yaw,g_pitch);
    return 1;
}

int move_yaw(void)
{
    return g_yaw;
}

int move_airborne(void)
{
    return g_air;
}

int move_crouched(void)
{
    return g_crouch;
}

int move_in_water(void)
{
    return g_water;
}

int move_splashed(void)
{
    return g_splash;
}

void move_set_weight(int weight)
{
    g_weight = weight;
}

int move_climbing(void)
{
    return g_climb;
}

static void face(int delta)
{
    g_yaw = (g_yaw + delta) & 0x7ff;
}

static void wish(int region, int speed, float *dx, float *dz)
{
    int heading;
    float yaw;
    float fx;
    float fz;

    heading = g_yaw;
    if (region == 6) {
        heading -= 512;
    } else if (region == 8) {
        heading += 512;
    } else if (region == 7) {
        heading += 1024;
    }
    heading = (heading + 4096) & 0x7ff;
    yaw = (float)heading * (DF_TAU / (float)CIRCLE);
    fx = df_sin(yaw);
    fz = df_cos(yaw);
    *dx = fx * (float)speed;
    *dz = fz * (float)speed;
}

static void stand_up(const Scene *scene)
{
    if (!g_crouch) {
        return;
    }
    if (scene && !body_can_stand(&g_body, scene, WALK_HEIGHT)) {
        return;
    }
    g_crouch = 0;
    body_set_size(&g_body, WALK_HEIGHT, WALK_EYE);
}

static int wall_near(const Scene *scene)
{
    UseHit hit;
    float yaw;
    yaw = g_yaw*(DF_TAU/2048);
    interact_cast(scene,g_body.feet[0],g_body.feet[1]+2,g_body.feet[2],
                    df_sin(yaw),0,df_cos(yaw),g_body.radius+8,&hit);
    return hit.model >= 0 && hit.wall <= g_body.radius+8;
}

static void climb_update(const Scene *scene, float dx, float dz, float moved, float dt)
{
    Survival *survival;
    float length, into;
    int chance;

    survival = survival_state();
    if (survival->climbing) {
        if (!scene || g_water || (character_state()->conditions & 8) || !wall_near(scene)) {
            survival->climbing = 0;
            /* The last probe is at foot height. Once it clears the lip, let
             * the walking step take the feet onto that floor. */
            g_body.grounded = 1;
        } else if (game_state()->minutes-survival->climb_minute > 1) {
            survival->climb_minute = game_state()->minutes; skill_use(18);
            chance = character_state()->skill[18]+(character_state()->race == 6 ? 30 : 0);
            if (!(character_state()->conditions & 0x20000) && random_range(1,100) > 95 && random_range(1,100) > chance) {
                survival->climbing = 0; survival->climb_failed = 1;
            }
        }
    } else {
        if (g_body.grounded) { survival->climb_failed = 0; }
        length = df_sqrt(dx*dx+dz*dz);
        into = length > 0 ? df_absf((dx*g_body.wall_normal[0]+dz*g_body.wall_normal[2])/length) : 0;
        if (scene && g_body.hit_wall && !g_water && !survival->climb_failed &&
            !(character_state()->conditions & 8) && length > 0 && moved < 25 && into >= 251.0f/256) {
            survival->climb_time += dt;
            if (survival->climb_time > 14/18.2065f) {
                survival->climbing = 1; survival->climb_time = 0;
                survival->climb_minute = game_state()->minutes;
                skill_use(18); msg_set("Climbing mode.");
            }
        } else { survival->climb_time = 0; survival->climb_minute = game_state()->minutes; }
    }
    g_climb = survival->climbing;
}

static void crouch_down(void)
{
    if (g_crouch) {
        return;
    }
    g_crouch = 1;
    body_set_size(&g_body, WALK_HEIGHT * 0.5f, WALK_EYE * 0.5f);
}

static int steering_speed(int region, int base, const MoveIn *input)
{
    int weight;

    if (!input->mouse_steer) {
        return region_speed(region, base);
    }
    switch (region) {
    case 0:
    case 1:
    case 2:
        weight = input->weight_up;
        break;
    case 6:
        weight = input->weight_left;
        break;
    case 7:
        weight = input->weight_down;
        break;
    case 8:
        weight = input->weight_right;
        break;
    default:
        return 0;
    }
    return base * weight / (region == 1 ? 220 : 256);
}

void move_frame(const Scene *scene, const MoveIn *in)
{
    CharacterState *who;
    int speed;
    int region;
    int running;
    int wet;
    float dx;
    float dz;
    float step_x;
    float step_z;
    float moved;
    float land;
    int jumped;
    int was_ground;
    int swimming, free_move;
    float vertical;

    if (!in || in->dt <= 0.0f) {
        return;
    }
    if (!g_ready) {
        move_init();
    }
    who = character_state();
    if (who->conditions & 1) {
        if (scene) { body_step(&g_body,scene,0,0,in->dt); }
        return;
    }
    if (in->pressed && in->pressed[CROUCH]) {
        if (g_crouch) {
            stand_up(scene);
        } else {
            crouch_down();
        }
    }
    if (in->view_mode) {
        g_yaw = (g_yaw + in->mouse_dx * game_config()->sensitivity_x) & 0x7ff;
        g_pitch += in->mouse_dy * game_config()->sensitivity_y * (game_config()->reverse_y ? 1 : -1);
        if (g_pitch > 256) {
            g_pitch = 256;
        }
        if (g_pitch < -256) {
            g_pitch = -256;
        }
    }
    if (!g_air && in->held) {
        int look;
        look = in->held[LOOK_UP] - in->held[LOOK_DOWN];
        g_pitch += df_iround((float)look * 160.0f * in->dt);
        if (g_pitch > 256) { g_pitch = 256; }
        if (g_pitch < -256) { g_pitch = -256; }
    }
    if (!g_air && in->pressed && in->pressed[LOOK_HOME]) {
        g_pitch = 0;
    }
    region = in->region;
    if (g_air) {
        region = -1;
    }
    running = in->held && in->held[RUN];
    wet = 0;
    if (scene) {
        float eye_y;

        eye_y = g_body.feet[1] + g_body.eye_now;
        wet = scene_water_body(scene, g_body.feet[0], eye_y, g_body.feet[2]) != WATER_DRY;
    }
    g_splash = wet && !g_water;
    if (g_splash) {
        g_body.vy = 0.0f;
    }
    g_water = wet;
    swimming = scene && scene_water_body(scene,g_body.feet[0],g_body.feet[1]+g_body.eye_now,g_body.feet[2]) >= WATER_WAIST;
    free_move = swimming || (who->conditions & 8) || survival_state()->climbing;
    if (free_move) { g_air = 0; }
    speed = ground_speed(who->attribute[ATTR_SPD], g_crouch, running && !wet, who->skill[21], wet && !(who->conditions & 0x100000),
                            who->skill[17]);
    if (!g_air && in->view_mode && in->held && in->held[SLOW]) {
        speed = view_slow(speed);
    }
    speed = steering_speed(region, speed, in);
    if (!g_air && (region == 0 || region == 3)) {
        face(-df_iround((float)(in->mouse_steer ? in->weight_left : TURN_PER_SEC) *
             (float)(game_config()->turn_rate * 2 + 4) * 0.25f * in->dt));
    } else if (!g_air && (region == 2 || region == 5)) {
        face(df_iround((float)(in->mouse_steer ? in->weight_right : TURN_PER_SEC) *
             (float)(game_config()->turn_rate * 2 + 4) * 0.25f * in->dt));
    }
    if (g_air) {
        step_x = g_vx * in->dt;
        step_z = g_vz * in->dt;
    } else {
        wish(region, speed, &dx, &dz);
        g_vx = dx;
        g_vz = dz;
        step_x = dx * in->dt;
        step_z = dz * in->dt;
    }
    jumped = 0;
    if (!free_move && !g_air && g_body.grounded && in->pressed && in->pressed[JUMP]) {
        int mag;
        int impulse;

        mag = jump_magnitude(who->attribute[ATTR_SPD], who->attribute[ATTR_STR], who->skill[SKILL_JUMPING]);
        impulse = jump_impulse(mag, (who->conditions & 0x10000) != 0, (who->class_flags & 2) != 0);
        if (body_jump(&g_body, jump_ups(impulse))) {
            skill_use(3);
            fatigue_spend(11);
            g_air = 1;
            jumped = 1;
        }
    }
    was_ground = g_body.grounded && !jumped;
    if (scene) {
        if (free_move) {
            vertical = survival_state()->climbing ? speed*in->dt/3 : 0;
            if (swimming || (who->conditions & 8)) {
                vertical = in->held ? (in->held[FLOAT_UP]-in->held[FLOAT_DOWN])*80*in->dt : 0;
                if (swimming && g_weight > 250 && !(in->held && in->held[FLOAT_DOWN])) { vertical -= 80*in->dt; }
            }
            g_body.vy = 0;
            body_fly_step(&g_body,scene,step_x,vertical,step_z);
            g_body.top = g_body.feet[1];
        } else {
            if ((who->conditions & 0x4000) && g_body.vy < -60) { g_body.vy = -60; }
            body_step(&g_body, scene, step_x, step_z, in->dt);
        }
    }
    moved = g_body.feet[0] - g_prev_x;
    moved = moved * moved;
    {
        float mz;

        mz = g_body.feet[2] - g_prev_z;
        moved = moved + mz * mz;
    }
    climb_update(scene,step_x,step_z,moved,in->dt);
    g_prev_x = g_body.feet[0];
    g_prev_z = g_body.feet[2];
    if (was_ground && !g_body.grounded) {
        g_air = 1;
    }
    if (g_body.grounded) {
        if (g_air && body_landed(&g_body, &land, 0)) {
            int damage;

            if (!jumped) {
                damage = fall_damage(fall_v(land));
                damage_player(damage,-1);
            }
        }
        g_air = 0;
    } else if (jumped) {
        g_air = 1;
    }
    if (swimming || (who->conditions & 8) || g_climb) { g_air = 0; }
    /* head_bob_update's ten samples over its 1000-tick walking cycle. */
    g_bob_offset = 0;
    if ((options_flags() & 2) && g_body.grounded && !g_air && !wet && !(who->conditions & 8)) {
        if (moved > 0.001f) {
            g_bob_ms += in->dt * 1000.0f;
            while (g_bob_ms >= 1000.0f) { g_bob_ms -= 1000.0f; }
        } else { g_bob_ms = 0; }
        g_bob_offset = (float)bob_sample(df_ifloor(g_bob_ms));
    } else { g_bob_ms = 0; }
    state_pose(g_body.feet[0], g_body.feet[1] + g_body.eye_now,
                  g_body.feet[2], g_yaw, g_pitch);
}

void move_camera(Camera *cam)
{
    float x;
    float y;
    float z;
    float yaw;
    float pitch;

    if (!cam) {
        return;
    }
    body_eye(&g_body, &x, &y, &z);
    yaw = (float)g_yaw * (DF_TAU / (float)CIRCLE);
    pitch = (float)g_pitch * (DF_TAU / (float)CIRCLE);
    camera_set(cam, x, y + g_bob_offset, z, yaw, pitch);
}

void move_capture(MotionState *out)
{
    int i;
    for (i = 0; i < 3; i++) {
        out->feet[i] = g_body.feet[i];
        out->safe[i] = g_body.safe[i];
    }
    out->eye = g_body.eye_now;
    out->vy = g_body.vy;
    out->top = g_body.top;
    out->vx = g_vx;
    out->vz = g_vz;
    out->bob_ms = g_bob_ms;
    out->yaw = g_yaw;
    out->pitch = g_pitch;
    out->airborne = g_air;
    out->crouched = g_crouch;
    out->water = g_water;
    out->weight = g_weight;
    out->grounded = g_body.grounded;
    out->have_safe = g_body.have_safe;
}

void move_restore(const MotionState *motion)
{
    int i;

    move_init();
    if (motion->crouched) { crouch_down(); }
    body_place(&g_body, motion->feet[0], motion->feet[1], motion->feet[2]);
    for (i = 0; i < 3; i++) { g_body.safe[i] = motion->safe[i]; }
    g_body.eye_now = motion->eye;
    g_body.vy = motion->vy;
    g_body.top = motion->top;
    g_body.grounded = (df_u8)motion->grounded;
    g_body.have_safe = (df_u8)motion->have_safe;
    g_vx = motion->vx;
    g_vz = motion->vz;
    g_bob_ms = motion->bob_ms;
    g_bob_offset = (options_flags() & 2) ? (float)bob_sample(df_ifloor(g_bob_ms)) : 0;
    g_yaw = motion->yaw;
    g_pitch = motion->pitch;
    g_air = motion->airborne;
    g_crouch = motion->crouched;
    g_water = motion->water;
    g_climb = survival_state()->climbing;
    g_weight = motion->weight;
    g_prev_x = g_body.feet[0];
    g_prev_z = g_body.feet[2];
}
void move_shift(float dx, float dz)
{
    MotionState motion;
    move_capture(&motion);
    motion.feet[0] -= dx; motion.safe[0] -= dx;
    motion.feet[2] -= dz; motion.safe[2] -= dz;
    move_restore(&motion);
    state_pose(g_body.feet[0],g_body.feet[1]+g_body.eye_now,g_body.feet[2],g_yaw,g_pitch);
}
