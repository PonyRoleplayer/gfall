#include "engine/scene/interact.h"
#include "engine/scene/walker.h"
#include "engine/scene/body.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/sound/sound.h"

/*
 * See interact.h and docs/interact.md. Distances are world units, 40 a
 * metre (Daggerfall Unity's scale).
 */

/* A moving model is heard full within 2 m and not past 30: a door across a hall. */
#define MOVER_INNER 80.0f
#define MOVER_REACH 1200.0f
/* A chain longer than this is taken to loop back on itself. */
#define CHAIN_MAX 64
/* A usable this much behind the wall the ray met still counts: its face is the wall. */
#define WALL_SLACK 0.5f
#define SAY_MAX 64

static UseHandler g_handler;
static int g_locks = 1;
static int g_view;
static int g_aimed = -1;
static char g_say[SAY_MAX];
static float g_say_left;
/* The last frame's scene and the heap it was on: a place's heap dies with it. */
static const Scene *g_scene;
static df_u32 g_scene_epoch;

void interact_set_handler(UseHandler fn)
{
    g_handler = fn;
}

void interact_set_locks(int on)
{
    g_locks = on ? 1 : 0;
}

int interact_locks(void)
{
    return g_locks;
}

void interact_set_view(int units)
{
    /* A range too small to hold anything is "on": `useview 1`. */
    if (units > 0 && units < INTERACT_VIEW_SMALL) {
        units = INTERACT_VIEW_ON;
    }
    g_view = units < 0 ? 0 : (units > INTERACT_VIEW_MAX ? INTERACT_VIEW_MAX : units);
    if (!g_view) {
        g_aimed = -1;
    }
}

int interact_view(void)
{
    return g_view;
}

/* trigview: its range, 0 off. */
static int g_trig_view;

void interact_set_trigger_view(int units)
{
    if (units > 0 && units < INTERACT_VIEW_SMALL) {
        units = INTERACT_VIEW_ON;
    }
    g_trig_view = units < 0 ? 0 : (units > INTERACT_VIEW_MAX ? INTERACT_VIEW_MAX : units);
}

int interact_trigger_view(void)
{
    return g_trig_view;
}

int interact_aimed(void)
{
    return g_aimed;
}

const Scene *interact_scene(void)
{
    return g_scene && g_scene_epoch == mem_epoch() ? g_scene : 0;
}

void interact_say(const char *text)
{
    df_copy(g_say, text ? text : "", SAY_MAX);
    g_say_left = g_say[0] ? INTERACT_SAY : 0.0f;
}

const char *interact_saying(int *alpha)
{
    float half;

    half = INTERACT_SAY * 0.5f;
    if (g_say_left <= 0.0f || !g_say[0]) {
        if (alpha) {
            *alpha = 0;
        }
        return 0;
    }
    if (alpha) {
        *alpha = g_say_left >= half ? 255 : df_iround(255.0f * g_say_left / half);
    }
    return g_say;
}

/*
 * Clip the ray's span t0..t1 to the slab lo..hi of one axis, where the
 * ray starts at o and moves d along it. 0 when nothing of it is left.
 */
static int slab(float o, float d, float lo, float hi, float *t0, float *t1)
{
    float a;
    float b;
    float inv;

    if (d > -1.0e-6f && d < 1.0e-6f) {
        return o >= lo && o <= hi;
    }
    inv = 1.0f / d;
    a = (lo - o) * inv;
    b = (hi - o) * inv;
    if (a > b) {
        float s;

        s = a;
        a = b;
        b = s;
    }
    if (a > *t0) {
        *t0 = a;
    }
    if (b < *t1) {
        *t1 = b;
    }
    return *t0 <= *t1;
}

/* Where the ray enters a box of half sizes e about c, in the box's own axes. */
static int box_enter(const float *o, const float *d, const float *c, const float *e,
                     float len, float *t)
{
    float t0;
    float t1;
    int k;

    t0 = 0.0f;
    t1 = len;
    for (k = 0; k < 3; k++) {
        if (!slab(o[k], d[k], c[k] - e[k], c[k] + e[k], &t0, &t1)) {
            return 0;
        }
    }
    *t = t0;
    return 1;
}

