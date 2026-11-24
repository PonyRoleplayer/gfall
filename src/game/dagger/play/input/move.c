#include "game/dagger/play/state.h"
#include "game/dagger/tables/magic.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/input/move.h"

#include "game/dagger/play/character/vitals.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/ui/options.h"
#include "game/dagger/tables/tables.h"
#include "game/dagger/tables/itemdefs.h"

#include "engine/core/df.h"
#include "engine/scene/scene.h"
#include "engine/scene/walker.h"
#include "engine/scene/interact.h"
#include "game/dagger/play/ui/msg.h"

/* Default mouse_turn_rate 0 makes the turn cap 4. At weight 255 that is 255 steps a second. */
#define TURN_PER_SEC 255

/* View movement rates are multiples of the current walk/run speed per second. */
#define VIEW_ACCEL 11.0f
#define VIEW_BRAKE 10.0f
#define VIEW_REVERSE 17.0f
#define VIEW_AIR_ACCEL 1.35f
#define VIEW_DIAGONAL 0.70710678f

#define SPRINT_MULTIPLIER 2
#define SPRINT_FATIGUE_PER_SECOND (4 * FATIGUE_UNIT)
#define SPRINT_FATIGUE_RESERVE FATIGUE_UNIT

/* World units per step at the neutral walk speed, and camera travel in units. */
#define VIEW_STEP_DISTANCE 96.0f
#define VIEW_BOB_HEIGHT 2.35f
#define VIEW_BOB_RATE 16.0f
#define VIEW_LAND_TIME 0.20f
#define VIEW_LAND_MIN_SPEED 55.0f
#define VIEW_LAND_SCALE 0.022f
#define VIEW_LAND_DEPTH 3.5f

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
static const ObjectInfo *g_mount;
static df_u32 g_mount_id;
static float g_mount_speed;
static float g_vx;
static float g_vz;
static float g_sprint_fatigue;
static float g_prev_x;
static float g_prev_z;
static float g_bob_ms;
static float g_bob_offset;
static int g_view;
static float g_view_phase;
static float g_view_height;
static float g_land_age;
static float g_land_depth;

static void reset_bob(void)
{
    g_bob_ms = g_bob_offset = 0;
    g_view_phase = g_view_height = 0;
    g_land_age = g_land_depth = 0;
}

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
    g_mount = 0;
    g_mount_id = 0;
    g_mount_speed = 0;
    g_vx = 0.0f;
    g_vz = 0.0f;
    g_sprint_fatigue = 0.0f;
    g_view = options_mouse() == 1;
    reset_bob();
    walker_set_game(&g_body);
    walker_set_on(1);
    interact_set_body(&g_body);
}

Body *move_body(void)
{
    return &g_body;
}

int move_mounted(void)
{
    if (!g_mount || g_mount->id != g_mount_id || g_mount->parent != game_state()->inventory_id) { return MOUNT_FOOT; }
    return g_mount->index == 1 ? MOUNT_HORSE : MOUNT_CART;
}

float move_mount_speed(void) { return move_mounted() ? g_mount_speed : 0; }

static const ObjectInfo *mount_item(df_u32 id)
{
    const ObjectInfo *item = object_at(id);

    if (!item || item->type != RECORD_ITEM || item->group != ITEM_GROUP_TRANSPORT || item->index < 0 || item->index > 1 ||
        item->parent != game_state()->inventory_id || (item->flags & ITEM_UNPAID)) { return 0; }
    return item;
}

static void mount_set(const ObjectInfo *item)
{
    float lift = item ? RIDE_EYE_ADD : 0;

    g_mount = item;
    g_mount_id = item ? item->id : 0;
    g_mount_speed = 0;
    g_crouch = g_climb = 0;
    survival_state()->climb_time = 0;
    body_set_size(&g_body, WALK_HEIGHT + lift, WALK_EYE + lift);
}

