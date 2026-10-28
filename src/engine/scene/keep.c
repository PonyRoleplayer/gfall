#include "engine/scene/keep.h"
#include "engine/scene/interact.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"

/*
 * See keep.h and docs/interact.md. The rows in key order, binary
 * searched, in a heap of their own (never a place's): grown by doubling,
 * the old block freed.
 */

static Keep *g_rows;
static int g_n;
static int g_cap;
static MemHeap *g_heap;

static int key_cmp(KeepKey a, KeepKey b)
{
    if (a.place != b.place) {
        return a.place < b.place ? -1 : 1;
    }
    if (a.thing != b.thing) {
        return a.thing < b.thing ? -1 : 1;
    }
    return 0;
}

static int keyed(KeepKey k)
{
    return k.place != 0 || k.thing != 0;
}

/* The row's index, or where it would go (as -1 - index). */
static int search(KeepKey key)
{
    int lo;
    int hi;

    lo = 0;
    hi = g_n - 1;
    while (lo <= hi) {
        int mid;
        int c;

        mid = (lo + hi) / 2;
        c = key_cmp(g_rows[mid].key, key);
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

static int grow(void)
{
    MemHeap *was;
    Keep *rows;
    int cap;

    if (!g_heap) {
        g_heap = mem_heap_new();
        if (!g_heap) {
            return 0;
        }
    }
    cap = g_cap ? g_cap * 2 : 256;
    was = mem_heap_use(g_heap);
    rows = (Keep *)mem_alloc((df_u32)cap * (df_u32)sizeof(Keep));
    mem_heap_use(was);
    if (!rows) {
        log_text(LOG_ERROR, "keep: no memory for more rows");
        return 0;
    }
    if (g_rows) {
        mem_copy(rows, g_rows, (df_u32)g_n * (df_u32)sizeof(Keep));
        mem_heap_free(g_heap, g_rows);
    }
    g_rows = rows;
    g_cap = cap;
    return 1;
}

Keep *keep_find(KeepKey key)
{
    int i;

    i = search(key);
    return i >= 0 ? &g_rows[i] : 0;
}

Keep *keep_get(KeepKey key, int kind)
{
    int i;
    int k;
    Keep *r;

    i = search(key);
    if (i >= 0) {
        return &g_rows[i];
    }
    if (g_n >= g_cap && !grow()) {
        return 0;
    }
    i = -1 - i;
    for (k = g_n; k > i; k--) {
        g_rows[k] = g_rows[k - 1];
    }
    g_n++;
    r = &g_rows[i];
    mem_zero(r, sizeof(Keep));
    r->key = key;
    r->kind = (df_u8)kind;
    r->live = 1;
    return r;
}

static void drop(int i)
{
    for (; i + 1 < g_n; i++) {
        g_rows[i] = g_rows[i + 1];
    }
    g_n--;
}

static int game_set(const Keep *r)
{
    int i;

    for (i = 0; i < KEEP_GAME_BYTES && !r->game[i]; i++) {
    }
    return i < KEEP_GAME_BYTES;
}

int keep_count(void)
{
    return g_n;
}

const Keep *keep_at(int i)
{
    return i >= 0 && i < g_n ? &g_rows[i] : 0;
}

int keep_put(const Keep *row)
{
    Keep *r;

    if (!row || !keyed(row->key)) {
        return 0;
    }
    r = keep_get(row->key, row->kind);
    if (!r) {
        return 0;
    }
    *r = *row;
    return 1;
}

void keep_clear(void)
{
    g_n = 0;
}

static float df_abs_f(float v)
{
    return v < 0.0f ? -v : v;
}

void keep_save(Scene *scene, int first_mover, int nmovers, const Entity *ents, int nents,
               float ox, float oy, float oz)
{
    int i;

    for (i = first_mover; scene && scene->movers && i < first_mover + nmovers && i < scene->nmovers; i++) {
        const Mover *m;
        int at_end;
        int at;

        m = &scene->movers[i];
        if (!keyed(m->key) || m->teleport) {
            continue;
        }
        at_end = mover_at_end(m);
        at = search(m->key);
        if (!at_end && m->lock == m->lock0) {
            /* As its file has it: no row, unless the game keeps something of its own. */
            if (at >= 0 && !game_set(&g_rows[at])) {
                drop(at);
            } else if (at >= 0) {
                g_rows[at].at_end = 0;
                g_rows[at].lock = m->lock;
            }
            continue;
        }
        {
            Keep *r;

            r = keep_get(m->key, KEEP_MOVER);
            if (!r) {
                return;
            }
            r->at_end = (df_u8)at_end;
            r->lock = m->lock;
        }
    }
    for (i = 0; ents && i < nents; i++) {
        const Entity *e;
        float px, py, pz;
        int same;
        int at;

        e = &ents[i];
        if (!keyed(e->key)) {
            continue;
        }
        px = e->x + ox;
        py = e->y + oy;
        pz = e->z + oz;
        /* Without a home noted (no keep_restore at its build), it is kept as it is. */
        same = e->homed && e->live && !e->act && !e->hold
            && df_abs_f(px - e->home[0]) <= 1.0f && df_abs_f(py - e->home[1]) <= 1.0f
            && df_abs_f(pz - e->home[2]) <= 1.0f;
        at = search(e->key);
        if (same) {
            /* Home as its file has it: no row, or the game's bytes alone. */
            if (at >= 0 && !game_set(&g_rows[at])) {
                drop(at);
            } else if (at >= 0) {
                g_rows[at].placed = 0;
            }
            continue;
        }
        {
            Keep *r;

            r = keep_get(e->key, KEEP_ENTITY);
            if (!r) {
                return;
            }
            r->x = px;
            r->y = py;
            r->z = pz;
            r->yaw = e->yaw;
            r->live = e->live;
            r->moving = e->moving;
            r->act = e->act;
            r->hold = e->hold;
            r->frame = e->frame;
            r->placed = 1;
        }
    }
}

void keep_restore(Scene *scene, int first_mover, int nmovers, Entity *ents, int nents,
                  float ox, float oy, float oz)
{
    int i;

    for (i = first_mover; scene && scene->movers && i < first_mover + nmovers && i < scene->nmovers; i++) {
        Mover *m;
        const Keep *r;

        m = &scene->movers[i];
        if (!keyed(m->key) || m->teleport) {
            continue;
        }
        r = keep_find(m->key);
        if (!r || r->kind != KEEP_MOVER) {
            continue;
        }
        m->lock = r->lock;
        mover_set_end(m, r->at_end);
    }
    for (i = 0; ents && i < nents; i++) {
        Entity *e;
        const Keep *r;

        e = &ents[i];
        /* Where the file put it: what keep_save compares with. */
        e->home[0] = e->x + ox;
        e->home[1] = e->y + oy;
        e->home[2] = e->z + oz;
        e->homed = 1;
        if (!keyed(e->key)) {
            continue;
        }
        r = keep_find(e->key);
        if (!r || r->kind != KEEP_ENTITY) {
            continue;
        }
        /* A row the game made for its bytes alone has no place written: it stands at home. */
        if (r->placed) {
            e->x = r->x - ox;
            e->y = r->y - oy;
            e->z = r->z - oz;
            e->yaw = r->yaw;
            e->live = r->live;
            e->moving = r->moving;
            e->act = r->act;
            e->hold = r->hold;
            e->frame = r->frame;
        }
    }
}
