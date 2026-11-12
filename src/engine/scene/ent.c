#include "engine/scene/ent.h"
#include "engine/core/mem.h"

/*
 * See ent.h and docs/entities.md, The registry. Nothing is allocated:
 * the pool, the ranges' table and the generations are static. Index
 * space, one id each:
 *
 *   0 .. ENT_POOL - 1                      the pool
 *   ENT_POOL .. + ENT_SCENE_MAX - 1        Scene.entities' entries
 *   then ENT_RANGE_MAX for each range slot  a game's ranges
 */

#define SCENE_BASE ENT_POOL
#define RANGE_BASE (ENT_POOL + ENT_SCENE_MAX)

typedef struct Range {
    Entity *first;
    int n;
    df_u8 used;
    df_u8 residence;
} Range;

static Entity g_pool[ENT_POOL];
static df_u8 g_used[ENT_POOL];
static int g_count;
static Range g_range[ENT_RANGES];
/* Each index's generation: an id that does not carry it names something gone. */
static df_u8 g_gen[ENT_IDS];
/* The place the pool and the ranges are in: its scene and its heap's epoch. 0 with none. */
static const Scene *g_scene;
static df_u32 g_epoch;

/*
 * The near-gather (ent_near_solid): every entity with a radius, by the
 * cell of the ground its feet stood in when the grid was built, hashed
 * into buckets. Built lazily, once a frame (g_stamp moves in ent_frame)
 * or when what is held changed (g_dirty). Between builds an entity may
 * move; a query reaches GRID_PAD further, so one that moved less than
 * that since the build is still found. Before the first ent_frame there
 * is no grid and the callers walk everything, as host tests step bodies
 * with no frames.
 */
#define GRID_CELL 512.0f
#define GRID_BUCKETS 256
#define GRID_PAD 256.0f
/* A query spanning more cells than this across walks everything instead. */
#define GRID_SPAN 6
/* Fewer entities with a radius than this: walking them all is as cheap, and never stale. */
#define GRID_MIN 32

static const Entity *g_grid_ent[ENT_IDS];
static int g_grid_cx[ENT_IDS];
static int g_grid_cz[ENT_IDS];
static short g_grid_next[ENT_IDS];
static short g_bucket[GRID_BUCKETS];
static int g_grid_n;
static float g_grid_radius;
static const Scene *g_grid_scene;
/* The scene's array the grid was built from: another array, or another length, builds it again. */
static const Entity *g_grid_array;
static int g_grid_count;
/* 1: too few to be worth a grid; the callers walk everything. */
static int g_grid_small;
static df_u32 g_stamp;
static df_u32 g_built;
static int g_dirty;

static EntId make_id(int index)
{
    return (EntId)(index + 1) | ((EntId)g_gen[index] << 16);
}

static void bump(int first, int n)
{
    int i;

    for (i = first; i < first + n && i < ENT_IDS; i++) {
        g_gen[i]++;
    }
}

static void free_entry(int i)
{
    if (g_used[i]) {
        g_count--;
    }
    g_used[i] = 0;
    /* Zeroed: not live, so the walkers skip it like a free entry of Scene.entities. */
    mem_zero(&g_pool[i], sizeof(Entity));
}

void ent_clear(void)
{
    int i;

    for (i = 0; i < ENT_POOL; i++) {
        free_entry(i);
    }
    for (i = 0; i < ENT_RANGES; i++) {
        g_range[i].used = 0;
    }
    g_count = 0;
    g_dirty = 1;
    /* A new place: no grid until its first frame (ent_frame). */
    g_stamp = 0;
    /* Every id of the place reads as gone: the pool's, the scene's entries', the ranges'. */
    bump(0, ENT_IDS);
    g_scene = 0;
}

/* Drop the place when its heap is gone. */
static void check_epoch(void)
{
    if (g_scene && g_epoch != mem_epoch()) {
        ent_clear();
    }
}

/* 1 when scene is the registry's place, still standing. */
static int in_place(const Scene *scene)
{
    check_epoch();
    return g_scene && scene == g_scene;
}

/* scene becomes the registry's place, what stood in another one gone. */
static void enter(const Scene *scene)
{
    if (!in_place(scene)) {
        ent_clear();
        g_scene = scene;
        g_epoch = mem_epoch();
    }
}

