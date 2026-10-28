#include "engine/scene/walker.h"
#include "engine/scene/body.h"
#include "engine/gfx/rig.h"
#include "engine/core/mem.h"

/*
 * See walker.h and docs/collision.md. The viewers' walking: one Body of
 * a person's size (body.c has the rules) that follows the console's
 * camera. Each step takes how far the camera moved across the ground as
 * the body's wanted motion, steps the body, and puts the camera at its
 * eye (rig_first).
 */

/* A camera that moved further than this in a frame was set, not walked. */
#define TELEPORT 768.0f

static int g_on;
static int g_ready;
static Body g_body;
static int g_body_made;
/*
 * The place the body is in, and its heap. A door to another place moves
 * the camera less than TELEPORT (a house's inside is where the house
 * stands), so a new place is told by these, not by the distance.
 */
static const Scene *g_scene;
static df_u32 g_epoch;
static float g_last[3];

static Body *body(void)
{
    if (!g_body_made) {
        body_init(&g_body, WALK_RADIUS, WALK_HEIGHT, WALK_STEP, WALK_EYE);
        g_body_made = 1;
    }
    return &g_body;
}

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
    if (!g_on || !g_ready) {
        return 0;
    }
    return body_jump(body(), up_speed);
}

int walker_landed(float *speed, float *height)
{
    return body_landed(body(), speed, height);
}

int walker_body(float *x, float *feet_y, float *z)
{
    if (!g_on || !g_ready) {
        return 0;
    }
    *x = g_body.feet[0];
    *feet_y = g_body.feet[1];
    *z = g_body.feet[2];
    return 1;
}

const Body *walker_own_body(void)
{
    return &g_body;
}

int walker_grounded(void)
{
    return g_on && g_body.grounded;
}

void walker_place(void)
{
    g_ready = 0;
}

void walker_unstick(void)
{
    body_unstick(body());
}

void walker_step(Camera *cam, const Scene *scene, float dt)
{
    float dx;
    float dz;

    if (!g_on || !cam || !scene) {
        g_ready = 0;
        return;
    }
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
        body_place(body(), cam->x, cam->y - WALK_EYE, cam->z);
        g_ready = 1;
        dx = 0.0f;
        dz = 0.0f;
    }
    body_step(body(), scene, dx, dz, dt);
    rig_first(cam, body());
    g_last[0] = cam->x;
    g_last[1] = cam->y;
    g_last[2] = cam->z;
}
