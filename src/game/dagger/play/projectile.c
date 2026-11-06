#include "game/dagger/play/projectile.h"
#include "engine/core/df.h"

/* See projectile.h and docs/game.md (Magic, the projectile). */

static Projectile g_proj[PROJ_MAX];

int projectile_launch(const ProjectileShot *shot)
{
    int i;
    int k;

    if (!shot || shot->kind == PROJ_NONE || shot->range <= 0.0f) {
        return -1;
    }
    for (i = 0; i < PROJ_MAX; i++) {
        Projectile *p;

        p = &g_proj[i];
        if (p->kind != PROJ_NONE) {
            continue;
        }
        p->kind = shot->kind;
        p->from = shot->from;
        p->tag = shot->tag;
        p->gravity = shot->gravity;
        p->left = shot->range;
        for (k = 0; k < 3; k++) {
            p->at[k] = shot->at[k];
            p->vel[k] = shot->dir[k] * shot->speed;
        }
        return i;
    }
    return -1;
}

/* A flight's end, written for the game. */
static void end_at(ProjectileEnd *e, int entry, const Projectile *p, int result, const float *at, const float *dir, void *target)
{
    int k;

    e->entry = entry;
    e->result = result;
    e->kind = p->kind;
    e->from = p->from;
    e->tag = p->tag;
    e->target = target;
    for (k = 0; k < 3; k++) {
        e->at[k] = at[k];
        e->dir[k] = dir[k];
    }
}

int projectile_step(float dt, const ProjectileWorld *world, ProjectileEnd *ends, int max)
{
    int i;
    int k;
    int made;

    made = 0;
    for (i = 0; i < PROJ_MAX; i++) {
        Projectile *p;
        float step[3];
        float to[3];
        float dir[3];
        float len;
        float t_wall;
        float t_target;
        void *who;
        int walled;

        p = &g_proj[i];
        if (p->kind == PROJ_NONE) {
            continue;
        }
        /* Gravity pulls first, so a long frame bends the same arc as two short ones, close enough for a shot. */
        p->vel[1] -= p->gravity * dt;
        for (k = 0; k < 3; k++) {
            step[k] = p->vel[k] * dt;
        }
        len = df_sqrt(step[0] * step[0] + step[1] * step[1] + step[2] * step[2]);
        if (len < 0.001f) {
            /* Standing still in the air is no flight: let it go, as an end of its range. */
            p->kind = PROJ_NONE;
            continue;
        }
        /* The last of its range is a shorter step. */
        if (len > p->left) {
            for (k = 0; k < 3; k++) {
                step[k] *= p->left / len;
            }
            len = p->left;
        }
        for (k = 0; k < 3; k++) {
            dir[k] = step[k] / len;
            to[k] = p->at[k] + step[k];
        }
        t_wall = len;
        walled = world && world->wall && world->wall(world->user, p->at, to, &t_wall) && t_wall <= len;
        t_target = len;
        who = world && world->target ? world->target(world->user, p, p->at, dir, len, &t_target) : 0;
        if (who && t_target > len) {
            who = 0;
        }
        if (who && (!walled || t_target <= t_wall)) {
            float hit[3];

            for (k = 0; k < 3; k++) {
                hit[k] = p->at[k] + dir[k] * t_target;
            }
            if (made < max) {
                end_at(&ends[made++], i, p, PROJ_HIT_TARGET, hit, dir, who);
            }
            p->kind = PROJ_NONE;
            continue;
        }
        if (walled) {
            float hit[3];

            for (k = 0; k < 3; k++) {
                hit[k] = p->at[k] + dir[k] * t_wall;
            }
            if (made < max) {
                end_at(&ends[made++], i, p, PROJ_HIT_WALL, hit, dir, 0);
            }
            p->kind = PROJ_NONE;
            continue;
        }
        for (k = 0; k < 3; k++) {
            p->at[k] = to[k];
        }
        p->left -= len;
        if (p->left <= 0.0f) {
            if (made < max) {
                end_at(&ends[made++], i, p, PROJ_EXPIRED, p->at, dir, 0);
            }
            p->kind = PROJ_NONE;
        }
    }
    return made;
}

void projectile_clear(void)
{
    int i;

    for (i = 0; i < PROJ_MAX; i++) {
        g_proj[i].kind = PROJ_NONE;
    }
}

int projectile_count(void)
{
    int i;
    int n;

    n = 0;
    for (i = 0; i < PROJ_MAX; i++) {
        if (g_proj[i].kind != PROJ_NONE) {
            n++;
        }
    }
    return n;
}

const Projectile *projectile_get(int i)
{
    return i >= 0 && i < PROJ_MAX && g_proj[i].kind != PROJ_NONE ? &g_proj[i] : 0;
}