void ent_place(const Scene *scene)
{
    if (scene) {
        enter(scene);
    }
}

int ent_lists(const Scene *scene, EntList *out, int cap)
{
    int n;
    int i;

    n = 0;
    if (!scene || cap < 1) {
        return 0;
    }
    if (scene->entities && scene->nentities > 0) {
        out[n].first = scene->entities;
        out[n].n = scene->nentities;
        n++;
    }
    if (!in_place(scene)) {
        return n;
    }
    if (g_count > 0 && n < cap) {
        out[n].first = g_pool;
        out[n].n = ENT_POOL;
        n++;
    }
    for (i = 0; i < ENT_RANGES && n < cap; i++) {
        if (g_range[i].used) {
            out[n].first = g_range[i].first;
            out[n].n = g_range[i].n;
            n++;
        }
    }
    return n;
}

int ent_register_range(const Scene *scene, Entity *first, int n, int residence)
{
    int i;

    if (!scene || !first || n < 1) {
        return 0;
    }
    enter(scene);
    for (i = 0; i < ENT_RANGES && g_range[i].used; i++) {
    }
    if (i == ENT_RANGES) {
        return 0;
    }
    g_range[i].first = first;
    g_range[i].n = n < ENT_RANGE_MAX ? n : ENT_RANGE_MAX;
    g_range[i].used = 1;
    g_range[i].residence = (df_u8)residence;
    bump(RANGE_BASE + i * ENT_RANGE_MAX, ENT_RANGE_MAX);
    g_dirty = 1;
    return 1;
}

void ent_unregister_range(Entity *first)
{
    int i;

    check_epoch();
    for (i = 0; i < ENT_RANGES; i++) {
        if (g_range[i].used && g_range[i].first == first) {
            g_range[i].used = 0;
            bump(RANGE_BASE + i * ENT_RANGE_MAX, ENT_RANGE_MAX);
            g_dirty = 1;
        }
    }
}

EntId ent_add(const Scene *scene, const Entity *e)
{
    int i;

    if (!scene || !e) {
        return 0;
    }
    enter(scene);
    for (i = 0; i < ENT_POOL && g_used[i]; i++) {
    }
    if (i == ENT_POOL) {
        return 0;
    }
    g_pool[i] = *e;
    if (g_pool[i].leash < 0.0f) {
        g_pool[i].leash = 0.0f;
    }
    g_used[i] = 1;
    g_gen[i]++;
    g_count++;
    g_dirty = 1;
    return make_id(i);
}

/* The entity at index while it is still the one id names; 0 when gone. */
static Entity *at(EntId id, int *index)
{
    int i;

    check_epoch();
    i = (int)(id & 0xffffu) - 1;
    if (i < 0 || i >= ENT_IDS || g_gen[i] != (df_u8)(id >> 16)) {
        return 0;
    }
    *index = i;
    if (i < SCENE_BASE) {
        return g_used[i] ? &g_pool[i] : 0;
    }
    if (i < RANGE_BASE) {
        i -= SCENE_BASE;
        /* The scene's array is its builder's; the game owns its entities, so they are handed out to change. */
        return g_scene && g_scene->entities && i < g_scene->nentities ? (Entity *)&g_scene->entities[i] : 0;
    }
    i -= RANGE_BASE;
    if (!g_range[i / ENT_RANGE_MAX].used || i % ENT_RANGE_MAX >= g_range[i / ENT_RANGE_MAX].n) {
        return 0;
    }
    return &g_range[i / ENT_RANGE_MAX].first[i % ENT_RANGE_MAX];
}

Entity *ent_get(EntId id)
{
    int i;

    return at(id, &i);
}

void ent_remove(EntId id)
{
    int i;

    if (at(id, &i) && i < SCENE_BASE) {
        free_entry(i);
    }
}

int ent_count(void)
{
    check_epoch();
    return g_count;
}

