#include "game/dagger/places/people.h"
#include "game/dagger/formats/climate.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/places/wild.h"
#include "game/dagger/formats/worldmap.h"
#include "game/dagger/places/wres.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/scene/clock.h"
#include "engine/scene/stream.h"

/* See people.h and docs/entities.md. */

/* A walking cell: 64 world units, 64 to a block's side. */
#define CELL 64.0
/* Daggerfall Unity's: 24 people for every 16 blocks, 1 to 4 of those; ticks a second for who comes and goes. */
#define PER_16_BLOCKS 24
#define TICK 0.1f
/* No one appears for a camera farther than this outside a town's rectangle. */
#define TOWN_REACH 2500.0
/* Walking speed (1.3 m/s), where a person stands still for a close, still camera (2.5 m), and how still. */
#define WALK_SPEED 52.0f
#define IDLE_NEAR 100.0f
#define STILL_SPEED 8.0f
/* The motor's chances (MobilePersonMotor): a random turn, and taking a worse cell anyway. */
#define TURN_CHANCE 0.025f
#define DOWNGRADE_CHANCE 0.20f
/* Seeks with nowhere to go before a person is taken away. */
#define STUCK 4
/* Picture sets: a person's archive, its five walking records and its idle one. */
#define SETS 32
#define SET_PICTURES 6

enum {
    SEEK,
    MOVE,
    IDLE
};

/* North, south, east, west: a step in world units, and the facing yaw. */
static const int g_step_x[4] = { 0, 0, 1, -1 };
static const int g_step_z[4] = { 1, -1, 0, 0 };
static const float g_dir_yaw[4] = { 0.0f, DF_TAU * 0.5f, DF_TAU * 0.25f, DF_TAU * 0.75f };

typedef struct Person {
    int state;
    int dir;
    int seeks;
    int set;
    /* World centres of the cell it is on and the cell it walks to. */
    double cx;
    double cz;
    double tx;
    double tz;
} Person;

typedef struct Set {
    int archive;
    int slot[SET_PICTURES];
    int next;
    int ready;
} Set;

static int g_on;
static struct Scene *g_scene;
static Entity g_ent[PEOPLE_MAX];
static Person g_per[PEOPLE_MAX];
static Set g_set[SETS];
static int g_nset;
static int g_pictures;
static float g_tick;
static df_u32 g_seed = 0x6d2b79f5u;
/* The camera last frame, for how fast it moves. */
static float g_last_x;
static float g_last_z;
static int g_have_last;

static float rnd(void)
{
    g_seed = g_seed * 1664525u + 1013904223u;
    return (float)(g_seed >> 8) * (1.0f / 16777216.0f);
}

static int pick(int n)
{
    int k;

    k = (int)(rnd() * (float)n);
    return k < 0 ? 0 : (k >= n ? n - 1 : k);
}

void people_begin(struct Scene *scene)
{
    int i;

    people_end();
    for (i = 0; i < PEOPLE_MAX; i++) {
        mem_zero(&g_ent[i], sizeof(Entity));
        mem_zero(&g_per[i], sizeof(Person));
    }
    g_nset = 0;
    g_pictures = 0;
    g_tick = 0.0f;
    g_have_last = 0;
    g_scene = scene;
    scene->entities = g_ent;
    scene->nentities = PEOPLE_MAX;
    g_on = 1;
}

void people_end(void)
{
    if (g_on && g_scene) {
        g_scene->entities = 0;
        g_scene->nentities = 0;
    }
    /* The pictures are the stream's: they go with it (wres_end). */
    g_on = 0;
    g_scene = 0;
    g_nset = 0;
}

void people_shift(float dx, float dz)
{
    int i;

    for (i = 0; g_on && i < PEOPLE_MAX; i++) {
        g_ent[i].x -= dx;
        g_ent[i].z -= dz;
    }
    g_last_x -= dx;
    g_last_z -= dz;
}

int people_count(void)
{
    int i;
    int n;

    n = 0;
    for (i = 0; g_on && i < PEOPLE_MAX; i++) {
        n += g_ent[i].live;
    }
    return n;
}