/* The ray in a model's frame: its axes are m's columns, which are unit and square. */
static void to_model(const Instance *in, const float *o, const float *d, float *lo, float *ld)
{
    float rx, ry, rz;

    rx = o[0] - in->x;
    ry = o[1] - in->y;
    rz = o[2] - in->z;
    lo[0] = in->m[0] * rx + in->m[3] * ry + in->m[6] * rz;
    lo[1] = in->m[1] * rx + in->m[4] * ry + in->m[7] * rz;
    lo[2] = in->m[2] * rx + in->m[5] * ry + in->m[8] * rz;
    ld[0] = in->m[0] * d[0] + in->m[3] * d[1] + in->m[6] * d[2];
    ld[1] = in->m[1] * d[0] + in->m[4] * d[1] + in->m[7] * d[2];
    ld[2] = in->m[2] * d[0] + in->m[5] * d[1] + in->m[8] * d[2];
}

/* Where the ray first meets usable u, within len. 0 when it does not. */
static int usable_enter(const Scene *scene, const Usable *u, const float *o, const float *d,
                        float len, float *t)
{
    float lo[3];
    float ld[3];
    float c[3];
    float e[3];

    if (u->model >= 0) {
        const Instance *in;

        if (u->model >= scene->nmodels) {
            return 0;
        }
        in = &scene->models[u->model];
        if (!in->mesh) {
            return 0;
        }
        mesh_measure(in->mesh);
        if (in->mesh->bound_n < 1) {
            return 0;
        }
        to_model(in, o, d, lo, ld);
        c[0] = in->mesh->cx;
        c[1] = in->mesh->cy;
        c[2] = in->mesh->cz;
        e[0] = in->mesh->ex;
        e[1] = in->mesh->ey;
        e[2] = in->mesh->ez;
        return box_enter(lo, ld, c, e, len, t);
    }
    if (u->sprite >= 0) {
        const Flat *fl;

        if (u->sprite >= scene->nsprites) {
            return 0;
        }
        fl = &scene->sprites[u->sprite];
        c[0] = fl->x;
        c[1] = fl->y;
        c[2] = fl->z;
        e[0] = fl->w * 0.5f;
        e[1] = fl->h * 0.5f;
        e[2] = fl->w * 0.5f;
        return box_enter(o, d, c, e, len, t);
    }
    {
        float rx, ry, rz;

        /* Across the face (t = up x n), up, out along the normal. */
        rx = o[0] - u->cx;
        ry = o[1] - u->cy;
        rz = o[2] - u->cz;
        lo[0] = u->nz * rx - u->nx * rz;
        lo[1] = ry;
        lo[2] = u->nx * rx + u->nz * rz;
        ld[0] = u->nz * d[0] - u->nx * d[2];
        ld[1] = d[1];
        ld[2] = u->nx * d[0] + u->nz * d[2];
        c[0] = 0.0f;
        c[1] = 0.0f;
        c[2] = 0.0f;
        e[0] = u->hw;
        e[1] = u->hh;
        e[2] = u->hd;
        return box_enter(lo, ld, c, e, len, t);
    }
}

/* The nearest model triangle along the ray within len; -1 for none. */
static int first_wall(const Scene *scene, const float *o, const float *d, float len, float *t)
{
    int best;
    int i;

    best = -1;
    for (i = 0; i < scene->nmodels; i++) {
        const Instance *in;
        Bound b;
        float c[3];
        float e[3];
        float at;

        in = &scene->models[i];
        if (!in->mesh || in->mesh->ntris < 1) {
            continue;
        }
        if (scene->bounds) {
            b = scene->bounds[i];
        } else if (!instance_world_box(in, &b)) {
            continue;
        }
        c[0] = b.cx;
        c[1] = b.cy;
        c[2] = b.cz;
        e[0] = b.ex;
        e[1] = b.ey;
        e[2] = b.ez;
        if (!box_enter(o, d, c, e, len, &at)) {
            continue;
        }
        if (instance_ray(in, o, d, len, &at)) {
            /* Shorter from here on: only a nearer wall can win. */
            len = at;
            best = i;
        }
    }
    *t = len;
    return best;
}

