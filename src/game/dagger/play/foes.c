#include "game/dagger/play/foes.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/loot.h"
#include "game/dagger/play/rules.h"
#include "engine/core/log.h"
#include "engine/scene/clock.h"
#include "engine/scene/interact.h"
#include "game/dagger/play/sfx.h"
#include "game/dagger/places/dunstream.h"
#include "game/dagger/formats/sndfile.h"
#include "game/dagger/tables/enemies.h"
#include "engine/scene/body.h"
#include "engine/scene/stream.h"
#include "engine/scene/walker.h"

/* See foes.h. */

/* A creature turns at most this far a classic tick: DFU's 20 degrees. */
#define TURN (20.0f * DF_TAU / 360.0f)
/* It swings only when the player is this near straight ahead; the blow lands within the second. */
#define AIM_START (22.5f * DF_TAU / 360.0f)
#define AIM_LAND (35.156f * DF_TAU / 360.0f)
/* So near, facing does not matter (DFU's 0.25 m). */
#define POINT_BLANK 10.0f
/* It stops a little short of reach, so its swing still lands. */
#define STOP (FOES_MELEE * 0.85f)
/* A knock-back, classic's speed units (EnemyMotor): the most it starts at, the most it moves at, what a tick takes off, and where it ends. */
#define KNOCK_MOST 40
#define KNOCK_MOVES_MOST 25.0f
#define KNOCK_DECAY 5.0f
#define KNOCK_ENDS 5
/* One such unit is 1 / 3.95 m/s (classic's 39.5 to the metre, over ten), in units a second. */
#define KNOCK_UNITS (RULES_UNITS_PER_M / 3.95f)
/* Each search goes this much further past the last point, up to the most (a metre a step, EnemyMotor's 11). */
#define SEARCH_STEP RULES_UNITS_PER_M
#define SEARCH_MOST 11
/* A ray that stops within this of a door's sweep stopped at the door; past it, this far on. */
#define DOOR_SLACK 8.0f
#define DOOR_THROUGH 48.0f
/* Sleepers looked at a tick; a mind looks every this many ticks. */
#define LOOKS_PER_TICK 2
#define SIGHT_EVERY 6
/* Its attack pose's frames a second, and the frame its blow lands on (of five). */
#define ATTACK_RATE 10.0f
#define ATTACK_HIT_FRAME 2
#define ATTACK_FRAMES 5
/* Ticks a frame may catch up on after a stall. */
#define TICKS_MAX 4
/* A move sound every 2 to 5 seconds while it walks. */
#define MOVE_SOUND_MIN 2.0f
#define MOVE_SOUND_SPAN 3.0f

typedef struct Mind {
    Entity *e;
    KeepKey key;
    int kind;
    int flies;
    Body body;
    /* The player seen at its last look; where it last saw them. */
    int sees;
    float last[3];
    int give_up;
    int look_in;
    /* Seconds before it may swing again; the swing's time, -1 when not swinging; its blow taken. */
    float melee;
    float swing;
    int landed;
    float move_sound;
    /* Seconds it has not moved for (foes_due): its next move takes them all. */
    float owed;
    /* The player heard, not seen: it keeps after them but cannot swing. */
    int hears;
    /* The way the player was going when last known, across the ground; how far past that it has searched. */
    float heading[2];
    int search;
    /* The door its sight last met, or -1. */
    int door;
    /* A knock-back: its speed in classic's units, and which way. */
    float knock;
    float knock_dir[2];
} Mind;

static Mind g_mind[FOES_MINDS];
static int g_on = 1;
static float g_ticks;
/* Frames stepped: which far minds move this one. */
static int g_frame;
/* The last game minute a sleeper's notice was rolled against the player: one use of Stealth a minute. */
static int g_stealth_minute = -1;
/* The furthest any blow has come from, feet to feet, since the last reset: what foes_blow_reach reads. */
static float g_blow_reach;
/* The next sleeper to look at, counted across the stream's slots. */
static int g_scan;

void foes_set_on(int on)
{
    int i;

    g_on = on ? 1 : 0;
    if (g_on) {
        return;
    }
    /* Still: no one walks or swings on. */
    for (i = 0; i < FOES_MINDS; i++) {
        if (g_mind[i].e) {
            g_mind[i].e->moving = 0;
            if (g_mind[i].e->act == ENTITY_ATTACK) {
                g_mind[i].e->act = 0;
                g_mind[i].e->hold = 0;
            }
            g_mind[i].swing = -1.0f;
        }
    }
}