int ent_next(int i, EntId *id)
{
    check_epoch();
    for (; i < ENT_IDS; i++) {
        const Entity *e;
        int k;

        if (i < SCENE_BASE) {
            if (g_used[i]) {
                break;
            }
            continue;
        }
        if (i < RANGE_BASE) {
            k = i - SCENE_BASE;
            if (!g_scene || !g_scene->entities || k >= g_scene->nentities) {
                /* Past the scene's array: on to the ranges. */
                i = RANGE_BASE - 1;
                continue;
            }
            if (g_scene->entities[k].live) {
                break;
            }
            continue;
        }
        k = i - RANGE_BASE;
        if (!g_range[k / ENT_RANGE_MAX].used || k % ENT_RANGE_MAX >= g_range[k / ENT_RANGE_MAX].n) {
            /* The rest of this range slot holds nothing. */
            i = RANGE_BASE + (k / ENT_RANGE_MAX + 1) * ENT_RANGE_MAX - 1;
            continue;
        }
        e = &g_range[k / ENT_RANGE_MAX].first[k % ENT_RANGE_MAX];
        if (e->live) {
            break;
        }
    }
    if (i >= ENT_IDS) {
        return 0;
    }
    *id = make_id(i);
    return i + 1;
}

EntId ent_id(const Entity *e)
{
    int i;

    check_epoch();
    if (!e) {
        return 0;
    }
    if (e >= g_pool && e < g_pool + ENT_POOL) {
        i = (int)(e - g_pool);
        return g_used[i] ? make_id(i) : 0;
    }
    if (g_scene && g_scene->entities && e >= g_scene->entities
        && e < g_scene->entities + g_scene->nentities) {
        i = (int)(e - g_scene->entities);
        return i < ENT_SCENE_MAX ? make_id(SCENE_BASE + i) : 0;
    }
    for (i = 0; i < ENT_RANGES; i++) {
        if (g_range[i].used && e >= g_range[i].first && e < g_range[i].first + g_range[i].n) {
            return make_id(RANGE_BASE + i * ENT_RANGE_MAX + (int)(e - g_range[i].first));
        }
    }
    return 0;
}

EntId ent_by_key(KeepKey key)
{
    int i;

    if (key.place == 0 && key.thing == 0) {
        return 0;
    }
    /* Every entry that holds something, live or not: a creature whose pictures are not read yet is still that creature. */
    check_epoch();
    for (i = 0; i < ENT_POOL; i++) {
        if (g_used[i] && g_pool[i].key.place == key.place && g_pool[i].key.thing == key.thing) {
            return make_id(i);
        }
    }
    if (g_scene && g_scene->entities) {
        for (i = 0; i < g_scene->nentities && i < ENT_SCENE_MAX; i++) {
            const Entity *e;

            e = &g_scene->entities[i];
            if (e->key.place == key.place && e->key.thing == key.thing) {
                return make_id(SCENE_BASE + i);
            }
        }
    }
    for (i = 0; i < ENT_RANGES; i++) {
        int k;

        for (k = 0; g_range[i].used && k < g_range[i].n; k++) {
            if (g_range[i].first[k].key.place == key.place && g_range[i].first[k].key.thing == key.thing) {
                return make_id(RANGE_BASE + i * ENT_RANGE_MAX + k);
            }
        }
    }
    return 0;
}

void ent_renew(const Scene *scene, int first, int n)
{
    if (!scene || first < 0 || n < 1) {
        return;
    }
    /* Names only matter in the registry's place; another scene's entries get theirs when it is entered. */
    if (in_place(scene) && first < ENT_SCENE_MAX) {
        bump(SCENE_BASE + first, first + n <= ENT_SCENE_MAX ? n : ENT_SCENE_MAX - first);
    }
    g_dirty = 1;
}

void ent_frame(const Scene *scene, const Camera *cam)
{
    int i;

    /* The frame's place is the registry's: its entries are named from now on. */
    ent_place(scene);
    /* The near-gather is built again at its first query this frame. */
    g_stamp++;
    if (g_stamp == 0) {
        g_stamp = 1;
    }
    if (!cam || g_count == 0) {
        return;
    }
    for (i = 0; i < ENT_POOL; i++) {
        float dx, dz;

        if (!g_used[i] || g_pool[i].leash <= 0.0f) {
            continue;
        }
        dx = g_pool[i].x - cam->x;
        dz = g_pool[i].z - cam->z;
        if (dx * dx + dz * dz > g_pool[i].leash * g_pool[i].leash) {
            free_entry(i);
        }
    }
}

void ent_shift(const Scene *scene, float dx, float dz)
{
    int i;

    if (!in_place(scene)) {
        return;
    }
    for (i = 0; i < ENT_POOL; i++) {
        if (g_used[i]) {
            g_pool[i].x -= dx;
            g_pool[i].z -= dz;
        }
    }
}

