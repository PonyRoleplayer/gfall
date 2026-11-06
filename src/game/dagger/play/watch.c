#include "game/dagger/play/watch.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/sfx.h"
#include "game/dagger/places/people.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/tables/enemies.h"
#include "engine/core/df.h"
#include "engine/scene/interact.h"
#include "engine/scene/keep.h"

/* See watch.h and docs/game.md (People). */

/* One shout a second: the guard is closing, not a single line of three words. */
#define HALT_GAP 1.0f
/* The wanted row: not a location, so a building's load does not drop it. */
#define WANTED_PLACE 0x57415443u
/* DFU spawns 2..5 when no guard was already in the street. */
#define SPAWN_MIN 2
#define SPAWN_EXTRA 4
/* A guard this near the crime is close enough; farther, the street is given its own (units: a few seconds at a run). */
#define WATCH_CLOSE 1800.0f

static const char *g_line[WATCH_HALTS];
static int g_n;
static int g_said;
static float g_wait;
static Entity *g_guard;

static KeepKey wanted_key(void)
{
    KeepKey k;

    k.place = WANTED_PLACE;
    k.thing = 1u;
    return k;
}

static int still_guard(const Entity *e)
{
    return e && people_index(e) >= 0 && people_is_guard(e);
}

static void shout(const Entity *e)
{
    const EnemyKind *k;

    interact_say("HALT");
    k = enemy_kind(ENEMY_CITY_WATCH);
    if (!k) {
        return;
    }
    if (still_guard(e)) {
        sfx_at(k->bark_sound, e->x, e->y + entity_height(e) * 0.5f, e->z);
    } else {
        sfx_play(k->bark_sound);
    }
}

/* Hostile, held out of the street, and chasing `at`. 1 when they are a guard. */
static int turn_guard(int slot, const float *at)
{
    Entity *e;

    e = people_at(slot);
    if (!e || !people_is_guard(e)) {
        return 0;
    }
    people_set_hostile(e, 1);
    people_hold(slot);
    if (at && !foes_minding(e)) {
        e->yaw = df_atan2(at[0] - e->x, at[2] - e->z);
        e->moving = 1;
        foes_summon(e, ENEMY_CITY_WATCH, at);
    }
    return 1;
}

static int turn_all(const float *at)
{
    int i;
    int n;

    n = 0;
    for (i = 0; i < PEOPLE_MAX; i++) {
        n += turn_guard(i, at);
    }
    return n;
}

static void remember_nearest(const float *at)
{
    int slot;

    if (!still_guard(g_guard)) {
        g_guard = 0;
    }
    if (!at) {
        return;
    }
    slot = people_nearest_guard(at[0], at[2]);
    if (slot >= 0) {
        g_guard = people_at(slot);
    }
}

void watch_reset(void)
{
    g_n = 0;
    g_said = 0;
    g_wait = 0.0f;
    g_guard = 0;
}

int watch_wanted(void)
{
    Keep *r;

    r = keep_find(wanted_key());
    return r && r->game[0] != 0;
}

void watch_pardon(void)
{
    int i;

    watch_reset();
    keep_forget(wanted_key());
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;

        e = people_at(i);
        if (!e || !people_hostile(e)) {
            continue;
        }
        foes_release(e);
        people_set_hostile(e, 0);
        people_unhold(e);
    }
}

void watch_frame(float dt)
{
    if (g_said <= 0 || g_said >= g_n) {
        return;
    }
    g_wait += dt;
    if (g_wait < HALT_GAP) {
        return;
    }
    g_wait = 0.0f;
    shout(still_guard(g_guard) ? g_guard : 0);
    g_said++;
}

void watch_pursue(const Camera *cam, const float *at)
{
    int region;
    int index;
    int block;
    int record;
    float x;
    float z;

    if (!watch_wanted()) {
        return;
    }
    /* A building is still the town. The wild and a dungeon are not. */
    if (cam && !travel_town_here(cam, &region, &index, &x, &z, &block, &record)) {
        watch_pardon();
        return;
    }
    if (!at) {
        return;
    }
    turn_all(at);
    remember_nearest(at);
}

void watch_alarm(const Camera *cam, const float *at)
{
    Keep *row;
    int n;
    int i;

    row = keep_get(wanted_key(), KEEP_ENTITY);
    if (row) {
        row->game[0] = 1;
    }
    g_n = WATCH_HALTS;
    for (i = 0; i < WATCH_HALTS; i++) {
        g_line[i] = "HALT";
    }
    g_said = 0;
    g_wait = 0.0f;
    g_guard = 0;
    n = turn_all(at);
    /* The loaded guards come, but a street's guards may be far off: with none close, some are made near. */
    remember_nearest(at);
    if (n > 0 && g_guard && at) {
        float dx;
        float dz;

        dx = g_guard->x - at[0];
        dz = g_guard->z - at[2];
        if (dx * dx + dz * dz > WATCH_CLOSE * WATCH_CLOSE) {
            n = 0;
        }
    }
    if (n == 0 && cam) {
        int want;

        want = SPAWN_MIN + combat_roll(SPAWN_EXTRA);
        for (i = 0; i < want; i++) {
            int slot;

            slot = people_spawn_guard(cam);
            if (slot >= 0) {
                turn_guard(slot, at);
            }
        }
    }
    remember_nearest(at);
    shout(g_guard);
    g_said = 1;
}

void watch_on_kill(const Camera *cam, const float *at)
{
    watch_alarm(cam, at);
}

int watch_halts(void)
{
    return g_n;
}

const char *watch_halt(int i)
{
    if (i < 0 || i >= g_n) {
        return 0;
    }
    return g_line[i];
}

Entity *watch_guard(void)
{
    return still_guard(g_guard) ? g_guard : 0;
}
