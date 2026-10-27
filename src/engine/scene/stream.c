#include "engine/scene/stream.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/core/prof.h"

/* See stream.h and docs/streaming.md. */

#define SLOTS STREAM_SLOTS

typedef struct Slot {
    int used;
    int done;
    int cx;
    int cz;
    void *data;
    /* What the fill wrote: the ground's near and far models, then the rest. */
    int near;
    int far;
    int models;
    int sprites;
    int lights;
    int emitters;
    int usables;
    int movers;
    /* Boxes of the near ground, the far ground, and the rest with its sprites. */
    Bound box_near;
    Bound box_far;
    Bound box_rest;
    /* 1 while the ground span shows the far models. */
    int far_on;
} Slot;

static int g_open;
static Scene *g_scene;
static StreamSource g_src;
static StreamCaps g_cap;
static int g_radius;
static int g_ox;
static int g_oz;
static int g_shifts;
static int g_ccx;
static int g_ccz;
static Slot g_slot[SLOTS];
static Instance *g_models;
static Bound *g_bounds;
static Flat *g_sprites;
static Light *g_lights;
static Emitter *g_emitters;
static Usable *g_usables;
static Mover *g_movers;
static Span *g_spans;
static int g_filling = -1;
/* The longest single fill call since stream_worst last asked, and the step it was. */
static df_u32 g_worst;
static int g_worst_step;
static StreamOut g_out;

static int iabs(int v)
{
    return v < 0 ? -v : v;
}

/* Every array of slot s empty, so every pass skips it. */
static void empty_slot(int s)
{
    int i;

    for (i = s * g_cap.models; i < (s + 1) * g_cap.models; i++) {
        g_models[i].mesh = 0;
        g_bounds[i].cx = g_bounds[i].cy = g_bounds[i].cz = 0.0f;
        /* Negative: the collision grid and the use ray pass over it. */
        g_bounds[i].ex = g_bounds[i].ey = g_bounds[i].ez = -1.0f;
    }
    for (i = s * g_cap.sprites; i < (s + 1) * g_cap.sprites; i++) {
        g_sprites[i].w = 0.0f;
        g_sprites[i].h = 0.0f;
        g_sprites[i].glow = 0;
        g_sprites[i].sight = 0;
        g_sprites[i].thin = 0;
        g_sprites[i].flip = 0;
        g_sprites[i].slot = 0xffff;
    }
    for (i = s * g_cap.lights; i < (s + 1) * g_cap.lights; i++) {
        g_lights[i].radius = 0.0f;
        g_lights[i].flicker = 0;
    }
    for (i = s * g_cap.emitters; i < (s + 1) * g_cap.emitters; i++) {
        g_emitters[i].clip = 0;
        g_emitters[i].reach = 0.0f;
    }
    for (i = s * g_cap.usables; i < (s + 1) * g_cap.usables; i++) {
        g_usables[i].reach = 0.0f;
        g_usables[i].model = -1;
        g_usables[i].sprite = -1;
        g_usables[i].mover = -1;
    }
    for (i = s * g_cap.movers; i < (s + 1) * g_cap.movers; i++) {
        g_movers[i].inst = 0;
        g_movers[i].state = MOVER_START;
        g_movers[i].next = -1;
    }
    g_spans[2 * s].first = s * g_cap.models;
    g_spans[2 * s].count = 0;
    g_spans[2 * s].scount = 0;
    g_spans[2 * s + 1].first = s * g_cap.models;
    g_spans[2 * s + 1].count = 0;
    g_spans[2 * s + 1].sfirst = s * g_cap.sprites;
    g_spans[2 * s + 1].scount = 0;
    g_spans[2 * s].efirst = 0;
    g_spans[2 * s].ecount = 0;
    g_spans[2 * s + 1].efirst = s * g_cap.emitters;
    g_spans[2 * s + 1].ecount = 0;
    g_slot[s].used = 0;
    g_slot[s].done = 0;
    g_slot[s].data = 0;
}

