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
}

void ent_frame(const Scene *scene, const Camera *cam)
{
    int i;

    /* The frame's place is the registry's: its entries are named from now on. */
    ent_place(scene);
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
