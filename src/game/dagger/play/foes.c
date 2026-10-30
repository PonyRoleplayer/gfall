#include "game/dagger/play/foes.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/rules.h"
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
} Mind;

static Mind g_mind[FOES_MINDS];
static int g_on = 1;
static float g_ticks;
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

    for (i = 0; i < FOES_MINDS; i++) {
        g_mind[i].e = 0;
    }
    g_ticks = 0.0f;
    g_scan = 0;
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
static void look_for(const Player *p, const Scene *scene, const float *peye)
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
        /* 180 degrees: anything not behind it. */
        if (dx * dx + dy * dy + dz * dz > FOES_SIGHT * FOES_SIGHT || df_sin(e->yaw) * dx + df_cos(e->yaw) * dz < 0.0f) {
            continue;
        }
        if (combat_sight(scene, eye, peye)) {
            Mind *m;

            m = wake(e, kind);
            if (m) {
                m->sees = 1;
                m->last[0] = p->body.feet[0];
                m->last[1] = p->body.feet[1];
                m->last[2] = p->body.feet[2];
            }
        }
    }
}

/* DFU's melee timer: 1500 to 3000, less 50 a level past 10, reflexes average; classic's frames to seconds. */
static float melee_timer(int player_level)
{
    int t;

    t = 1500 + combat_roll(1501) - 50 * (player_level - 10);
    return t < 0 ? 0.0f : (float)t / 980.0f;
}

/* A classic tick of one mind: its sight now and then, the give-up count, the turn, a swing begun. */
static void think(Mind *m, const Player *p, const Scene *scene, const float *peye)
{
    const EnemyKind *k;
    Entity *e;
    float to;
    float dx;
    float dz;
    float dist;

    e = m->e;
    if (--m->look_in <= 0) {
        float eye[3];

        m->look_in = SIGHT_EVERY;
        eye_of(e, eye);
        m->sees = combat_sight(scene, eye, peye);
    }
    if (m->sees) {
        m->give_up = FOES_GIVE_UP;
        m->last[0] = p->body.feet[0];
        m->last[1] = p->body.feet[1];
        m->last[2] = p->body.feet[2];
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
    dist = df_sqrt(dx * dx + dz * dz);
    if (k && m->sees && m->swing < 0.0f && e->act == 0 && m->melee <= 0.0f && dist <= FOES_MELEE
        && absf(turn_between(e->yaw, df_atan2(dx, dz))) <= AIM_START && e->pose[ENTITY_ATTACK].w[0]) {
        int speed;

        speed = k->attribute[ATTR_SPEED] < 8 ? 8 : k->attribute[ATTR_SPEED];
        if (combat_roll(speed) >= speed / 8 + 6) {
            m->swing = 0.0f;
            m->landed = 0;
            e->act = ENTITY_ATTACK;
            e->hold = 1;
            e->frame = 0;
            e->moving = 0;
            m->melee = melee_timer(p->sheet.level);
            sfx_at(k->attack_sound, e->x, e->y + entity_height(e) * 0.5f, e->z);
        }
    }
}

/* Its blow on the swing's hit frame: the player still in reach and before it. 1 when it killed them. */
static int land(Mind *m, Player *p)
{
    const EnemyKind *k;
    Entity *e;
    Foe f;
    Blow b;
    float dx;
    float dz;
    float dist;
    int died;

    e = m->e;
    k = enemy_kind(m->kind);
    if (!k) {
        return 0;
    }
    dx = p->body.feet[0] - e->x;
    dz = p->body.feet[2] - e->z;
    dist = df_sqrt(dx * dx + dz * dz);
    if (dist > FOES_MELEE + POINT_BLANK
        || (dist > POINT_BLANK && absf(turn_between(e->yaw, df_atan2(dx, dz))) > AIM_LAND)) {
        return 0;
    }
    combat_foe(&f, k, p->sheet.level, 0);
    combat_foe_blow(&f, &p->sheet, &b);
    if (!b.hit || b.damage <= 0) {
        sfx_at(SND_SWING_HIGH, e->x, e->y + entity_height(e) * 0.5f, e->z);
        return 0;
    }
    sfx_play(sfx_pick(SND_HIT_FIRST, SND_HITS));
    died = player_hurt(p, b.damage);
    if (died >= 0) {
        sfx_play(sfx_pain(p->sheet.race, p->sheet.female));
    }
    return died > 0;
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
            return land(m, p);
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
            float dy;
            float rise;

            /* A flyer keeps level with the player's middle, climbing at its walking speed. */
            dy = m->last[1] - m->body.feet[1];
            rise = step;
            m->body.feet[1] += dy > rise ? rise : (dy < -rise ? -rise : dy);
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
    for (i = 0; i < FOES_MINDS; i++) {
        if (g_mind[i].e && act(&g_mind[i], p, scene, dt)) {
            died = 1;
        }
    }
    return died;
}