/* The chunk in slot s goes: the source lets go of it, then the slot is empty. */
static void drop_slot(int s)
{
    int was_done;

    was_done = g_slot[s].done;
    if (g_src.drop) {
        g_src.drop(g_src.user, g_slot[s].cx, g_slot[s].cz, g_slot[s].data);
    }
    empty_slot(s);
    g_scene->version++;
    if (was_done && g_src.changed) {
        g_src.changed(g_src.user, s, 0);
    }
}

#define ALLOC(type, n) ((type *)mem_alloc((df_u32)(n) * (df_u32)sizeof(type)))

int stream_open(Scene *scene, const StreamSource *src, int radius, int cx, int cz)
{
    int s;

    stream_close();
    if (!scene || !src || !src->fill || src->caps.models < 1) {
        return 0;
    }
    g_src = *src;
    g_cap = src->caps;
    /* Every kind has room for one at least, so no slot range is empty of storage. */
    g_cap.sprites = g_cap.sprites > 0 ? g_cap.sprites : 1;
    g_cap.lights = g_cap.lights > 0 ? g_cap.lights : 1;
    g_cap.emitters = g_cap.emitters > 0 ? g_cap.emitters : 1;
    g_cap.usables = g_cap.usables > 0 ? g_cap.usables : 1;
    g_cap.movers = g_cap.movers > 0 ? g_cap.movers : 1;
    g_models = ALLOC(Instance, SLOTS * g_cap.models);
    g_bounds = ALLOC(Bound, SLOTS * g_cap.models);
    g_sprites = ALLOC(Flat, SLOTS * g_cap.sprites);
    g_lights = ALLOC(Light, SLOTS * g_cap.lights);
    g_emitters = ALLOC(Emitter, SLOTS * g_cap.emitters);
    g_usables = ALLOC(Usable, SLOTS * g_cap.usables);
    g_movers = ALLOC(Mover, SLOTS * g_cap.movers);
    g_spans = ALLOC(Span, 2 * SLOTS);
    if (!g_models || !g_bounds || !g_sprites || !g_lights || !g_emitters || !g_usables
        || !g_movers || !g_spans) {
        log_text(LOG_ERROR, "stream: out of memory for its slots");
        return 0;
    }
    g_scene = scene;
    for (s = 0; s < SLOTS; s++) {
        empty_slot(s);
    }
    scene->models = g_models;
    scene->bounds = g_bounds;
    scene->nmodels = SLOTS * g_cap.models;
    scene->spans = g_spans;
    scene->nspans = 2 * SLOTS;
    scene->sprites = g_sprites;
    scene->nsprites = SLOTS * g_cap.sprites;
    scene->sprite_spans = 1;
    scene->lights = g_lights;
    scene->nlights = SLOTS * g_cap.lights;
    scene->emitters = g_emitters;
    scene->nemitters = SLOTS * g_cap.emitters;
    scene->usables = g_usables;
    scene->nusables = SLOTS * g_cap.usables;
    scene->movers = g_movers;
    scene->nmovers = SLOTS * g_cap.movers;
    scene->version++;
    g_ox = cx;
    g_oz = cz;
    g_ccx = cx;
    g_ccz = cz;
    g_shifts = 0;
    g_filling = -1;
    stream_set_radius(radius);
    g_open = 1;
    log_print(LOG_INFO, "stream: %d slots of %d models and %d sprites, radius %d, at chunk %d %d",
              SLOTS, g_cap.models, g_cap.sprites, g_radius, cx, cz);
    return 1;
}

void stream_close(void)
{
    int s;

    if (g_open) {
        for (s = 0; s < SLOTS; s++) {
            if (g_slot[s].used) {
                drop_slot(s);
            }
        }
    }
    g_open = 0;
    g_scene = 0;
    g_filling = -1;
}

