#include "engine/scene/spawn.h"
#include "engine/scene/body.h"
#include "engine/scene/collide.h"
#include "engine/scene/walker.h"
#include "engine/core/mem.h"

/*
 * See spawn.h and docs/spawn.md. The pool is static: a spawn's address
 * never moves, and nothing is allocated. Which scene it belongs to and
 * the heap epoch it was made under are noted with it; any call that
 * finds either changed empties it first, so a spawn never outlives its
 * place even when the game forgets to clear it.
 */

static Entity g_pool[SPAWN_MAX];
static float g_leash[SPAWN_MAX];
static df_u8 g_used[SPAWN_MAX];
/* Bumped at each add, never 0: an old id for a reused entry reads as gone. */
static df_u8 g_gen[SPAWN_MAX];
static int g_count;
/* The place the pool is in: its scene and its heap's epoch. 0 with none. */
static const Scene *g_scene;
static df_u32 g_epoch;

/* Directions round the ring, and radii across it, that spawn_find tries. */
#define FIND_DIRS 16
#define FIND_RINGS 3
/* A ray to the spot is gathered and cast in pieces this long, so no gather holds a whole road. */
#define RAY_PIECE 256.0f
/* Outside the view by this much more than half its width, a spot is out of sight. */
#define VIEW_MARGIN 10.0f

static void free_entry(int i)
{
    if (g_used[i]) {
        g_count--;
    }
    g_used[i] = 0;
    /* Zeroed: not live, so the walkers skip it like a free entry of Scene.entities. */
    mem_zero(&g_pool[i], sizeof(Entity));
}

void spawn_clear(void)
{
    int i;

    for (i = 0; i < SPAWN_MAX; i++) {
        free_entry(i);
    }
    g_count = 0;
    g_scene = 0;
}

/* Empty the pool when its place is gone: another heap. */
static void check_epoch(void)
{
    if (g_scene && g_epoch != mem_epoch()) {
        spawn_clear();
    }
}

/* 1 when scene is the pool's place, still standing. */
static int in_place(const Scene *scene)
{
    check_epoch();
    return g_scene && scene == g_scene;
}

SpawnId spawn_add(const Scene *scene, const Entity *e, float leash)
{
    int i;

    if (!scene || !e) {
        return 0;
    }
    /* Another place: what stood in the old one is gone. */
    if (!in_place(scene)) {
        spawn_clear();
        g_scene = scene;
        g_epoch = mem_epoch();
    }
    for (i = 0; i < SPAWN_MAX; i++) {
        if (!g_used[i]) {
            break;
        }
    }
    if (i == SPAWN_MAX) {
        return 0;
    }
    g_pool[i] = *e;
    g_leash[i] = leash > 0.0f ? leash : 0.0f;
    g_used[i] = 1;
    g_gen[i]++;
    if (g_gen[i] == 0) {
        g_gen[i] = 1;
    }
    g_count++;
    return (SpawnId)(i + 1) | ((SpawnId)g_gen[i] << 8);
}

/* The entry id names while it is still that spawn; -1 when gone. */
static int entry_of(SpawnId id)
{
    int i;

    check_epoch();
    i = (int)(id & 0xff) - 1;
    if (i < 0 || i >= SPAWN_MAX || !g_used[i] || g_gen[i] != (df_u8)(id >> 8)) {
        return -1;
    }
    return i;
}

Entity *spawn_get(SpawnId id)
{
    int i;

    i = entry_of(id);
    return i < 0 ? 0 : &g_pool[i];
}

void spawn_remove(SpawnId id)
{
    int i;

    i = entry_of(id);
    if (i >= 0) {
        free_entry(i);
    }
}

int spawn_next(int i, SpawnId *id)
{
    check_epoch();
    for (; i < SPAWN_MAX; i++) {
        if (g_used[i]) {
            *id = (SpawnId)(i + 1) | ((SpawnId)g_gen[i] << 8);
            return i + 1;
        }
    }
    return 0;
}

int spawn_count(void)
{
    check_epoch();
    return g_count;
}

const Entity *spawn_entities(const Scene *scene, int *n)
{
    if (!in_place(scene) || g_count == 0) {
        *n = 0;
        return 0;
    }
    *n = SPAWN_MAX;
    return g_pool;
}

void spawn_frame(const Scene *scene, const Camera *cam)
{
    int i;

    if (!in_place(scene) || g_count == 0) {
        return;
    }
    for (i = 0; i < SPAWN_MAX; i++) {
        float dx, dz;

        if (!g_used[i] || g_leash[i] <= 0.0f) {
            continue;
        }
        dx = g_pool[i].x - cam->x;
        dz = g_pool[i].z - cam->z;
        if (dx * dx + dz * dz > g_leash[i] * g_leash[i]) {
            free_entry(i);
        }
    }
}

void spawn_shift(const Scene *scene, float dx, float dz)
{
    int i;

    if (!in_place(scene)) {
        return;
    }
    for (i = 0; i < SPAWN_MAX; i++) {
        if (g_used[i]) {
            g_pool[i].x -= dx;
            g_pool[i].z -= dz;
        }
    }
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
        if (o->live && o->radius > 0.0f && meets(x, y, z, r, h, o->x, o->y, o->z, o->radius, entity_height(o))) {
            return 1;
        }
    }
    return 0;
}

static int crowded(const Scene *scene, float x, float y, float z, float r, float h)
{
    const Entity *pool;
    float bx, by, bz;
    int n;

    if (crowded_by(scene->entities, scene->nentities, x, y, z, r, h)) {
        return 1;
    }
    pool = spawn_entities(scene, &n);
    if (crowded_by(pool, n, x, y, z, r, h)) {
        return 1;
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
