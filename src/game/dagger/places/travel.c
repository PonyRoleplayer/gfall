#include "game/dagger/places/travel.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/dunstream.h"
#include "game/dagger/places/people.h"
#include "game/dagger/places/town.h"
#include "game/dagger/places/wild.h"
#include "game/dagger/places/world.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/formats/worldmap.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/scene/stream.h"
#include "engine/sound/music.h"
#include "engine/sound/sound.h"

/* See travel.h and docs/game.md (Places and travel). */

/* How the player comes to stand in the place being opened. */
enum {
    /* The place's own start: a dungeon's enter marker, a street's point. */
    LAND_START,
    /* By the nearest passage of g_land_what, a step out from it. */
    LAND_PASSAGE,
    /* Where a save says. */
    LAND_SAVED,
    /* On the walkable cell nearest the place's start: a town's middle can be inside a building. */
    LAND_STREET
};

/* A town's walking cells are 64 units; how many rings of them to look through for a street. */
#define CELL_SIDE 64.0
#define STREET_RINGS 32

static TravelWhere g_here;
static TravelWhere g_next;
static int g_asked;
static int g_open;
static int g_land;
static int g_land_what;
/* The door that led in, in the town's frame: where a house's way out is looked for. */
static Usable g_door;
/* A save's feet, the origin they were in, the way it looked. */
static float g_saved_feet[3];
static int g_saved_origin[2];
static float g_saved_yaw;
static float g_saved_pitch;
/* The open place's records: a stream and a house both read from them for as long as they are open. */
static Dungeon g_dungeon;
static Dungeon g_town;
static World *g_world;
/* The place's own start, as it opened. */
static TravelPose g_start;
static char g_said[48];
static const char *g_error;

/* The world's stream, or the dungeon's. */
static float chunk_of(int kind)
{
    return kind == TRAVEL_WORLD ? (float)STREAM_CHUNK : DUNSTREAM_BLOCK;
}

void travel_start_game(void)
{
    mem_zero(&g_next, sizeof(g_next));
    g_next.kind = TRAVEL_DUNGEON;
    g_land = LAND_START;
    g_asked = 0;
}

int travel_go_to_town(int region, int index)
{
    int px;
    int py;

    if (!worldmap_open() || !worldmap_pixel_of(region, index, &px, &py)) {
        return 0;
    }
    mem_zero(&g_next, sizeof(g_next));
    g_next.kind = TRAVEL_WORLD;
    g_next.located = 1;
    g_next.region = region;
    g_next.index = index;
    g_land = LAND_STREET;
    g_asked = 1;
    return 1;
}

void travel_from_save(const TravelWhere *where, const float *feet, const int *origin, float yaw, float pitch)
{
    g_next = *where;
    g_land = LAND_SAVED;
    g_saved_feet[0] = feet[0];
    g_saved_feet[1] = feet[1];
    g_saved_feet[2] = feet[2];
    g_saved_origin[0] = origin[0];
    g_saved_origin[1] = origin[1];
    g_saved_yaw = yaw;
    g_saved_pitch = pitch;
    g_asked = 0;
}

/* Remembers the step outside door u, in world units, for the way back. */
static void remember_back(const Usable *u, TravelWhere *next)
{
    float x;
    float y;
    float z;

    doors_landing(u, TRAVEL_LAND_OUT, 0.0f, &x, &y, &z, &next->back_yaw);
    stream_world(x, z, &next->back_x, &next->back_z);
    next->has_back = 1;
}

/* Out to the world: at the door the player came in by, or else by the location's way down. */
static const char *ask_world(void)
{
    g_next.kind = TRAVEL_WORLD;
    g_next.located = g_here.located;
    g_next.region = g_here.region;
    g_next.index = g_here.index;
    g_next.has_back = g_here.has_back;
    g_next.back_x = g_here.back_x;
    g_next.back_z = g_here.back_z;
    g_next.back_yaw = g_here.back_yaw;
    g_land = LAND_START;
    if (!g_here.has_back) {
        int outside;

        /* A dungeon first met from within: the hold, in a new game. Its outside on the map. */
        outside = dungeon_outside(g_here.region, g_dungeon.exterior_id);
        if (outside < 0) {
            return "It leads nowhere.";
        }
        g_next.located = 1;
        g_next.index = outside;
        g_land = LAND_PASSAGE;
        g_land_what = DOOR_DUNGEON;
    }
    g_asked = 1;
    return 0;
}

