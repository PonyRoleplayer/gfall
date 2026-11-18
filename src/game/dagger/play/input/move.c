#include "game/dagger/play/state.h"
#include "game/dagger/tables/magic.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/input/move.h"

#include "game/dagger/play/character/vitals.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/ui/options.h"
#include "game/dagger/tables/tables.h"

#include "engine/core/df.h"
#include "engine/scene/scene.h"
#include "engine/scene/walker.h"
#include "engine/scene/interact.h"
#include "game/dagger/play/ui/msg.h"

/* Default mouse_turn_rate 0 makes the turn cap 4. At weight 255 that is 255 steps a second. */
#define TURN_PER_SEC 255

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
        speed = spd + CROUCH_SPEED_ADD;
    } else {
        speed = WALK_ADD + (spd - ATTRIBUTE_NEUTRAL);
    }
    if (running && !in_water) {
        speed = speed * (((run_skill << RATIO_SHIFT) / RUN_SKILL_DIVISOR) + RUN_BASE) / RATIO_ONE;
    } else if (in_water) {
        speed >>= WATER_SPEED_SHIFT;
        speed = speed + speed * ((swim_skill << RATIO_SHIFT) / RUN_SKILL_DIVISOR) / RATIO_ONE;
    }
    return speed;
}

int region_speed(int region, int base)
{
    if (region < 0 || region == STEER_TURN_LEFT || region == STEER_CENTRE || region == STEER_TURN_RIGHT) {
        return 0;
    }
    if (region == STEER_FORWARD) {
        return base * STEER_WEIGHT_KEYS / STEER_FORWARD_DIV;
    }
    return base * STEER_WEIGHT_KEYS / STEER_DIV;
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
    if (region == STEER_SLIDE_LEFT) {
        heading -= ANGLE_QUARTER;
    } else if (region == STEER_SLIDE_RIGHT) {
        heading += ANGLE_QUARTER;
    } else if (region == STEER_BACK) {
        heading += ANGLE_HALF;
    }
    heading = (heading + 2 * ANGLE_FULL) & ANGLE_MASK;
    yaw = (float)heading * (DF_TAU / (float)ANGLE_FULL);
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
    yaw = g_yaw*(DF_TAU/ANGLE_FULL);
    interact_cast(scene,g_body.feet[0],g_body.feet[1]+2,g_body.feet[2],
                    df_sin(yaw),0,df_cos(yaw),g_body.radius+CLIMB_PROBE_PAD,&hit);
    return hit.model >= 0 && hit.wall <= g_body.radius+CLIMB_PROBE_PAD;
}

