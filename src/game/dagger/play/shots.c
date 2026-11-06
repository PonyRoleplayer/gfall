#include "game/dagger/play/shots.h"
#include "game/dagger/play/combat.h"

/* See shots.h and docs/game.md. */

/* The bow each arrow in the air left, by its entry's number (the projectile's tag). */
static Item g_bow[PROJ_MAX];

/* The projectile's questions, answered by combat's. */
typedef struct ShotsWorld {
    const Scene *scene;
} ShotsWorld;

static int world_wall(void *user, const float *a, const float *b, float *t)
{
    const ShotsWorld *w;

    w = (const ShotsWorld *)user;
    return combat_ray(w->scene, a, b, t);
}

/* Only the player's arrows are flown so far, and they strike the creatures. */
static void *world_target(void *user, const Projectile *p, const float *a, const float *dir, float len, float *t)
{
    const ShotsWorld *w;

    w = (const ShotsWorld *)user;
    if (p->from != PROJ_FROM_PLAYER) {
        return 0;
    }
    return combat_aim_at(w->scene, a, dir, len, t);
}

void shots_clear(void)
{
    projectile_clear();
}

int shots_arrow(const float *eye, const float *dir, const Item *bow)
{
    ProjectileShot shot;
    int i;
    int k;

    shot.kind = PROJ_ARROW;
    shot.from = PROJ_FROM_PLAYER;
    shot.speed = PROJ_ARROW_SPEED;
    shot.gravity = 0.0f;
    shot.range = PROJ_ARROW_RANGE;
    for (k = 0; k < 3; k++) {
        shot.at[k] = eye[k];
        shot.dir[k] = dir[k];
    }
    shot.tag = 0;
    i = projectile_launch(&shot);
    if (i < 0) {
        return 0;
    }
    g_bow[i] = *bow;
    return 1;
}

/* The bow an arrow struck with: the one in hand if it is still the kind it was loosed from, else the one it left. */
static Item *bow_for(Inventory *pack, int entry)
{
    Item *hand;

    hand = inventory_hand(pack);
    if (hand && hand->template_id == g_bow[entry].template_id) {
        return hand;
    }
    return &g_bow[entry];
}

int shots_frame(float dt, const Scene *scene, Character *pc, Inventory *pack, const float *feet)
{
    ProjectileEnd ends[PROJ_MAX];
    ProjectileWorld world;
    ShotsWorld mine;
    int i;
    int n;

    if (projectile_count() == 0) {
        return 0;
    }
    mine.scene = scene;
    world.user = &mine;
    world.wall = world_wall;
    world.target = world_target;
    n = projectile_step(dt, &world, ends, PROJ_MAX);
    for (i = 0; i < n; i++) {
        if (ends[i].kind == PROJ_ARROW && ends[i].result == PROJ_HIT_TARGET && ends[i].target) {
            combat_missile_strike(pc, bow_for(pack, ends[i].entry), (Entity *)ends[i].target, feet);
        }
    }
    return n;
}