void stream_reload(void)
{
    int s;

    if (!g_open) {
        return;
    }
    for (s = 0; s < SLOTS; s++) {
        if (g_slot[s].used) {
            drop_slot(s);
        }
    }
    g_filling = -1;
}

/* Chunks within rings of the camera's that are not loaded yet. */
static int missing_within(int rings)
{
    int n;
    int x;
    int z;

    n = 0;
    for (z = -rings; z <= rings; z++) {
        for (x = -rings; x <= rings; x++) {
            int k;
            int have;

            have = 0;
            for (k = 0; k < SLOTS && !have; k++) {
                have = g_slot[k].used && g_slot[k].done
                    && g_slot[k].cx == g_ccx + x && g_slot[k].cz == g_ccz + z;
            }
            n += !have;
        }
    }
    return n;
}

int stream_missing(int rings)
{
    return g_open ? missing_within(rings < g_radius ? rings : g_radius) : 0;
}

void *stream_chunk_data(int cx, int cz)
{
    int s;

    for (s = 0; g_open && s < SLOTS; s++) {
        if (g_slot[s].used && g_slot[s].done && g_slot[s].cx == cx && g_slot[s].cz == cz) {
            return g_slot[s].data;
        }
    }
    return 0;
}

void stream_local(double wx, double wz, float *x, float *z)
{
    *x = (float)(wx - (double)g_ox * (double)STREAM_CHUNK);
    *z = (float)(wz - (double)g_oz * (double)STREAM_CHUNK);
}

void stream_worst(float *ms, int *step)
{
    df_u32 per;

    per = prof_per_ms();
    *ms = per ? (float)g_worst / (float)per : 0.0f;
    *step = g_worst_step;
    g_worst = 0;
    g_worst_step = -1;
}

int stream_active(void)
{
    return g_open;
}

void stream_set_radius(int radius)
{
    g_radius = radius < 1 ? 1 : (radius > STREAM_RADIUS_MAX ? STREAM_RADIUS_MAX : radius);
}

int stream_radius(void)
{
    return g_radius;
}

void stream_chunk_at(const Camera *cam, int *cx, int *cz)
{
    *cx = g_ox + df_ifloor(cam->x / (float)STREAM_CHUNK);
    *cz = g_oz + df_ifloor(cam->z / (float)STREAM_CHUNK);
}

void stream_origin(int *cx, int *cz)
{
    *cx = g_ox;
    *cz = g_oz;
}

void stream_world(float x, float z, double *wx, double *wz)
{
    *wx = (double)g_ox * (double)STREAM_CHUNK + (double)x;
    *wz = (double)g_oz * (double)STREAM_CHUNK + (double)z;
}

int stream_slot_chunk(int slot, int *cx, int *cz)
{
    if (slot < 0 || slot >= SLOTS || !g_slot[slot].used || !g_slot[slot].done) {
        return 0;
    }
    *cx = g_slot[slot].cx;
    *cz = g_slot[slot].cz;
    return 1;
}

void stream_slot_models(int slot, int *first, int *near, int *far, int *rest)
{
    const Slot *sl;

    sl = &g_slot[slot];
    *first = slot * g_cap.models;
    *near = sl->done ? sl->near : 0;
    *far = sl->done ? sl->far : 0;
    *rest = sl->done ? sl->models - sl->near - sl->far : 0;
}

void stream_slot_lights(int slot, int *first, int *count)
{
    *first = slot * g_cap.lights;
    *count = g_slot[slot].done ? g_slot[slot].lights : 0;
}

void stream_stats(int *loaded, int *waiting, int *filling, int *shifts)
{
    int s;
    int n;

    n = 0;
    for (s = 0; s < SLOTS; s++) {
        n += g_slot[s].used && g_slot[s].done;
    }
    if (loaded) {
        *loaded = n;
    }
    if (waiting) {
        /* Within the radius and not loaded yet. */
        *waiting = missing_within(g_radius);
    }
    if (filling) {
        *filling = g_filling >= 0;
    }
    if (shifts) {
        *shifts = g_shifts;
    }
}