int foes_on(void)
{
    return g_on;
}

void foes_reset(void)
{
    int i;

    g_blow_reach = 0.0f;
    for (i = 0; i < FOES_MINDS; i++) {
        g_mind[i].e = 0;
    }
    g_ticks = 0.0f;
    g_scan = 0;
}

void foes_shift(float dx, float dz)
{
    int i;

    for (i = 0; i < FOES_MINDS; i++) {
        if (g_mind[i].e) {
            g_mind[i].body.feet[0] -= dx;
            g_mind[i].body.feet[2] -= dz;
            g_mind[i].body.safe[0] -= dx;
            g_mind[i].body.safe[2] -= dz;
            g_mind[i].last[0] -= dx;
            g_mind[i].last[2] -= dz;
        }
    }
}

/* What a mind is about, in a word or two. */
static const char *mind_doing(const Mind *m)
{
    if (m->knock > 0.0f) {
        return "knocked back";
    }
    if (m->swing >= 0.0f) {
        return "swinging";
    }
    if (m->sees) {
        return "chasing";
    }
    if (m->hears) {
        return "hearing";
    }
    return m->search ? "searching" : "looking";
}

void foes_report(const float *feet)
{
    int i;
    int n;

    n = 0;
    for (i = 0; i < FOES_MINDS; i++) {
        const Mind *m;
        const EnemyKind *kind;
        float dx;
        float dy;
        float dz;

        m = &g_mind[i];
        if (!m->e) {
            continue;
        }
        kind = enemy_kind(m->kind);
        dx = m->e->x - feet[0];
        dy = m->e->y - feet[1];
        dz = m->e->z - feet[2];
        log_print(LOG_PRINT, "%s: %s, %d away, gives up in %d", kind ? kind->name : "?", mind_doing(m),
                  df_iround(df_sqrt(dx * dx + dy * dy + dz * dz)), FOES_GIVE_UP - m->give_up);
        n++;
    }
    log_print(LOG_PRINT, "%d awake of %d, ai %s.", n, FOES_MINDS, g_on ? "on" : "off");
}

int foes_awake(void)
{
    int i;
    int n;

    n = 0;
    for (i = 0; i < FOES_MINDS; i++) {
        n += g_mind[i].e != 0;
    }
    return n;
}

static Mind *mind_of(const Entity *e)
{
    int i;

    for (i = 0; i < FOES_MINDS; i++) {
        if (g_mind[i].e == e) {
            return &g_mind[i];
        }
    }
    return 0;
}

/* The player's eye and feet. */
static void player_points(const Player *p, float *eye, float *feet)
{
    body_eye(&p->body, &eye[0], &eye[1], &eye[2]);
    feet[0] = p->body.feet[0];
    feet[1] = p->body.feet[1];
    feet[2] = p->body.feet[2];
}

/* A creature's eye: a third of its height down from its top, as DFU's. */
static void eye_of(const Entity *e, float *out)
{
    out[0] = e->x;
    out[1] = e->y + entity_height(e) * (2.0f / 3.0f);
    out[2] = e->z;
}

/* The smallest turn from a to b, -pi..pi. */
static float turn_between(float a, float b)
{
    float d;

    d = df_wrap_tau(b - a);
    return d > DF_TAU * 0.5f ? d - DF_TAU : d;
}

static float absf(float v)
{
    return v < 0.0f ? -v : v;
}

