#include "engine/scene/walker.h"
#include "engine/scene/entity.h"
#include "engine/scene/collide.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/*
 * See walker.h and docs/collision.md. A frame is one gather, one sweep
 * of the upper body across the ground, a look for the floor under the
 * footprint, and one sweep up or down to it. Gravity is semi-implicit
 * Euler, v += g dt then y += v dt: two multiply-adds, nothing next to
 * the sweep.
 *
 * g_feet is the point between the feet; the upper body's centre is
 * WALK_STEP + WALK_UPPER above it.
 */

/* A camera that moved further than this in a frame was set, not walked. */
#define TELEPORT 768.0f
/* Below the place's lowest point by this much, the body has fallen out. */
#define FALL_OUT 1024.0f
/* Rings the unstick search tries, 8 to 256 units out, at four heights. */
#define RINGS 6
#define DIRS 16
/* The footprint: the middle and eight points this far out, as a fraction of the radius. */
#define FOOT 0.7f

static int g_on;
static int g_ready;
/*
 * The place the body is in, and its heap. A door to another place moves
 * the camera less than TELEPORT (a house's inside is where the house
 * stands), so a new place is told by these, not by the distance.
 */
static const Scene *g_scene;
static df_u32 g_epoch;
static int g_grounded;
static int g_want_unstick;
static float g_feet[3];
static float g_vy;
/*
 * The top of the body's arc while it is off the ground (its feet), and
 * a landing not yet taken (walker_landed): the speed down at the floor,
 * and the height fallen from the top.
 */
static float g_top;
static int g_landed;
static float g_land_speed;
static float g_land_height;
static float g_last[3];
static float g_safe[3];
static int g_have_safe;
static float g_foot_x[9];
static float g_foot_z[9];
static int g_foot_ready;

void walker_set_on(int on)
{
    on = on ? 1 : 0;
    if (on != g_on) {
        /* Take the camera where it is on the next step. */
        g_ready = 0;
    }
    g_on = on;
}

int walker_on(void)
{
    return g_on;
}

int walker_jump(float up_speed)
{
    if (!g_on || !g_ready || !g_grounded || up_speed <= 0.0f) {
        return 0;
    }
    g_vy = up_speed;
    g_grounded = 0;
    g_top = g_feet[1];
    return 1;
}

int walker_landed(float *speed, float *height)
{
    if (!g_landed) {
        return 0;
    }
    g_landed = 0;
    if (speed) {
        *speed = g_land_speed;
    }
    if (height) {
        *height = g_land_height;
    }
    return 1;
}

int walker_body(float *x, float *feet_y, float *z)
{
    if (!g_on || !g_ready) {
        return 0;
    }
    *x = g_feet[0];
    *feet_y = g_feet[1];
    *z = g_feet[2];
    return 1;
}

int walker_grounded(void)
{
    return g_on && g_grounded;
}

void walker_place(void)
{
    g_ready = 0;
}

void walker_unstick(void)
{
    g_want_unstick = 1;
}

