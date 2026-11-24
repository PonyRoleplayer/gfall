#include "game/dagger/play/state.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/combat/encounter.h"
#include "game/dagger/play/items/ground.h"
#include "game/dagger/play/quests/quest.h"
#include "../seam.h"
#include "../screen/screen.h"
#include "../sound.h"

#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/ui/pic.h"
#include "engine/scene/interact.h"

#include "game/dagger/places/people.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/people/guild.h"

#include "game/dagger/play/play.h"

#include "people.h"
#include "../world/world.h"
static int seam_guards_present(void) { return dungeon_guards(); }

static int guards_count(void)
{
    int i, n = 0;

    for (i = 0; i < dungeon_count(); i++) {
        const Entity *e = dungeon_entity(i);
        if (e && e->live && !e->dead) { n++; }
    }
    return n;
}
/*
 * The street's people and the watch (people and law, the town's creatures): the dagger layer's people pool stands for the original's pedestrians. A guard is a person of archive 399; the watch
 * is those guards (and, when called, more made near the player) taken into GFALL's own creatures (kind 146, the original's stats and mind) and woken knowing where the player is. A swing's hit
 * kills whoever is within 90 units and 200 of the view (the original's pedestrian pass), the victim itself a witness; a killing seen is murder (crime 5) and brings the guards. Neither
 * the pictures nor the walking of the people are GFALL's.
 */
/* collide_line_of_sight: a ray from the person's chest to the player's eye that no wall stops. */
static int street_sees(const Entity *e, const float *eye)
{
    UseHit hit;
    float dx, dy, dz, length;

    dx = eye[0] - e->x; dy = eye[1] - (e->y + 40.0f); dz = eye[2] - e->z;
    length = df_sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 1.0f) { return 1; }
    dx /= length; dy /= length; dz /= length;
    interact_cast(&seam_level()->scene, e->x, e->y + 40.0f, e->z, dx, dy, dz, length, &hit);
    return hit.wall >= length;
}

/* people_check_witnesses: bit 0 another person, bit 1 a guard, with the player in sight; nothing in a dungeon. */
static int street_witnesses(void)
{
    Camera cam;
    float eye[3];
    int i, bits, bit;

    if (world_where()->where.kind == WHERE_DUNGEON) { return 0; }
    move_camera(&cam);
    eye[0] = cam.x; eye[1] = cam.y; eye[2] = cam.z;
    bits = 0;
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;
        float dx, dz;

        e = people_at(i);
        /* A guard of the street is a person too, and the one whose sight brings the watch (people_check_witnesses: bit 1). */
        if (!e || !(people_is_citizen(e) || people_is_guard(e))) { continue; }
        dx = e->x - cam.x; dz = e->z - cam.z;
        if (dx * dx + dz * dz > 2048.0f * 2048.0f) { continue; }
        /* Each look is a wall cast over the whole street: one for a bit not yet set, and none once both are. */
        bit = dungeon_index(e) >= 0 || people_is_guard(e) ? 2 : 1;
        if (bits & bit) { continue; }
        if (street_sees(e, eye)) { bits |= bit; if (bits == 3) { break; } }
    }
    return bits;
}

/* A pedestrian becomes a creature of the watch where they stand. */
static int street_enlist(int index)
{
    Entity *e;
    int foe;

    e = people_at(index);
    if (!e || !(people_is_guard(e) || people_is_citizen(e))) { return 0; }
    foe = dungeon_index(e);
    if (foe < 0) {
        if (guards_count() > GUARDS_CREATURES_MOST) { return 0; }
        if (!people_hold_guard(index)) { return 0; }
        foe = dungeon_add(e, ENEMY_CITY_WATCH, 0, gfall_creature_rate(ENEMY_ARCHIVE_WATCH));
        if (foe < 0) {
            people_unhold(e);
            return 0;
        }
        /* A guard of the watch in pursuit is not talked to (the street's talk refuses the hostile). */
        people_set_hostile(e, 1);
        dungeon_loaded(foe);
    }
    dungeon_wake(foe);
    return 1;
}