/* A mind for e, its body its size where it stands. 0 when every mind is taken. */
static Mind *wake(Entity *e, int kind)
{
    const EnemyKind *k;
    Mind *m;
    float h;
    float r;
    int i;

    m = mind_of(e);
    if (m) {
        return m;
    }
    for (i = 0; i < FOES_MINDS && g_mind[i].e; i++) {
    }
    if (i == FOES_MINDS) {
        return 0;
    }
    m = &g_mind[i];
    m->e = e;
    m->key = e->key;
    m->kind = kind;
    m->flies = enemy_flies(kind) || enemy_spectral(kind);
    h = entity_height(e);
    if (h < 16.0f) {
        h = 16.0f;
    }
    /* A quarter of its picture's width across, as round as the pictures allow. */
    r = (float)entity_pose(e)->w[0] * 0.25f;
    r = r < 8.0f ? 8.0f : (r > 24.0f ? 24.0f : r);
    body_init(&m->body, r, h, h * 0.3f < WALK_STEP ? h * 0.3f : WALK_STEP, h * (2.0f / 3.0f));
    m->body.gravity = m->flies ? 0.0f : rules_gravity() * RULES_UNITS_PER_M;
    m->body.self = e;
    body_place(&m->body, e->x, e->y, e->z);
    /* Solid now: the player and the others stop at it. */
    e->radius = r;
    m->sees = 0;
    m->give_up = FOES_GIVE_UP;
    m->look_in = i % SIGHT_EVERY;
    m->melee = 0.0f;
    m->swing = -1.0f;
    m->landed = 0;
    m->hears = 0;
    m->search = 0;
    m->heading[0] = 0.0f;
    m->heading[1] = 0.0f;
    m->door = -1;
    m->knock = 0.0f;
    m->owed = 0.0f;
    m->move_sound = MOVE_SOUND_MIN + (float)combat_roll(100) * (MOVE_SOUND_SPAN / 100.0f);
    k = enemy_kind(kind);
    if (k) {
        sfx_at(k->bark_sound, e->x, e->y + h * 0.5f, e->z);
    }
    return m;
}

void foes_alert(Entity *e)
{
    Mind *m;
    int kind;

    if (!g_on || !e || e->dead) {
        return;
    }
    kind = dunstream_kind(e, 0);
    if (kind < 0) {
        return;
    }
    m = wake(e, kind);
    if (m) {
        /* It knows where the blow came from: as if it saw, a full give-up count. */
        m->give_up = FOES_GIVE_UP;
        m->look_in = 0;
    }
}

void foes_knock(Entity *e, const float *from, int speed)
{
    Mind *m;
    float dx;
    float dz;
    float len;

    m = e ? mind_of(e) : 0;
    if (!m || speed <= KNOCK_ENDS) {
        return;
    }
    dx = e->x - from[0];
    dz = e->z - from[2];
    len = df_sqrt(dx * dx + dz * dz);
    if (len < 1.0f) {
        return;
    }
    m->knock_dir[0] = dx / len;
    m->knock_dir[1] = dz / len;
    m->knock = (float)(speed > KNOCK_MOST ? KNOCK_MOST : speed);
}

int foes_rouse_nearest(const float *at)
{
    Entity *best;
    float best_at;
    int slot;

    best = 0;
    best_at = FOES_ROUSE_REACH * FOES_ROUSE_REACH;
    for (slot = 0; slot < stream_slots(); slot++) {
        Entity *ents;
        int first;
        int n;
        int i;

        ents = stream_slot_entities(slot, &first, &n);
        for (i = 0; ents && i < n; i++) {
            float dx;
            float dz;
            float d;

            if (!ents[i].live || ents[i].dead || dunstream_kind(&ents[i], 0) < 0 || dunstream_passive(&ents[i]) || mind_of(&ents[i])) {
                continue;
            }
            dx = ents[i].x - at[0];
            dz = ents[i].z - at[2];
            d = dx * dx + dz * dz;
            if (d < best_at) {
                best = &ents[i];
                best_at = d;
            }
        }
    }
    if (!best) {
        return 0;
    }
    foes_alert(best);
    return mind_of(best) != 0;
}

int foes_stealth_chance(float distance, int stealth)
{
    return 2 * ((df_ifloor(distance) * stealth) >> 10);
}

/* A number for this creature and this game minute: the same however often it is asked within the minute. */
static int minute_roll(const Entity *e, int minute)
{
    df_u32 h;

    h = e->key.thing ^ ((df_u32)minute * 0x9E3779B1u);
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return (int)(h % 100u);
}

/* Whether a sleeper notices the player, sight or no sight, this minute. */
static int notices(const Entity *e, Player *p, float distance)
{
    int minute;

    if (distance > FOES_STEALTH) {
        return 0;
    }
    minute = df_ifloor(clock_minute());
    /* One use of the skill a game minute that it is rolled against. */
    if (minute != g_stealth_minute) {
        g_stealth_minute = minute;
        character_tally(&p->sheet, SKILL_STEALTH, 1);
    }
    return minute_roll(e, minute) >= foes_stealth_chance(distance, p->sheet.skill[SKILL_STEALTH]);
}