int interact_cast(const Scene *scene, float x, float y, float z,
                  float dx, float dy, float dz, float len, UseHit *hit)
{
    float o[3];
    float d[3];
    float wall;
    float best_t;
    int best;
    int i;

    hit->usable = -1;
    hit->dist = 0.0f;
    hit->model = -1;
    hit->wall = len;
    if (!scene || len <= 0.0f) {
        return 0;
    }
    o[0] = x;
    o[1] = y;
    o[2] = z;
    d[0] = dx;
    d[1] = dy;
    d[2] = dz;
    hit->model = first_wall(scene, o, d, len, &wall);
    hit->wall = wall;
    best = -1;
    best_t = 0.0f;
    for (i = 0; i < scene->nusables; i++) {
        const Usable *u;
        float reach;
        float t;

        u = &scene->usables[i];
        reach = u->reach < wall + WALL_SLACK ? u->reach : wall + WALL_SLACK;
        if (!usable_enter(scene, u, o, d, reach, &t)) {
            continue;
        }
        if (best < 0 || t < best_t) {
            best = i;
            best_t = t;
        }
    }
    if (best < 0) {
        return 0;
    }
    hit->usable = best;
    hit->dist = best_t;
    return 1;
}

int interact_aim_from(const Scene *scene, float x, float y, float z,
                      float dx, float dy, float dz, UseHit *hit)
{
    float len;
    int i;

    hit->usable = -1;
    hit->dist = 0.0f;
    hit->model = -1;
    hit->wall = 0.0f;
    if (!scene || scene->nusables < 1) {
        return 0;
    }
    len = 0.0f;
    for (i = 0; i < scene->nusables; i++) {
        if (scene->usables[i].reach > len) {
            len = scene->usables[i].reach;
        }
    }
    return interact_cast(scene, x, y, z, dx, dy, dz, len, hit);
}

int interact_aim(const Scene *scene, const Camera *cam, UseHit *hit)
{
    Basis basis;

    if (!cam) {
        hit->usable = -1;
        hit->dist = 0.0f;
        hit->model = -1;
        hit->wall = 0.0f;
        return 0;
    }
    camera_basis(cam, &basis);
    /* Forward with the pitch: the middle of the glass. */
    return interact_aim_from(scene, cam->x, cam->y, cam->z,
                             basis.fx * basis.cp, basis.sp, basis.fz * basis.cp, hit);
}

/* Each axis in turn, as the place loaders turn a model (rdb.c). */
static void rot_x(float a, float *y, float *z)
{
    float c, s, y1;

    c = df_cos(a);
    s = df_sin(a);
    y1 = *y * c - *z * s;
    *z = *y * s + *z * c;
    *y = y1;
}

static void rot_y(float a, float *x, float *z)
{
    float c, s, x1;

    c = df_cos(a);
    s = df_sin(a);
    x1 = *x * c + *z * s;
    *z = -*x * s + *z * c;
    *x = x1;
}

static void rot_z(float a, float *x, float *y)
{
    float c, s, x1;

    c = df_cos(a);
    s = df_sin(a);
    x1 = *x * c - *y * s;
    *y = *x * s + *y * c;
    *x = x1;
}

/*
 * The pose at t: the start turned about the model's own axes, Z, then X,
 * then Y, as Unity's Transform.Rotate in Self space, which Daggerfall
 * Unity's actions use, then moved along the world.
 */