Entity *ent_pool(const Scene *scene, int *n)
{
    if (!in_place(scene) || g_count == 0) {
        *n = 0;
        return 0;
    }
    *n = ENT_POOL;
    return g_pool;
}

/* The bucket of a ground cell. Unsigned sums: a wrap only moves the bucket. */
static int bucket_of(int cx, int cz)
{
    return (int)(((df_u32)cx * 73856093u ^ (df_u32)cz * 19349663u) & (GRID_BUCKETS - 1));
}

static void grid_build(const Scene *scene)
{
    EntList lists[ENT_LISTS];
    int nlists;
    int li;
    int i;

    for (i = 0; i < GRID_BUCKETS; i++) {
        g_bucket[i] = -1;
    }
    g_grid_n = 0;
    g_grid_radius = 0.0f;
    nlists = ent_lists(scene, lists, ENT_LISTS);
    for (li = 0; li < nlists; li++) {
        for (i = 0; i < lists[li].n; i++) {
            const Entity *e;
            int b;

            e = &lists[li].first[i];
            /* Every entity with a radius, live or not: one becoming solid before the next build is still found. */
            if (e->radius <= 0.0f) {
                continue;
            }
            if (g_grid_n >= ENT_IDS) {
                /* More than the grid holds: no grid this frame, the callers walk everything. */
                g_grid_scene = 0;
                return;
            }
            g_grid_ent[g_grid_n] = e;
            g_grid_cx[g_grid_n] = df_ifloor(e->x / GRID_CELL);
            g_grid_cz[g_grid_n] = df_ifloor(e->z / GRID_CELL);
            b = bucket_of(g_grid_cx[g_grid_n], g_grid_cz[g_grid_n]);
            g_grid_next[g_grid_n] = g_bucket[b];
            g_bucket[b] = (short)g_grid_n;
            if (e->radius > g_grid_radius) {
                g_grid_radius = e->radius;
            }
            g_grid_n++;
        }
    }
    g_grid_scene = scene;
    g_grid_array = scene->entities;
    g_grid_count = scene->nentities;
    g_grid_small = g_grid_n < GRID_MIN;
}

int ent_near_solid(const Scene *scene, float x, float z, float r, const Entity **out, int cap)
{
    float reach;
    int x0, x1, z0, z1;
    int cx, cz;
    int n;

    if (!scene || g_stamp == 0) {
        return -1;
    }
    check_epoch();
    if (g_built != g_stamp || g_dirty || g_grid_scene != scene || g_grid_array != scene->entities
        || g_grid_count != scene->nentities) {
        grid_build(scene);
        g_built = g_stamp;
        g_dirty = 0;
    }
    if (!g_grid_scene || g_grid_small) {
        return -1;
    }
    reach = r + g_grid_radius + GRID_PAD;
    x0 = df_ifloor((x - reach) / GRID_CELL);
    x1 = df_ifloor((x + reach) / GRID_CELL);
    z0 = df_ifloor((z - reach) / GRID_CELL);
    z1 = df_ifloor((z + reach) / GRID_CELL);
    if (x1 - x0 >= GRID_SPAN || z1 - z0 >= GRID_SPAN) {
        return -1;
    }
    n = 0;
    for (cz = z0; cz <= z1; cz++) {
        for (cx = x0; cx <= x1; cx++) {
            int k;

            for (k = g_bucket[bucket_of(cx, cz)]; k >= 0; k = g_grid_next[k]) {
                /* Another cell sharing the bucket is that cell's query's, not this one's: each entity once. */
                if (g_grid_cx[k] != cx || g_grid_cz[k] != cz) {
                    continue;
                }
                if (n >= cap) {
                    return -1;
                }
                out[n++] = g_grid_ent[k];
            }
        }
    }
    return n;
}

void ent_regather(void)
{
    g_dirty = 1;
}

/* --- kept entities the game made ------------------------------------ */

static EntityReviveHook g_revive;

void ent_set_revive_hook(EntityReviveHook fn)
{
    g_revive = fn;
}

