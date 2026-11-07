#include "game/dagger/play/conjure.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/places/dunstream.h"
#include "game/dagger/tables/enemies.h"
#include "engine/scene/spawn.h"
#include "engine/scene/stream.h"
#include "engine/core/log.h"

/* See conjure.h. */

/* Where a creature is looked for: 4 to 16 m off across the ground, its floor within 3 m up or down (DFU's FoeSpawner spawns out of sight a few metres off). */
#define CONJURE_INNER (4.0f * RULES_UNITS_PER_M)
#define CONJURE_OUTER (16.0f * RULES_UNITS_PER_M)
#define CONJURE_CLIMB (3.0f * RULES_UNITS_PER_M)
/* Freed when the player is this far off across the ground: it does not wander the dungeon for good. */
#define CONJURE_LEASH (64.0f * RULES_UNITS_PER_M)

static const Scene *g_scene;
static const Camera *g_cam;

void conjure_use(const Scene *scene, const Camera *cam)
{
    g_scene = scene;
    g_cam = cam;
}

Entity *conjure_rest_foe(const float *feet, int level, unsigned seed)
{
    Entity e;
    SpawnSpot want;
    SpawnId id;
    Entity *made;
    int kind;
    float x;
    float y;
    float z;
    float h;

    if (!g_scene || !g_cam || !dunstream_active()) {
        return 0;
    }
    kind = enemy_encounter(dunstream_dungeon_type(), level, (df_u32)seed);
    /* A water dweller needs water: an interrupted rest on dry floor brings none of those. */
    if (kind < 0 || enemy_aquatic(kind) || !dunstream_look(&e, kind, 0)) {
        return 0;
    }
    h = (float)entity_pose(&e)->h[0];
    want.x = feet[0];
    want.y = feet[1];
    want.z = feet[2];
    want.inner = CONJURE_INNER;
    want.outer = CONJURE_OUTER;
    want.radius = (float)entity_pose(&e)->w[0] * 0.25f;
    want.radius = want.radius < 8.0f ? 8.0f : (want.radius > 24.0f ? 24.0f : want.radius);
    want.height = h < 16.0f ? 16.0f : h;
    want.climb = CONJURE_CLIMB;
    want.flags = SPAWN_UNSEEN | SPAWN_DRY;
    want.seed = (df_u32)seed;
    if (!spawn_find(g_scene, g_cam, &want, &x, &y, &z)) {
        log_text(LOG_INFO, "conjure: no room out of sight for the rest's creature");
        return 0;
    }
    e.x = x;
    e.y = y;
    e.z = z;
    /* A flyer hangs at its middle's height off the floor, as a block's do. */
    if (enemy_flies(kind) || enemy_spectral(kind)) {
        e.y += h * 0.5f;
    }
    e.yaw = 0.0f;
    if (!dunstream_conjured_key(kind, 0, &e.key)) {
        return 0;
    }
    e.live = 1;
    id = spawn_add(g_scene, &e, CONJURE_LEASH);
    made = id ? spawn_get(id) : 0;
    if (made) {
        /* It came for the sleeper: awake and hunting at once. */
        foes_alert(made);
    }
    return made;
}

int creature_groups(void)
{
    return stream_slots() + 1;
}

Entity *creature_group(int g, int *n)
{
    int first;

    *n = 0;
    if (g < stream_slots()) {
        return stream_slot_entities(g, &first, n);
    }
    if (g == stream_slots() && g_scene) {
        /* The pool's own array (spawn_get hands out the same entries to move): the game's to change. */
        return (Entity *)spawn_entities(g_scene, n);
    }
    return 0;
}