int move_mount(const Scene *scene, df_u32 id)
{
    const ObjectInfo *item;

    if (!id) {
        if (g_mount) { mount_set(0); }
        return 1;
    }
    item = mount_item(id);
    if (!item || !scene || !scene->outdoor || !g_body.grounded || g_water || survival_state()->climbing || curse_beast() ||
        (character_state()->conditions & COND_LEVITATING) || !body_can_stand(&g_body, scene, WALK_HEIGHT + RIDE_EYE_ADD)) {
        return 0;
    }
    mount_set(item);
    return 1;
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
    g_vx = g_vz = 0;
    reset_bob();
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
    g_vx = g_vz = 0;
    reset_bob();
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

static void view_wish(const MoveIn *in, int speed, float *vx, float *vz)
{
    int forward, side, turn;
    float yaw, sy, cy, scale;

    forward = side = turn = 0;
    if (in->held) {
        const unsigned char *held = in->held;

        forward = held[FORWARD] - held[BACK];
        turn = held[TURN_RIGHT] - held[TURN_LEFT];
        side = (held[SLIDE_RIGHT] || (held[STRAFE] && held[TURN_RIGHT]))
             - (held[SLIDE_LEFT] || (held[STRAFE] && held[TURN_LEFT]));
        if (held[STRAFE]) { turn = 0; }
    }
    if (turn) {
        face(df_iround((float)(turn * TURN_PER_SEC * (game_config()->turn_rate * 2 + 4))
                       * 0.25f * in->dt));
    }
    if (forward == 0 && side == 0) {
        *vx = *vz = 0;
        return;
    }
    yaw = (float)g_yaw * (DF_TAU / ANGLE_FULL);
    sy = df_sin(yaw);
    cy = df_cos(yaw);
    scale = forward && side ? (float)speed * VIEW_DIAGONAL : (float)speed;
    *vx = (sy * (float)forward + cy * (float)side) * scale;
    *vz = (cy * (float)forward - sy * (float)side) * scale;
}

/* Approach the wish velocity and integrate that change over the frame. */
static void view_accelerate(float vx, float vz, float speed, float dt, float *dx, float *dz)
{
    float change_x, change_z, distance, rate, time;
    int steering;

    steering = vx != 0 || vz != 0;
    rate = VIEW_ACCEL;
    if (g_air) {
        rate = steering ? VIEW_AIR_ACCEL : 0;
    } else if (!steering) {
        rate = VIEW_BRAKE;
    } else if (vx * g_vx + vz * g_vz < 0) {
        rate = VIEW_REVERSE;
    }
    change_x = vx - g_vx;
    change_z = vz - g_vz;
    distance = df_sqrt(change_x * change_x + change_z * change_z);
    rate *= speed;
    if (rate <= 0 || distance <= 0) {
        *dx = g_vx * dt;
        *dz = g_vz * dt;
    } else if (distance <= rate * dt) {
        time = distance / rate;
        *dx = vx * dt - change_x * time * 0.5f;
        *dz = vz * dt - change_z * time * 0.5f;
        g_vx = vx;
        g_vz = vz;
    } else {
        float fraction = rate * dt / distance;

        change_x *= fraction;
        change_z *= fraction;
        *dx = (g_vx + change_x * 0.5f) * dt;
        *dz = (g_vz + change_z * 0.5f) * dt;
        g_vx += change_x;
        g_vz += change_z;
    }
}

/* The body has already swept and slid. Keep the velocity along its wall. */
static void view_wall_velocity(float moved, float dx, float dz)
{
    float nx, nz, into;

    if (!g_body.hit_wall) { return; }
    if (moved < 0.001f && moved < (dx * dx + dz * dz) * 0.01f) {
        g_vx = g_vz = 0;
        return;
    }
    nx = g_body.wall_normal[0];
    nz = g_body.wall_normal[2];
    into = g_vx * nx + g_vz * nz;
    if (into < 0) {
        into /= nx * nx + nz * nz;
        g_vx -= into * nx;
        g_vz -= into * nz;
    }
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

static void view_bob(float distance_squared, float impact, float dt)
{
    float target, change, progress, dip;

    if (!(options_flags() & 2)) {
        reset_bob();
        return;
    }
    target = 0;
    if (distance_squared > 0) {
        float distance = df_sqrt(distance_squared);
        float speed = distance / (dt * WALK_ADD);

        if (speed > 1) { speed = 1; }
        target = VIEW_BOB_HEIGHT * speed * (g_crouch ? CROUCH_SCALE : 1.0f);
        g_view_phase += distance / VIEW_STEP_DISTANCE;
        g_view_phase -= (float)df_ifloor(g_view_phase);
    }
    change = VIEW_BOB_RATE * dt;
    if (target > g_view_height + change) { target = g_view_height + change; }
    if (target < g_view_height - change) { target = g_view_height - change; }
    g_view_height = target;

    if (impact > VIEW_LAND_MIN_SPEED) {
        g_land_depth = (impact - VIEW_LAND_MIN_SPEED) * VIEW_LAND_SCALE;
        if (g_land_depth > VIEW_LAND_DEPTH) { g_land_depth = VIEW_LAND_DEPTH; }
        g_land_age = 0;
    } else if (g_land_depth > 0) {
        g_land_age += dt;
    }
    dip = 0;
    if (g_land_depth > 0) {
        progress = g_land_age / VIEW_LAND_TIME;
        if (progress < 1) {
            float remaining = 1.0f - progress;

            /* A quick dip and slower recovery, with its peak at one third. */
            dip = g_land_depth * (6.75f * progress * remaining * remaining);
        } else {
            g_land_depth = 0;
        }
    }
    g_bob_offset = (g_view_height > 0 ? g_view_height * df_sin(g_view_phase * DF_TAU) : 0) - dip;
}

int move_frame(const Scene *scene, const MoveIn *in)
{
    CharacterState *who;
    int speed;
    int region;
    int running, sprinting, slow, mounted;
    int wet;
    float step_x;
    float step_z;
    float moved;
    float land;
    float climb_x;
    float climb_z;
    int jumped;
    int was_ground;
    int water_depth, swimming, free_move;
    float vertical;

    if (!in || in->dt <= 0.0f) {
        return 0;
    }
    if (!g_ready) {
        move_init();
    }
    if (g_view != in->view_mode) {
        g_view = in->view_mode;
        if (!g_air) { g_vx = g_vz = 0; }
        reset_bob();
    }
    who = character_state();
    mounted = move_mounted();
    if (g_mount && (!mounted || !scene || !scene->outdoor || (who->conditions & COND_LEVITATING) || curse_beast())) {
        move_mount(0, 0);
        mounted = 0;
    }
    if (who->conditions & COND_PARALYZED) {
        g_mount_speed = 0;
        if (scene) { body_step(&g_body,scene,0,0,in->dt); }
        if (g_view) {
            g_vx = g_vz = 0;
            view_bob(0, 0, in->dt);
            g_prev_x = g_body.feet[0];
            g_prev_z = g_body.feet[2];
            state_pose(g_body.feet[0], g_body.feet[1] + g_body.eye_now,
                       g_body.feet[2], g_yaw, g_pitch);
        }
        return 0;
    }
    if (!mounted && in->pressed && in->pressed[CROUCH]) {
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
    if ((!g_air || g_view) && in->held) {
        int look;
        look = in->held[LOOK_UP] - in->held[LOOK_DOWN];
        g_pitch += df_iround((float)look * LOOK_KEY_SPEED * in->dt);
        if (g_pitch > PITCH_LIMIT) { g_pitch = PITCH_LIMIT; }
        if (g_pitch < -PITCH_LIMIT) { g_pitch = -PITCH_LIMIT; }
    }
    if ((!g_air || g_view) && in->pressed && in->pressed[LOOK_HOME]) {
        g_pitch = 0;
    }
    region = g_air ? -1 : in->region;
    running = !mounted && in->held && in->held[RUN];
    water_depth = scene ? scene_water_body(scene, g_body.feet[0], g_body.feet[1] + (mounted ? WALK_EYE : g_body.eye_now), g_body.feet[2]) : WATER_DRY;
    wet = water_depth != WATER_DRY;
    g_splash = wet && !g_water;
    if (g_splash) {
        g_body.vy = 0.0f;
    }
    g_water = wet;
    swimming = water_depth >= WATER_WAIST;
    if (mounted && swimming) {
        move_mount(0, 0);
        mounted = 0;
    }
    free_move = swimming || (who->conditions & COND_LEVITATING) || survival_state()->climbing;
    if (free_move) { g_air = 0; }
    slow = !g_air && g_view && in->held && in->held[SLOW];
    sprinting = !mounted && !wet && !free_move && !g_crouch && !g_air && g_body.grounded && !slow &&
                in->held && in->held[SPRINT] && (who->fatigue > SPRINT_FATIGUE_RESERVE || god_mode());
    speed = ground_speed(who->attribute[ATTR_SPD], g_crouch, running && !sprinting && !wet, who->skill[SKILL_RUNNING], !mounted && wet && !(who->conditions & COND_WATER_WALKING),
                            who->skill[SKILL_SWIMMING]);
    if (mounted) { speed += mounted == MOUNT_HORSE ? HORSE_SPEED_ADD : CART_SPEED_ADD; }
    if (slow) { speed = view_slow(speed); }
    if (g_view) {
        view_wish(in, speed, &climb_x, &climb_z);
        if (sprinting && (climb_x || climb_z)) {
            speed *= SPRINT_MULTIPLIER;
            climb_x *= SPRINT_MULTIPLIER;
            climb_z *= SPRINT_MULTIPLIER;
        }
        if (free_move) {
            g_vx = climb_x;
            g_vz = climb_z;
            step_x = g_vx * in->dt;
            step_z = g_vz * in->dt;
            if (g_vx == 0 && g_vz == 0) { speed = 0; }
        } else {
            view_accelerate(climb_x, climb_z, (float)speed, in->dt, &step_x, &step_z);
        }
    } else {
        if (sprinting) { speed *= SPRINT_MULTIPLIER; }
        speed = steering_speed(region, speed, in);
        if (!g_air && (region == STEER_FORWARD_LEFT || region == STEER_TURN_LEFT)) {
            face(-df_iround((float)(in->mouse_steer ? in->weight_left : TURN_PER_SEC) *
                 (float)(game_config()->turn_rate * 2 + 4) * 0.25f * in->dt));
        } else if (!g_air && (region == STEER_FORWARD_RIGHT || region == STEER_TURN_RIGHT)) {
            face(df_iround((float)(in->mouse_steer ? in->weight_right : TURN_PER_SEC) *
                 (float)(game_config()->turn_rate * 2 + 4) * 0.25f * in->dt));
        }
        if (!g_air) { wish(region, speed, &g_vx, &g_vz); }
        step_x = g_vx * in->dt;
        step_z = g_vz * in->dt;
        climb_x = step_x;
        climb_z = step_z;
    }
    jumped = 0;
    if (!mounted && !free_move && !g_air && g_body.grounded && in->pressed && in->pressed[JUMP]) {
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
    /* The sweep still blocks the body during ascent. Keep its approach
     * velocity so it can carry through once its feet clear the edge. */
    if (g_view && scene && !free_move && g_body.vy <= 0) { view_wall_velocity(moved, step_x, step_z); }
    if (!mounted) { climb_update(scene,climb_x,climb_z,moved,in->dt); }
    sprinting = sprinting && !g_climb && (climb_x || climb_z) && moved > 0.001f;
    if (sprinting && !god_mode()) {
        int cost, available;

        g_sprint_fatigue += in->dt * SPRINT_FATIGUE_PER_SECOND;
        cost = df_ifloor(g_sprint_fatigue);
        g_sprint_fatigue -= cost;
        /* Sprint gives out before the normal exhaustion and collapse rules kick in. */
        available = who->fatigue - SPRINT_FATIGUE_RESERVE;
        fatigue_spend(cost < available ? cost : available);
    }
    g_mount_speed = mounted && g_body.grounded ? df_sqrt(moved) / in->dt : 0;
    g_prev_x = g_body.feet[0];
    g_prev_z = g_body.feet[2];
    if ((was_ground || (g_view && scene)) && !g_body.grounded) {
        g_air = 1;
    }
    land = 0;
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
    if (g_view) {
        float distance_squared = 0;

        if (was_ground && g_body.grounded && !wet && !free_move && !g_climb) {
            float requested = step_x * step_x + step_z * step_z;

            distance_squared = moved < requested ? moved : requested;
        }
        view_bob(distance_squared, wet || free_move ? 0 : land, in->dt);
    } else if ((options_flags() & 2) && g_body.grounded && !g_air && !wet && !(who->conditions & COND_LEVITATING)) {
        /* head_bob_update's ten samples over its 1000-tick walking cycle. */
        if (moved > 0.001f) {
            g_bob_ms += in->dt * MS_PER_SECOND;
            while (g_bob_ms >= MS_PER_SECOND) { g_bob_ms -= MS_PER_SECOND; }
        } else { g_bob_ms = 0; }
        g_bob_offset = (float)bob_sample(df_ifloor(g_bob_ms));
    } else { g_bob_ms = g_bob_offset = 0; }
    state_pose(g_body.feet[0], g_body.feet[1] + g_body.eye_now,
                  g_body.feet[2], g_yaw, g_pitch);
    return sprinting;
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
    out->mount = move_mounted() ? g_mount_id : 0;
    out->eye = !out->mount && g_body.eye_now > WALK_EYE ? WALK_EYE : g_body.eye_now;
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
    if (motion->mount) { mount_set(mount_item(motion->mount)); }
    else if (motion->crouched) { crouch_down(); }
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
    g_bob_offset = !g_view && (options_flags() & 2) ? (float)bob_sample(df_ifloor(g_bob_ms)) : 0;
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
    /* Rebase the live body without restarting its contacts or camera motion. */
    g_body.feet[0] -= dx;
    g_body.feet[2] -= dz;
    g_body.safe[0] -= dx;
    g_body.safe[2] -= dz;
    g_prev_x -= dx;
    g_prev_z -= dz;
    state_pose(g_body.feet[0],g_body.feet[1]+g_body.eye_now,g_body.feet[2],g_yaw,g_pitch);
}