/* The door in the way at a point its ray stopped, by number; -1 for none. */
static int door_at(const Scene *scene, const float *at)
{
    int i;

    for (i = 0; i < scene->nmovers; i++) {
        const Mover *d;
        Bound b;

        d = &scene->movers[i];
        if (!d->inst || !d->door || !mover_sweep(d, &b)) {
            continue;
        }
        if (absf(at[0] - b.cx) <= b.ex + DOOR_SLACK && absf(at[1] - b.cy) <= b.ey + DOOR_SLACK
            && absf(at[2] - b.cz) <= b.ez + DOOR_SLACK) {
            return i;
        }
    }
    return -1;
}

/*
 * Whether it hears the player: within reach, through a door but not a wall
 * (EnemySenses' ray meets only static geometry). A ray that stops at a door
 * is tried again from past it, and the door is remembered.
 */
static int hears_player(Mind *m, const Scene *scene, const float *from, const float *peye)
{
    float d[3];
    float len;
    float t;
    float at[3];
    int door;
    int i;

    for (i = 0; i < 3; i++) {
        d[i] = peye[i] - from[i];
    }
    len = df_sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len > FOES_HEARING || len < 1.0f || !combat_ray(scene, from, peye, &t)) {
        return len <= FOES_HEARING;
    }
    for (i = 0; i < 3; i++) {
        d[i] /= len;
        at[i] = from[i] + d[i] * t;
    }
    door = door_at(scene, at);
    if (door < 0) {
        return 0;
    }
    m->door = door;
    for (i = 0; i < 3; i++) {
        at[i] += d[i] * DOOR_THROUGH;
    }
    return combat_sight(scene, at, peye);
}

/* A door it knows of within reach, closed and not locked, opens, if it is a creature that can. */
static void open_door(Mind *m, const Scene *scene, const EnemyKind *k)
{
    const Mover *d;
    Bound b;
    float dx;
    float dz;

    if (!k || !k->opens_doors || m->door < 0 || m->door >= scene->nmovers) {
        return;
    }
    d = &scene->movers[m->door];
    if (!d->inst || !d->door || d->lock > 0 || mover_at_end(d) || mover_busy(scene, m->door) || !mover_sweep(d, &b)) {
        return;
    }
    dx = b.cx - m->e->x;
    dz = b.cz - m->e->z;
    if (dx * dx + dz * dz <= FOES_DOOR_REACH * FOES_DOOR_REACH) {
        mover_start(scene, m->door);
        m->door = -1;
    }
}

/* What it knows of the player now: where they are, and the way they were going. */
static void know_player(Mind *m, const float *pfeet)
{
    float dx;
    float dz;
    float len;

    dx = pfeet[0] - m->last[0];
    dz = pfeet[2] - m->last[2];
    len = df_sqrt(dx * dx + dz * dz);
    if (len > 1.0f) {
        m->heading[0] = dx / len;
        m->heading[1] = dz / len;
    }
    m->last[0] = pfeet[0];
    m->last[1] = pfeet[1];
    m->last[2] = pfeet[2];
    m->give_up = FOES_GIVE_UP;
    m->search = 0;
}

/* Entity number n across the stream's slots, or 0. */
static Entity *nth(int n, int *total)
{
    Entity *found;
    int slot;
    int at;

    found = 0;
    at = 0;
    for (slot = 0; slot < stream_slots(); slot++) {
        Entity *ents;
        int first;
        int count;

        ents = stream_slot_entities(slot, &first, &count);
        if (!ents) {
            continue;
        }
        if (!found && n >= at && n < at + count) {
            found = &ents[n - at];
        }
        at += count;
    }
    *total = at;
    return found;
}