static void pose_at(const Mover *m, float t, Instance *out)
{
    float r[9];
    int j;

    for (j = 0; j < 3; j++) {
        float v[3];

        v[0] = j == 0 ? 1.0f : 0.0f;
        v[1] = j == 1 ? 1.0f : 0.0f;
        v[2] = j == 2 ? 1.0f : 0.0f;
        if (m->rz != 0.0f) {
            rot_z(m->rz * t, &v[0], &v[1]);
        }
        if (m->rx != 0.0f) {
            rot_x(m->rx * t, &v[1], &v[2]);
        }
        if (m->ry != 0.0f) {
            rot_y(m->ry * t, &v[0], &v[2]);
        }
        r[j] = v[0];
        r[3 + j] = v[1];
        r[6 + j] = v[2];
    }
    for (j = 0; j < 3; j++) {
        out->m[j * 3 + 0] = m->m0[j * 3] * r[0] + m->m0[j * 3 + 1] * r[3] + m->m0[j * 3 + 2] * r[6];
        out->m[j * 3 + 1] = m->m0[j * 3] * r[1] + m->m0[j * 3 + 1] * r[4] + m->m0[j * 3 + 2] * r[7];
        out->m[j * 3 + 2] = m->m0[j * 3] * r[2] + m->m0[j * 3 + 1] * r[5] + m->m0[j * 3 + 2] * r[8];
    }
    out->x = m->x0 + m->tx * t;
    out->y = m->y0 + m->ty * t;
    out->z = m->z0 + m->tz * t;
}

void mover_init(Mover *movers, int n)
{
    int i;
    int k;

    for (i = 0; i < n; i++) {
        Mover *m;

        m = &movers[i];
        m->state = MOVER_START;
        m->t = 0.0f;
        if (!m->inst) {
            continue;
        }
        m->x0 = m->inst->x;
        m->y0 = m->inst->y;
        m->z0 = m->inst->z;
        for (k = 0; k < 9; k++) {
            m->m0[k] = m->inst->m[k];
        }
    }
}

int mover_sweep(const Mover *m, Bound *out)
{
    float minx, miny, minz, maxx, maxy, maxz;
    int k;

    if (!m || !m->inst || !out) {
        return 0;
    }
    minx = miny = minz = 0.0f;
    maxx = maxy = maxz = 0.0f;
    /* Eight steps of the way: a quarter turn's box is near enough round. */
    for (k = 0; k <= 8; k++) {
        Instance in;
        Bound b;

        in.mesh = m->inst->mesh;
        pose_at(m, (float)k * 0.125f, &in);
        if (!instance_world_box(&in, &b)) {
            return 0;
        }
        if (k == 0 || b.cx - b.ex < minx) {
            minx = b.cx - b.ex;
        }
        if (k == 0 || b.cy - b.ey < miny) {
            miny = b.cy - b.ey;
        }
        if (k == 0 || b.cz - b.ez < minz) {
            minz = b.cz - b.ez;
        }
        if (k == 0 || b.cx + b.ex > maxx) {
            maxx = b.cx + b.ex;
        }
        if (k == 0 || b.cy + b.ey > maxy) {
            maxy = b.cy + b.ey;
        }
        if (k == 0 || b.cz + b.ez > maxz) {
            maxz = b.cz + b.ez;
        }
    }
    out->cx = (minx + maxx) * 0.5f;
    out->cy = (miny + maxy) * 0.5f;
    out->cz = (minz + maxz) * 0.5f;
    out->ex = (maxx - minx) * 0.5f;
    out->ey = (maxy - miny) * 0.5f;
    out->ez = (maxz - minz) * 0.5f;
    return 1;
}

int mover_busy(const Scene *scene, int i)
{
    int steps;

    for (steps = 0; scene && i >= 0 && i < scene->nmovers && steps < CHAIN_MAX; steps++) {
        const Mover *m;

        m = &scene->movers[i];
        if (m->state == MOVER_FORWARD || m->state == MOVER_BACK) {
            return 1;
        }
        i = m->next;
    }
    return 0;
}

static void sound_from(const Mover *m, const SoundClip *clip)
{
    float x, y, z;

    if (!clip || !m->inst || !m->inst->mesh) {
        return;
    }
    instance_point(m->inst, m->inst->mesh->cx, m->inst->mesh->cy, m->inst->mesh->cz, &x, &y, &z);
    sound_play_at(clip, x, y, z, MOVER_INNER, MOVER_REACH, 1.0f, 0);
}

/*
 * A teleport a chain reached (mover_start), for the frame to take
 * (interact_teleport); g_tele_arrive until the next interact_frame,
 * which takes what the body lands in as already entered: a teleporter
 * at the far end does not send it straight back.
 */
