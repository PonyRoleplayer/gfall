#include "game/ogfall/play/state.h"
#include "game/ogfall/play/move.h"

#include "game/ogfall/play/vitals.h"
#include "game/ogfall/play/combat.h"
#include "game/ogfall/play/config.h"
#include "game/ogfall/play/options.h"
#include "game/ogfall/tables/tables.h"

#include "engine/core/df.h"
#include "engine/scene/scene.h"
#include "engine/scene/walker.h"

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

int og_ground_speed(int spd, int crouched, int running, int run_skill, int in_water, int swim_skill)
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

int og_region_speed(int region, int base)
{
    if (region < 0 || region == 3 || region == 4 || region == 5) {
        return 0;
    }
    if (region == 1) {
        return base * 255 / 220;
    }
    return base * 255 / 256;
}

void og_move_init(void)
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
}

Body *og_move_body(void)
{
    return &g_body;
}

void og_move_place(float x, float eye_y, float z, int classic_yaw)
{
    if (!g_ready) {
        og_move_init();
    }
    g_yaw = classic_yaw & 0x7ff;
    g_pitch = 0;
    g_air = 0;
    body_place(&g_body, x, eye_y - WALK_EYE, z);
    g_prev_x = x;
    g_prev_z = z;
    og_state_pose(x, eye_y, z, g_yaw, g_pitch);
}

int og_move_yaw(void)
{
    return g_yaw;
}

int og_move_airborne(void)
{
    return g_air;
}

int og_move_crouched(void)
{
    return g_crouch;
}

int og_move_in_water(void)
{
    return g_water;
}

int og_move_splashed(void)
{
    return g_splash;
}

void og_move_set_weight(int weight)
{
    g_weight = weight;
}

int og_move_climbing(void)
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

static void crouch_down(void)
{
    if (g_crouch) {
        return;
    }
    g_crouch = 1;
    body_set_size(&g_body, WALK_HEIGHT * 0.5f, WALK_EYE * 0.5f);
}

static int steering_speed(int region, int base, const OgMoveIn *input)
{
    int weight;

    if (!input->mouse_steer) {
        return og_region_speed(region, base);
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

void og_move_frame(const Scene *scene, const OgMoveIn *in)
{
    OgCharacter *who;
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

    if (!in || in->dt <= 0.0f) {
        return;
    }
    if (!g_ready) {
        og_move_init();
    }
    who = og_character();
    if (in->pressed && in->pressed[OG_CROUCH]) {
        if (g_crouch) {
            stand_up(scene);
        } else {
            crouch_down();
        }
    }
    if (in->view_mode) {
        g_yaw = (g_yaw + in->mouse_dx * og_config()->sensitivity_x) & 0x7ff;
        g_pitch += in->mouse_dy * og_config()->sensitivity_y * (og_config()->reverse_y ? 1 : -1);
        if (g_pitch > 256) {
            g_pitch = 256;
        }
        if (g_pitch < -256) {
            g_pitch = -256;
        }
    }
    if (!g_air && in->held) {
        int look;
        look = in->held[OG_LOOK_UP] - in->held[OG_LOOK_DOWN];
        g_pitch += df_iround((float)look * 160.0f * in->dt);
        if (g_pitch > 256) { g_pitch = 256; }
        if (g_pitch < -256) { g_pitch = -256; }
    }
    if (!g_air && in->pressed && in->pressed[OG_LOOK_HOME]) {
        g_pitch = 0;
    }
    region = in->region;
    if (g_air) {
        region = -1;
    }
    running = in->held && in->held[OG_RUN];
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
    speed = og_ground_speed(who->attribute[OG_SPD], g_crouch, running, who->skill[21], wet,
                            who->skill[17]);
    if (!g_air && in->view_mode && in->held && in->held[OG_SLOW]) {
        speed = og_view_slow(speed);
    }
    speed = steering_speed(region, speed, in);
    if (!g_air && (region == 0 || region == 3)) {
        face(-df_iround((float)(in->mouse_steer ? in->weight_left : TURN_PER_SEC) *
             (float)(og_config()->turn_rate * 2 + 4) * 0.25f * in->dt));
    } else if (!g_air && (region == 2 || region == 5)) {
        face(df_iround((float)(in->mouse_steer ? in->weight_right : TURN_PER_SEC) *
             (float)(og_config()->turn_rate * 2 + 4) * 0.25f * in->dt));
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
    if (wet && g_weight > 250 && !(in->held && in->held[OG_FLOAT_DOWN])) {
        g_body.vy -= 80.0f * in->dt;
    }
    jumped = 0;
    if (!g_air && g_body.grounded && in->pressed && in->pressed[OG_JUMP]) {
        int mag;
        int impulse;

        mag = og_jump_magnitude(who->attribute[OG_SPD], who->attribute[OG_STR], who->skill[OG_SKILL_JUMPING]);
        impulse = og_jump_impulse(mag, (who->conditions & 0x10000) != 0, (who->class_flags & 2) != 0);
        if (body_jump(&g_body, og_jump_ups(impulse))) {
            who->fatigue = og_fatigue_add(who->fatigue, -11, who->attribute[OG_STR], who->attribute[OG_END]);
            g_air = 1;
            jumped = 1;
        }
    }
    was_ground = g_body.grounded && !jumped;
    if (scene) {
        body_step(&g_body, scene, step_x, step_z, in->dt);
    }
    moved = g_body.feet[0] - g_prev_x;
    moved = moved * moved;
    {
        float mz;

        mz = g_body.feet[2] - g_prev_z;
        moved = moved + mz * mz;
    }
    g_climb = 0;
    if (!g_air && speed > 0 && moved < 25.0f) {
        /* 5 squared. A slide along the wall is not a scrape. */
        g_climb = 1;
        g_body.vy = 0.0f;
    }
    g_prev_x = g_body.feet[0];
    g_prev_z = g_body.feet[2];
    if (was_ground && !g_body.grounded) {
        g_air = 1;
    }
    if (g_body.grounded) {
        if (g_air && body_landed(&g_body, &land, 0)) {
            int damage;

            if (!jumped) {
                damage = og_fall_damage(og_fall_v(land));
                og_damage_player(damage,-1);
            }
        }
        g_air = 0;
    } else if (jumped) {
        g_air = 1;
    }
    /* head_bob_update's ten samples over its 1000-tick walking cycle. */
    g_bob_offset = 0;
    if ((og_options_flags() & 2) && g_body.grounded && !g_air && !wet && !(who->conditions & 8)) {
        if (moved > 0.001f) {
            g_bob_ms += in->dt * 1000.0f;
            while (g_bob_ms >= 1000.0f) { g_bob_ms -= 1000.0f; }
        } else { g_bob_ms = 0; }
        g_bob_offset = (float)og_bob_sample(df_ifloor(g_bob_ms));
    } else { g_bob_ms = 0; }
    og_state_pose(g_body.feet[0], g_body.feet[1] + g_body.eye_now,
                  g_body.feet[2], g_yaw, g_pitch);
}

void og_move_camera(Camera *cam)
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

void og_move_capture(OgMotion *out)
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

void og_move_restore(const OgMotion *motion)
{
    int i;

    og_move_init();
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
    g_bob_offset = (og_options_flags() & 2) ? (float)og_bob_sample(df_ifloor(g_bob_ms)) : 0;
    g_yaw = motion->yaw;
    g_pitch = motion->pitch;
    g_air = motion->airborne;
    g_crouch = motion->crouched;
    g_water = motion->water;
    g_weight = motion->weight;
    g_prev_x = g_body.feet[0];
    g_prev_z = g_body.feet[2];
}