int people_pictures(void)
{
    return g_pictures;
}

/*
 * The archive a new person wears, as Daggerfall Unity's billboard picks
 * it: a guard one time in 32, else a man or a woman, one of four outfits
 * of the climate's people (Redguards in the desert and the rainforest,
 * Nords in the mountains, Bretons elsewhere).
 */
static const int g_breton[2][4] = { { 385, 386, 391, 394 }, { 453, 454, 455, 456 } };
static const int g_redguard[2][4] = { { 381, 382, 383, 384 }, { 395, 396, 397, 398 } };
static const int g_nord[2][4] = { { 387, 388, 389, 390 }, { 392, 393, 451, 452 } };
#define GUARD_ARCHIVE 399

/* The climate's people: a man's four outfits, then a woman's. */
static const int (*people_of(int climate))[4]
{
    if (climate == CLIMATE_DESERT || climate == CLIMATE_DESERT2 || climate == CLIMATE_RAINFOREST) {
        return g_redguard;
    }
    if (climate == CLIMATE_MOUNTAIN) {
        return g_nord;
    }
    return g_breton;
}

static int choose_archive(int climate)
{
    int woman;
    int outfit;

    if (pick(32) == 0) {
        return GUARD_ARCHIVE;
    }
    woman = pick(2);
    outfit = pick(4);
    return people_of(climate)[woman][outfit];
}

/* Record k (0..5) of a set: walking 0 to 4, then idle (the guard's 15, everyone else's 5). */
static int set_record(int archive, int k)
{
    return k < 5 ? k : (archive == GUARD_ARCHIVE ? 15 : 5);
}

int people_slots(int climate, df_u16 *out, int cap)
{
    const int (*folk)[4];
    int n;
    int a;
    int k;

    folk = people_of(climate);
    n = 0;
    for (a = 0; a < 9; a++) {
        int archive;

        archive = a < 8 ? folk[a / 4][a % 4] : GUARD_ARCHIVE;
        for (k = 0; k < SET_PICTURES; k++) {
            int slot;

            slot = wres_slot(archive, set_record(archive, k), WRES_SPRITE);
            if (slot >= 0 && n < cap) {
                out[n++] = (df_u16)slot;
            }
        }
    }
    return n;
}

int people_settle(int type)
{
    return type == WORLDMAP_TOWN_CITY || type == WORLDMAP_TOWN_HAMLET || type == WORLDMAP_TOWN_VILLAGE;
}

/* The set of an archive's pictures, registered and held the first time it is asked. */
static int set_of(int archive)
{
    Set *s;
    int i;

    for (i = 0; i < g_nset; i++) {
        if (g_set[i].archive == archive) {
            return i;
        }
    }
    if (g_nset >= SETS) {
        return -1;
    }
    s = &g_set[g_nset];
    s->archive = archive;
    s->next = 0;
    s->ready = 0;
    for (i = 0; i < SET_PICTURES; i++) {
        s->slot[i] = wres_slot(archive, set_record(archive, i), WRES_SPRITE);
        wres_hold(s->slot[i]);
    }
    return g_nset++;
}

/*
 * One part of a picture of a set not ready yet read (wres_load: its
 * pixels, or its chain); 1 when it did one. A set is ready once all six
 * are done. A town's block chunks read them first, under the stream's
 * budget (wild.c's hold_people), so here they are mostly ready already.
 */
static int load_one(void)
{
    int i;

    for (i = 0; i < g_nset; i++) {
        Set *s;

        s = &g_set[i];
        while (s->next < SET_PICTURES) {
            if (wres_load(s->slot[s->next])) {
                return 1;
            }
            if (wres_image(s->slot[s->next])) {
                g_pictures++;
            }
            s->next++;
        }
        s->ready = 1;
    }
    return 0;
}

static int size_of(int slot, df_u16 *w, df_u16 *h)
{
    const Image *img;
    int pw;
    int ph;

    img = wres_image(slot);
    if (!img) {
        return 0;
    }
    pw = texture_extent(img->width, img->xscale);
    ph = texture_extent(img->height, img->yscale);
    *w = (df_u16)pw;
    *h = (df_u16)ph;
    return pw > 0 && ph > 0;
}