/* A sleeper looks: the player within sight, before it, no wall between their eyes. */
static void look_for(Player *p, const Scene *scene, const float *peye)
{
    int tries;

    for (tries = 0; tries < LOOKS_PER_TICK; tries++) {
        Entity *e;
        float eye[3];
        float dx;
        float dy;
        float dz;
        int total;
        int kind;
        int far;
        int seen;

        e = nth(g_scan, &total);
        g_scan = total ? (g_scan + 1) % total : 0;
        if (!e || !e->live || e->dead || mind_of(e)) {
            continue;
        }
        kind = dunstream_kind(e, 0);
        if (kind < 0 || dunstream_passive(e)) {
            continue;
        }
        eye_of(e, eye);
        dx = peye[0] - eye[0];
        dy = peye[1] - eye[1];
        dz = peye[2] - eye[2];
        /* 180 degrees: anything not behind it; or, sight or no sight, it notices the player. */
        far = dx * dx + dy * dy + dz * dz > FOES_SIGHT * FOES_SIGHT;
        seen = !far && df_sin(e->yaw) * dx + df_cos(e->yaw) * dz >= 0.0f && combat_sight(scene, eye, peye);
        if (seen || (!far && notices(e, p, df_sqrt(dx * dx + dz * dz)))) {
            Mind *m;

            m = wake(e, kind);
            if (m) {
                m->sees = seen;
                m->last[0] = p->body.feet[0];
                m->last[1] = p->body.feet[1];
                m->last[2] = p->body.feet[2];
            }
        }
    }
}

/*
 * DFU's melee timer: 1500 to 3000, less 50 a level past 10, 450 a step of
 * reflexes past average (classic's FALL.EXE, docs/game.md), the slower the
 * player's reflexes the longer; classic's frames to seconds.
 */
static float melee_timer(const Character *pc)
{
    int t;

    t = 1500 + combat_roll(1501) - 50 * (pc->level - 10) + 450 * (pc->reflexes - RULES_REFLEX_AVERAGE);
    return t < 0 ? 0.0f : (float)t / 980.0f;
}

/* A classic tick of one mind: its sight now and then, the give-up count, the turn, a swing begun. */
static void think(Mind *m, const Player *p, const Scene *scene, const float *peye)
{
    const EnemyKind *k;
    Entity *e;
    float to;
    float dx;
    float dy;
    float dz;
    float dist;

    e = m->e;
    if (--m->look_in <= 0) {
        float eye[3];

        m->look_in = SIGHT_EVERY;
        eye_of(e, eye);
        m->sees = combat_sight(scene, eye, peye);
        m->hears = !m->sees && hears_player(m, scene, eye, peye);
    }
    open_door(m, scene, enemy_kind(m->kind));
    if (m->sees || m->hears) {
        know_player(m, p->body.feet);
    } else if (--m->give_up <= 0) {
        /* Lost: it stands where it is, asleep again until it sees the player. */
        e->moving = 0;
        m->e = 0;
        return;
    }
    dx = m->last[0] - e->x;
    dz = m->last[2] - e->z;
    to = turn_between(e->yaw, df_atan2(dx, dz));
    e->yaw = df_wrap_tau(e->yaw + (to > TURN ? TURN : (to < -TURN ? -TURN : to)));
    /* A swing: the player seen, in reach and nearly ahead, the timer run out, a roll on its Speed. */
    k = enemy_kind(m->kind);
    /* Feet to feet, height and all: one on the floor above, or a bat overhead, is not in reach. */
    dy = m->last[1] - e->y;
    dist = df_sqrt(dx * dx + dy * dy + dz * dz);
    if (k && m->sees && m->swing < 0.0f && e->act == 0 && m->melee <= 0.0f && dist <= FOES_MELEE
        && absf(turn_between(e->yaw, df_atan2(dx, dz))) <= AIM_START && e->pose[ENTITY_ATTACK].w[0]) {
        int speed;

        speed = k->attribute[ATTR_SPEED] < 8 ? 8 : k->attribute[ATTR_SPEED];
        /* Classic swings on a roll under Speed / 8 + 6 (FALL.EXE's AI): about one tick in four at Speed 50. */
        if (combat_roll(speed) < speed / 8 + 6) {
            m->swing = 0.0f;
            m->landed = 0;
            e->act = ENTITY_ATTACK;
            e->hold = 1;
            e->frame = 0;
            e->moving = 0;
            m->melee = melee_timer(&p->sheet);
            sfx_at(k->attack_sound, e->x, e->y + entity_height(e) * 0.5f, e->z);
        }
    }
}

/*
 * Its blow on the swing's hit frame: the player still in reach feet to feet,
 * height and all, before it, and no wall come between in the moments since it
 * began. 1 when it killed them.
 */
