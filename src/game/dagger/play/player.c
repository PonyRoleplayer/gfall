#include "game/dagger/play/player.h"
#include "game/dagger/play/viewopt.h"
#include "game/dagger/play/combat.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/scene/interact.h"
#include "game/dagger/play/rules.h"
#include "engine/core/df.h"
#include "engine/gfx/rig.h"
#include "engine/scene/walker.h"

/* See player.h. */

/* Radians a mouse count turns; looking up or down stops short of straight. */
/* Seconds of running to a use of the skill: four of classic's 0.0549254 s updates. */
#define RUN_USE_EVERY 0.2197f
/* Seconds to a point of breath: 19 of classic's 0.0549254 s ticks. */
#define BREATH_EVERY 1.0436f
#define LOOK_SENS 0.003f
#define PITCH_LIMIT 1.55334303f
/* Both keys at once go no faster than one: DFU's limitDiagonalSpeed. */
#define DIAGONAL 0.70710678f
/*
 * Swimming, ours (the playtest of 2026-11-07 found classic's slow and tedious): the speed a walk's times
 * SWIM_LEAST plus the skill's share up to the whole walk at 100, SWIM_STROKE more with the run key; the
 * velocity eased toward what the keys ask at SWIM_EASE a second, so a stroke glides on; afloat, the eye
 * SURFACE_EYE above the water, drifting up at SWIM_RISE when nothing is held; a hop out at the surface
 * at the jump's speed, SWIM_HOP_SECONDS of falling as on land so it can land on the bank; pushed against
 * a ledge at the surface for SWIM_PUSH_SECONDS, it hops by itself.
 */
#define SWIM_LEAST 0.6f
#define SWIM_STROKE 1.35f
#define SWIM_EASE 6.0f
#define SURFACE_EYE 8.0f
#define SWIM_RISE 24.0f
#define SWIM_HOP_SECONDS 0.6f
#define SWIM_PUSH_SECONDS 0.25f

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
    p->run_time = 0.0f;
    p->swimming = 0;
    p->under = 0;
    mem_zero(p->swim_v, sizeof(p->swim_v));
    p->water_jump = 0.0f;
    p->push_time = 0.0f;
    p->breath = 0;
    p->breath_time = 0.0f;
    p->running = 0;
    p->riding = 0;
    p->crouched = 0;
    p->crouch_want = 0;
    p->speed_xz = 0.0f;
    p->bob_phase = VIEWOPT_CREST;
    p->bob_walk = 0;
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

void player_turn(Player *p, float yaw, float pitch)
{
    p->yaw = df_wrap_tau(p->yaw + yaw);
    pitch += p->pitch;
    p->pitch = pitch > PITCH_LIMIT ? PITCH_LIMIT : (pitch < -PITCH_LIMIT ? -PITCH_LIMIT : pitch);
}

/* A walk on the ground advances HeadBobber's phase. Still, swimming or flying does not. */
static void note_bob(Player *p, float f, float r, float mps, float dt)
{
    float horiz;

    horiz = f * f + r * r;
    if (!p->body.grounded || p->swimming || p->flying || horiz <= 0.0f) {
        p->bob_walk = 0;
        p->bob_phase = VIEWOPT_CREST;
        return;
    }
    p->bob_walk = 1;
    p->bob_phase = df_wrap_tau(p->bob_phase + mps * df_sqrt(horiz) * VIEWOPT_RATE * dt);
}

void player_step(Player *p, const Input *in, const Scene *scene, float dt, int collide)
{
    player_turn(p, (float)in->look_dx * LOOK_SENS, (float)-in->look_dy * LOOK_SENS);
    player_step_motion(p, in, scene, dt, collide, (float)in->move_f, (float)in->move_r);
}

/* How far the feet went over the ground since x0, z0, a second's worth. */
static void note_speed(Player *p, float x0, float z0, float dt)
{
    float dx;
    float dz;

    dx = p->body.feet[0] - x0;
    dz = p->body.feet[2] - z0;
    p->speed_xz = dt > 0.0f ? df_sqrt(dx * dx + dz * dz) / dt : 0.0f;
}

int player_sneaking(const Player *p)
{
    /* Half a walk, with a hair's room: a crouch walks at exactly that, and a wall's slide must not tip it either way. */
    return p->speed_xz <= rules_walk_speed(p->sheet.attribute[ATTR_SPEED]) * RULES_UNITS_PER_M * 0.5f * 1.02f;
}

/*
 * A frame in the water (the SWIM_* note above). 1 when it swam; 0 when it is out of the water, wading on
 * the floor, or in its hop out, and walks or falls as on land.
 */