/* The centre of the cell of world point (wx, wz), on the grid of a town whose corner is (x0, z0). */
static void cell_centre(double x0, double z0, double wx, double wz, double *cx, double *cz)
{
    double fx;
    double fz;

    fx = (wx - x0) / CELL;
    fz = (wz - z0) / CELL;
    *cx = x0 + ((double)(int)fx + 0.5) * CELL;
    *cz = z0 + ((double)(int)fz + 0.5) * CELL;
}

/* 1 when another person stands on, or walks to, the cell at (cx, cz). */
static int taken(int self, double cx, double cz)
{
    int i;

    for (i = 0; i < PEOPLE_MAX; i++) {
        const Person *p;

        if (i == self || !g_ent[i].live) {
            continue;
        }
        p = &g_per[i];
        if ((p->cx == cx && p->cz == cz) || (p->tx == cx && p->tz == cz)) {
            return 1;
        }
    }
    return 0;
}

/* The camera may see this point appear or go: farther than PEOPLE_POP, or behind it. */
static int out_of_sight(const Camera *cam, float x, float z)
{
    float dx;
    float dz;

    dx = x - cam->x;
    dz = z - cam->z;
    if (dx * dx + dz * dz > PEOPLE_POP * PEOPLE_POP) {
        return 1;
    }
    return dx * df_sin(cam->yaw) + dz * df_cos(cam->yaw) < 0.0f;
}

static void set_target(int i, int dir)
{
    Person *p;

    p = &g_per[i];
    p->dir = dir;
    p->tx = p->cx + (double)g_step_x[dir] * CELL;
    p->tz = p->cz + (double)g_step_z[dir] * CELL;
    p->state = MOVE;
    p->seeks = 0;
    g_ent[i].yaw = g_dir_yaw[dir];
    g_ent[i].moving = 1;
}

/*
 * The next cell, as MobilePersonMotor seeks it: straight on when that
 * cell is as good as this one; else, most of the time, the best of the
 * others; now and then a random turn. Nowhere to go counts a seek.
 */
static void seek(int i)
{
    Person *p;
    int w[4];
    int here;
    int ahead;
    int d;

    p = &g_per[i];
    p->seeks++;
    for (d = 0; d < 4; d++) {
        double nx;
        double nz;

        nx = p->cx + (double)g_step_x[d] * CELL;
        nz = p->cz + (double)g_step_z[d] * CELL;
        w[d] = taken(i, nx, nz) ? 0 : wild_nav(nx, nz);
    }
    here = wild_nav(p->cx, p->cz);
    ahead = w[p->dir];
    if (rnd() < TURN_CHANCE) {
        ahead = 0;
    }
    if (ahead == 0) {
        d = pick(4);
        if (w[d] > 0) {
            set_target(i, d);
        }
        return;
    }
    if (ahead < here && rnd() > DOWNGRADE_CHANCE) {
        int best;
        int k;
        int start;

        best = p->dir;
        start = pick(4);
        for (k = 0; k < 4; k++) {
            d = (start + k) & 3;
            if (w[d] > w[best]) {
                best = d;
            }
        }
        set_target(i, best);
        return;
    }
    set_target(i, p->dir);
}

/* A step toward the target; there, it seeks the next. */
static void walk(int i, float secs)
{
    Person *p;
    Entity *e;
    float tx;
    float tz;
    float dx;
    float dz;
    float d;
    float step;

    p = &g_per[i];
    e = &g_ent[i];
    stream_local(p->tx, p->tz, &tx, &tz);
    dx = tx - e->x;
    dz = tz - e->z;
    d = df_sqrt(dx * dx + dz * dz);
    step = WALK_SPEED * secs;
    if (d <= step || d < 0.5f) {
        e->x = tx;
        e->z = tz;
        p->cx = p->tx;
        p->cz = p->tz;
        p->state = SEEK;
        return;
    }
    e->x += dx / d * step;
    e->z += dz / d * step;
}