static void move_box(Bound *b, float sx, float sz)
{
    b->cx -= sx;
    b->cz -= sz;
}

/* Move the origin to the camera's chunk: everything loaded, and the camera, by whole chunks. */
static void rebase(Camera *cam, int ccx, int ccz)
{
    float sx;
    float sz;
    int s;

    sx = (float)((ccx - g_ox) * STREAM_CHUNK);
    sz = (float)((ccz - g_oz) * STREAM_CHUNK);
    for (s = 0; s < SLOTS; s++) {
        Slot *sl;
        int i;
        int k;

        sl = &g_slot[s];
        /* A chunk being filled is still in its own frame: placed when it is done. */
        if (!sl->used || !sl->done) {
            continue;
        }
        for (k = 0; k < sl->models; k++) {
            i = s * g_cap.models + k;
            g_models[i].x -= sx;
            g_models[i].z -= sz;
            move_box(&g_bounds[i], sx, sz);
        }
        for (k = 0; k < sl->sprites; k++) {
            i = s * g_cap.sprites + k;
            g_sprites[i].x -= sx;
            g_sprites[i].z -= sz;
        }
        for (k = 0; k < sl->lights; k++) {
            i = s * g_cap.lights + k;
            g_lights[i].x -= sx;
            g_lights[i].z -= sz;
        }
        for (k = 0; k < sl->emitters; k++) {
            i = s * g_cap.emitters + k;
            g_emitters[i].x -= sx;
            g_emitters[i].z -= sz;
        }
        for (k = 0; k < sl->usables; k++) {
            i = s * g_cap.usables + k;
            g_usables[i].cx -= sx;
            g_usables[i].cz -= sz;
        }
        for (k = 0; k < sl->movers; k++) {
            i = s * g_cap.movers + k;
            g_movers[i].x0 -= sx;
            g_movers[i].z0 -= sz;
        }
        move_box(&sl->box_near, sx, sz);
        move_box(&sl->box_far, sx, sz);
        move_box(&sl->box_rest, sx, sz);
        g_spans[2 * s].cx -= sx;
        g_spans[2 * s].cz -= sz;
        g_spans[2 * s + 1].cx -= sx;
        g_spans[2 * s + 1].cz -= sz;
    }
    if (g_src.shift) {
        g_src.shift(g_src.user, sx, sz);
    }
    cam->x -= sx;
    cam->z -= sz;
    log_print(LOG_DEBUG, "stream: origin from chunk %d %d to %d %d", g_ox, g_oz, ccx, ccz);
    g_ox = ccx;
    g_oz = ccz;
    g_shifts++;
    g_scene->version++;
}

/* A box round a run of model boxes; empty (negative) when the run is. */
static Bound box_of(int first, int count)
{
    Bound out;
    float minx, miny, minz, maxx, maxy, maxz;
    int i;

    minx = miny = minz = 1.0e30f;
    maxx = maxy = maxz = -1.0e30f;
    for (i = first; i < first + count; i++) {
        const Bound *b;

        b = &g_bounds[i];
        if (b->ex < 0.0f) {
            continue;
        }
        minx = b->cx - b->ex < minx ? b->cx - b->ex : minx;
        miny = b->cy - b->ey < miny ? b->cy - b->ey : miny;
        minz = b->cz - b->ez < minz ? b->cz - b->ez : minz;
        maxx = b->cx + b->ex > maxx ? b->cx + b->ex : maxx;
        maxy = b->cy + b->ey > maxy ? b->cy + b->ey : maxy;
        maxz = b->cz + b->ez > maxz ? b->cz + b->ez : maxz;
    }
    if (maxx < minx) {
        out.cx = out.cy = out.cz = 0.0f;
        out.ex = out.ey = out.ez = -1.0f;
        return out;
    }
    out.cx = (minx + maxx) * 0.5f;
    out.cy = (miny + maxy) * 0.5f;
    out.cz = (minz + maxz) * 0.5f;
    out.ex = (maxx - minx) * 0.5f;
    out.ey = (maxy - miny) * 0.5f;
    out.ez = (maxz - minz) * 0.5f;
    return out;
}

