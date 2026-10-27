#include "engine/scene/entity.h"
#include "engine/core/mem.h"

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

static int g_population = 1;

void entity_set_population(int on)
{
    g_population = on ? 1 : 0;
}

int entity_population(void)
{
    return g_population;
}
