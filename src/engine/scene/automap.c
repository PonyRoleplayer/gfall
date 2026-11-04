#include "engine/scene/automap.h"
#include "engine/scene/stream.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See automap.h and docs/automap.md. */

/* ---- The rows: every model seen, in every place, sorted by key ---- */

static AutomapRow *g_rows;
static int g_n;
static int g_cap;
/* The rows' own heap, never a place's: they outlive every place. */
static MemHeap *g_heap;
/* Moves when rows are put back or cleared, so the per-model cache below is made again. */
static df_u32 g_rows_serial;
static df_u32 g_place;

static int row_cmp(const AutomapRow *a, const AutomapRow *b)
{
    if (a->place != b->place) {
        return a->place < b->place ? -1 : 1;
    }
    if (a->mesh != b->mesh) {
        return a->mesh < b->mesh ? -1 : 1;
    }
    if (a->x != b->x) {
        return a->x < b->x ? -1 : 1;
    }
    if (a->z != b->z) {
        return a->z < b->z ? -1 : 1;
    }
    if (a->y != b->y) {
        return a->y < b->y ? -1 : 1;
    }
    return 0;
}

/* The row's index among the first n, or where it would go there, as -1 - index. */
static int search(const AutomapRow *key, int n)
{
    int lo;
    int hi;

    lo = 0;
    hi = n - 1;
    while (lo <= hi) {
        int mid;
        int c;

        mid = (lo + hi) / 2;
        c = row_cmp(&g_rows[mid], key);
        if (c == 0) {
            return mid;
        }
        if (c < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return -1 - lo;
}

/* Room for one more row: doubled, the old block given back. */
static int grow(void)
{
    MemHeap *was;
    AutomapRow *rows;
    int cap;

    if (g_n < g_cap) {
        return 1;
    }
    if (!g_heap) {
        g_heap = mem_heap_new();
        if (!g_heap) {
            return 0;
        }
    }
    cap = g_cap ? g_cap * 2 : 512;
    was = mem_heap_use(g_heap);
    rows = (AutomapRow *)mem_alloc((df_u32)cap * (df_u32)sizeof(AutomapRow));
    mem_heap_use(was);
    if (!rows) {
        log_text(LOG_ERROR, "automap: no memory for more seen models");
        return 0;
    }
    if (g_rows) {
        mem_copy(rows, g_rows, (df_u32)g_n * (df_u32)sizeof(AutomapRow));
        mem_heap_free(g_heap, g_rows);
    }
    g_rows = rows;
    g_cap = cap;
    return 1;
}

/* The row in its place among the rest; 1 when it is there after (already or now). */
static int insert(const AutomapRow *row)
{
    int at;
    int k;

    at = search(row, g_n);
    if (at >= 0) {
        return 1;
    }
    if (!grow()) {
        return 0;
    }
    at = -1 - at;
    for (k = g_n; k > at; k--) {
        g_rows[k] = g_rows[k - 1];
    }
    g_rows[at] = *row;
    g_n++;
    return 1;
}

int automap_count(void)
{
    return g_n;
}

const AutomapRow *automap_at(int i)
{
    return i >= 0 && i < g_n ? &g_rows[i] : 0;
}

int automap_put(const AutomapRow *row)
{
    if (!row || !insert(row)) {
        return 0;
    }
    g_rows_serial++;
    return 1;
}

void automap_clear(void)
{
    g_n = 0;
    g_rows_serial++;
}

void automap_set_place(df_u32 place)
{
    g_place = place;
}

df_u32 automap_place(void)
{
    return g_place;
}

/* ---- Per model of the scene in hand: seen or not, made from the rows ---- */

enum {
    MODEL_NONE = 0,
    MODEL_UNSEEN = 1,
    MODEL_SEEN = 2
};
/* With the state: the model is a mover's, whose key is where it starts, not where it stands. */
#define MODEL_MOVER 4
#define MODEL_STATE(s) ((s) & 3)

static df_u8 *g_state;
static int g_state_cap;
/* What the states were made for: another scene, version, heap, place or rows makes them again. */
static const Scene *g_cache_scene;
static int g_cache_version;
static int g_cache_models;
static df_u32 g_cache_epoch;
static df_u32 g_cache_place;
static df_u32 g_cache_serial;
/* The scene's origin in the place's frame, whole units: a stream's chunk origin, else 0. */
static df_i32 g_ox;
static df_i32 g_oz;

static void key_at(df_u32 mesh, float x, float y, float z, AutomapRow *row)
{
    row->place = g_place;
    row->mesh = mesh;
    /*
     * Local positions are within 2^22 units of the scene's origin,
     * df_iround's range; the origin is added as a whole number, so a key
     * is the same before and after the stream's origin moves.
     */
    row->x = (df_i32)df_iround(x) + g_ox;
    row->y = (df_i32)df_iround(y);
    row->z = (df_i32)df_iround(z) + g_oz;
}

/* Model i's key: a mover's by where it starts, so a door half open or a lift raised is the same model. */
static void model_key(const Scene *scene, int i, AutomapRow *row)
{
    const Instance *in;
    int k;

    in = &scene->models[i];
    if (g_state[i] & MODEL_MOVER) {
        for (k = 0; k < scene->nmovers; k++) {
            const Mover *m;

            m = &scene->movers[k];
            if (m->inst && m->model == i) {
                key_at(in->mesh->id, m->x0, m->y0, m->z0, row);
                return;
            }
        }
    }
    key_at(in->mesh->id, in->x, in->y, in->z, row);
}

/*
 * The states for scene, made again only when something they were made for
 * changed: a stream's chunk coming or going changes the scene's version.
 * A lookup a model, about 3000 in a dungeon's ring; nothing per frame.
 */
static int ensure(const Scene *scene)
{
    df_i32 ox;
    df_i32 oz;
    int i;

    if (!scene || scene->nmodels < 1 || !g_place) {
        return 0;
    }
    if (!stream_scene_origin(scene, &ox, &oz)) {
        ox = 0;
        oz = 0;
    }
    if (scene == g_cache_scene && scene->version == g_cache_version && scene->nmodels == g_cache_models
        && mem_epoch() == g_cache_epoch && g_place == g_cache_place && g_rows_serial == g_cache_serial
        && ox == g_ox && oz == g_oz) {
        return 1;
    }
    if (scene->nmodels > g_state_cap) {
        MemHeap *was;
        df_u8 *state;

        if (!g_heap) {
            g_heap = mem_heap_new();
            if (!g_heap) {
                return 0;
            }
        }
        was = mem_heap_use(g_heap);
        state = (df_u8 *)mem_alloc((df_u32)scene->nmodels);
        mem_heap_use(was);
        if (!state) {
            log_text(LOG_ERROR, "automap: no memory for the place's models");
            return 0;
        }
        if (g_state) {
            mem_heap_free(g_heap, g_state);
        }
        g_state = state;
        g_state_cap = scene->nmodels;
    }
    g_ox = ox;
    g_oz = oz;
    mem_zero(g_state, (df_u32)scene->nmodels);
    /* Movers first: their models take their start as their key. */
    for (i = 0; scene->movers && i < scene->nmovers; i++) {
        const Mover *m;

        m = &scene->movers[i];
        if (m->inst && m->model >= 0 && m->model < scene->nmodels && scene->models[m->model].mesh) {
            g_state[m->model] = MODEL_MOVER;
        }
    }
    for (i = 0; i < scene->nmodels; i++) {
        AutomapRow row;

        if (!scene->models[i].mesh) {
            g_state[i] = MODEL_NONE;
            continue;
        }
        model_key(scene, i, &row);
        g_state[i] = (df_u8)((g_state[i] & MODEL_MOVER) | (search(&row, g_n) >= 0 ? MODEL_SEEN : MODEL_UNSEEN));
    }
    g_cache_scene = scene;
    g_cache_version = scene->version;
    g_cache_models = scene->nmodels;
    g_cache_epoch = mem_epoch();
    g_cache_place = g_place;
    g_cache_serial = g_rows_serial;
    return 1;
}

/* Model i seen now: its row in, its state with it. */
static void mark(const Scene *scene, int i)
{
    AutomapRow row;

    if (MODEL_STATE(g_state[i]) != MODEL_UNSEEN) {
        return;
    }
    model_key(scene, i, &row);
    if (insert(&row)) {
        g_state[i] = (df_u8)((g_state[i] & MODEL_MOVER) | MODEL_SEEN);
    }
}

/* Model i's world box; 0 for an empty slot. */
static int model_box(const Scene *scene, int i, Bound *b)
{
    if (scene->bounds) {
        *b = scene->bounds[i];
        return b->ex >= 0.0f;
    }
    return instance_world_box(&scene->models[i], b);
}

/*
 * Squared distance from the eye to a box, height counted twice over: the
 * floor above or below is passed nearer than a room along the corridor.
 */
static float box_dist2(const Camera *cam, float cx, float cy, float cz, float ex, float ey, float ez, float height)
{
    float dx;
    float dy;
    float dz;

    dx = df_absf(cam->x - cx) - ex;
    dy = (df_absf(cam->y - cy) - ey) * height;
    dz = df_absf(cam->z - cz) - ez;
    dx = dx > 0.0f ? dx : 0.0f;
    dy = dy > 0.0f ? dy : 0.0f;
    dz = dz > 0.0f ? dz : 0.0f;
    return dx * dx + dy * dy + dz * dz;
}

/* Models first..end-1 the eye is within AUTOMAP_NEAR_RANGE of. */
static void mark_near(const Scene *scene, const Camera *cam, int first, int end)
{
    int i;

    for (i = first; i < end && i < scene->nmodels; i++) {
        Bound b;

        if (MODEL_STATE(g_state[i]) == MODEL_UNSEEN && model_box(scene, i, &b)
            && box_dist2(cam, b.cx, b.cy, b.cz, b.ex, b.ey, b.ez, 2.0f) <= AUTOMAP_NEAR_RANGE * AUTOMAP_NEAR_RANGE) {
            mark(scene, i);
        }
    }
}

void automap_frame(const Scene *scene, const Camera *cam, const int *drawn, int ndrawn)
{
    int k;

    if (!g_place || !cam || !ensure(scene)) {
        return;
    }
    /* Drawn and in range: seen. */
    for (k = 0; drawn && k < ndrawn; k++) {
        Bound b;
        int i;

        i = drawn[k];
        if (i >= 0 && i < scene->nmodels && MODEL_STATE(g_state[i]) == MODEL_UNSEEN && model_box(scene, i, &b)
            && box_dist2(cam, b.cx, b.cy, b.cz, b.ex, b.ey, b.ez, 1.0f) <= AUTOMAP_SEEN_RANGE * AUTOMAP_SEEN_RANGE) {
            mark(scene, i);
        }
    }
    /* Near, drawn or not (behind the eye, round a corner): a span's box first. */
    if (scene->spans && scene->nspans > 0) {
        for (k = 0; k < scene->nspans; k++) {
            const Span *sp;

            sp = &scene->spans[k];
            if (sp->count > 0 && box_dist2(cam, sp->cx, sp->cy, sp->cz, sp->ex, sp->ey, sp->ez, 2.0f)
                                     <= AUTOMAP_NEAR_RANGE * AUTOMAP_NEAR_RANGE) {
                mark_near(scene, cam, sp->first, sp->first + sp->count);
            }
        }
    } else {
        mark_near(scene, cam, 0, scene->nmodels);
    }
}

int automap_seen(const Scene *scene, int model)
{
    if (!ensure(scene) || model < 0 || model >= scene->nmodels) {
        return 0;
    }
    return MODEL_STATE(g_state[model]) == MODEL_SEEN;
}

/* Shell sort of the rows by key: no C library here, and a reveal adds thousands at once. */
static void sort_rows(void)
{
    int gap;

    for (gap = g_n / 2; gap > 0; gap /= 2) {
        int i;

        for (i = gap; i < g_n; i++) {
            AutomapRow row;
            int k;

            row = g_rows[i];
            for (k = i; k >= gap && row_cmp(&g_rows[k - gap], &row) > 0; k -= gap) {
                g_rows[k] = g_rows[k - gap];
            }
            g_rows[k] = row;
        }
    }
}

void automap_reveal(const Scene *scene)
{
    int sorted;
    int write;
    int i;

    if (!ensure(scene)) {
        return;
    }
    /* Appended unsorted, then sorted once: one insert a model moved every row after it. */
    sorted = g_n;
    for (i = 0; i < scene->nmodels; i++) {
        AutomapRow row;

        if (MODEL_STATE(g_state[i]) != MODEL_UNSEEN) {
            continue;
        }
        model_key(scene, i, &row);
        if (search(&row, sorted) < 0 && grow()) {
            g_rows[g_n++] = row;
        }
    }
    sort_rows();
    /* Two models with one key (the same mesh twice on one spot) leave one row. */
    write = 0;
    for (i = 0; i < g_n; i++) {
        if (write == 0 || row_cmp(&g_rows[write - 1], &g_rows[i]) != 0) {
            g_rows[write++] = g_rows[i];
        }
    }
    g_n = write;
    g_rows_serial++;
}

/* ---- Drawing: the seen faces as screen polygons ---- */

typedef struct V3 {
    float x, y, z;
} V3;

/* A view's projection, worked out once a call. */
typedef struct Proj {
    const AutomapView *view;
    float rcx, rcy;
    /* Top: the map's right and up across the floor. */
    float rx, rz, fx, fz;
    /* 3D: the camera's basis and focal length in pixels. */
    Basis basis;
    float focal;
} Proj;

/* From above, depth is how far below this a point is: past the slice's top, so always positive. */
#define TOP_EYE_ABOVE 64.0f
/* A 3D view's nearest distance, as the world's. */
#define VIEW_NEAR DF_NEAR

static void proj_init(Proj *p, const AutomapView *v)
{
    p->view = v;
    p->rcx = (v->x0 + v->x1) * 0.5f;
    p->rcy = (v->y0 + v->y1) * 0.5f;
    /* Camera.yaw's sense: 0 looks along +z, which is up on the map; right is +x. */
    p->rx = df_cos(v->yaw);
    p->rz = -df_sin(v->yaw);
    p->fx = df_sin(v->yaw);
    p->fz = df_cos(v->yaw);
    camera_basis(&v->cam, &p->basis);
    {
        float fov;
        float half;

        fov = v->fov < 10.0f ? 10.0f : (v->fov > 150.0f ? 150.0f : v->fov);
        half = fov * (DF_TAU / 720.0f);
        p->focal = (v->x1 - v->x0) * 0.5f * df_cos(half) / df_sin(half);
    }
}

/* Keeps the part where side * (y - level) >= 0, cutting from the inside end as the world's clips do. */
static int clip_y(const V3 *in, int n, V3 *out, float level, float side)
{
    float d[AUTOMAP_CORNERS];
    int m;
    int i;

    for (i = 0; i < n; i++) {
        d[i] = side * (in[i].y - level);
    }
    m = 0;
    for (i = 0; i < n && m < AUTOMAP_CORNERS - 1; i++) {
        int j;

        j = (i + 1) % n;
        if (d[i] >= 0.0f) {
            out[m++] = in[i];
        }
        if ((d[i] >= 0.0f) != (d[j] >= 0.0f)) {
            const V3 *a;
            const V3 *b;
            float t;

            a = d[i] >= 0.0f ? &in[i] : &in[j];
            b = d[i] >= 0.0f ? &in[j] : &in[i];
            t = (d[i] >= 0.0f ? d[i] : d[j]) / (df_absf(d[i]) + df_absf(d[j]));
            out[m].x = a->x + (b->x - a->x) * t;
            out[m].y = a->y + (b->y - a->y) * t;
            out[m].z = a->z + (b->z - a->z) * t;
            m++;
        }
    }
    return m;
}

/* Keeps the part in front of the camera, z >= VIEW_NEAR in view space. */
static int clip_near(const V3 *in, int n, V3 *out)
{
    float d[AUTOMAP_CORNERS];
    int m;
    int i;

    for (i = 0; i < n; i++) {
        d[i] = in[i].z - VIEW_NEAR;
    }
    m = 0;
    for (i = 0; i < n && m < AUTOMAP_CORNERS - 1; i++) {
        int j;

        j = (i + 1) % n;
        if (d[i] >= 0.0f) {
            out[m++] = in[i];
        }
        if ((d[i] >= 0.0f) != (d[j] >= 0.0f)) {
            const V3 *a;
            const V3 *b;
            float t;

            a = d[i] >= 0.0f ? &in[i] : &in[j];
            b = d[i] >= 0.0f ? &in[j] : &in[i];
            t = (d[i] >= 0.0f ? d[i] : d[j]) / (df_absf(d[i]) + df_absf(d[j]));
            out[m].x = a->x + (b->x - a->x) * t;
            out[m].y = a->y + (b->y - a->y) * t;
            out[m].z = a->z + (b->z - a->z) * t;
            m++;
        }
    }
    return m;
}

/* A corner's distance inside one edge of the rectangle: 0 left, 1 right, 2 bottom, 3 top. */
static float rect_side(const AutomapView *v, int edge, const AutomapCorner *c)
{
    switch (edge) {
    case 0:
        return c->x - v->x0;
    case 1:
        return v->x1 - c->x;
    case 2:
        return c->y - v->y0;
    default:
        return v->y1 - c->y;
    }
}

/* The polygon cut to the rectangle, edge by edge. Returns the corners left in c. */
static int clip_rect(const AutomapView *v, AutomapCorner *c, int n)
{
    AutomapCorner tmp[AUTOMAP_CORNERS];
    int edge;

    for (edge = 0; edge < 4 && n >= 3; edge++) {
        float d[AUTOMAP_CORNERS];
        int m;
        int i;

        for (i = 0; i < n; i++) {
            d[i] = rect_side(v, edge, &c[i]);
        }
        m = 0;
        for (i = 0; i < n && m < AUTOMAP_CORNERS - 1; i++) {
            int j;

            j = (i + 1) % n;
            if (d[i] >= 0.0f) {
                tmp[m++] = c[i];
            }
            if ((d[i] >= 0.0f) != (d[j] >= 0.0f)) {
                const AutomapCorner *a;
                const AutomapCorner *b;
                float t;

                a = d[i] >= 0.0f ? &c[i] : &c[j];
                b = d[i] >= 0.0f ? &c[j] : &c[i];
                t = (d[i] >= 0.0f ? d[i] : d[j]) / (df_absf(d[i]) + df_absf(d[j]));
                tmp[m].x = a->x + (b->x - a->x) * t;
                tmp[m].y = a->y + (b->y - a->y) * t;
                tmp[m].oow = a->oow + (b->oow - a->oow) * t;
                m++;
            }
        }
        for (i = 0; i < m; i++) {
            c[i] = tmp[i];
        }
        n = m;
    }
    return n;
}

/* A world point on the screen, unclipped; 0 behind a 3D view's camera. */
static int project(const Proj *p, float x, float y, float z, AutomapCorner *out)
{
    const AutomapView *v;

    v = p->view;
    if (v->mode == AUTOMAP_TOP) {
        float dx;
        float dz;

        dx = x - v->cx;
        dz = z - v->cz;
        out->x = p->rcx + (dx * p->rx + dz * p->rz) * v->scale;
        out->y = p->rcy + (dx * p->fx + dz * p->fz) * v->scale;
        out->oow = 1.0f / (v->top + TOP_EYE_ABOVE - y);
        return 1;
    }
    {
        float vx, vy, vz;

        camera_view(&v->cam, &p->basis, x, y, z, &vx, &vy, &vz);
        if (vz < VIEW_NEAR) {
            return 0;
        }
        out->oow = 1.0f / vz;
        out->x = p->rcx + vx * p->focal * out->oow;
        out->y = p->rcy + vy * p->focal * out->oow;
        return 1;
    }
}

/*
 * A face's colour: dark at the slice's bottom, light at its top, in the
 * blue-grey of classic's dungeon map. A 3D view also shades by the face's
 * turn to a light from above and the south-east, so walls read from floors.
 */
static df_u32 face_rgb(const AutomapView *v, float y, float nx, float ny, float nz)
{
    float t;
    float light;
    int r;
    int g;
    int b;

    t = v->top > v->bottom ? (y - v->bottom) / (v->top - v->bottom) : 1.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    light = 1.0f;
    if (v->mode == AUTOMAP_3D) {
        float d;

        d = nx * 0.35f + ny * 0.85f + nz * 0.40f;
        light = 0.5f + 0.5f * (d > 0.0f ? d : 0.0f);
    }
    r = df_iround((40.0f + 130.0f * t) * light);
    g = df_iround((48.0f + 130.0f * t) * light);
    b = df_iround((80.0f + 140.0f * t) * light);
    return ((df_u32)r << 16) | ((df_u32)g << 8) | (df_u32)b;
}

/* One face's world corners, cut, projected, cut to the rectangle and sent. 1 when sent. */
static int emit_face(const Proj *p, const V3 *world, int n, df_u32 rgb, AutomapEmit emit, void *ctx)
{
    V3 a[AUTOMAP_CORNERS];
    V3 b[AUTOMAP_CORNERS];
    AutomapCorner c[AUTOMAP_CORNERS];
    const AutomapView *v;
    int i;

    v = p->view;
    n = clip_y(world, n, a, v->top, -1.0f);
    n = n >= 3 ? clip_y(a, n, b, v->bottom, 1.0f) : 0;
    if (n < 3) {
        return 0;
    }
    if (v->mode == AUTOMAP_3D) {
        for (i = 0; i < n; i++) {
            camera_view(&v->cam, &p->basis, b[i].x, b[i].y, b[i].z, &a[i].x, &a[i].y, &a[i].z);
        }
        n = clip_near(a, n, b);
        for (i = 0; i < n; i++) {
            c[i].oow = 1.0f / b[i].z;
            c[i].x = p->rcx + b[i].x * p->focal * c[i].oow;
            c[i].y = p->rcy + b[i].y * p->focal * c[i].oow;
        }
    } else {
        for (i = 0; i < n; i++) {
            project(p, b[i].x, b[i].y, b[i].z, &c[i]);
        }
    }
    n = n >= 3 ? clip_rect(v, c, n) : 0;
    if (n < 3) {
        return 0;
    }
    emit(ctx, c, n, rgb);
    return 1;
}

/*
 * A face's corners from its triangles when they fan from its first corner,
 * as ARCH3D's loader makes them; 0 when they do not (each triangle then on
 * its own). At most AUTOMAP_CORNERS - 8, leaving room for the cuts.
 */
static int face_corners(const Instance *in, const Face *face, V3 *out)
{
    const Tri *tris;
    const Tri *t0;
    int t;
    int n;

    tris = in->mesh->tris;
    t0 = &tris[face->first];
    if (face->ntris + 2 > AUTOMAP_CORNERS - 8) {
        return 0;
    }
    for (t = face->first + 1; t < face->first + face->ntris; t++) {
        if (tris[t].x[0] != t0->x[0] || tris[t].y[0] != t0->y[0] || tris[t].z[0] != t0->z[0]
            || tris[t].x[1] != tris[t - 1].x[2] || tris[t].y[1] != tris[t - 1].y[2]
            || tris[t].z[1] != tris[t - 1].z[2]) {
            return 0;
        }
    }
    instance_point(in, t0->x[0], t0->y[0], t0->z[0], &out[0].x, &out[0].y, &out[0].z);
    instance_point(in, t0->x[1], t0->y[1], t0->z[1], &out[1].x, &out[1].y, &out[1].z);
    n = 2;
    for (t = face->first; t < face->first + face->ntris; t++) {
        instance_point(in, tris[t].x[2], tris[t].y[2], tris[t].z[2], &out[n].x, &out[n].y, &out[n].z);
        n++;
    }
    return n;
}

/* 1 when model i's box can reach the view at all: within the slice, and near the rectangle or in front. */
static int box_in_view(const Proj *p, const Scene *scene, int i)
{
    const AutomapView *v;
    AutomapCorner c;
    Bound b;

    v = p->view;
    if (!model_box(scene, i, &b) || b.cy + b.ey < v->bottom || b.cy - b.ey > v->top) {
        return 0;
    }
    if (v->mode == AUTOMAP_TOP) {
        float r;

        project(p, b.cx, b.cy, b.cz, &c);
        r = (b.ex + b.ez) * v->scale;
        return c.x + r >= v->x0 && c.x - r <= v->x1 && c.y + r >= v->y0 && c.y - r <= v->y1;
    }
    {
        float vx, vy, vz;

        camera_view(&v->cam, &p->basis, b.cx, b.cy, b.cz, &vx, &vy, &vz);
        return vz + b.ex + b.ey + b.ez >= VIEW_NEAR;
    }
}

int automap_polygons(const Scene *scene, const AutomapView *view, AutomapEmit emit, void *ctx)
{
    Proj p;
    int sent;
    int i;

    if (!view || !emit || !ensure(scene)) {
        return 0;
    }
    proj_init(&p, view);
    sent = 0;
    for (i = 0; i < scene->nmodels; i++) {
        const Instance *in;
        int f;

        if (MODEL_STATE(g_state[i]) != MODEL_SEEN || !box_in_view(&p, scene, i)) {
            continue;
        }
        in = &scene->models[i];
        if (!in->mesh->faces || !in->mesh->tris) {
            continue;
        }
        for (f = 0; f < in->mesh->nfaces; f++) {
            const Face *face;
            const Tri *t0;
            V3 corner[AUTOMAP_CORNERS];
            float nx, ny, nz, len, y;
            df_u32 rgb;
            int n;
            int k;

            face = &in->mesh->faces[f];
            if (face->ntris < 1 || face->first < 0 || face->first + face->ntris > in->mesh->ntris) {
                continue;
            }
            t0 = &in->mesh->tris[face->first];
            /* A face the world does not draw is not on the map either. */
            if (t0->slot == 0xffff) {
                continue;
            }
            nx = in->m[0] * face->nx + in->m[1] * face->ny + in->m[2] * face->nz;
            ny = in->m[3] * face->nx + in->m[4] * face->ny + in->m[5] * face->nz;
            nz = in->m[6] * face->nx + in->m[7] * face->ny + in->m[8] * face->nz;
            len = df_sqrt(nx * nx + ny * ny + nz * nz);
            if (len < 1.0e-6f) {
                continue;
            }
            nx /= len;
            ny /= len;
            nz /= len;
            /*
             * From above, the floors: a face turned up (the world's own
             * facing rule, seen from straight overhead). From a camera,
             * what the world would draw from there.
             */
            if (view->mode == AUTOMAP_TOP && ny <= 0.5f) {
                continue;
            }
            if (view->mode == AUTOMAP_3D
                && !instance_faces(in, &view->cam, &p.basis, face->nx, face->ny, face->nz, t0->x[0], t0->y[0], t0->z[0])) {
                continue;
            }
            n = face_corners(in, face, corner);
            if (n >= 3) {
                y = 0.0f;
                for (k = 0; k < n; k++) {
                    y += corner[k].y;
                }
                rgb = face_rgb(view, y / (float)n, nx, ny, nz);
                sent += emit_face(&p, corner, n, rgb, emit, ctx);
                continue;
            }
            /* Not a clean fan: each triangle on its own, in the face's colour. */
            for (k = face->first; k < face->first + face->ntris; k++) {
                const Tri *t;
                int c;

                t = &in->mesh->tris[k];
                for (c = 0; c < 3; c++) {
                    instance_point(in, t->x[c], t->y[c], t->z[c], &corner[c].x, &corner[c].y, &corner[c].z);
                }
                rgb = face_rgb(view, (corner[0].y + corner[1].y + corner[2].y) / 3.0f, nx, ny, nz);
                sent += emit_face(&p, corner, 3, rgb, emit, ctx);
            }
        }
    }
    return sent;
}

int automap_to_screen(const AutomapView *view, float x, float y, float z, float *sx, float *sy)
{
    Proj p;
    AutomapCorner c;

    if (!view) {
        return 0;
    }
    proj_init(&p, view);
    if (!project(&p, x, y, z, &c)) {
        return 0;
    }
    *sx = c.x;
    *sy = c.y;
    return c.x >= view->x0 && c.x <= view->x1 && c.y >= view->y0 && c.y <= view->y1;
}