/* Grow a box to hold a point's box of half size e. */
static void grow(Bound *b, float x, float y, float z, float e)
{
    float minx, miny, minz, maxx, maxy, maxz;

    if (b->ex < 0.0f) {
        b->cx = x;
        b->cy = y;
        b->cz = z;
        b->ex = b->ey = b->ez = e;
        return;
    }
    minx = b->cx - b->ex < x - e ? b->cx - b->ex : x - e;
    miny = b->cy - b->ey < y - e ? b->cy - b->ey : y - e;
    minz = b->cz - b->ez < z - e ? b->cz - b->ez : z - e;
    maxx = b->cx + b->ex > x + e ? b->cx + b->ex : x + e;
    maxy = b->cy + b->ey > y + e ? b->cy + b->ey : y + e;
    maxz = b->cz + b->ez > z + e ? b->cz + b->ez : z + e;
    b->cx = (minx + maxx) * 0.5f;
    b->cy = (miny + maxy) * 0.5f;
    b->cz = (minz + maxz) * 0.5f;
    b->ex = (maxx - minx) * 0.5f;
    b->ey = (maxy - miny) * 0.5f;
    b->ez = (maxz - minz) * 0.5f;
}

static void set_span(Span *sp, const Bound *b)
{
    sp->cx = b->cx;
    sp->cy = b->cy;
    sp->cz = b->cz;
    sp->ex = b->ex > 0.0f ? b->ex : 0.0f;
    sp->ey = b->ey > 0.0f ? b->ey : 0.0f;
    sp->ez = b->ez > 0.0f ? b->ez : 0.0f;
}

/* The ground span shows the near models within STREAM_NEAR rings, else the far ones (if any). */
static void choose_ground(int s, int ccx, int ccz)
{
    Slot *sl;
    Span *sp;
    int far;
    int base;

    sl = &g_slot[s];
    sp = &g_spans[2 * s];
    base = s * g_cap.models;
    far = sl->far > 0 && (iabs(sl->cx - ccx) > STREAM_NEAR || iabs(sl->cz - ccz) > STREAM_NEAR);
    if (far) {
        sp->first = base + sl->near;
        sp->count = sl->far;
        set_span(sp, &sl->box_far);
    } else {
        sp->first = base;
        sp->count = sl->near;
        set_span(sp, &sl->box_near);
    }
    sl->far_on = far;
}