/*
 * A new person on a free walking cell round the camera, out of its
 * sight; anywhere round it when the frame is not up yet (people_fill).
 * 1 when one came.
 */
static int spawn(const Camera *cam, double x0, double z0, int climate, int anywhere)
{
    double cwx;
    double cwz;
    int slot;
    int attempt;
    int set;
    int k;

    for (slot = 0; slot < PEOPLE_MAX && g_ent[slot].live; slot++) {
    }
    if (slot == PEOPLE_MAX) {
        return 0;
    }
    /* Its clothes: their pictures read first, a few frames, before anyone wears them. */
    set = set_of(choose_archive(climate));
    if (set < 0 || !g_set[set].ready) {
        return 0;
    }
    stream_world(cam->x, cam->z, &cwx, &cwz);
    for (attempt = 0; attempt < 10; attempt++) {
        double wx;
        double wz;
        double cx;
        double cz;
        float lx;
        float lz;
        Entity *e;
        Person *p;

        wx = cwx + (double)((rnd() * 2.0f - 1.0f) * PEOPLE_SPAWN);
        wz = cwz + (double)((rnd() * 2.0f - 1.0f) * PEOPLE_SPAWN);
        cell_centre(x0, z0, wx, wz, &cx, &cz);
        stream_local(cx, cz, &lx, &lz);
        /* Within PEOPLE_RECYCLE too: the square's corners reach past it, and one there went the next frame. */
        if ((lx - cam->x) * (lx - cam->x) + (lz - cam->z) * (lz - cam->z) > PEOPLE_RECYCLE * PEOPLE_RECYCLE
            || wild_nav(cx, cz) == 0 || taken(-1, cx, cz) || (!anywhere && !out_of_sight(cam, lx, lz))) {
            continue;
        }
        e = &g_ent[slot];
        p = &g_per[slot];
        mem_zero(e, sizeof(Entity));
        mem_zero(p, sizeof(Person));
        /* Walking, five views; standing, one picture that faces the viewer. */
        for (k = 0; k < ENTITY_VIEWS; k++) {
            e->walk.slot[k] = (df_u16)g_set[set].slot[k];
            e->idle.slot[k] = 0xffff;
            if (!size_of(g_set[set].slot[k], &e->walk.w[k], &e->walk.h[k])) {
                return 0;
            }
        }
        e->idle.slot[0] = (df_u16)g_set[set].slot[5];
        if (!size_of(g_set[set].slot[5], &e->idle.w[0], &e->idle.h[0])) {
            return 0;
        }
        e->x = lx;
        e->z = lz;
        e->y = wild_ground(cx, cz);
        e->walk.rate = 4;
        e->idle.rate = 1;
        e->phase = (df_u8)pick(256);
        e->moving = 1;
        e->live = 1;
        p->set = set;
        p->cx = p->tx = cx;
        p->cz = p->tz = cz;
        p->dir = pick(4);
        e->yaw = g_dir_yaw[p->dir];
        p->state = SEEK;
        return 1;
    }
    return 0;
}

/*
 * A settlement near, and the engine letting walkers come: a city, a
 * hamlet or a village, not a farm, a temple, a tavern, a graveyard or
 * a dungeon's entrance (docs/entities.md). Its corner, its population
 * and its climate; *day 1 from 6:00 to 18:00.
 */
static int town_near(const Camera *cam, double *x0, double *z0, int *most, int *climate, int *day)
{
    double cwx;
    double cwz;
    double x1;
    double z1;
    int blocks;
    int type;
    int hour;
    int minute;

    clock_hm(&hour, &minute);
    *day = hour >= 6 && hour < 18;
    stream_world(cam->x, cam->z, &cwx, &cwz);
    if (!wild_town(cwx, cwz, x0, z0, &x1, &z1, &blocks, climate, &type)
        || !people_settle(type) || !entity_population()
        || !(cwx > *x0 - TOWN_REACH && cwx < x1 + TOWN_REACH && cwz > *z0 - TOWN_REACH && cwz < z1 + TOWN_REACH)) {
        return 0;
    }
    /* Daggerfall Unity's: 24 for every 16 blocks, 1 to 4 sixteens. */
    *most = blocks / 16;
    *most = (*most < 1 ? 1 : (*most > 4 ? 4 : *most)) * PER_16_BLOCKS;
    *most = *most > PEOPLE_MAX ? PEOPLE_MAX : *most;
    return 1;
}