const char *travel_use(const Usable *u)
{
    Usable in;
    int region;
    int index;
    int under;

    if (!g_open) {
        return "It leads nowhere.";
    }
    switch (u->what) {
    case DOOR_EXIT:
    case DOOR_LEAVE:
        if ((u->what == DOOR_EXIT) != (g_here.kind == TRAVEL_DUNGEON)) {
            return "It leads nowhere.";
        }
        return ask_world();
    case DOOR_ENTER:
    case DOOR_DUNGEON:
        if (g_here.kind != TRAVEL_WORLD || !travel_door_in_town(u, &in, &region, &index)) {
            return "It leads nowhere.";
        }
        mem_zero(&g_next, sizeof(g_next));
        g_next.located = 1;
        g_next.region = region;
        remember_back(u, &g_next);
        if (u->what == DOOR_ENTER) {
            g_next.kind = TRAVEL_INSIDE;
            g_next.index = index;
            g_next.block = u->tag >> 8;
            g_next.record = u->tag & 0xff;
            g_door = in;
            g_land = LAND_PASSAGE;
            g_land_what = DOOR_LEAVE;
        } else {
            Dungeon town;

            if (!exterior_load_at(region, index, &town)) {
                return "It leads nowhere.";
            }
            under = dungeon_under(region, town.location_id);
            if (under < 0) {
                return "It leads nowhere.";
            }
            g_next.kind = TRAVEL_DUNGEON;
            g_next.index = under;
            g_land = LAND_PASSAGE;
            g_land_what = DOOR_EXIT;
        }
        g_asked = 1;
        return 0;
    default:
        return "It leads nowhere.";
    }
}

int travel_next_is_world(void)
{
    return g_next.kind == TRAVEL_WORLD;
}

int travel_asked(void)
{
    return g_asked;
}

void travel_keep_now(void)
{
    if (!g_open) {
        return;
    }
    if (g_here.kind == TRAVEL_INSIDE) {
        world_keep_save(g_world);
    } else {
        stream_keep_now();
    }
}

void travel_leave(void)
{
    if (!g_open) {
        return;
    }
    music_stop();
    sound_stop_all();
    /* A stream keeps what its chunks changed as it closes them; a place loaded whole is asked. */
    if (g_here.kind == TRAVEL_DUNGEON) {
        dunstream_close();
    } else if (g_here.kind == TRAVEL_WORLD) {
        wild_close();
    } else {
        world_keep_save(g_world);
        g_world = 0;
    }
    texture_cache_clear();
    mem_temp_end();
    g_open = 0;
}

/* The next place's own kind opened over level: 0 when its files do not read. */
static int open_dungeon(Level *level)
{
    int ok;

    ok = g_next.located ? dungeon_load_at(g_next.region, g_next.index, &g_dungeon) : dungeon_load(&g_dungeon);
    if (!ok) {
        g_error = dungeon_error();
        return 0;
    }
    /* A hold by its name now has its numbers, for the way out. */
    g_next.located = 1;
    g_next.region = g_dungeon.region;
    g_next.index = g_dungeon.index;
    if (!dunstream_open(&g_dungeon, level, &g_start.x, &g_start.y, &g_start.z, &g_start.yaw, &g_start.pitch)) {
        g_error = "The dungeon's stream did not open.";
        return 0;
    }
    return 1;
}