/* guard_spawn(0): one guard near the player, out of sight where a cell allows it. */
static int street_spawn(void)
{
    Camera cam;
    int index;

    if (guards_count() > GUARDS_CREATURES_MOST) { return 0; }
    move_camera(&cam);
    index = people_spawn_guard(&cam);
    if (index < 0) { return 0; }
    if (street_enlist(index)) { return 1; }
    people_fell(people_at(index));
    return 0;
}

/* Every guard comes. A full summons also converts a quarter of the pedestrians outside the player's forward arc. */
static int street_convert(int guards_only)
{
    Camera cam;
    int i, n;

    move_camera(&cam);
    n = 0;
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;
        float dx, dz, diff;

        e = people_at(i);
        /* people_is_citizen is false for a guard: the guards walking the street are the first the watch is made of. */
        if (!e || !(people_is_citizen(e) || people_is_guard(e))) { continue; }
        dx = e->x - cam.x; dz = e->z - cam.z;
        /* GFALL's crowd extends beyond combat range. A distant pedestrian would never start pursuit. */
        if (dx * dx + dz * dz > (float)FOE_ACTIVE_RANGE * FOE_ACTIVE_RANGE) { continue; }
        if (people_is_guard(e)) {
            n += street_enlist(i);
            continue;
        }
        if (guards_only) { continue; }
        diff = df_absf(df_atan2(dx, dz) - cam.yaw);
        while (diff > DF_TAU) { diff -= DF_TAU; }
        if (diff > DF_TAU / 2) { diff = DF_TAU - diff; }
        if (diff < 600.0f * DF_TAU / 2048.0f) { continue; }
        if (random_range(0, 32767) & 3) { continue; }
        n += street_enlist(i);
    }
    return n;
}

/* damage_spawn_splash on a person struck: a splash of blood in the people pool for a second, taken down by street_blood_tick. */
#define STREET_BLOODS 4
#define WATCH_RECOGNITION_RANGE 512.0f
#define WATCH_RECOGNITION_INTERVAL 0.25f
static Entity *g_street_blood[STREET_BLOODS];
static float g_street_blood_left[STREET_BLOODS];
static float g_watch_recognition;

/* A close patrol can join an active pursuit or recognize a banished player.
 * The regional clock still handles the chance of a report from farther away. */
static void street_recognize(float seconds)
{
    const PlaceWhere *where = &world_where()->where;
    Camera cam;
    float eye[3];
    int i, pursuing;

    if (where->kind != WHERE_WORLD || !where->located || where->region < 0 || where->region >= REGIONS) { return; }
    g_watch_recognition -= seconds;
    if (g_watch_recognition > 0.0f) { return; }
    g_watch_recognition = WATCH_RECOGNITION_INTERVAL;
    pursuing = guards_present();
    if (!pursuing && !(law_state()->punishment[where->region] & PUNISH_BANISHED)) { return; }
    move_camera(&cam);
    eye[0] = cam.x; eye[1] = cam.y; eye[2] = cam.z;
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e = people_at(i);
        float dx, dz;

        if (!people_is_guard(e) || people_hostile(e)) { continue; }
        dx = e->x - cam.x; dz = e->z - cam.z;
        if (dx * dx + dz * dz > WATCH_RECOGNITION_RANGE * WATCH_RECOGNITION_RANGE ||
            !street_sees(e, eye) || !street_enlist(i)) { continue; }
        if (!pursuing) { law_note(CRIME_CONSPIRACY); }
        pursuing = 1;
    }
}