void people_fill(const Camera *cam)
{
    double x0, z0;
    int most;
    int climate;
    int day;
    int a;
    int k;

    if (!g_on || !cam || !town_near(cam, &x0, &z0, &most, &climate, &day) || !day) {
        return;
    }
    /* Every outfit of the climate's people and the guard's, read now (the chunks read most already). */
    for (a = 0; a < 9; a++) {
        set_of(a < 8 ? people_of(climate)[a / 4][a % 4] : GUARD_ARCHIVE);
    }
    while (load_one()) {
    }
    /* The town's people, round the camera, in sight too: nothing was seen yet. */
    for (k = 0; k < most * 8 && people_count() < most; k++) {
        spawn(cam, x0, z0, climate, 1);
    }
}

void people_update(const Camera *cam, float secs)
{
    double x0, z0;
    int most;
    int climate;
    int in_town;
    int day;
    int still;
    int i;

    if (!g_on || !cam) {
        return;
    }
    in_town = town_near(cam, &x0, &z0, &most, &climate, &day);
    /* Still: the camera has hardly moved since the last frame. */
    still = 0;
    if (g_have_last && secs > 0.0f) {
        float dx;
        float dz;

        dx = cam->x - g_last_x;
        dz = cam->z - g_last_z;
        still = dx * dx + dz * dz < STILL_SPEED * STILL_SPEED * secs * secs;
    }
    g_last_x = cam->x;
    g_last_z = cam->z;
    g_have_last = 1;
    /*
     * Out of town and everyone gone: the clothes are let go, so the
     * stream's RAM cap (wres_trim) may free their pictures; the next town
     * asks for its own.
     */
    if (!in_town && g_nset > 0 && people_count() == 0) {
        int s;
        int k;

        for (s = 0; s < g_nset; s++) {
            for (k = 0; k < SET_PICTURES; k++) {
                wres_release(g_set[s].slot[k]);
            }
        }
        g_nset = 0;
    }
    /* A picture of the clothes asked for, one a frame. */
    load_one();
    /* Who comes, ten times a second: one at most a tick, up to the town's population. */
    g_tick += secs;
    if (g_tick >= TICK) {
        g_tick = 0.0f;
        if (in_town && day && people_count() < most) {
            spawn(cam, x0, z0, climate, 0);
        }
    }
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;
        Person *p;
        float dx;
        float dz;
        float d2;

        e = &g_ent[i];
        p = &g_per[i];
        if (!e->live) {
            continue;
        }
        dx = e->x - cam->x;
        dz = e->z - cam->z;
        d2 = dx * dx + dz * dz;
        /* Taken away far off, at dusk, out of the town, or stuck; never in sight. */
        if ((d2 > PEOPLE_RECYCLE * PEOPLE_RECYCLE || !day || !in_town || p->seeks > STUCK)
            && out_of_sight(cam, e->x, e->z)) {
            e->live = 0;
            continue;
        }
        /* A close, still camera: they stop and face it. */
        if (still && d2 < IDLE_NEAR * IDLE_NEAR) {
            if (p->state != IDLE) {
                p->state = IDLE;
            }
            e->moving = 0;
            e->yaw = df_atan2(cam->x - e->x, cam->z - e->z);
            continue;
        }
        if (p->state == IDLE) {
            p->state = p->tx != p->cx || p->tz != p->cz ? MOVE : SEEK;
            e->moving = 1;
            e->yaw = g_dir_yaw[p->dir];
        }
        if (p->state == SEEK) {
            seek(i);
        }
        if (p->state == MOVE) {
            walk(i, secs);
        }
    }
}