static int open_world(Level *level, int radius)
{
    g_start.yaw = g_next.back_yaw;
    g_start.pitch = 0.0f;
    if (g_next.has_back) {
        if (!wild_open_at(g_next.back_x, g_next.back_z, radius, level, &g_start.x, &g_start.y, &g_start.z)) {
            g_error = "The world did not open.";
            return 0;
        }
    } else {
        int px;
        int py;
        double wx;
        double wz;
        double x0;
        double z0;
        double x1;
        double z1;
        int blocks;
        int climate;
        int type;

        if (!worldmap_pixel_of(g_next.region, g_next.index, &px, &py)
            || !wild_open(px, py, radius, level, &g_start.x, &g_start.y, &g_start.z)) {
            g_error = "The world did not open.";
            return 0;
        }
        /* The town's middle, not its pixel's: its way down has to come within the radius. */
        stream_world(g_start.x, g_start.z, &wx, &wz);
        if (wild_town(wx, wz, &x0, &z0, &x1, &z1, &blocks, &climate, &type)) {
            double mx;
            double mz;

            mx = (x0 + x1) * 0.5;
            mz = (z0 + z1) * 0.5;
            g_start.x += (float)(mx - wx);
            g_start.y += wild_ground(mx, mz) - wild_ground(wx, wz);
            g_start.z += (float)(mz - wz);
        }
    }
    return 1;
}

static int open_inside(Level *level)
{
    if (!exterior_load_at(g_next.region, g_next.index, &g_town)) {
        g_error = dungeon_error();
        return 0;
    }
    g_world = world_load_inside(&g_town, g_next.block, g_next.record);
    if (!g_world) {
        g_error = world_error();
        return 0;
    }
    world_as_level(g_world, level);
    return 1;
}

static int open_next(Level *level, int radius)
{
    if (g_next.kind == TRAVEL_DUNGEON) {
        return open_dungeon(level);
    }
    return g_next.kind == TRAVEL_WORLD ? open_world(level, radius) : open_inside(level);
}

int travel_open(Level *level, int radius)
{
    g_asked = 0;
    g_error = 0;
    if (!worldmap_open()) {
        g_error = "The world map did not read.";
        return 0;
    }
    if (!mem_temp_begin()) {
        g_error = "No memory for the place.";
        return 0;
    }
    if (!open_next(level, radius)) {
        /* A door that leads to nothing readable is shut: back on the step it was opened from. */
        if (g_next.kind == TRAVEL_WORLD || !g_next.has_back) {
            mem_temp_end();
            return 0;
        }
        log_print(LOG_WARN, "travel: %s", g_error ? g_error : "the place did not load");
        texture_cache_clear();
        mem_temp_end();
        if (!mem_temp_begin()) {
            g_error = "No memory for the place.";
            return 0;
        }
        g_next.kind = TRAVEL_WORLD;
        g_land = LAND_START;
        df_copy(g_said, "It will not open.", (int)sizeof(g_said));
        if (!open_world(level, radius)) {
            mem_temp_end();
            return 0;
        }
    }
    g_here = g_next;
    g_open = 1;
    return 1;
}

const char *travel_error(void)
{
    return g_error ? g_error : "The place did not load.";
}

const char *travel_said(void)
{
    static char said[48];

    if (!g_said[0]) {
        return 0;
    }
    df_copy(said, g_said, (int)sizeof(said));
    g_said[0] = 0;
    return said;
}

/* Everything round the camera before a first frame, under the hold. */
static void fill_round(Camera *cam)
{
    if (g_here.kind == TRAVEL_DUNGEON) {
        dunstream_fill(cam);
    } else if (g_here.kind == TRAVEL_WORLD) {
        wild_fill(cam);
    }
}

/* Moves the pose to the nearest walkable cell, ring by ring out from it. A place with none, a farm in a field, stays. */
static void land_on_street(TravelPose *p, float eye)
{
    double wx;
    double wz;
    int ring;

    stream_world(p->x, p->z, &wx, &wz);
    for (ring = 0; ring <= STREET_RINGS; ring++) {
        int a;
        int b;

        for (a = -ring; a <= ring; a++) {
            for (b = -ring; b <= ring; b++) {
                double cx;
                double cz;

                /* The ring's edge alone: what is inside it was looked at already. */
                if (a != -ring && a != ring && b != -ring && b != ring) {
                    continue;
                }
                cx = wx + (double)a * CELL_SIDE;
                cz = wz + (double)b * CELL_SIDE;
                if (wild_nav(cx, cz) > 0) {
                    p->x += (float)(cx - wx);
                    p->y = wild_ground(cx, cz) + eye;
                    p->z += (float)(cz - wz);
                    return;
                }
            }
        }
    }
    log_text(LOG_INFO, "travel: no walking cell near the town's middle");
}