static void put(float *dst, const float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* The upper body's centre for feet at f. */
static void upper(const float *f, float *c)
{
    c[0] = f[0];
    c[1] = f[1] + WALK_STEP + WALK_UPPER;
    c[2] = f[2];
}

/* Gather round the whole body at feet f, reaching r across, down below and up above. */
static void gather_round(const float *f, float r, float below, float above)
{
    collide_gather(f[0] - WALK_RADIUS - r, f[1] - below, f[2] - WALK_RADIUS - r,
                   f[0] + WALK_RADIUS + r, f[1] + WALK_HEIGHT + above, f[2] + WALK_RADIUS + r);
}

/*
 * The highest floor under the footprint at x, z, from top down to
 * bottom: the middle and eight points round it. 1 and its height in *y.
 */
static int floor_under(float x, float z, float top, float bottom, float *y)
{
    float best;
    int found;
    int k;

    if (!g_foot_ready) {
        g_foot_x[0] = 0.0f;
        g_foot_z[0] = 0.0f;
        for (k = 1; k < 9; k++) {
            float a;

            a = (float)(k - 1) * (DF_TAU / 8.0f);
            g_foot_x[k] = WALK_RADIUS * FOOT * df_cos(a);
            g_foot_z[k] = WALK_RADIUS * FOOT * df_sin(a);
        }
        g_foot_ready = 1;
    }
    found = 0;
    best = bottom;
    for (k = 0; k < 9; k++) {
        float h;

        if (collide_floor(x + g_foot_x[k], z + g_foot_z[k], top, best, &h)) {
            best = h;
            found = 1;
        }
    }
    if (found) {
        *y = best;
    }
    return found;
}

/* 1 when the upper body at feet f is clear and a floor holds it within a step. */
static int free_at(const float *f, float *floor_y)
{
    float c[3];

    upper(f, c);
    return !collide_overlap(c)
        && floor_under(f[0], f[2], f[1] + WALK_STEP, f[1] - WALK_STEP, floor_y);
}

/*
 * The nearest free place: rings of DIRS points 8, 16 ... 256 units out,
 * each at the feet's height and 24, 48 and 96 above, nearest first; the
 * feet then go onto the floor found there. Then the last place it stood.
 */
static int unstick(void)
{
    static const float lift[4] = { 0.0f, 24.0f, 48.0f, 96.0f };
    float r;
    float floor_y;
    int ring;

    gather_round(g_feet, 4.0f, WALK_STEP + 4.0f, 4.0f);
    if (free_at(g_feet, &floor_y)) {
        return 1;
    }
    r = 8.0f;
    for (ring = 0; ring < RINGS; ring++, r *= 2.0f) {
        int h;

        gather_round(g_feet, r + 4.0f, WALK_STEP + 8.0f, 100.0f);
        for (h = 0; h < 4; h++) {
            int k;

            for (k = 0; k < DIRS; k++) {
                float p[3];
                float a;

                a = (float)k * (DF_TAU / (float)DIRS);
                p[0] = g_feet[0] + r * df_cos(a);
                p[1] = g_feet[1] + lift[h];
                p[2] = g_feet[2] + r * df_sin(a);
                if (free_at(p, &floor_y)) {
                    p[1] = floor_y;
                    if (free_at(p, &floor_y)) {
                        put(g_feet, p);
                        g_vy = 0.0f;
                        return 1;
                    }
                }
            }
        }
    }
    if (g_have_safe) {
        put(g_feet, g_safe);
        g_vy = 0.0f;
        return 1;
    }
    return 0;
}

/*
 * Fallen out of the place: back on the highest floor under where it
 * fell, or, with none, the last place it stood, or on top of it all.
 */
static void recover(void)
{
    float miny;
    float maxy;
    float floor_y;

    collide_extent(&miny, &maxy);
    collide_gather(g_feet[0] - WALK_RADIUS, miny - 64.0f, g_feet[2] - WALK_RADIUS,
                   g_feet[0] + WALK_RADIUS, maxy + 64.0f, g_feet[2] + WALK_RADIUS);
    if (floor_under(g_feet[0], g_feet[2], maxy + 64.0f, miny - 64.0f, &floor_y)) {
        g_feet[1] = floor_y;
    } else if (g_have_safe) {
        put(g_feet, g_safe);
    } else {
        g_feet[1] = maxy;
    }
    g_vy = 0.0f;
    log_text(LOG_INFO, "walker: fell out of the place, put back on the ground");
}

/*
 * Up or down to feet height y, the upper body swept so nothing above is
 * passed through. Returns 1 when it got all the way.
 */
static int rise_to(float y)
{
    float c[3];
    float want;

    want = y - g_feet[1];
    if (df_absf(want) < 1.0e-4f) {
        return 1;
    }
    upper(g_feet, c);
    collide_slide(c, 0.0f, want, 0.0f, 0);
    /* Sliding off a slope may move the body across: keep where it went. */
    g_feet[0] = c[0];
    g_feet[2] = c[2];
    g_feet[1] = c[1] - WALK_STEP - WALK_UPPER;
    return df_absf(g_feet[1] - y) < 0.05f;
}

/*
 * The scene's solid entities (Entity.radius) the body meets, pushed out
 * of round them across the floor, the push swept through collide like
 * any move so it never goes into a wall. Townspeople and creatures stand
 * in the way; they are not models, so collide does not see them.
 */
static void push_off_entities(const Scene *scene)
{
    float px;
    float pz;
    int i;

    if (!scene->entities) {
        return;
    }
    px = 0.0f;
    pz = 0.0f;
    for (i = 0; i < scene->nentities; i++) {
        const Entity *e;
        float dx;
        float dz;
        float d2;
        float reach;
        float d;

        e = &scene->entities[i];
        if (!e->live || e->radius <= 0.0f || e->y + entity_height(e) <= g_feet[1]
            || e->y >= g_feet[1] + WALK_HEIGHT) {
            continue;
        }
        reach = e->radius + WALK_RADIUS;
        dx = g_feet[0] - e->x;
        dz = g_feet[2] - e->z;
        d2 = dx * dx + dz * dz;
        if (d2 >= reach * reach) {
            continue;
        }
        d = df_sqrt(d2);
        if (d < 0.01f) {
            /* Right on it: out along +x, any way will do. */
            px += reach;
            continue;
        }
        px += dx / d * (reach - d);
        pz += dz / d * (reach - d);
    }
    if (px != 0.0f || pz != 0.0f) {
        float c[3];

        upper(g_feet, c);
        collide_slide(c, px, 0.0f, pz, 0);
        g_feet[0] = c[0];
        g_feet[2] = c[2];
    }
}

void walker_step(Camera *cam, const Scene *scene, float dt)
{
    float dx;
    float dz;
    float dy;
    float travel;
    float floor_y;
    float miny;
    float maxy;
    float before[3];
    float c[3];

    if (!g_on || !cam || !scene || !collide_use(scene)) {
        g_ready = 0;
        return;
    }
    collide_set_shape(WALK_RADIUS, WALK_UPPER);
    dx = cam->x - g_last[0];
    dz = cam->z - g_last[2];
    if (scene != g_scene || mem_epoch() != g_epoch) {
        g_scene = scene;
        g_epoch = mem_epoch();
        g_ready = 0;
    }
    /*
     * First step, a new place, or the camera was set (setpos): take it
     * where it is. Walked there, the body would be swept from where it
     * stood, through the wall a door is in.
     */
    if (!g_ready || dx * dx + dz * dz > TELEPORT * TELEPORT
        || df_absf(cam->y - g_last[1]) > TELEPORT) {
        g_feet[0] = cam->x;
        g_feet[1] = cam->y - WALK_EYE;
        g_feet[2] = cam->z;
        g_vy = 0.0f;
        g_grounded = 0;
        /* Set, not walked: a fall from here counts from here, and no landing is owed. */
        g_top = g_feet[1];
        g_landed = 0;
        g_have_safe = 0;
        g_ready = 1;
        g_want_unstick = 1;
        dx = 0.0f;
        dz = 0.0f;
    }
    if (g_want_unstick) {
        g_want_unstick = 0;
        if (!unstick()) {
            log_text(LOG_WARN, "walker: no free place near; the body stays where it is");
        }
    }
    if (dt > 0.0f) {
        g_vy -= WALK_GRAVITY * dt;
        if (g_vy < -WALK_TERMINAL) {
            g_vy = -WALK_TERMINAL;
        }
    }
    dy = g_vy * dt;
    /* One gather round everything this frame's moves can reach. */
    travel = df_absf(dx) > df_absf(dz) ? df_absf(dx) : df_absf(dz);
    gather_round(g_feet, travel + 2.0f, WALK_STEP - dy + 2.0f, 2.0f);

    /* Across: the upper body meets walls; anything under a step it walks over. */
    put(before, g_feet);
    if (dx != 0.0f || dz != 0.0f) {
        upper(g_feet, c);
        collide_slide(c, dx, 0.0f, dz, 0);
        g_feet[0] = c[0];
        g_feet[2] = c[2];
    }

    push_off_entities(scene);

    /*
     * Down: standing, the floor may be a step up or a step down; falling,
     * anywhere in this frame's fall. The highest under the footprint wins.
     */
    if (g_grounded) {
        if (floor_under(g_feet[0], g_feet[2], g_feet[1] + WALK_STEP, g_feet[1] - WALK_STEP, &floor_y)) {
            if (rise_to(floor_y)) {
                g_vy = 0.0f;
            } else if (floor_y > before[1]) {
                /* No room over the step for the head: stay off it. */
                g_feet[0] = before[0];
                g_feet[2] = before[2];
            }
        } else {
            /* Off an edge: the fall counts from here. */
            g_grounded = 0;
            g_top = g_feet[1];
        }
    }
    if (!g_grounded) {
        if (dy <= 0.0f && floor_under(g_feet[0], g_feet[2], g_feet[1] + 0.5f, g_feet[1] + dy - 0.5f, &floor_y)) {
            float speed;

            speed = -g_vy;
            rise_to(floor_y);
            g_grounded = df_absf(g_feet[1] - floor_y) < 0.05f;
            g_vy = 0.0f;
            if (g_grounded) {
                /* Landed: how fast, and from how high (walker_landed, once). */
                g_landed = 1;
                g_land_speed = speed > 0.0f ? speed : 0.0f;
                g_land_height = g_top > g_feet[1] ? g_top - g_feet[1] : 0.0f;
                g_top = g_feet[1];
            }
        } else if (!rise_to(g_feet[1] + dy)) {
            /* Struck something on the way: a ceiling, a slope. */
            g_vy = 0.0f;
        }
        if (!g_grounded && g_feet[1] > g_top) {
            g_top = g_feet[1];
        }
    }

    collide_extent(&miny, &maxy);
    if (g_feet[1] < miny - FALL_OUT) {
        recover();
    }
    /* A floor under the middle, not just the footprint's edge: safe to come back to. */
    if (g_grounded && collide_floor(g_feet[0], g_feet[2], g_feet[1] + 1.0f, g_feet[1] - 1.0f, &floor_y)) {
        put(g_safe, g_feet);
        g_have_safe = 1;
    }
    cam->x = g_feet[0];
    cam->y = g_feet[1] + WALK_EYE;
    cam->z = g_feet[2];
    g_last[0] = cam->x;
    g_last[1] = cam->y;
    g_last[2] = cam->z;
}