static int g_tele;
static int g_tele_arrive;
static float g_tele_x, g_tele_y, g_tele_z, g_tele_yaw;

int interact_teleport(float *x, float *y, float *z, float *yaw)
{
    if (!g_tele) {
        return 0;
    }
    g_tele = 0;
    *x = g_tele_x;
    *y = g_tele_y;
    *z = g_tele_z;
    *yaw = g_tele_yaw;
    return 1;
}

int mover_start(const Scene *scene, int i)
{
    int steps;
    int moved;

    if (!scene || i < 0 || i >= scene->nmovers || mover_busy(scene, i)) {
        return 0;
    }
    moved = 0;
    for (steps = 0; i >= 0 && i < scene->nmovers && steps < CHAIN_MAX; steps++) {
        Mover *m;

        m = &scene->movers[i];
        if (m->teleport) {
            /* No pose to toggle: every time it is reached, the player goes. */
            g_tele = 1;
            g_tele_arrive = 1;
            g_tele_x = m->to_x;
            g_tele_y = m->to_y;
            g_tele_z = m->to_z;
            g_tele_yaw = m->to_yaw;
            sound_from(m, m->go_sound);
            moved = 1;
        } else if (m->state == MOVER_START) {
            m->state = MOVER_FORWARD;
            sound_from(m, m->go_sound);
            moved = 1;
        } else if (m->state == MOVER_END && !m->once) {
            m->state = MOVER_BACK;
            sound_from(m, m->back_sound);
            moved = 1;
        }
        i = m->next;
    }
    return moved;
}

void mover_step(const Scene *scene, float secs)
{
    int i;

    if (!scene || !scene->movers) {
        return;
    }
    for (i = 0; i < scene->nmovers; i++) {
        Mover *m;
        float step;

        m = &scene->movers[i];
        if (m->state != MOVER_FORWARD && m->state != MOVER_BACK) {
            continue;
        }
        step = m->secs > 0.0f ? secs / m->secs : 1.0f;
        if (m->state == MOVER_FORWARD) {
            m->t += step;
            if (m->t >= 1.0f) {
                m->t = 1.0f;
                m->state = MOVER_END;
            }
        } else {
            m->t -= step;
            if (m->t <= 0.0f) {
                m->t = 0.0f;
                m->state = MOVER_START;
            }
        }
        if (m->inst) {
            pose_at(m, m->t, m->inst);
        }
    }
}

int interact_use(const Scene *scene, int i)
{
    const Usable *u;

    if (!scene || i < 0 || i >= scene->nusables) {
        return 0;
    }
    u = &scene->usables[i];
    if (u->kind == USE_MOVER) {
        const Mover *m;

        if (u->mover < 0 || u->mover >= scene->nmovers) {
            return 0;
        }
        m = &scene->movers[u->mover];
        /* Open or opening, a lock does not hold it: it shuts. */
        if (g_locks && m->lock > 0 && m->state == MOVER_START) {
            interact_say(m->door ? "This door is locked." : "It will not move.");
            log_print(LOG_INFO, "use: %s %d, locked (%d)", u->label ? u->label : "mover", i, m->lock);
            return 0;
        }
        if (mover_busy(scene, u->mover)) {
            log_print(LOG_INFO, "use: %s %d, still moving", u->label ? u->label : "mover", i);
            return 0;
        }
        log_print(LOG_INFO, "use: %s %d, %s", u->label ? u->label : "mover", i,
                  m->state == MOVER_START ? "to its end" : "back");
        return mover_start(scene, u->mover);
    }
    if (g_handler && g_handler(scene, u, i)) {
        return 1;
    }
    log_print(LOG_INFO, "use: %s %d, nothing here takes it", u->label ? u->label : "usable", i);
    return 0;
}

/*
 * The touch usable the body was in last frame (-1 none), and the scene it
 * was of: one is used on the frame the body comes into it, not each
 * frame it stays.
 */
static int g_touching = -1;
static const Scene *g_touch_scene;

