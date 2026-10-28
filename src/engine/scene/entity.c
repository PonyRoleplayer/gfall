#include "engine/scene/entity.h"
#include "engine/core/mem.h"
#include "engine/scene/walker.h"

/* See entity.h and docs/entities.md. */

/* An eighth of a turn, and half of it: the edges between views. */
#define EIGHTH (DF_TAU / 8.0f)

const EntityPose *entity_pose(const Entity *e)
{
    if (e->moving || e->idle.slot[0] == 0xffff) {
        return &e->walk;
    }
    return &e->idle;
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

int entity_blocked(const Scene *scene, const Entity *self, float x, float z)
{
    float lo;
    float hi;
    float bx, by, bz;
    int i;

    if (!self || self->radius <= 0.0f) {
        return 0;
    }
    lo = self->y;
    hi = self->y + entity_height(self);
    if (walker_body(&bx, &by, &bz) && hi > by && lo < by + WALK_HEIGHT
        && circles_meet(x, z, self->radius, bx, bz, WALK_RADIUS)) {
        return 1;
    }
    for (i = 0; scene && scene->entities && i < scene->nentities; i++) {
        const Entity *o;

        o = &scene->entities[i];
        if (o == self || !o->live || o->radius <= 0.0f) {
            continue;
        }
        if (hi > o->y && lo < o->y + entity_height(o) && circles_meet(x, z, self->radius, o->x, o->z, o->radius)
            /* Already overlapping (born so): let it walk out, not freeze. */
            && !circles_meet(self->x, self->z, self->radius, o->x, o->z, o->radius)) {
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