void travel_land(const Level *level, Camera *cam, float eye, TravelPose *out)
{
    TravelPose p;

    p = g_start;
    if (g_here.kind == TRAVEL_INSIDE) {
        world_start(g_world, &p.x, &p.y, &p.z, &p.yaw, &p.pitch);
    }
    if (g_land == LAND_SAVED) {
        int ox;
        int oz;
        float chunk;

        /* The feet as saved, in the frame of the origin they were saved in, come to this stream's. */
        ox = 0;
        oz = 0;
        if (g_here.kind != TRAVEL_INSIDE) {
            stream_origin(&ox, &oz);
        }
        chunk = chunk_of(g_here.kind);
        p.x = g_saved_feet[0] + (float)(g_saved_origin[0] - ox) * chunk;
        p.y = g_saved_feet[1] + eye;
        p.z = g_saved_feet[2] + (float)(g_saved_origin[1] - oz) * chunk;
        p.yaw = g_saved_yaw;
        p.pitch = g_saved_pitch;
    }
    camera_set(cam, p.x, p.y, p.z, p.yaw, p.pitch);
    fill_round(cam);
    if (g_land == LAND_STREET) {
        land_on_street(&p, eye);
        camera_set(cam, p.x, p.y, p.z, p.yaw, p.pitch);
        fill_round(cam);
    }
    if (g_land == LAND_PASSAGE) {
        const Usable *u;

        if (g_here.kind == TRAVEL_INSIDE) {
            int k;

            k = world_passage(g_world, DOOR_LEAVE, g_door.cx, g_door.cy, g_door.cz);
            u = k >= 0 ? world_usable(g_world, k) : 0;
        } else {
            u = travel_nearest_passage(&level->scene, g_land_what, p.x, p.z);
        }
        if (u) {
            doors_landing(u, g_here.kind == TRAVEL_INSIDE ? TRAVEL_LAND_INTO : TRAVEL_LAND_OUT, eye,
                          &p.x, &p.y, &p.z, &p.yaw);
            p.pitch = 0.0f;
            camera_set(cam, p.x, p.y, p.z, p.yaw, p.pitch);
            fill_round(cam);
        } else {
            log_print(LOG_WARN, "travel: no passage %d near the landing", g_land_what);
        }
    }
    *out = p;
}

void travel_update(Camera *cam, int radius, float budget_ms, float dt, float *shift_x, float *shift_z)
{
    int ox;
    int oz;
    int nx;
    int nz;

    *shift_x = 0.0f;
    *shift_z = 0.0f;
    if (!g_open || g_here.kind == TRAVEL_INSIDE) {
        return;
    }
    stream_origin(&ox, &oz);
    if (g_here.kind == TRAVEL_DUNGEON) {
        dunstream_update(cam, budget_ms);
    } else {
        int px;
        int py;
        int region;
        int index;

        stream_set_radius(radius);
        wild_update(cam, budget_ms);
        people_update(cam, dt);
        /* The last town's region is the region: a walk across a border takes the law and the bank with it. */
        wild_here(cam, &px, &py);
        if (worldmap_location(px, py, &region, &index)) {
            g_here.located = 1;
            g_here.region = region;
            g_here.index = index;
        }
    }
    stream_origin(&nx, &nz);
    *shift_x = (float)(nx - ox) * chunk_of(g_here.kind);
    *shift_z = (float)(nz - oz) * chunk_of(g_here.kind);
}

int travel_must_reload(void)
{
    return g_open && g_here.kind == TRAVEL_WORLD && (wild_season_turned() || wild_crowded());
}

void travel_reload(Camera *cam)
{
    if (wild_season_turned()) {
        log_text(LOG_INFO, "the season turned: the world again in its pictures");
        wild_reseason(cam);
    } else {
        log_text(LOG_INFO, "the meshes passed their cap: the world again round the camera");
        wild_restart(cam);
    }
}

