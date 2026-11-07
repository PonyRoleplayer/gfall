#include "game/dagger/play/shots.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/cast.h"
#include "game/dagger/play/sfx.h"
#include "game/dagger/formats/sndfile.h"
#include "game/dagger/places/wres.h"
#include "game/dagger/places/people.h"
#include "game/dagger/formats/texture.h"
#include "engine/core/mem.h"
#include "engine/scene/spawn.h"

/* See shots.h and docs/game.md. */

/* The bow each arrow in the air left, by its entry's number (the projectile's tag). */
static Item g_bow[PROJ_MAX];

/* The projectile's questions, answered by combat's. */
typedef struct ShotsWorld {
    const Scene *scene;
    const Player *player;
} ShotsWorld;

static int world_wall(void *user, const float *a, const float *b, float *t)
{
    const ShotsWorld *w;

    w = (const ShotsWorld *)user;
    return combat_ray(w->scene, a, b, t);
}

/* The player's shots meet creatures; a creature's meet the player, a box about their body. */
static void *world_target(void *user, const Projectile *p, const float *a, const float *dir, float len, float *t)
{
    const ShotsWorld *w;

    w = (const ShotsWorld *)user;
    if (p->from == PROJ_FROM_FOE) {
        float low[3];
        float high[3];

        low[0] = w->player->body.feet[0] - w->player->body.radius;
        low[1] = w->player->body.feet[1];
        low[2] = w->player->body.feet[2] - w->player->body.radius;
        high[0] = w->player->body.feet[0] + w->player->body.radius;
        high[1] = w->player->body.feet[1] + w->player->body.height;
        high[2] = w->player->body.feet[2] + w->player->body.radius;
        return combat_ray_box(a, dir, low, high, len, t) ? (void *)w->player : 0;
    }
    /* The nearer of a creature and a person of the street. */
    {
        Entity *creature;
        Entity *person;
        float tc;
        float tp;

        creature = combat_aim_at(w->scene, a, dir, len, &tc);
        person = combat_aim_person_at(w->scene, a, dir, len, &tp);
        if (creature && (!person || tc <= tp)) {
            *t = tc;
            return creature;
        }
        if (person) {
            *t = tp;
        }
        return person;
    }
}

/* ---- Pictures. ---- */

#define MISSILE_ARCHIVE 375
#define MISSILE_RATE 5
#define BURST_RATE 15
#define BURST_TIME 0.6f
#define BURSTS 8

/* The spawn each missile in the air shows as, by its entry in projectile.c; and the bursts showing. */
static SpawnId g_view[PROJ_MAX];
static struct {
    SpawnId id;
    float left;
} g_burst[BURSTS];
/* Which element's pictures were made resident for this stream: bit 0 the flight's, bit 1 the burst's. */
static df_u8 g_ready[ELEMENT_COUNT];
static df_u32 g_epoch;
static int g_epoch_set;

/* A billboard of an element's picture (record 0 flight, 1 burst) put at its middle, or 0 when it cannot be had. */
static SpawnId billboard(const Scene *scene, int element, int record, int rate, const float *at)
{
    Entity e;
    const Image *img;
    int slot;
    int w;
    int h;

    if (!g_epoch_set || g_epoch != mem_epoch()) {
        mem_zero(g_ready, sizeof(g_ready));
        g_epoch = mem_epoch();
        g_epoch_set = 1;
    }
    slot = wres_slot(MISSILE_ARCHIVE + element, record, WRES_SPRITE);
    if (slot < 0) {
        return 0;
    }
    if (!(g_ready[element] & (1 << record))) {
        if (!wres_acquire(slot)) {
            return 0;
        }
        g_ready[element] |= (df_u8)(1 << record);
    }
    img = wres_image(slot);
    if (!img) {
        return 0;
    }
    w = texture_extent(img->width, img->xscale);
    h = texture_extent(img->height, img->yscale);
    if (w < 1 || h < 1) {
        return 0;
    }
    entity_init(&e);
    e.pose[ENTITY_WALK].slot[0] = (df_u16)slot;
    e.pose[ENTITY_WALK].w[0] = (df_u16)w;
    e.pose[ENTITY_WALK].h[0] = (df_u16)h;
    e.pose[ENTITY_WALK].rate = (df_u8)rate;
    e.x = at[0];
    e.y = at[1] - (float)h * 0.5f;
    e.z = at[2];
    e.moving = 1;
    e.live = 1;
    return spawn_add(scene, &e, 0.0f);
}