/* The chunk is filled: into the local frame, its indices into the scene's, its boxes, its spans. */
static void finish(int s)
{
    Slot *sl;
    Span *rest;
    float ox;
    float oz;
    int i;
    int base;

    sl = &g_slot[s];
    ox = (float)((sl->cx - g_ox) * STREAM_CHUNK);
    oz = (float)((sl->cz - g_oz) * STREAM_CHUNK);
    base = s * g_cap.models;
    sl->near = g_out.near_n;
    sl->far = g_out.far_n;
    sl->models = g_out.models_n;
    sl->sprites = g_out.sprites_n;
    sl->lights = g_out.lights_n;
    sl->emitters = g_out.emitters_n;
    sl->usables = g_out.usables_n;
    sl->movers = g_out.movers_n;
    sl->data = g_out.data;
    for (i = base; i < base + sl->models; i++) {
        Bound *b;

        g_models[i].x += ox;
        g_models[i].z += oz;
        b = &g_bounds[i];
        if (!g_models[i].mesh || !instance_world_box(&g_models[i], b)) {
            b->cx = g_models[i].x;
            b->cy = g_models[i].y;
            b->cz = g_models[i].z;
            b->ex = b->ey = b->ez = g_models[i].mesh ? 0.0f : -1.0f;
        }
    }
    for (i = 0; i < sl->sprites; i++) {
        Flat *fl;

        fl = &g_sprites[s * g_cap.sprites + i];
        fl->x += ox;
        fl->z += oz;
    }
    for (i = 0; i < sl->lights; i++) {
        g_lights[s * g_cap.lights + i].x += ox;
        g_lights[s * g_cap.lights + i].z += oz;
    }
    for (i = 0; i < sl->emitters; i++) {
        g_emitters[s * g_cap.emitters + i].x += ox;
        g_emitters[s * g_cap.emitters + i].z += oz;
    }
    for (i = 0; i < sl->usables; i++) {
        Usable *u;

        u = &g_usables[s * g_cap.usables + i];
        u->cx += ox;
        u->cz += oz;
        if (u->model >= 0) {
            u->model += base;
        }
        if (u->sprite >= 0) {
            u->sprite += s * g_cap.sprites;
        }
        if (u->mover >= 0) {
            u->mover += s * g_cap.movers;
        }
    }
    for (i = 0; i < sl->movers; i++) {
        Mover *m;

        m = &g_movers[s * g_cap.movers + i];
        m->model += base;
        m->inst = &g_models[m->model];
        m->x0 += ox;
        m->z0 += oz;
        if (m->next >= 0) {
            m->next += s * g_cap.movers;
        }
    }
    sl->box_near = box_of(base, sl->near);
    sl->box_far = box_of(base + sl->near, sl->far);
    sl->box_rest = box_of(base + sl->near + sl->far, sl->models - sl->near - sl->far);
    for (i = 0; i < sl->sprites; i++) {
        const Flat *fl;

        fl = &g_sprites[s * g_cap.sprites + i];
        if (fl->w > 0.0f) {
            grow(&sl->box_rest, fl->x, fl->y, fl->z, (fl->w > fl->h ? fl->w : fl->h) * 0.5f);
        }
    }
    rest = &g_spans[2 * s + 1];
    rest->first = base + sl->near + sl->far;
    rest->count = sl->models - sl->near - sl->far;
    rest->sfirst = s * g_cap.sprites;
    rest->scount = sl->sprites;
    rest->efirst = s * g_cap.emitters;
    rest->ecount = sl->emitters;
    set_span(rest, &sl->box_rest);
    sl->done = 1;
    choose_ground(s, g_ccx, g_ccz);
    g_scene->version++;
    if (g_src.changed) {
        g_src.changed(g_src.user, s, 1);
    }
}

static int loaded_or_filling(int cx, int cz)
{
    int s;

    for (s = 0; s < SLOTS; s++) {
        if (g_slot[s].used && g_slot[s].cx == cx && g_slot[s].cz == cz) {
            return 1;
        }
    }
    return 0;
}