void travel_here(TravelWhere *where, int *origin)
{
    *where = g_here;
    origin[0] = 0;
    origin[1] = 0;
    if (g_open && g_here.kind != TRAVEL_INSIDE) {
        stream_origin(&origin[0], &origin[1]);
    }
}

int travel_town_here(const Camera *cam, int *region, int *index, float *x, float *z, int *block, int *record)
{
    double wx;
    double wz;
    double ox;
    double oz;

    *block = -1;
    *record = -1;
    if (!g_open) {
        return 0;
    }
    if (g_here.kind == TRAVEL_INSIDE) {
        *region = g_here.region;
        *index = g_here.index;
        *x = cam->x;
        *z = cam->z;
        *block = g_here.block;
        *record = g_here.record;
        return 1;
    }
    if (g_here.kind != TRAVEL_WORLD) {
        return 0;
    }
    stream_world(cam->x, cam->z, &wx, &wz);
    if (!wild_location_at(wx, wz, region, index, &ox, &oz)) {
        return 0;
    }
    *x = (float)(wx - ox);
    *z = (float)(wz - oz);
    return 1;
}

int travel_region(void)
{
    return g_here.region;
}

int travel_building(int *type, int *faction, int *quality, unsigned *place_key)
{
    const TownBuilding *t;

    if (!g_open || g_here.kind != TRAVEL_INSIDE) {
        return 0;
    }
    *type = world_building(g_world, faction);
    t = town_building(g_here.region, g_here.index, g_here.block, g_here.record);
    *quality = t ? t->quality : 10;
    /* As world.c keys a building's things (docs/game.md, What the player changed). */
    *place_key = (3u << 30) | (((unsigned)g_here.record + 1u) << 24) | ((unsigned)g_town.location_id & 0xFFFFFFu);
    return 1;
}

int travel_door_building(const Usable *door, int *type, int *quality)
{
    const TownBuilding *t;
    Usable in;
    int region;
    int index;

    if (!travel_door_in_town(door, &in, &region, &index)) {
        return 0;
    }
    t = town_building(region, index, door->tag >> 8, door->tag & 0xff);
    if (!t) {
        return 0;
    }
    *type = t->type;
    *quality = t->quality;
    return 1;
}

int travel_out_to_street(void)
{
    if (!g_open || g_here.kind != TRAVEL_INSIDE) {
        return 0;
    }
    ask_world();
    return g_asked;
}

int travel_radius(float far)
{
    int r;

    r = (df_iround(far) + STREAM_CHUNK - 1) / STREAM_CHUNK;
    return r < 1 ? 1 : (r > STREAM_RADIUS_MAX ? STREAM_RADIUS_MAX : r);
}

const Usable *travel_nearest_passage(const Scene *scene, int what, float x, float z)
{
    const Usable *best;
    float best_d;
    int i;

    best = 0;
    best_d = 0.0f;
    for (i = 0; i < scene->nusables; i++) {
        const Usable *u;
        float dx;
        float dz;

        u = &scene->usables[i];
        /* A slot's empty places have no reach. */
        if (u->reach <= 0.0f || u->kind != USE_PASSAGE || u->what != what) {
            continue;
        }
        dx = u->cx - x;
        dz = u->cz - z;
        if (!best || dx * dx + dz * dz < best_d) {
            best = u;
            best_d = dx * dx + dz * dz;
        }
    }
    return best;
}

int travel_door_in_town(const Usable *door, Usable *in, int *region, int *index)
{
    double wx;
    double wz;
    double ox;
    double oz;

    stream_world(door->cx, door->cz, &wx, &wz);
    if (!wild_location_at(wx, wz, region, index, &ox, &oz)) {
        return 0;
    }
    *in = *door;
    in->cx = (float)(wx - ox);
    in->cz = (float)(wz - oz);
    /* And in height: a town loaded whole stands at 0, the stream's at its level. */
    in->cy -= wild_ground(wx, wz);
    return 1;
}