static void climb_update(const Scene *scene, float dx, float dz, float moved, float dt)
{
    Survival *survival;
    float length, into;
    int chance;

    survival = survival_state();
    if (survival->climbing) {
        if (!scene || g_water || (character_state()->conditions & COND_LEVITATING) || !wall_near(scene)) {
            survival->climbing = 0;
            /* The last probe is at foot height. Once it clears the lip, let
             * the walking step take the feet onto that floor. */
            g_body.grounded = 1;
        } else if (game_state()->minutes-survival->climb_minute > 1) {
            survival->climb_minute = game_state()->minutes; skill_use(SKILL_CLIMBING);
            chance = character_state()->skill[SKILL_CLIMBING]+(curse_race() == PLAYER_RACE_KHAJIIT ? KHAJIIT_CLIMB_BONUS : 0);
            if (!(character_state()->conditions & COND_CLIMBING) && random_range(1,PERCENT) > CLIMB_SLIP_ROLL && random_range(1,PERCENT) > chance) {
                survival->climbing = 0; survival->climb_failed = 1;
            }
        }
    } else {
        if (g_body.grounded) { survival->climb_failed = 0; }
        length = df_sqrt(dx*dx+dz*dz);
        into = length > 0 ? df_absf((dx*g_body.wall_normal[0]+dz*g_body.wall_normal[2])/length) : 0;
        if (scene && g_body.hit_wall && !g_water && !survival->climb_failed &&
            !(character_state()->conditions & COND_LEVITATING) && length > 0 && moved < CLIMB_MOVED_MAX*CLIMB_MOVED_MAX && into >= (float)CLIMB_COS_MIN/RATIO_ONE) {
            survival->climb_time += dt;
            if (survival->climb_time > CLIMB_DELAY_TICKS/BIOS_TICKS_PER_SECOND) {
                survival->climbing = 1; survival->climb_time = 0;
                survival->climb_minute = game_state()->minutes;
                skill_use(SKILL_CLIMBING); msg_set("Climbing mode.");
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
    body_set_size(&g_body, WALK_HEIGHT * CROUCH_SCALE, WALK_EYE * CROUCH_SCALE);
}

static int steering_speed(int region, int base, const MoveIn *input)
{
    int weight;

    if (!input->mouse_steer) {
        return region_speed(region, base);
    }
    switch (region) {
    case STEER_FORWARD_LEFT:
    case STEER_FORWARD:
    case STEER_FORWARD_RIGHT:
        weight = input->weight_up;
        break;
    case STEER_SLIDE_LEFT:
        weight = input->weight_left;
        break;
    case STEER_BACK:
        weight = input->weight_down;
        break;
    case STEER_SLIDE_RIGHT:
        weight = input->weight_right;
        break;
    default:
        return 0;
    }
    return base * weight / (region == STEER_FORWARD ? STEER_FORWARD_DIV : STEER_DIV);
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
    if (who->conditions & COND_PARALYZED) {
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
        g_yaw = (g_yaw + in->mouse_dx * game_config()->sensitivity_x) & ANGLE_MASK;
        g_pitch += in->mouse_dy * game_config()->sensitivity_y * (game_config()->reverse_y ? 1 : -1);
        if (g_pitch > PITCH_LIMIT) {
            g_pitch = PITCH_LIMIT;
        }
        if (g_pitch < -PITCH_LIMIT) {
            g_pitch = -PITCH_LIMIT;
        }
    }
    if (!g_air && in->held) {
        int look;
        look = in->held[LOOK_UP] - in->held[LOOK_DOWN];
        g_pitch += df_iround((float)look * LOOK_KEY_SPEED * in->dt);
        if (g_pitch > PITCH_LIMIT) { g_pitch = PITCH_LIMIT; }
        if (g_pitch < -PITCH_LIMIT) { g_pitch = -PITCH_LIMIT; }
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
    free_move = swimming || (who->conditions & COND_LEVITATING) || survival_state()->climbing;
    if (free_move) { g_air = 0; }
    speed = ground_speed(who->attribute[ATTR_SPD], g_crouch, running && !wet, who->skill[SKILL_RUNNING], wet && !(who->conditions & COND_WATER_WALKING),
                            who->skill[SKILL_SWIMMING]);
    if (!g_air && in->view_mode && in->held && in->held[SLOW]) {
        speed = view_slow(speed);
    }
    speed = steering_speed(region, speed, in);
    if (!g_air && (region == STEER_FORWARD_LEFT || region == STEER_TURN_LEFT)) {
        face(-df_iround((float)(in->mouse_steer ? in->weight_left : TURN_PER_SEC) *
             (float)(game_config()->turn_rate * 2 + 4) * 0.25f * in->dt));
    } else if (!g_air && (region == STEER_FORWARD_RIGHT || region == STEER_TURN_RIGHT)) {
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
        impulse = jump_impulse(mag, (who->conditions & COND_ACROBAT) != 0, (who->class_flags & CLASS_JUMP_BONUS) != 0);
        if (body_jump(&g_body, jump_ups(impulse))) {
            skill_use(SKILL_JUMPING);
            fatigue_spend(JUMP_FATIGUE);
            g_air = 1;
            jumped = 1;
        }
    }
    was_ground = g_body.grounded && !jumped;
    if (scene) {
        if (free_move) {
            vertical = survival_state()->climbing ? speed*in->dt/CLIMB_SPEED_DIV : 0;
            if (swimming || (who->conditions & COND_LEVITATING)) {
                vertical = in->held ? (in->held[FLOAT_UP]-in->held[FLOAT_DOWN])*FLOAT_SPEED*in->dt : 0;
                /* A swimmer over the weight sinks, unless pressing down already or levitating or water-walking (intrface.c 616). */
                if (swimming && g_weight > SWIM_SINK_WEIGHT && !(in->held && in->held[FLOAT_DOWN]) && !(who->conditions & (COND_LEVITATING | COND_WATER_WALKING))) { vertical -= FLOAT_SPEED*in->dt; }
            }
            g_body.vy = 0;
            body_fly_step(&g_body,scene,step_x,vertical,step_z);
            g_body.top = g_body.feet[1];
        } else {
            if ((who->conditions & COND_SLOWFALL) && g_body.vy < -SLOWFALL_SPEED) { g_body.vy = -SLOWFALL_SPEED; }
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
    if (swimming || (who->conditions & COND_LEVITATING) || g_climb) { g_air = 0; }
    /* head_bob_update's ten samples over its 1000-tick walking cycle. */
    g_bob_offset = 0;
    if ((options_flags() & 2) && g_body.grounded && !g_air && !wet && !(who->conditions & COND_LEVITATING)) {
        if (moved > 0.001f) {
            g_bob_ms += in->dt * MS_PER_SECOND;
            while (g_bob_ms >= MS_PER_SECOND) { g_bob_ms -= MS_PER_SECOND; }
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
    yaw = (float)g_yaw * (DF_TAU / (float)ANGLE_FULL);
    pitch = (float)g_pitch * (DF_TAU / (float)ANGLE_FULL);
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