static int land(Mind *m, Player *p, const Scene *scene)
{
    const EnemyKind *k;
    Entity *e;
    Foe f;
    Blow b;
    Inventory kit;
    float dx;
    float dy;
    float dz;
    float ground;
    float dist;
    float eye[3];
    float peye[3];
    float pfeet[3];
    int died;

    e = m->e;
    k = enemy_kind(m->kind);
    if (!k) {
        return 0;
    }
    dx = p->body.feet[0] - e->x;
    dy = p->body.feet[1] - e->y;
    dz = p->body.feet[2] - e->z;
    ground = df_sqrt(dx * dx + dz * dz);
    dist = df_sqrt(ground * ground + dy * dy);
    if (dist > FOES_MELEE + POINT_BLANK
        || (ground > POINT_BLANK && absf(turn_between(e->yaw, df_atan2(dx, dz))) > AIM_LAND)) {
        return 0;
    }
    player_points(p, peye, pfeet);
    eye_of(e, eye);
    if (!combat_sight(scene, eye, peye)) {
        return 0;
    }
    g_blow_reach = dist > g_blow_reach ? dist : g_blow_reach;
    combat_foe(&f, k, p->sheet.level, 0);
    /* A class enemy strikes with what it carries (loot_class_kit). */
    if (k->id >= ENEMY_FIRST_CLASS) {
        loot_class_kit(e, p->sheet.level, &kit, 0);
        f.weapon = inventory_weapon(&kit);
    }
    combat_foe_blow(&f, &p->sheet, &b);
    /* A blow come at the player trains Dodging, hit or not (EnemyAttack.ApplyDamageToPlayer). */
    character_tally(&p->sheet, SKILL_DODGING, 1);
    if (!b.hit || b.damage <= 0) {
        sfx_at(SND_SWING_HIGH, e->x, e->y + entity_height(e) * 0.5f, e->z);
        return 0;
    }
    sfx_play(sfx_pick(SND_HIT_FIRST, SND_HITS));
    player_armor_struck(p, b.part, b.damage, combat_roll(100));
    died = player_hurt(p, b.damage);
    if (died >= 0) {
        sfx_play(sfx_pain(p->sheet.race, p->sheet.female));
    }
    return died > 0;
}

/* A flyer keeps level with the player's feet, climbing or sinking at most this far. */
static void fly_level(Mind *m, float most)
{
    float dy;

    dy = m->last[1] - m->body.feet[1];
    m->body.feet[1] += dy > most ? most : (dy < -most ? -most : dy);
}