/* The nearest chunk within the radius that is not loaded, ring by ring. 0 when none is missing. */
static int next_missing(int ccx, int ccz, int *cx, int *cz)
{
    int d;

    for (d = 0; d <= g_radius; d++) {
        int x;
        int z;

        for (z = -d; z <= d; z++) {
            for (x = -d; x <= d; x++) {
                if (iabs(x) != d && iabs(z) != d) {
                    continue;
                }
                if (!loaded_or_filling(ccx + x, ccz + z)) {
                    *cx = ccx + x;
                    *cz = ccz + z;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int free_slot(void)
{
    int s;

    for (s = 0; s < SLOTS; s++) {
        if (!g_slot[s].used) {
            return s;
        }
    }
    return -1;
}

static void start_fill(int s, int cx, int cz)
{
    g_slot[s].used = 1;
    g_slot[s].done = 0;
    g_slot[s].cx = cx;
    g_slot[s].cz = cz;
    g_slot[s].data = 0;
    g_out.models = &g_models[s * g_cap.models];
    g_out.sprites = &g_sprites[s * g_cap.sprites];
    g_out.lights = &g_lights[s * g_cap.lights];
    g_out.emitters = &g_emitters[s * g_cap.emitters];
    g_out.usables = &g_usables[s * g_cap.usables];
    g_out.movers = &g_movers[s * g_cap.movers];
    g_out.near_n = 0;
    g_out.far_n = 0;
    g_out.models_n = 0;
    g_out.sprites_n = 0;
    g_out.lights_n = 0;
    g_out.emitters_n = 0;
    g_out.usables_n = 0;
    g_out.movers_n = 0;
    g_out.step = 0;
    g_out.data = 0;
    g_filling = s;
}

/* The fill's counts within the slot's room. */
static void clamp_out(void)
{
    g_out.models_n = g_out.models_n < g_cap.models ? g_out.models_n : g_cap.models;
    g_out.near_n = g_out.near_n < g_out.models_n ? g_out.near_n : g_out.models_n;
    g_out.far_n = g_out.near_n + g_out.far_n <= g_out.models_n ? g_out.far_n : g_out.models_n - g_out.near_n;
    g_out.sprites_n = g_out.sprites_n < g_cap.sprites ? g_out.sprites_n : g_cap.sprites;
    g_out.lights_n = g_out.lights_n < g_cap.lights ? g_out.lights_n : g_cap.lights;
    g_out.emitters_n = g_out.emitters_n < g_cap.emitters ? g_out.emitters_n : g_cap.emitters;
    g_out.usables_n = g_out.usables_n < g_cap.usables ? g_out.usables_n : g_cap.usables;
    g_out.movers_n = g_out.movers_n < g_cap.movers ? g_out.movers_n : g_cap.movers;
}

void stream_update(Camera *cam, float budget_ms)
{
    df_u32 start;
    df_u32 budget;
    int ccx;
    int ccz;
    int far;
    int s;

    if (!g_open || !cam) {
        return;
    }
    prof_begin(PROF_STREAM);
    stream_chunk_at(cam, &ccx, &ccz);
    if (iabs(ccx - g_ox) > STREAM_REBASE || iabs(ccz - g_oz) > STREAM_REBASE) {
        rebase(cam, ccx, ccz);
    }
    g_ccx = ccx;
    g_ccz = ccz;
    /* Out of range, past the kept rings: dropped, even half filled. */
    far = g_radius + STREAM_KEEP;
    for (s = 0; s < SLOTS; s++) {
        if (g_slot[s].used && (iabs(g_slot[s].cx - ccx) > far || iabs(g_slot[s].cz - ccz) > far)) {
            if (s == g_filling) {
                /* Half filled: its data is still the fill's. */
                g_slot[s].data = g_out.data;
                g_filling = -1;
            }
            drop_slot(s);
        }
    }
    /* Near ground or far, by the camera's chunk. */
    for (s = 0; s < SLOTS; s++) {
        if (g_slot[s].used && g_slot[s].done) {
            choose_ground(s, ccx, ccz);
        }
    }
    start = prof_now();
    budget = budget_ms > 0.0f ? (df_u32)df_iround(budget_ms * (float)prof_per_ms()) : 0u;
    for (;;) {
        int done;

        if (g_filling < 0) {
            int cx;
            int cz;

            if (!next_missing(ccx, ccz, &cx, &cz)) {
                break;
            }
            s = free_slot();
            if (s < 0) {
                break;
            }
            start_fill(s, cx, cz);
        }
        {
            df_u32 t0;
            int step;

            step = g_out.step;
            t0 = prof_now();
            done = g_src.fill(g_src.user, g_slot[g_filling].cx, g_slot[g_filling].cz, &g_out);
            t0 = prof_now() - t0;
            if (t0 > g_worst) {
                g_worst = t0;
                g_worst_step = step;
            }
        }
        clamp_out();
        if (done) {
            s = g_filling;
            g_filling = -1;
            finish(s);
        }
        /* Uncalibrated, or no budget: one step a frame. */
        if (budget == 0u || prof_now() - start >= budget) {
            break;
        }
    }
    prof_end(PROF_STREAM);
}
