#include "engine/scene/entity.h"
#include "engine/scene/body.h"
#include "engine/core/mem.h"
#include "engine/scene/walker.h"
#include "engine/scene/spawn.h"

/* See entity.h and docs/entities.md. */

/* An eighth of a turn, and half of it: the edges between views. */
#define EIGHTH (DF_TAU / 8.0f)

void entity_init(Entity *e)
{
    int p;
    int k;

    mem_zero(e, sizeof(Entity));
    for (p = 0; p < ENTITY_POSES; p++) {
        for (k = 0; k < ENTITY_VIEWS; k++) {
            e->pose[p].slot[k] = 0xffff;
        }
    }
}

int entity_solid(const Entity *e)
{
    return e && e->live && e->radius > 0.0f && !(e->flags & (ENT_GHOST | ENT_HIDDEN));
}

/* 1 when the game filled pose p in: a picture and a size. */
static int posed(const EntityPose *p)
{
    return p->slot[0] != 0xffff && (p->w[0] > 0 || p->h[0] > 0);
}

const EntityPose *entity_pose(const Entity *e)
{
    if (e->act >= ENTITY_ATTACK && e->act < ENTITY_POSES && posed(&e->pose[e->act])) {
        return &e->pose[e->act];
    }
    if (e->moving || e->pose[ENTITY_IDLE].slot[0] == 0xffff) {
        return &e->pose[ENTITY_WALK];
    }
    return &e->pose[ENTITY_IDLE];
}

int entity_view(const Entity *e, float cx, float cz, int *flip)
{
    const EntityPose *pose;
    float toward;
    float d;
    int o;

    *flip = 0;
    pose = entity_pose(e);
    if (pose->slot[1] == 0xffff) {
        return 0;
    }
    /*
     * The yaw from the entity to the camera; then how far the entity's
     * own facing is turned from it. 0: it faces the camera (record 0). A
     * quarter turn more: it faces the camera's left, side on (record 2);
     * a quarter less, the right, mirrored. Half a turn: its back (4).
     */
    toward = df_atan2(cx - e->x, cz - e->z);
    d = e->yaw - toward;
    while (d > DF_TAU * 0.5f) {
        d -= DF_TAU;
    }
    while (d < -DF_TAU * 0.5f) {
        d += DF_TAU;
    }
    o = df_ifloor(df_absf(d) / EIGHTH + 0.5f);
    o = o > 4 ? 4 : o;
    *flip = (d < 0.0f && o > 0 && o < 4) ^ (int)((pose->right >> o) & 1u);
    return o;
}

int entity_flat(const Entity *e, float cx, float cz, Flat *out)
{
    const EntityPose *pose;
    int flip;
    int view;

    pose = entity_pose(e);
    view = entity_view(e, cx, cz, &flip);
    if (pose->slot[view] == 0xffff) {
        return 0;
    }
    mem_zero(out, sizeof(Flat));
    out->w = (float)pose->w[view];
    out->h = (float)pose->h[view];
    out->x = e->x;
    out->y = e->y + out->h * 0.5f;
    out->z = e->z;
    out->slot = pose->slot[view];
    out->rate = pose->rate;
    out->phase = e->phase;
    out->flip = (df_u8)flip;
    out->held = e->hold;
    out->frame = e->frame;
    return 1;
}

float entity_height(const Entity *e)
{
    const EntityPose *pose;

    pose = entity_pose(e);
    return (float)(pose->h[0] ? pose->h[0] : pose->w[0]);
}

/* 1 when circles of radii ra and rb at a and b meet across the floor. */
static int circles_meet(float ax, float az, float ra, float bx, float bz, float rb)
{
    float dx;
    float dz;

    dx = ax - bx;
    dz = az - bz;
    return dx * dx + dz * dz < (ra + rb) * (ra + rb);
}

/* 1 when self, moved to x, z and standing lo..hi, would walk into a solid entity of list[0..n-1]. */
/* The near-gather's candidates: as many as stand round one entity at once, and more walks everything. */
#define NEAR_CAP 128

/* 1 when o stops self's step to x, z (its body from lo to hi). */
static int blocked_one(const Entity *o, const Entity *self, float x, float z, float lo, float hi)
{
    if (o == self || !entity_solid(o)) {
        return 0;
    }
    return hi > o->y && lo < o->y + entity_height(o) && circles_meet(x, z, self->radius, o->x, o->z, o->radius)
        /* Already overlapping (born so): let it walk out, not freeze. */
        && !circles_meet(self->x, self->z, self->radius, o->x, o->z, o->radius);
}

static int blocked_by(const Entity *list, int n, const Entity *self, float x, float z, float lo, float hi)
{
    int i;

    for (i = 0; list && i < n; i++) {
        if (blocked_one(&list[i], self, x, z, lo, hi)) {
            return 1;
        }
    }
    return 0;
}

int entity_blocked(const Scene *scene, const Entity *self, float x, float z)
{
    EntList lists[ENT_LISTS];
    float lo;
    float hi;
    float bx, by, bz;
    int n;
    int i;

    if (!self || self->radius <= 0.0f) {
        return 0;
    }
    lo = self->y;
    hi = self->y + entity_height(self);
    /* The player's body, by its own size (a game's may not be the walker's). */
    if (walker_body(&bx, &by, &bz) && hi > by && lo < by + walker_own_body()->height
        && circles_meet(x, z, self->radius, bx, bz, walker_own_body()->radius)) {
        return 1;
    }
    if (!scene) {
        return 0;
    }
    /*
     * The place's own, the game's ranges and those made while playing
     * (docs/entities.md, The registry): the ones near the step, by the
     * grid, or all of them when there is no grid to ask. The step's end
     * and the feet now are both tested, so the reach covers the step too.
     */
    {
        const Entity *near_ents[NEAR_CAP];
        float dx, dz;
        float reach;

        dx = x - self->x;
        dz = z - self->z;
        reach = self->radius + (dx < 0.0f ? -dx : dx) + (dz < 0.0f ? -dz : dz);
        n = ent_near_solid(scene, self->x, self->z, reach, near_ents, NEAR_CAP);
        if (n >= 0) {
            for (i = 0; i < n; i++) {
                if (blocked_one(near_ents[i], self, x, z, lo, hi)) {
                    return 1;
                }
            }
            return 0;
        }
    }
    n = ent_lists(scene, lists, ENT_LISTS);
    for (i = 0; i < n; i++) {
        if (blocked_by(lists[i].first, lists[i].n, self, x, z, lo, hi)) {
            return 1;
        }
    }
    return 0;
}

static int g_population = 1;

void entity_set_population(int on)
{
    g_population = on ? 1 : 0;
}

int entity_population(void)
{
    return g_population;
}