/* A frame of one mind: its swing's frames, or its walk toward where it last saw the player. */
static int act(Mind *m, Player *p, const Scene *scene, float dt)
{
    Entity *e;
    float dx;
    float dz;
    float dist;

    e = m->e;
    if (m->melee > 0.0f) {
        m->melee -= dt;
    }
    /* Knocked back: it slides, dying away a little each classic tick, and does nothing else. */
    if (m->knock > KNOCK_ENDS) {
        float ground;

        ground = (m->knock > KNOCK_MOVES_MOST ? KNOCK_MOVES_MOST : m->knock) * KNOCK_UNITS * dt;
        body_step(&m->body, scene, m->knock_dir[0] * ground, m->knock_dir[1] * ground, dt);
        e->x = m->body.feet[0];
        e->y = m->body.feet[1];
        e->z = m->body.feet[2];
        m->knock -= KNOCK_DECAY * (dt / FOES_TICK);
        e->moving = 0;
        return 0;
    }
    if (m->swing >= 0.0f) {
        int frame;

        /* Struck mid-swing, it flinches instead: the swing is lost. */
        if (e->act != ENTITY_ATTACK) {
            m->swing = -1.0f;
            return 0;
        }
        m->swing += dt;
        frame = df_ifloor(m->swing * ATTACK_RATE);
        if (frame >= ATTACK_FRAMES) {
            m->swing = -1.0f;
            e->act = 0;
            e->hold = 0;
            return 0;
        }
        e->frame = (df_u8)frame;
        if (!m->landed && frame >= ATTACK_HIT_FRAME) {
            m->landed = 1;
            return land(m, p, scene);
        }
        return 0;
    }
    /* A flinch stands it still. */
    if (e->act == ENTITY_HURT) {
        e->moving = 0;
        return 0;
    }
    dx = m->last[0] - e->x;
    dz = m->last[2] - e->z;
    dist = df_sqrt(dx * dx + dz * dz);
    /* Near enough (in reach of the player, or at where it saw them): it stands and faces. */
    if (dist <= (m->sees ? STOP : m->body.radius)) {
        /* A flyer overhead still comes down to the one it sees, or it would hang out of reach for good. */
        if (m->flies && m->sees) {
            const EnemyKind *flyer;

            flyer = enemy_kind(m->kind);
            fly_level(m, rules_walk_speed(flyer ? flyer->attribute[ATTR_SPEED] : 50) * RULES_UNITS_PER_M * dt);
            e->y = m->body.feet[1];
        }
        /* At where the player was last known and they are not here: on past it, along the way they were going. */
        if (!m->sees && !m->hears && m->search < SEARCH_MOST) {
            m->search++;
            m->last[0] += m->heading[0] * SEARCH_STEP * (float)m->search;
            m->last[2] += m->heading[1] * SEARCH_STEP * (float)m->search;
        }
        e->moving = 0;
        return 0;
    }
    {
        const EnemyKind *k;
        float step;
        float sx;
        float sz;

        k = enemy_kind(m->kind);
        step = rules_walk_speed(k ? k->attribute[ATTR_SPEED] : 50) * RULES_UNITS_PER_M * dt;
        if (step > dist - (m->sees ? STOP : 0.0f)) {
            step = dist - (m->sees ? STOP : 0.0f);
        }
        /* Along its facing, as classic walks: it turns first, then goes. */
        sx = df_sin(e->yaw) * step;
        sz = df_cos(e->yaw) * step;
        if (m->flies) {
            fly_level(m, step);
        }
        body_step(&m->body, scene, sx, sz, dt);
        e->moving = 1;
        e->x = m->body.feet[0];
        e->y = m->body.feet[1];
        e->z = m->body.feet[2];
        m->move_sound -= dt;
        if (m->move_sound <= 0.0f && k) {
            sfx_at(k->move_sound, e->x, e->y + entity_height(e) * 0.5f, e->z);
            m->move_sound = MOVE_SOUND_MIN + (float)combat_roll(100) * (MOVE_SOUND_SPAN / 100.0f);
        }
    }
    return 0;
}

float foes_blow_reach(void)
{
    return g_blow_reach;
}

int foes_due(float distance, int frame, int index)
{
    int every;

    every = distance > FOES_FAR ? 4 : (distance > FOES_NEAR ? 2 : 1);
    return ((frame + index) & (every - 1)) == 0;
}

/* The ground between a mind and the player's feet, on the level: enough to choose how often it moves. */
static float ground_distance(const Mind *m, const float *pfeet)
{
    float dx;
    float dz;

    dx = m->body.feet[0] - pfeet[0];
    dz = m->body.feet[2] - pfeet[2];
    return df_sqrt(dx * dx + dz * dz);
}

int foes_frame(Player *p, const Scene *scene, float dt)
{
    float peye[3];
    float pfeet[3];
    int ticks;
    int died;
    int i;

    if (!g_on || !scene) {
        return 0;
    }
    /* A mind whose creature died, or whose block went (its slot holds another now), is let go. */
    for (i = 0; i < FOES_MINDS; i++) {
        Entity *e;

        e = g_mind[i].e;
        if (e && (!e->live || e->dead || e->key.place != g_mind[i].key.place || e->key.thing != g_mind[i].key.thing)) {
            g_mind[i].e = 0;
        }
    }
    player_points(p, peye, pfeet);
    g_ticks += dt;
    ticks = 0;
    while (g_ticks >= FOES_TICK && ticks < TICKS_MAX) {
        g_ticks -= FOES_TICK;
        ticks++;
        look_for(p, scene, peye);
        for (i = 0; i < FOES_MINDS; i++) {
            if (g_mind[i].e) {
                think(&g_mind[i], p, scene, peye);
            }
        }
    }
    if (g_ticks > FOES_TICK) {
        g_ticks = 0.0f;
    }
    died = 0;
    g_frame++;
    for (i = 0; i < FOES_MINDS; i++) {
        Mind *m;

        m = &g_mind[i];
        if (!m->e) {
            continue;
        }
        m->owed += dt;
        if (foes_due(ground_distance(m, pfeet), g_frame, i)) {
            float owed;

            owed = m->owed;
            m->owed = 0.0f;
            if (act(m, p, scene, owed)) {
                died = 1;
            }
        }
    }
    return died;
}
