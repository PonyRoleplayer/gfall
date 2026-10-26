#include "engine/scene/stream.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/core/prof.h"

/* See stream.h and docs/streaming.md. */

/* Slots enough for every chunk within the radius plus the rings kept. */
#define SIDE_MAX (2 * (STREAM_RADIUS_MAX + STREAM_KEEP) + 1)
#define SLOTS (SIDE_MAX * SIDE_MAX)

typedef struct Slot {
    int used;
    int done;
    int cx;
    int cz;
} Slot;

static int g_open;
static Scene *g_scene;
static StreamSource g_src;
static int g_radius;
static int g_ox;
static int g_oz;
static int g_shifts;
/* The camera's chunk at the last update. */
static int g_ccx;
static int g_ccz;
static Slot g_slot[SLOTS];
static Instance *g_models;
static Bound *g_bounds;
static Span *g_spans;
/* The slot being filled, -1 for none, and its fill's state. */
static int g_filling = -1;
static StreamOut g_out;

static int iabs(int v)
{
    return v < 0 ? -v : v;
}

static void empty_slot(int s)
{
    int cap;
    int i;
    Span *sp;

    cap = g_src.models;
    for (i = s * cap; i < (s + 1) * cap; i++) {
        g_models[i].mesh = 0;
        g_bounds[i].cx = 0.0f;
        g_bounds[i].cy = 0.0f;
        g_bounds[i].cz = 0.0f;
        /* Negative: the collision grid and the use ray pass over it. */
        g_bounds[i].ex = -1.0f;
        g_bounds[i].ey = -1.0f;
        g_bounds[i].ez = -1.0f;
    }
    sp = &g_spans[s];
    sp->first = s * cap;
    sp->count = 0;
    sp->cx = sp->cy = sp->cz = 0.0f;
    sp->ex = sp->ey = sp->ez = 0.0f;
    g_slot[s].used = 0;
    g_slot[s].done = 0;
}

int stream_open(Scene *scene, const StreamSource *src, int radius, int cx, int cz)
{
    df_u32 n;
    int s;

    stream_close();
    if (!scene || !src || !src->fill || src->models < 1) {
        return 0;
    }
    g_src = *src;
    n = (df_u32)SLOTS * (df_u32)src->models;
    g_models = (Instance *)mem_alloc(n * (df_u32)sizeof(Instance));
    g_bounds = (Bound *)mem_alloc(n * (df_u32)sizeof(Bound));
    g_spans = (Span *)mem_alloc((df_u32)SLOTS * (df_u32)sizeof(Span));
    if (!g_models || !g_bounds || !g_spans) {
        log_text(LOG_ERROR, "stream: out of memory for its slots");
        return 0;
    }
    mem_zero(g_models, n * (df_u32)sizeof(Instance));
    for (s = 0; s < SLOTS; s++) {
        empty_slot(s);
    }
    g_scene = scene;
    scene->models = g_models;
    scene->bounds = g_bounds;
    scene->nmodels = (int)n;
    scene->spans = g_spans;
    scene->nspans = SLOTS;
    scene->version++;
    g_ox = cx;
    g_oz = cz;
    g_ccx = cx;
    g_ccz = cz;
    g_shifts = 0;
    g_filling = -1;
    stream_set_radius(radius);
    g_open = 1;
    log_print(LOG_INFO, "stream: %d slots of %d models, radius %d, at chunk %d %d",
              SLOTS, src->models, g_radius, cx, cz);
    return 1;
}