static int swim_step(Player *p, const Input *in, const Scene *scene, float dt, float f, float r, float sy, float cy)
{
    float surface;
    float eye_y;
    float want[3];
    float speed;
    float sp;
    float cp;
    float ease;
    float wx;
    float wz;
    float moved;
    int depth;
    int k;

    depth = scene ? scene_water_body(scene, p->body.feet[0], p->body.feet[1] + p->body.eye, p->body.feet[2]) : WATER_DRY;
    p->under = depth == WATER_HEAD;
    if (p->water_jump > 0.0f) {
        p->water_jump -= dt;
        p->swimming = 0;
        return 0;
    }
    /* Over the head, or over the waist with nothing underfoot: afloat. Standing in it, it wades. */
    p->swimming = depth == WATER_HEAD || (depth == WATER_WAIST && !p->body.grounded);
    if (!p->swimming || !scene_water_at(scene, p->body.feet[0], p->body.feet[2], &surface, 0)) {
        p->swimming = 0;
        mem_zero(p->swim_v, sizeof(p->swim_v));
        p->push_time = 0.0f;
        return 0;
    }
    speed = rules_walk_speed(p->sheet.attribute[ATTR_SPEED])
            * (SWIM_LEAST + (1.0f - SWIM_LEAST) * (float)p->sheet.skill[SKILL_SWIMMING] / 100.0f) * RULES_UNITS_PER_M;
    if (in->held & INPUT_BIT(INPUT_RUN)) {
        speed *= SWIM_STROKE;
    }
    /* Along the view, pitch and all; jump rises and crouch dives; idle, it drifts up. */
    sp = df_sin(p->pitch);
    cp = df_cos(p->pitch);
    want[0] = (sy * cp * f + cy * r) * speed;
    want[1] = sp * f * speed;
    want[2] = (cy * cp * f - sy * r) * speed;
    if (in->held & INPUT_BIT(INPUT_JUMP)) {
        want[1] += speed;
    } else if (in->held & INPUT_BIT(INPUT_CROUCH)) {
        want[1] -= speed;
    } else if (f == 0.0f && r == 0.0f) {
        want[1] = SWIM_RISE;
    }
    /* Afloat: the eye stays just above the water, unless it dives. */
    eye_y = p->body.feet[1] + p->body.eye;
    if (eye_y + want[1] * dt > surface + SURFACE_EYE) {
        want[1] = (surface + SURFACE_EYE - eye_y) / (dt > 0.0f ? dt : 1.0f);
        if (want[1] < -speed) {
            want[1] = -speed;
        }
    }
    ease = SWIM_EASE * dt > 1.0f ? 1.0f : SWIM_EASE * dt;
    for (k = 0; k < 3; k++) {
        p->swim_v[k] += (want[k] - p->swim_v[k]) * ease;
    }
    p->body.gravity = 0.0f;
    p->body.vy = p->swim_v[1];
    wx = p->swim_v[0] * dt;
    wz = p->swim_v[2] * dt;
    {
        float bx;
        float bz;

        bx = p->body.feet[0];
        bz = p->body.feet[2];
        body_step(&p->body, scene, wx, wz, dt);
        moved = (p->body.feet[0] - bx) * (p->body.feet[0] - bx) + (p->body.feet[2] - bz) * (p->body.feet[2] - bz);
    }
    /*
     * At the surface: jump hops out, as does swimming on into a ledge for a moment (it went less than a
     * third of the way it was sent). The hop falls as on land, so it lands on the bank.
     */
    if (!p->under && eye_y >= surface - SURFACE_EYE) {
        int blocked;

        blocked = f > 0.0f && wx * wx + wz * wz > 0.0f && moved < (wx * wx + wz * wz) * 0.1f;
        p->push_time = blocked ? p->push_time + dt : 0.0f;
        if ((in->pressed & INPUT_BIT(INPUT_JUMP)) || p->push_time >= SWIM_PUSH_SECONDS) {
            p->water_jump = SWIM_HOP_SECONDS;
            p->push_time = 0.0f;
            p->swimming = 0;
            p->body.gravity = rules_gravity() * RULES_UNITS_PER_M;
            /* body_jump asks for the ground, and in water there is none: the jump's speed up is set outright. */
            p->body.vy = rules_jump_speed(p->sheet.skill[SKILL_JUMPING]) * RULES_UNITS_PER_M;
            p->body.grounded = 0;
            p->body.top = p->body.feet[1];
        }
    } else {
        p->push_time = 0.0f;
    }
    return 1;
}

