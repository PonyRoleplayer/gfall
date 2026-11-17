#ifndef GFALL_ENCOUNTER_H
#define GFALL_ENCOUNTER_H

/* Original dungeon rest encounters. Live actors are kept entities of the engine's pool (docs/entities.md, Kept): the rolled six choices are saved here, the creatures themselves
 * (what they are, where they stand, health and AI) in their keyed Keep rows. See docs/game.md. */
#include "engine/scene/entity.h"
#include "engine/gfx/camera.h"
#define ENCOUNTERS 8
/*
 * encounter_pick_monster (mplace.c): the level band of a pick: ENC_BAND_NEAR_CHANCE percent level +- ENC_BAND_NEAR, ENC_BAND_LOW_CHANCE percent up to level + ENC_BAND_LOW_HIGH, else up to the
 * top of the table when the level is over ENC_BAND_WIDE_LEVEL (level + ENC_BAND_WIDE_LOW_HIGH if not); a low under 0 gives 0..ENC_TABLE_FLOOR_HIGH, a high over ENC_TABLE_LAST gives
 * ENC_TABLE_CEIL_LOW..ENC_TABLE_LAST. The land table has ENC_TABLE_SIZE entries, seeded by the location's number (ENC_SEED_MASK of the place key: it must be the number), and a rest takes
 * ENC_REST_CHOICES of them from ENC_REST_FIRST. A roll is made every ENC_ROLL_MINUTES minutes and succeeds 1 in 36 by day (the roll's top ENC_CHANCE_DAY) and 1 in 24 by night
 * (ENC_CHANCE_NIGHT). The wilderness table of a climate is its base plus ENC_TABLE_DAY by day and ENC_TABLE_NIGHT by night.
 */
#define ENC_BAND_NEAR_CHANCE 80
#define ENC_BAND_LOW_CHANCE 95
#define ENC_BAND_NEAR 3
#define ENC_BAND_LOW_HIGH 1
#define ENC_BAND_WIDE_LEVEL 5
#define ENC_BAND_WIDE_LOW_HIGH 2
#define ENC_TABLE_LAST 19
#define ENC_TABLE_FLOOR_HIGH 5
#define ENC_TABLE_CEIL_LOW 14
#define ENC_TABLE_SIZE 256
#define ENC_SEED_MASK 0xffffu
#define ENC_REST_FIRST 1
#define ENC_REST_CHOICES 6
#define ENC_ROLL_MINUTES 12
#define ENC_CHANCE_DAY 35
#define ENC_CHANCE_NIGHT 23
/* The wilderness tables by climate (FALL.EXE's climate_categories): where each climate's three (day, night, and the base) begin. The swamp has no day or night table. */
#define ENC_TABLE_DESERT 20
#define ENC_TABLE_MOUNTAIN 23
#define ENC_TABLE_RAINFOREST 26
#define ENC_TABLE_SWAMP 3
#define ENC_TABLE_SUBTROPICAL 29
#define ENC_TABLE_WOODS 32
#define ENC_TABLE_HAUNTED 35
#define ENC_TABLE_DAY 1
#define ENC_TABLE_NIGHT 2
/* Ours: by day outdoors a ghost or a wraith is rerolled up to ENC_DAY_REROLLS times; a creature further than ENC_OUTDOOR_RETIRE is retired; where a creature is put (distances from the player,
 * and how far up to look for a floor): a quest foe on a marker, a summoned one (mplace.c: 96..300), a hidden one (384..768), and one ENC_AHEAD in front of the player (1024). */
#define ENC_DAY_REROLLS 32
#define ENC_OUTDOOR_RETIRE 3072
#define ENC_AT_OUTER 192
#define ENC_AT_CLIMB 256
#define SUMMON_INNER 96
#define SUMMON_OUTER 300
#define SUMMON_CLIMB 128
#define ENC_HIDDEN_INNER 384
#define ENC_HIDDEN_OUTER 768
#define ENC_HIDDEN_CLIMB 384
#define ENC_AHEAD 1024
#define ENC_AHEAD_RING 128
#define ENC_AHEAD_CLIMB 1536
/* The saved actor table of earlier schemas: no longer filled (the keep rows say what each creature is), but kept in the layout so a save's bytes stay as they were. */
typedef struct EncounterRoll {
    KeepKey key;
    int kind, archive;
    float x, y, z;
} EncounterRoll;
typedef struct EncounterState {
    df_u32 place, serial;
    int choices[6];
    EncounterRoll actor[ENCOUNTERS];
} EncounterState;
EncounterState *encounter_state(void);
/* Per-place resource preparation, before the card sees the Level. */
void encounter_prepare(df_u32 place, int dungeon_type);
int encounter_open(const Scene *scene, int (*art)(int kind, int archive, Entity *out, int *rate));
typedef struct EncounterOutdoors {
    /* Climate at the camera, including ocean, and whether it is inside a settlement. */
    int (*site)(const Camera *camera, int *settlement);
    int (*art)(int kind, int archive, Entity *out, int *rate);
    void (*release_art)(const Entity *entity);
} EncounterOutdoors;
void encounter_open_outdoors(const Scene *scene, const EncounterOutdoors *world);
void encounter_update(const Scene *scene, const Camera *camera);
int encounter_outdoor_actor(const Entity *entity);
void encounter_close(void);
int encounter_try(const Camera *camera);
/* A creature of a kind placed in the dungeon by something other than the rest's chance (a quest's foe): it takes a free actor slot like a rest encounter, with this key thing, unseen 384 to 768 units
 * from the camera where there is floor. 0 when there is no room or no floor. */
int encounter_place(const Camera *camera, int kind, df_u32 thing);
/* The same on the floor near a point (a quest marker's), not unseen; 0 while the point has no floor loaded, to be tried again. */
int encounter_place_at(const Camera *camera, int kind, df_u32 thing, float x, float y, float z);
int encounter_summon(const Camera *camera, int kind, int team);
int encounter_release_soul(const Camera *camera, int kind);
void encounter_capture(void);
enum { ENCOUNTER_REST, ENCOUNTER_LOITER, ENCOUNTER_WALK };
/* Returns true only when an actor was successfully placed. */
int encounter_minute(const Scene *scene, const Camera *camera, df_u32 minute, int activity);
int encounter_pick(int dungeon_type, int level);

#endif