/* The first usable with touch whose model's box the body (the walker's, under the eye) is in; -1 none. */
/* The game's body for walk-into triggers (interact_set_body, below). */
static const Body *g_toucher;

static int touched(const Scene *scene, const Camera *cam)
{
    float lo;
    float hi;
    float x;
    float z;
    float r;
    int i;

    /* The game's body, if it named one; else the walker's, under the camera's eye. */
    if (g_toucher) {
        x = g_toucher->feet[0];
        z = g_toucher->feet[2];
        lo = g_toucher->feet[1];
        hi = lo + g_toucher->height;
        r = g_toucher->radius;
    } else {
        x = cam->x;
        z = cam->z;
        lo = cam->y - WALK_EYE;
        hi = lo + WALK_HEIGHT;
        r = WALK_RADIUS;
    }
    for (i = 0; i < scene->nusables; i++) {
        const Usable *u;
        const Bound *b;

        u = &scene->usables[i];
        if (!u->touch || u->model < 0 || u->model >= scene->nmodels || !scene->bounds) {
            continue;
        }
        b = &scene->bounds[u->model];
        if (b->ex < 0.0f || hi < b->cy - b->ey || lo > b->cy + b->ey
            || df_absf(x - b->cx) > b->ex + r
            || df_absf(z - b->cz) > b->ez + r) {
            continue;
        }
        return i;
    }
    return -1;
}

/*
 * A use the game asked for (interact_request_use), for the next frame's
 * use edge; from its own origin and way (interact_request_use_from) when
 * g_use_from, else from the camera.
 */
static int g_use_request;
static int g_use_from;
static float g_use_o[3];
static float g_use_d[3];

void interact_request_use(void)
{
    g_use_request = 1;
    g_use_from = 0;
}

void interact_request_use_from(float x, float y, float z, float dx, float dy, float dz)
{
    g_use_request = 1;
    g_use_from = 1;
    g_use_o[0] = x;
    g_use_o[1] = y;
    g_use_o[2] = z;
    g_use_d[0] = dx;
    g_use_d[1] = dy;
    g_use_d[2] = dz;
}

void interact_set_body(const Body *b)
{
    g_toucher = b;
}

int interact_take_use(void)
{
    int r;

    r = g_use_request;
    g_use_request = 0;
    return r;
}

int interact_touching(void)
{
    return g_touching;
}

void interact_frame(const Scene *scene, const Camera *cam, float secs, int used)
{
    UseHit hit;

    if (g_say_left > 0.0f) {
        g_say_left -= secs;
    }
    g_scene = scene;
    g_scene_epoch = mem_epoch();
    if (!scene) {
        g_aimed = -1;
        return;
    }
    if (used || g_view > 0) {
        /* A use the game asked for from its own origin (third person: the body's eye, its facing). */
        if (used && g_use_from) {
            interact_aim_from(scene, g_use_o[0], g_use_o[1], g_use_o[2], g_use_d[0], g_use_d[1], g_use_d[2], &hit);
        } else {
            interact_aim(scene, cam, &hit);
        }
        g_use_from = 0;
        g_aimed = hit.usable;
        if (used && hit.usable >= 0) {
            interact_use(scene, hit.usable);
        } else if (used) {
            /* Say what the click met, so a use that does nothing can be told from one not taken. */
            if (hit.model >= 0) {
                log_print(LOG_INFO, "use: nothing in reach, a wall %d away", df_iround(hit.wall));
            } else {
                log_text(LOG_INFO, "use: nothing in reach");
            }
        }
    }
    /* Walked into: a collision trigger (Usable.touch), once as the body comes in. */
    {
        int t;

        if (scene != g_touch_scene) {
            g_touching = -1;
            g_touch_scene = scene;
        }
        t = touched(scene, cam);
        if (g_tele) {
            /* A teleport waits for the frame to take it: nothing more goes off where the body still is. */
        } else if (g_tele_arrive) {
            /* Landed: what the body stands in counts as come into already. */
            g_tele_arrive = 0;
        } else if (t >= 0 && t != g_touching) {
            interact_use(scene, t);
        }
        g_touching = t;
    }
    mover_step(scene, secs);
}