void stream_close(void)
{
    g_open = 0;
    g_scene = 0;
    g_filling = -1;
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
        int x;
        int z;

        /* Within the radius and not loaded yet. */
        *waiting = 0;
        for (z = -g_radius; z <= g_radius; z++) {
            for (x = -g_radius; x <= g_radius; x++) {
                int k;
                int have;

                have = 0;
                for (k = 0; k < SLOTS && !have; k++) {
                    have = g_slot[k].used && g_slot[k].done
                        && g_slot[k].cx == g_ccx + x && g_slot[k].cz == g_ccz + z;
                }
                *waiting += !have;
            }
        }
    }
    if (filling) {
        *filling = g_filling >= 0;
    }
    if (shifts) {
        *shifts = g_shifts;
    }
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
        int i;
        Span *sp;

        /* A chunk being filled is still in its own frame: placed when it is done. */
        if (!g_slot[s].used || !g_slot[s].done) {
            continue;
        }
        sp = &g_spans[s];
        for (i = sp->first; i < sp->first + sp->count; i++) {
            g_models[i].x -= sx;
            g_models[i].z -= sz;
            g_bounds[i].cx -= sx;
            g_bounds[i].cz -= sz;
        }
        sp->cx -= sx;
        sp->cz -= sz;
    }
    cam->x -= sx;
    cam->z -= sz;
    log_print(LOG_DEBUG, "stream: origin from chunk %d %d to %d %d", g_ox, g_oz, ccx, ccz);
    g_ox = ccx;
    g_oz = ccz;
    g_shifts++;
    g_scene->version++;
}

/* The chunk is filled: into the local frame, its boxes, its span. */
static void finish(int s)
{
    Span *sp;
    float ox;
    float oz;
    float minx, miny, minz, maxx, maxy, maxz;
    int first;
    int i;

    ox = (float)((g_slot[s].cx - g_ox) * STREAM_CHUNK);
    oz = (float)((g_slot[s].cz - g_oz) * STREAM_CHUNK);
    first = s * g_src.models;
    minx = miny = minz = 1.0e30f;
    maxx = maxy = maxz = -1.0e30f;
    for (i = first; i < first + g_out.count; i++) {
        Bound *b;

        g_models[i].x += ox;
        g_models[i].z += oz;
        b = &g_bounds[i];
        if (!instance_world_box(&g_models[i], b)) {
            b->cx = g_models[i].x;
            b->cy = g_models[i].y;
            b->cz = g_models[i].z;
            b->ex = b->ey = b->ez = 0.0f;
        }
        minx = b->cx - b->ex < minx ? b->cx - b->ex : minx;
        miny = b->cy - b->ey < miny ? b->cy - b->ey : miny;
        minz = b->cz - b->ez < minz ? b->cz - b->ez : minz;
        maxx = b->cx + b->ex > maxx ? b->cx + b->ex : maxx;
        maxy = b->cy + b->ey > maxy ? b->cy + b->ey : maxy;
        maxz = b->cz + b->ez > maxz ? b->cz + b->ez : maxz;
    }
    sp = &g_spans[s];
    sp->first = first;
    sp->count = g_out.count;
    if (g_out.count > 0) {
        sp->cx = (minx + maxx) * 0.5f;
        sp->cy = (miny + maxy) * 0.5f;
        sp->cz = (minz + maxz) * 0.5f;
        sp->ex = (maxx - minx) * 0.5f;
        sp->ey = (maxy - miny) * 0.5f;
        sp->ez = (maxz - minz) * 0.5f;
    }
    g_slot[s].done = 1;
    g_scene->version++;
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
                g_filling = -1;
            }
            empty_slot(s);
            g_scene->version++;
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
            g_slot[s].used = 1;
            g_slot[s].done = 0;
            g_slot[s].cx = cx;
            g_slot[s].cz = cz;
            g_out.models = &g_models[s * g_src.models];
            g_out.cap = g_src.models;
            g_out.count = 0;
            g_out.step = 0;
            g_filling = s;
        }
        done = g_src.fill(g_src.user, g_slot[g_filling].cx, g_slot[g_filling].cz, &g_out);
        if (g_out.count > g_out.cap) {
            g_out.count = g_out.cap;
        }
        if (done) {
            finish(g_filling);
            g_filling = -1;
        }
        /* Uncalibrated, or no budget: one step a frame. */
        if (budget == 0u || prof_now() - start >= budget) {
            break;
        }
    }
    prof_end(PROF_STREAM);
}