void player_step_motion(Player *p, const Input *in, const Scene *scene, float dt, int collide, float f, float r)
{
    float mps;
    float x0;
    float z0;
    float step;
    float sy;
    float cy;

    x0 = p->body.feet[0];
    z0 = p->body.feet[2];
    /* Cursor speed is an input fraction, never a shortened physics timestep:
     * gravity, jumps, breath and fatigue continue at their real elapsed time.
     * Normalize only vectors longer than one. Ordinary single-key walking
     * pays no square root; the common full diagonal retains its constant. */
    if (f * f + r * r > 1.0f) {
        float scale;

        scale = (f == 1.0f || f == -1.0f) && (r == 1.0f || r == -1.0f)
                    ? DIAGONAL : 1.0f / df_sqrt(f * f + r * r);
        f *= scale;
        r *= scale;
    }
    /* Riding is its own speed, and cannot run (PlayerSpeedChanger). Crouched, half the walk, whatever is held. */
    if (p->riding) {
        mps = rules_ride_speed(p->sheet.attribute[ATTR_SPEED], p->riding == 2);
    } else if (p->crouched) {
        mps = rules_walk_speed(p->sheet.attribute[ATTR_SPEED]) * PLAYER_CROUCH_SPEED;
    } else {
        mps = (in->held & INPUT_BIT(INPUT_RUN))
                  ? rules_run_speed(p->sheet.attribute[ATTR_SPEED], p->sheet.skill[SKILL_RUNNING])
                  : rules_walk_speed(p->sheet.attribute[ATTR_SPEED]);
    }
    step = mps * RULES_UNITS_PER_M * dt;
    /* Running trains the skill: one use every fourth classic update of it (PlayerEntity). */
    p->running = !p->riding && !p->crouched && (in->held & INPUT_BIT(INPUT_RUN)) && (f != 0.0f || r != 0.0f);
    if (p->running) {
        p->run_time += dt;
        if (p->run_time >= RUN_USE_EVERY) {
            p->run_time -= RUN_USE_EVERY;
            character_tally(&p->sheet, SKILL_RUNNING, 1);
        }
    }
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
        p->speed_xz = 0.0f;
        note_bob(p, f, r, mps, dt);
        return;
    }
    /* Back from flying: the body starts where the eye is, out of any wall, its fall from there. */
    if (p->flying) {
        body_unstick(&p->body);
        p->flying = 0;
    }
    if (swim_step(p, in, scene, dt, f, r, sy, cy)) {
        note_speed(p, x0, z0, dt);
        note_bob(p, f, r, mps, dt);
        return;
    }
    p->body.gravity = rules_gravity() * RULES_UNITS_PER_M;
    if ((in->pressed & INPUT_BIT(INPUT_JUMP)) && !p->crouched) {
        /* Each jump from the ground trains the skill, and tires (PlayerEntity). */
        if (p->body.grounded) {
            character_tally(&p->sheet, SKILL_JUMPING, 1);
            p->sheet.fatigue -= RULES_FATIGUE_JUMP;
            p->sheet.fatigue = p->sheet.fatigue < 0 ? 0 : p->sheet.fatigue;
        }
        body_jump(&p->body, rules_jump_speed(p->sheet.skill[SKILL_JUMPING]) * RULES_UNITS_PER_M);
    }
    body_step(&p->body, scene, (sy * f + cy * r) * step, (cy * f - sy * r) * step, dt);
    note_speed(p, x0, z0, dt);
    note_bob(p, f, r, mps, dt);
}

void player_crouch(Player *p, const Scene *scene, int toggle, int held, int pressed)
{
    if (toggle) {
        if (pressed) {
            p->crouch_want = !p->crouch_want;
        }
    } else {
        p->crouch_want = held != 0;
    }
    /* A horse is no place to crouch. */
    if (p->riding) {
        p->crouch_want = 0;
    }
    if (p->crouch_want && !p->crouched) {
        body_set_size(&p->body, PLAYER_CROUCH_HEIGHT, PLAYER_CROUCH_EYE);
        p->crouched = 1;
    } else if (!p->crouch_want && p->crouched) {
        /* Up only with room for the head; else down still, and the next frame asks again. */
        if (!scene || body_can_stand(&p->body, scene, WALK_HEIGHT)) {
            body_set_size(&p->body, WALK_HEIGHT, WALK_EYE);
            p->crouched = 0;
        }
    }
}

