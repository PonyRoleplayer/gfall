#include "game/dagger/play/player.h"
#include "game/dagger/play/combat.h"
#include "engine/core/log.h"
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
    /* Running trains the skill: one use every fourth classic update of it (PlayerEntity). */
    p->running = (in->held & INPUT_BIT(INPUT_RUN)) && (f != 0.0f || r != 0.0f);
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
        return;
    }
    /* Back from flying: the body starts where the eye is, out of any wall, its fall from there. */
    if (p->flying) {
        body_unstick(&p->body);
        p->flying = 0;
    }
    p->swimming = scene && scene_water_body(scene, p->body.feet[0], p->body.feet[1] + p->body.eye, p->body.feet[2]) == WATER_HEAD;
    if (p->swimming) {
        float swim;
        float sp;
        float cp;
        float rise;

        /* No gravity in the water: it goes along the view at a swimmer's speed, and jump rises. */
        swim = rules_swim_speed(mps, p->sheet.skill[SKILL_SWIMMING]) * RULES_UNITS_PER_M;
        sp = df_sin(p->pitch);
        cp = df_cos(p->pitch);
        rise = (in->held & INPUT_BIT(INPUT_JUMP)) ? 1.0f : 0.0f;
        p->body.gravity = 0.0f;
        p->body.vy = (sp * f + rise) * swim;
        body_step(&p->body, scene, (sy * cp * f + cy * r) * swim * dt, (cy * cp * f - sy * r) * swim * dt, dt);
        return;
    }
    p->body.gravity = rules_gravity() * RULES_UNITS_PER_M;
    if (in->pressed & INPUT_BIT(INPUT_JUMP)) {
        /* Each jump from the ground trains the skill, and tires (PlayerEntity). */
        if (p->body.grounded) {
            character_tally(&p->sheet, SKILL_JUMPING, 1);
            p->sheet.fatigue -= RULES_FATIGUE_JUMP;
            p->sheet.fatigue = p->sheet.fatigue < 0 ? 0 : p->sheet.fatigue;
        }
        body_jump(&p->body, rules_jump_speed(p->sheet.skill[SKILL_JUMPING]) * RULES_UNITS_PER_M);
    }
    body_step(&p->body, scene, (sy * f + cy * r) * step, (cy * f - sy * r) * step, dt);
}

int player_breath(Player *p, float dt)
{
    if (!p->swimming) {
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
        inventory_take_off(&p->pack, EQUIP_RIGHT_HAND);
    }
}

void player_camera(const Player *p, Camera *cam)
{
    cam->yaw = p->yaw;
    cam->pitch = p->pitch;
    rig_first(cam, &p->body);
}
