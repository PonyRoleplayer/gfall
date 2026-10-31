#include "game/dagger/play/watch.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/sfx.h"
#include "game/dagger/places/people.h"
#include "game/dagger/tables/enemies.h"
#include "engine/core/df.h"
#include "engine/scene/interact.h"

/* See watch.h and docs/game.md (People). */

/* One shout a second: the guard is closing, not a single line of three words. */
#define HALT_GAP 1.0f

static const char *g_line[WATCH_HALTS];
static int g_n;
static int g_said;
static float g_wait;
static Entity *g_guard;

static void shout(const Entity *e)
{
    const EnemyKind *k;

    interact_say("HALT");
    k = enemy_kind(ENEMY_CITY_WATCH);
    if (!k) {
        return;
    }
    if (e) {
        sfx_at(k->bark_sound, e->x, e->y + entity_height(e) * 0.5f, e->z);
    } else {
        sfx_play(k->bark_sound);
    }
}

void watch_reset(void)
{
    g_n = 0;
    g_said = 0;
    g_wait = 0.0f;
    g_guard = 0;
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
    shout(g_guard);
    g_said++;
}

void watch_on_kill(const Camera *cam, const float *at)
{
    Entity *e;
    int slot;
    int i;

    g_n = WATCH_HALTS;
    for (i = 0; i < WATCH_HALTS; i++) {
        g_line[i] = "HALT";
    }
    g_said = 0;
    g_wait = 0.0f;
    g_guard = 0;
    slot = at ? people_nearest_guard(at[0], at[2]) : -1;
    if (slot < 0 && cam) {
        slot = people_spawn_guard(cam);
    }
    e = people_at(slot);
    if (e && people_is_guard(e) && at) {
        people_hold(slot);
        e->yaw = df_atan2(at[0] - e->x, at[2] - e->z);
        e->moving = 1;
        if (foes_summon(e, ENEMY_CITY_WATCH, at)) {
            g_guard = e;
        }
    }
    shout(g_guard);
    g_said = 1;
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
    return g_guard;
}