int player_breath(Player *p, float dt)
{
    if (!p->under) {
        p->breath = 0;
        p->breath_time = 0.0f;
        return 0;
    }
    /* The first frame under: a full breath. */
    if (p->breath <= 0) {
        p->breath = rules_max_breath(p->sheet.attribute[ATTR_ENDURANCE]);
        p->breath = p->breath < 1 ? 1 : p->breath;
    }
    p->breath_time += dt;
    /* tgm: the breath holds. */
    if (p->god) {
        p->breath_time = 0.0f;
        return 0;
    }
    while (p->breath_time >= BREATH_EVERY) {
        p->breath_time -= BREATH_EVERY;
        /* An Argonian keeps it half the time. */
        if (p->sheet.race == RACE_ARGONIAN && combat_roll(2)) {
            continue;
        }
        p->breath--;
        if (p->breath <= 0) {
            return 1;
        }
    }
    return 0;
}

int player_minute(Player *p, int enemies_near)
{
    Character *c;
    int loss;

    c = &p->sheet;
    /* tgm: nothing wears the player down, fatigue no more than health. */
    if (p->god) {
        return PLAYER_FINE;
    }
    loss = p->running ? RULES_FATIGUE_RUNNING : RULES_FATIGUE_MINUTE;
    if (p->swimming) {
        character_tally(c, SKILL_SWIMMING, 1);
        /* A swimmer's roll on the skill, an Argonian's needing none. */
        if (c->race != RACE_ARGONIAN && combat_roll(100) >= c->skill[SKILL_SWIMMING]) {
            loss = RULES_FATIGUE_SWIMMING;
        }
    }
    c->fatigue -= loss;
    if (c->fatigue > 0) {
        return PLAYER_FINE;
    }
    c->fatigue = 0;
    if (enemies_near || p->swimming) {
        return PLAYER_DIED;
    }
    /* It drops where it is: an hour's rest comes back, and Medical is used. */
    c->health += rules_rest_health(c->attribute[ATTR_ENDURANCE], c->skill[SKILL_MEDICAL], c->health_max);
    c->health = c->health > c->health_max ? c->health_max : c->health;
    c->fatigue = rules_rest_fatigue(c->fatigue_max / CHARACTER_FATIGUE_UNIT) * CHARACTER_FATIGUE_UNIT;
    c->magicka += rules_rest_magicka(c->magicka_max);
    c->magicka = c->magicka > c->magicka_max ? c->magicka_max : c->magicka;
    character_tally(c, SKILL_MEDICAL, 1);
    return PLAYER_COLLAPSED;
}

void player_wear_changed(Player *p)
{
    inventory_armor(&p->pack, p->sheet.armor);
}

/* A word that a thing has broken, for the message line. */
static void say_broken(const Item *it)
{
    static char line[64];
    char name[48];

    item_name(it, name, (int)sizeof(name));
    log_format(line, (int)sizeof(line), "Your %s has broken.", name);
    interact_say(line);
}

void player_armor_struck(Player *p, int part, int damage, int roll)
{
    int place;
    Item *it;

    place = inventory_armor_at_part(&p->pack, part);
    if (place < 0) {
        return;
    }
    it = &p->pack.item[p->pack.worn[place]];
    if (item_wear_down(it, damage, roll)) {
        say_broken(it);
        inventory_take_off(&p->pack, place);
        player_wear_changed(p);
    }
}

void player_check_weapon(Player *p)
{
    const Item *it;

    it = inventory_weapon(&p->pack);
    if (it && item_broken(it)) {
        say_broken(it);
        inventory_take_off(&p->pack, inventory_hand_place(&p->pack));
    }
}

void player_shift(Player *p, float dx, float dz)
{
    p->body.feet[0] -= dx;
    p->body.feet[2] -= dz;
    p->body.safe[0] -= dx;
    p->body.safe[2] -= dz;
}

void player_camera(const Player *p, Camera *cam)
{
    float right;
    float up;
    float pitch;
    float yaw;
    float sy;
    float cy;

    cam->yaw = p->yaw;
    cam->pitch = p->pitch;
    rig_first(cam, &p->body);
    viewopt_eye(p->bob_phase, p->bob_walk, p->running, &right, &up, &pitch, &yaw);
    if (right == 0.0f && up == 0.0f && pitch == 0.0f && yaw == 0.0f) {
        return;
    }
    /* The body's right, flat: the bob is not a picture shaken on the screen. */
    sy = df_sin(p->yaw);
    cy = df_cos(p->yaw);
    cam->x += cy * right;
    cam->z -= sy * right;
    cam->y += up;
    pitch += cam->pitch;
    cam->pitch = pitch > PITCH_LIMIT ? PITCH_LIMIT : (pitch < -PITCH_LIMIT ? -PITCH_LIMIT : pitch);
    cam->yaw = df_wrap_tau(cam->yaw + yaw);
}
