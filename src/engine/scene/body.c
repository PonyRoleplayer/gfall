#include "engine/scene/body.h"
#include "engine/scene/collide.h"
#include "engine/scene/entity.h"
#include "engine/scene/walker.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/*
 * See body.h and docs/collision.md. A frame is one gather, one sweep
 * of the upper body across the ground, a look for the floor under the
 * footprint, and one sweep up or down to it. Gravity is semi-implicit
 * Euler, v += g dt then y += v dt: two multiply-adds, nothing next to
 * the sweep. The rules are the walker's, which is one body now.
 */

/* Below the place's lowest point by this much, the body has fallen out. */
#define FALL_OUT 1024.0f
/* Rings the unstick search tries, 8 to 256 units out, at four heights. */
#define RINGS 6
#define DIRS 16
/* The footprint: the middle and eight points this far out, as a fraction of the radius. */
#define FOOT 0.7f

/* The footprint's eight directions, a unit circle, made once. */
static float g_foot_x[9];
static float g_foot_z[9];
static int g_foot_ready;

/* The upper body's half height: from a step above the feet to the head. */
static float upper_half(const Body *b)
{
    return (b->height - b->step) * 0.5f;
}

void body_init(Body *b, float radius, float height, float step, float eye)
{
    mem_zero(b, sizeof(Body));
    b->radius = radius;
    b->height = height;
    b->step = step;
    b->eye = eye;
    b->gravity = WALK_GRAVITY;
}

void body_place(Body *b, float x, float feet_y, float z)
{
    b->feet[0] = x;
    b->feet[1] = feet_y;
    b->feet[2] = z;
    b->vy = 0.0f;
    b->grounded = 0;
    /* Set, not walked: a fall from here counts from here, and no landing is owed. */
    b->top = feet_y;
    b->landed = 0;
    b->have_safe = 0;
    b->want_unstick = 1;
}

int body_jump(Body *b, float up_speed)
{
    if (!b->grounded || up_speed <= 0.0f) {
        return 0;
    }
    b->vy = up_speed;
    b->grounded = 0;
    b->top = b->feet[1];
    return 1;
}

int body_landed(Body *b, float *speed, float *height)
{
    if (!b->landed) {
        return 0;
    }
    b->landed = 0;
    if (speed) {
        *speed = b->land_speed;
    }
    if (height) {
        *height = b->land_height;
    }
    return 1;
}

void body_eye(const Body *b, float *x, float *y, float *z)
{
    *x = b->feet[0];
    *y = b->feet[1] + b->eye;
    *z = b->feet[2];
}

void body_unstick(Body *b)
{
    b->want_unstick = 1;
}

