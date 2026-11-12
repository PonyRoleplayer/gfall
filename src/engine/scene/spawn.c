#include "engine/scene/spawn.h"
#include "engine/scene/ent.h"
#include "engine/scene/body.h"
#include "engine/scene/collide.h"
#include "engine/scene/walker.h"
#include "engine/core/mem.h"

/*
 * See spawn.h and docs/spawn.md. The pool is the registry's (ent.c):
 * these calls are a wrapper over its transient entries, kept for the
 * games' callers while they move to ent_*. spawn_find is this file's own.
 */

/* Directions round the ring, and radii across it, that spawn_find tries. */
#define FIND_DIRS 16
#define FIND_RINGS 3
/* A ray to the spot is gathered and cast in pieces this long, so no gather holds a whole road. */
#define RAY_PIECE 256.0f
/* Outside the view by this much more than half its width, a spot is out of sight. */
#define VIEW_MARGIN 10.0f

/* A pool entry's id: an id of the registry whose index is in the pool. */
static int pooled(SpawnId id)
{
    int i;

    i = (int)(id & 0xffffu) - 1;
    return i >= 0 && i < ENT_POOL;
}

SpawnId spawn_add(const Scene *scene, const Entity *e, float leash)
{
    Entity copy;

    if (!scene || !e) {
        return 0;
    }
    copy = *e;
    copy.leash = leash > 0.0f ? leash : 0.0f;
    return ent_add(scene, &copy);
}

Entity *spawn_get(SpawnId id)
{
    return pooled(id) ? ent_get(id) : 0;
}

void spawn_remove(SpawnId id)
{
    if (pooled(id)) {
        ent_remove(id);
    }
}

void spawn_clear(void)
{
    ent_clear();
}

int spawn_next(int i, SpawnId *id)
{
    /* The pool's indices come first in the registry's walk: stop where they end. */
    i = ent_next(i, id);
    return i > 0 && i <= ENT_POOL ? i : 0;
}

int spawn_count(void)
{
    return ent_count();
}

const Entity *spawn_entities(const Scene *scene, int *n)
{
    return ent_pool(scene, n);
}

void spawn_frame(const Scene *scene, const Camera *cam)
{
    ent_frame(scene, cam);
}

void spawn_shift(const Scene *scene, float dx, float dz)
{
    ent_shift(scene, dx, dz);
}

/* 1 when a circle of radius at x, z, from feet_y up height, meets a solid entity or the player's body. */
static int meets(float ax, float ay, float az, float ar, float ah,
                 float bx, float by, float bz, float br, float bh)
{
    float dx, dz;

    if (ay + ah <= by || ay >= by + bh) {
        return 0;
    }
    dx = ax - bx;
    dz = az - bz;
    return dx * dx + dz * dz < (ar + br) * (ar + br);
}

static int crowded_by(const Entity *list, int n, float x, float y, float z, float r, float h)
{
    int i;

    for (i = 0; list && i < n; i++) {
        const Entity *o;

        o = &list[i];
        if (entity_solid(o) && meets(x, y, z, r, h, o->x, o->y, o->z, o->radius, entity_height(o))) {
            return 1;
        }
    }
    return 0;
}

static int crowded(const Scene *scene, float x, float y, float z, float r, float h)
{
    EntList lists[ENT_LISTS];
    float bx, by, bz;
    int n;
    int i;

    /* The place's own, the pool and the game's ranges. */
    n = ent_lists(scene, lists, ENT_LISTS);
    for (i = 0; i < n; i++) {
        if (crowded_by(lists[i].first, lists[i].n, x, y, z, r, h)) {
            return 1;
        }
    }
    return walker_body(&bx, &by, &bz)
        && meets(x, y, z, r, h, bx, by, bz, walker_own_body()->radius, walker_own_body()->height);
}

/*
 * 1 and the floor in *fy when a body of want's size stands at x, z: a
 * floor under its middle within climb of want->y, a floor within a third
 * of its height under four points round it (not hung over an edge), and
 * its body from that third up caught in nothing. A third is a person's
 * step (24 of 72): what is lower a body walks onto, so a slope or a
 * kerb under it does not count as a wall.
 */