/* 1 when pool entry i is a kept entity the game made: ENT_KEPT, a key. */
static int kept_made(int i)
{
    const Entity *e;

    e = &g_pool[i];
    return g_used[i] && (e->flags & ENT_KEPT) && (e->key.place != 0 || e->key.thing != 0);
}

/* Pool entry i's row: the whole entity, filed by made (1 a chunk, 2 the place). 0 with no memory. */
static int write_row(int i, int made, int cx, int cz, float ox, float oy, float oz)
{
    const Entity *e;
    Keep *row;

    e = &g_pool[i];
    row = keep_get(e->key, KEEP_ENTITY);
    if (!row) {
        return 0;
    }
    /* Its game bytes are the game's, kept as they are. */
    row->x = e->x + ox;
    row->y = e->y + oy;
    row->z = e->z + oz;
    row->yaw = e->yaw;
    row->dead = e->dead;
    row->moving = e->moving;
    row->act = e->act;
    row->hold = e->hold;
    row->frame = e->frame;
    row->placed = 1;
    row->made = (df_u8)made;
    row->residence = e->residence;
    row->flags = e->flags;
    row->owner = e->owner;
    row->cx = cx;
    row->cz = cz;
    return 1;
}

void ent_chunk_dropped(const Scene *scene, int cx, int cz, int chunk, float ox, float oz)
{
    int i;

    if (chunk <= 0 || !in_place(scene)) {
        return;
    }
    for (i = 0; i < ENT_POOL; i++) {
        if (!kept_made(i) || g_pool[i].residence == ENT_HOME_PLACE) {
            continue;
        }
        if (df_ifloor((g_pool[i].x + ox) / (float)chunk) != cx || df_ifloor((g_pool[i].z + oz) / (float)chunk) != cz) {
            continue;
        }
        /* Filed under the chunk it stands in, and gone with it: no floor is under it any more. */
        if (write_row(i, 1, cx, cz, ox, 0.0f, oz)) {
            free_entry(i);
            g_dirty = 1;
        }
    }
}

void ent_keep_now(const Scene *scene, int chunk, float ox, float oy, float oz)
{
    int i;

    if (!in_place(scene)) {
        return;
    }
    for (i = 0; i < ENT_POOL; i++) {
        if (!kept_made(i)) {
            continue;
        }
        if (chunk > 0 && g_pool[i].residence != ENT_HOME_PLACE) {
            write_row(i, 1, df_ifloor((g_pool[i].x + ox) / (float)chunk), df_ifloor((g_pool[i].z + oz) / (float)chunk),
                      ox, oy, oz);
        } else {
            write_row(i, 2, 0, 0, ox, oy, oz);
        }
    }
}

/* Every row filed as made (and, for a chunk, under cx, cz) offered to the hook, and put back as it was. */
static void revive(const Scene *scene, int made, int cx, int cz, float ox, float oy, float oz)
{
    int n;
    int i;

    if (!scene || !g_revive) {
        return;
    }
    enter(scene);
    n = keep_count();
    for (i = 0; i < n; i++) {
        const Keep *row;
        Entity e;

        row = keep_at(i);
        if (row->kind != KEEP_ENTITY || row->made != made || (made == 1 && (row->cx != cx || row->cz != cz))) {
            continue;
        }
        /* Already standing here (it never left, or came back another way): not twice. */
        if (ent_by_key(row->key)) {
            continue;
        }
        entity_init(&e);
        if (!g_revive(row, &e)) {
            continue;
        }
        e.key = row->key;
        e.x = row->x - ox;
        e.y = row->y - oy;
        e.z = row->z - oz;
        e.yaw = row->yaw;
        e.dead = row->dead;
        e.moving = row->moving;
        e.act = row->act;
        e.hold = row->hold;
        e.frame = row->frame;
        e.flags = row->flags | ENT_KEPT;
        e.owner = row->owner;
        e.residence = row->residence;
        if (!ent_add(scene, &e)) {
            /* The pool is full: it stays in the table, and comes back the next time its chunk fills. */
            continue;
        }
    }
}

void ent_chunk_filled(const Scene *scene, int cx, int cz, int chunk, float ox, float oz)
{
    if (chunk > 0) {
        revive(scene, 1, cx, cz, ox, 0.0f, oz);
    }
}

void ent_revive_place(const Scene *scene, float ox, float oy, float oz)
{
    revive(scene, 2, 0, 0, ox, oy, oz);
}