static void put(float *dst, const float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* The upper body's centre for feet at f. */
static void upper(const Body *b, const float *f, float *c)
{
    c[0] = f[0];
    c[1] = f[1] + b->step + upper_half(b);
    c[2] = f[2];
}

/* Gather round the whole body at feet f, reaching r across, down below and up above. */
static void gather_round(const Body *b, const float *f, float r, float below, float above)
{
    collide_gather(f[0] - b->radius - r, f[1] - below, f[2] - b->radius - r,
                   f[0] + b->radius + r, f[1] + b->height + above, f[2] + b->radius + r);
}

/*
 * The highest floor under the footprint at x, z, from top down to
 * bottom: the middle and eight points round it. 1 and its height in *y.
 */
static int floor_under(const Body *b, float x, float z, float top, float bottom, float *y)
{
    float best;
    float r;
    int found;
    int k;

    if (!g_foot_ready) {
        g_foot_x[0] = 0.0f;
        g_foot_z[0] = 0.0f;
        for (k = 1; k < 9; k++) {
            float a;

            a = (float)(k - 1) * (DF_TAU / 8.0f);
            g_foot_x[k] = df_cos(a);
            g_foot_z[k] = df_sin(a);
        }
        g_foot_ready = 1;
    }
    r = b->radius * FOOT;
    found = 0;
    best = bottom;
    for (k = 0; k < 9; k++) {
        float h;

        if (collide_floor(x + r * g_foot_x[k], z + r * g_foot_z[k], top, best, &h)) {
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
static int free_at(const Body *b, const float *f, float *floor_y)
{
    float c[3];

    upper(b, f, c);
    return !collide_overlap(c)
        && floor_under(b, f[0], f[2], f[1] + b->step, f[1] - b->step, floor_y);
}

/*
 * The nearest free place: rings of DIRS points 8, 16 ... 256 units out,
 * each at the feet's height and 24, 48 and 96 above, nearest first; the
 * feet then go onto the floor found there. Then the last place it stood.
 */
static int unstick(Body *b)
{
    static const float lift[4] = { 0.0f, 24.0f, 48.0f, 96.0f };
    float r;
    float floor_y;
    int ring;

    gather_round(b, b->feet, 4.0f, b->step + 4.0f, 4.0f);
    if (free_at(b, b->feet, &floor_y)) {
        return 1;
    }
    r = 8.0f;
    for (ring = 0; ring < RINGS; ring++, r *= 2.0f) {
        int h;

        gather_round(b, b->feet, r + 4.0f, b->step + 8.0f, 100.0f);
        for (h = 0; h < 4; h++) {
            int k;

            for (k = 0; k < DIRS; k++) {
                float p[3];
                float a;

                a = (float)k * (DF_TAU / (float)DIRS);
                p[0] = b->feet[0] + r * df_cos(a);
                p[1] = b->feet[1] + lift[h];
                p[2] = b->feet[2] + r * df_sin(a);
                if (free_at(b, p, &floor_y)) {
                    p[1] = floor_y;
                    if (free_at(b, p, &floor_y)) {
                        put(b->feet, p);
                        b->vy = 0.0f;
                        return 1;
                    }
                }
            }
        }
    }
    if (b->have_safe) {
        put(b->feet, b->safe);
        b->vy = 0.0f;
        return 1;
    }
    return 0;
}

/*
 * Fallen out of the place: back on the highest floor under where it
 * fell, or, with none, the last place it stood, or on top of it all.
 */
static void recover(Body *b)
{
    float miny;
    float maxy;
    float floor_y;

    collide_extent(&miny, &maxy);
    collide_gather(b->feet[0] - b->radius, miny - 64.0f, b->feet[2] - b->radius,
                   b->feet[0] + b->radius, maxy + 64.0f, b->feet[2] + b->radius);
    if (floor_under(b, b->feet[0], b->feet[2], maxy + 64.0f, miny - 64.0f, &floor_y)) {
        b->feet[1] = floor_y;
    } else if (b->have_safe) {
        put(b->feet, b->safe);
    } else {
        b->feet[1] = maxy;
    }
    b->vy = 0.0f;
    log_text(LOG_INFO, "body: fell out of the place, put back on the ground");
}

/*
 * Up or down to feet height y, the upper body swept so nothing above is
 * passed through. Returns 1 when it got all the way.
 */
static int rise_to(Body *b, float y)
{
    float c[3];
    float want;

    want = y - b->feet[1];
    if (df_absf(want) < 1.0e-4f) {
        return 1;
    }
    upper(b, b->feet, c);
    collide_slide(c, 0.0f, want, 0.0f, 0);
    /* Sliding off a slope may move the body across: keep where it went. */
    b->feet[0] = c[0];
    b->feet[2] = c[2];
    b->feet[1] = c[1] - b->step - upper_half(b);
    return df_absf(b->feet[1] - y) < 0.05f;
}

/* The push out of one circle (another's feet at ox, oz, radius orad, from oy up oh) the body meets. */
static void push_from(const Body *b, float ox, float oy, float oz, float orad, float oh, float *px, float *pz)
{
    float dx;
    float dz;
    float d2;
    float reach;
    float d;

    if (oy + oh <= b->feet[1] || oy >= b->feet[1] + b->height) {
        return;
    }
    reach = orad + b->radius;
    dx = b->feet[0] - ox;
    dz = b->feet[2] - oz;
    d2 = dx * dx + dz * dz;
    if (d2 >= reach * reach) {
        return;
    }
    d = df_sqrt(d2);
    if (d < 0.01f) {
        /* Right on it: out along +x, any way will do. */
        *px += reach;
        return;
    }
    *px += dx / d * (reach - d);
    *pz += dz / d * (reach - d);
}

/*
 * The scene's solid entities (Entity.radius) the body meets, all but its
 * own, and the walker's body when this is not it: pushed out of round
 * them across the floor, the push swept through collide like any move so
 * it never goes into a wall. They are not models: collide does not see them.
 */
static void push_off(Body *b, const Scene *scene)
{
    float px;
    float pz;
    float wx, wy, wz;
    int i;

    px = 0.0f;
    pz = 0.0f;
    for (i = 0; scene->entities && i < scene->nentities; i++) {
        const Entity *e;

        e = &scene->entities[i];
        if (e == b->self || !e->live || e->radius <= 0.0f) {
            continue;
        }
        push_from(b, e->x, e->y, e->z, e->radius, entity_height(e), &px, &pz);
    }
    if (b != walker_own_body() && walker_body(&wx, &wy, &wz)) {
        push_from(b, wx, wy, wz, WALK_RADIUS, WALK_HEIGHT, &px, &pz);
    }
    if (px != 0.0f || pz != 0.0f) {
        float c[3];

        upper(b, b->feet, c);
        collide_slide(c, px, 0.0f, pz, 0);
        b->feet[0] = c[0];
        b->feet[2] = c[2];
    }
}

void body_step(Body *b, const Scene *scene, float want_dx, float want_dz, float dt)
{
    float dy;
    float travel;
    float floor_y;
    float miny;
    float maxy;
    float before[3];
    float c[3];

    if (!b || !scene || !collide_use(scene)) {
        return;
    }
    collide_set_shape(b->radius, upper_half(b));
    /* A new place, or another heap: out of whatever it is caught in where it stands. */
    if (scene != b->scene || mem_epoch() != b->epoch) {
        b->scene = scene;
        b->epoch = mem_epoch();
        b->want_unstick = 1;
        b->have_safe = 0;
    }
    if (b->want_unstick) {
        b->want_unstick = 0;
        if (!unstick(b)) {
            log_text(LOG_WARN, "body: no free place near; it stays where it is");
        }
    }
    if (dt > 0.0f) {
        b->vy -= b->gravity * dt;
        if (b->vy < -WALK_TERMINAL) {
            b->vy = -WALK_TERMINAL;
        }
    }
    dy = b->vy * dt;
    /* One gather round everything this frame's moves can reach. */
    travel = df_absf(want_dx) > df_absf(want_dz) ? df_absf(want_dx) : df_absf(want_dz);
    gather_round(b, b->feet, travel + 2.0f, b->step - dy + 2.0f, 2.0f);

    /* Across: the upper body meets walls; anything under a step it walks over. */
    put(before, b->feet);
    if (want_dx != 0.0f || want_dz != 0.0f) {
        upper(b, b->feet, c);
        collide_slide(c, want_dx, 0.0f, want_dz, 0);
        b->feet[0] = c[0];
        b->feet[2] = c[2];
    }

    push_off(b, scene);

    /*
     * Down: standing, the floor may be a step up or a step down; falling,
     * anywhere in this frame's fall. The highest under the footprint wins.
     */
    if (b->grounded) {
        if (floor_under(b, b->feet[0], b->feet[2], b->feet[1] + b->step, b->feet[1] - b->step, &floor_y)) {
            if (rise_to(b, floor_y)) {
                b->vy = 0.0f;
            } else if (floor_y > before[1]) {
                /* No room over the step for the head: stay off it. */
                b->feet[0] = before[0];
                b->feet[2] = before[2];
            }
        } else {
            /* Off an edge: the fall counts from here. */
            b->grounded = 0;
            b->top = b->feet[1];
        }
    }
    if (!b->grounded) {
        if (dy <= 0.0f
            && floor_under(b, b->feet[0], b->feet[2], b->feet[1] + 0.5f, b->feet[1] + dy - 0.5f, &floor_y)) {
            float speed;

            speed = -b->vy;
            rise_to(b, floor_y);
            b->grounded = df_absf(b->feet[1] - floor_y) < 0.05f;
            b->vy = 0.0f;
            if (b->grounded) {
                /* Landed: how fast, and from how high (body_landed, once). */
                b->landed = 1;
                b->land_speed = speed > 0.0f ? speed : 0.0f;
                b->land_height = b->top > b->feet[1] ? b->top - b->feet[1] : 0.0f;
                b->top = b->feet[1];
            }
        } else if (!rise_to(b, b->feet[1] + dy)) {
            /* Struck something on the way: a ceiling, a slope. */
            b->vy = 0.0f;
        }
        if (!b->grounded && b->feet[1] > b->top) {
            b->top = b->feet[1];
        }
    }

    collide_extent(&miny, &maxy);
    if (b->feet[1] < miny - FALL_OUT) {
        recover(b);
    }
    /* A floor under the middle, not just the footprint's edge: safe to come back to. */
    if (b->grounded && collide_floor(b->feet[0], b->feet[2], b->feet[1] + 1.0f, b->feet[1] - 1.0f, &floor_y)) {
        put(b->safe, b->feet);
        b->have_safe = 1;
    }
}