void shots_flash(const Scene *scene, int element, const float *at)
{
    int i;

    if (element < 0 || element >= ELEMENT_COUNT) {
        return;
    }
    for (i = 0; i < BURSTS; i++) {
        if (!spawn_get(g_burst[i].id)) {
            g_burst[i].id = billboard(scene, element, 1, BURST_RATE, at);
            g_burst[i].left = BURST_TIME;
            return;
        }
    }
}

/* The bursts age out. */
static void bursts_step(float dt)
{
    int i;

    for (i = 0; i < BURSTS; i++) {
        if (g_burst[i].id && spawn_get(g_burst[i].id)) {
            g_burst[i].left -= dt;
            if (g_burst[i].left <= 0.0f) {
                spawn_remove(g_burst[i].id);
                g_burst[i].id = 0;
            }
        }
    }
}

/* Each missile in the air shown where it is: its spawn made when it first flies, moved after, gone when the missile is. */
static void views_step(const Scene *scene)
{
    int i;

    for (i = 0; i < PROJ_MAX; i++) {
        const Projectile *p;
        const Spell *s;

        p = projectile_get(i);
        s = p && p->kind == PROJ_SPELL ? cast_flying(i) : 0;
        if (!s) {
            if (g_view[i]) {
                spawn_remove(g_view[i]);
                g_view[i] = 0;
            }
            continue;
        }
        if (!spawn_get(g_view[i])) {
            g_view[i] = billboard(scene, s->element, 0, MISSILE_RATE, p->at);
        } else {
            Entity *e;

            e = spawn_get(g_view[i]);
            e->x = p->at[0];
            e->z = p->at[2];
            e->y = p->at[1] - (float)entity_pose(e)->h[0] * 0.5f;
        }
    }
}

void shots_clear(void)
{
    int i;

    projectile_clear();
    spawn_clear();
    mem_zero(g_view, sizeof(g_view));
    mem_zero(g_burst, sizeof(g_burst));
    for (i = 0; i < ELEMENT_COUNT; i++) {
        g_ready[i] = 0;
    }
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
    sfx_play(SND_ARROW_SHOOT);
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

int shots_frame(float dt, const Scene *scene, Player *player)
{
    Character *pc;
    Inventory *pack;
    const float *feet;
    ProjectileEnd ends[PROJ_MAX];
    ProjectileWorld world;
    ShotsWorld mine;
    int i;
    int n;

    bursts_step(dt);
    if (projectile_count() == 0) {
        views_step(scene);
        return 0;
    }
    pc = &player->sheet;
    pack = &player->pack;
    feet = player->body.feet;
    mine.scene = scene;
    mine.player = player;
    world.user = &mine;
    world.wall = world_wall;
    world.target = world_target;
    n = projectile_step(dt, &world, ends, PROJ_MAX);
    for (i = 0; i < n; i++) {
        /* What it struck is heard where it struck: an arrow's thud, a spell's element (playtest 2026-11-07: both silent). */
        if (ends[i].result == PROJ_HIT_TARGET || ends[i].result == PROJ_HIT_WALL) {
            const Spell *what;

            what = ends[i].kind == PROJ_SPELL ? cast_flying(ends[i].entry) : 0;
            sfx_at(ends[i].kind == PROJ_ARROW ? SND_ARROW_HIT : (what ? sfx_impact_clip(what->element) : -1),
                   ends[i].at[0], ends[i].at[1], ends[i].at[2]);
        }
        if (ends[i].kind == PROJ_ARROW && ends[i].result == PROJ_HIT_TARGET && ends[i].target) {
            Entity *hit;

            hit = (Entity *)ends[i].target;
            if (people_is_citizen(hit) || people_is_guard(hit)) {
                float eye[3];

                eye[0] = feet[0];
                eye[1] = feet[1] + player->body.eye;
                eye[2] = feet[2];
                combat_missile_strike_person(pc, bow_for(pack, ends[i].entry), hit, feet, eye, player->yaw);
            } else {
                combat_missile_strike(pc, bow_for(pack, ends[i].entry), hit, feet);
            }
        } else if (ends[i].kind == PROJ_SPELL) {
            const Spell *flying;

            /* Its burst where it ended, in its element, before the missile's record is let go. */
            flying = cast_flying(ends[i].entry);
            if (flying && ends[i].from == PROJ_FROM_PLAYER && flying->target != TARGET_AREA_RANGE && ends[i].result == PROJ_HIT_TARGET) {
                shots_flash(scene, flying->element, ends[i].at);
            }
            cast_arrived(&ends[i], player, scene);
        }
    }
    views_step(scene);
    return n;
}