static int fits(const SpawnSpot *want, float x, float z, float *fy)
{
    static const float round_x[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
    static const float round_z[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
    float lift;
    float half;
    float r;
    float c[3];
    float h;
    int k;

    lift = want->height / 3.0f;
    half = (want->height - lift) * 0.5f;
    collide_set_shape(want->radius, half);
    collide_gather(x - want->radius - 4.0f, want->y - want->climb - lift - 4.0f, z - want->radius - 4.0f,
                   x + want->radius + 4.0f, want->y + want->climb + want->height + 4.0f, z + want->radius + 4.0f);
    if (!collide_floor(x, z, want->y + want->climb, want->y - want->climb, fy)) {
        return 0;
    }
    r = want->radius * 0.7f;
    for (k = 0; k < 4; k++) {
        if (!collide_floor(x + r * round_x[k], z + r * round_z[k], *fy + lift, *fy - lift, &h)) {
            return 0;
        }
    }
    c[0] = x;
    c[1] = *fy + lift + half;
    c[2] = z;
    return !collide_overlap(c);
}

/* 1 when something of the place stands on the segment from a to b: cast a piece at a time. */
static int blocked(const float *a, const float *b)
{
    float d[3];
    float len;
    float at;
    int k;

    for (k = 0; k < 3; k++) {
        d[k] = b[k] - a[k];
    }
    len = df_sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1.0f) {
        return 0;
    }
    for (k = 0; k < 3; k++) {
        d[k] /= len;
    }
    /* The gather's scale is the shape's: a unit one, so the ray reads the world as it is. */
    collide_set_shape(1.0f, 1.0f);
    for (at = 0.0f; at < len; at += RAY_PIECE) {
        float o[3];
        float piece;
        float lo[3];
        float hi[3];
        float t;

        piece = len - at < RAY_PIECE ? len - at : RAY_PIECE;
        for (k = 0; k < 3; k++) {
            float end;

            o[k] = a[k] + d[k] * at;
            end = o[k] + d[k] * piece;
            lo[k] = (end < o[k] ? end : o[k]) - 1.0f;
            hi[k] = (end > o[k] ? end : o[k]) + 1.0f;
        }
        collide_gather(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
        if (collide_ray(o, d, piece, &t)) {
            return 1;
        }
    }
    return 0;
}

/*
 * 1 when the camera cannot see a body standing at feet x, fy, z: outside
 * its view across the ground, or a wall between the eye and both its
 * head and its middle. The view's height is not tested: a spot is near
 * the ground, and a camera looking at the sky still sees what is ahead.
 */
static int out_of_sight(const Camera *cam, const SpawnSpot *want, float x, float fy, float z)
{
    float dx, dz;
    float d;
    float along;
    float limit;
    float eye[3];
    float p[3];

    dx = x - cam->x;
    dz = z - cam->z;
    d = df_sqrt(dx * dx + dz * dz);
    if (d > want->radius) {
        /* yaw 0 looks along +z, turning toward +x (camera.h): the cosine of the angle off the view. */
        along = (dx * df_sin(cam->yaw) + dz * df_cos(cam->yaw)) / d;
        limit = df_cos((camera_fov() * 0.5f + VIEW_MARGIN) * (DF_TAU / 360.0f));
        if (along < limit) {
            return 1;
        }
    }
    eye[0] = cam->x;
    eye[1] = cam->y;
    eye[2] = cam->z;
    p[0] = x;
    p[1] = fy + want->height;
    p[2] = z;
    if (!blocked(eye, p)) {
        return 0;
    }
    p[1] = fy + want->height * 0.5f;
    return blocked(eye, p);
}

int spawn_find(const Scene *scene, const Camera *cam, const SpawnSpot *want, float *x, float *y, float *z)
{
    float lo;
    float hi;
    int ring;
    int start;

    if (!scene || !want || !collide_use(scene) || ((want->flags & SPAWN_UNSEEN) && !cam)) {
        return 0;
    }
    lo = want->inner < want->outer ? want->inner : want->outer;
    hi = want->inner < want->outer ? want->outer : want->inner;
    start = (int)(want->seed % FIND_DIRS);
    for (ring = 0; ring < FIND_RINGS; ring++) {
        float r;
        int k;

        r = lo + (hi - lo) * (float)ring / (float)(FIND_RINGS - 1);
        for (k = 0; k < FIND_DIRS; k++) {
            float a;
            float px, pz;
            float fy;

            /* Seven at a time round the ring (7 and 16 share nothing): the tries spread, not bunch. */
            a = (float)((start + k * 7) % FIND_DIRS) * (DF_TAU / (float)FIND_DIRS);
            px = want->x + r * df_sin(a);
            pz = want->z + r * df_cos(a);
            if (!fits(want, px, pz, &fy)
                || crowded(scene, px, fy, pz, want->radius, want->height)
                || ((want->flags & SPAWN_DRY) && scene_water_body(scene, px, fy + WALK_EYE, pz) != WATER_DRY)
                || ((want->flags & SPAWN_UNSEEN) && !out_of_sight(cam, want, px, fy, pz))) {
                continue;
            }
            *x = px;
            *y = fy;
            *z = pz;
            return 1;
        }
    }
    return 0;
}