static void street_blood(const Entity *e)
{
    int i;

    for (i = 0; i < STREET_BLOODS; i++) {
        if (g_street_blood_left[i] > 0.0f) { continue; }
        g_street_blood[i] = people_blood(e->x, e->y + 40.0f, e->z, 0);
        g_street_blood_left[i] = g_street_blood[i] ? 1.0f : 0.0f;
        return;
    }
}
void gfall_street_tick(float seconds)
{
    int i;

    for (i = 0; i < dungeon_count(); i++) {
        Entity *e;

        if (dungeon_kind(i) != ENEMY_CITY_WATCH) { continue; }
        e = dungeon_entity(i);
        if (people_index(e) < 0 || e->live) { continue; }
        dungeon_release_actor(i);
        people_fell(e);
    }
    for (i = 0; i < STREET_BLOODS; i++) {
        if (g_street_blood_left[i] <= 0.0f) { continue; }
        g_street_blood_left[i] -= seconds;
        if (g_street_blood_left[i] <= 0.0f && g_street_blood[i]) { g_street_blood[i]->live = 0; g_street_blood[i] = 0; }
    }
    street_recognize(seconds);
}

/* The pedestrian pass of a swing (func_000733AD.c 96..110). */
static int street_hit(const Camera *cam)
{
    int i, struck, bits;

    if (world_where()->where.kind == WHERE_DUNGEON) { return 0; }
    struck = 0;
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;
        float dx, dz, distance, dy;

        e = people_at(i);
        if (!e || !people_is_citizen(e) || dungeon_index(e) >= 0) { continue; }
        dx = e->x - cam->x; dz = e->z - cam->z; dy = move_body()->feet[1] - e->y;
        distance = df_sqrt(dx * dx + dz * dz + dy * dy);
        if (distance > 90.0f) { continue; }
        if (distance > 10.0f && df_cos(df_atan2(dx, dz) - cam->yaw) <= df_cos(200.0f * DF_TAU / 2048.0f)) { continue; }
        /* pedestrian_killed: the victim is still among the people, so they have seen it; murder, and the guards. */
        bits = street_witnesses();
        if (bits) { law_note(CRIME_MURDER); }
        street_blood(e);
        sound_at(17,e->x,e->y,e->z);
        people_fell(e);
        if (bits) { guards_summon(1); }
        guild_count_crime(GUILD_COUNT_KILL, 5, game_state()->minutes);
        struck++;
    }
    return struck;
}

/* A creature of the watch killed: the body is laid, the guard's corpse. */
static void street_died(int kind, Entity *e)
{
    if (kind == ENEMY_CITY_WATCH && people_index(e) >= 0) {
        if (!people_lay_corpse(e)) { e->live = 0; }
        /* The body's things lie on the ground there, a pile the use key opens (the street has no corpse usables). */
        {
            df_u32 pile;

            /* The watch has no loot table (class creatures carry their gear): the pile is made empty and the guard's gear made into it. */
            pile = ground_pile(e->x,e->y,e->z,dungeon_loot_list(dungeon_index(e)));
            dungeon_gear_to(dungeon_index(e),pile);
        }
    } else if (encounter_outdoor_actor(e)) {
        df_u32 pile;
        int index, table;

        index = dungeon_index(e);
        table = dungeon_loot_list(index);
        if (table < 0 && kind < 128) { return; }
        pile = ground_pile(e->x,e->y,e->z,table);
        dungeon_gear_to(index,pile);
    }
}

/* court_remove_creatures: the watch is sent away. */
static void street_dismiss(void)
{
    int i;

    for (i = 0; i < dungeon_count(); i++) {
        Entity *e;

        if (dungeon_kind(i) != ENEMY_CITY_WATCH) { continue; }
        e = dungeon_entity(i);
        if (people_index(e) >= 0) {
            dungeon_release_actor(i);
            people_fell(e);
        } else {
            dungeon_remove(i);
        }
    }
}

static int guards_dungeon(void) { return world_where()->where.kind == WHERE_DUNGEON; }

void gfall_street_open(void)
{
    mem_zero(g_street_blood, sizeof(g_street_blood));
    mem_zero(g_street_blood_left, sizeof(g_street_blood_left));
    g_watch_recognition = 0.0f;
}

void gfall_street_install(void)
{
    static const GuardWorld world = {
        seam_guards_present, guards_count, street_spawn, street_witnesses,
        guards_dungeon, street_dismiss, street_convert
    };

    dungeon_set_street(street_hit, street_died);
    guards_set_world(&world);
}
