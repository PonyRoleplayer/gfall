#ifndef DF_ENEMIES_H
#define DF_ENEMIES_H

#include "engine/assets/recfile.h"
#include "game/dagger/formats/rdb.h"

/*
 * The creatures a dungeon's blocks place: which kind stands at each
 * editor marker, and the pictures of each kind. Daggerfall's rules as
 * Daggerfall Unity has them (RDBLayout, EnemyBasics, RandomEncounters),
 * read for what they do. Read docs/entities.md before editing.
 *
 * A kind is Daggerfall's mobile id (DFU's MobileTypes): 0..42 the
 * monsters (a rat 0, a lich 32), 128..146 people of a class (a mage
 * 128, a knight 145, the city watch 146). Whether one is friend or foe
 * is the game's later business; here they only exist, where the file
 * puts them. Plain C: hostcheck runs it.
 *
 * The kinds and the encounter tables are records (docs/data.md): the
 * ENEM and ENCT chunks of MAIN.DAT, which the first run cooks from
 * classic's tables (classic.c), and of any mod's .DAT over it. Nothing
 * here knows a kind's numbers; a mod may add kinds past 146.
 */

/* A kind: EnemyBasics' MobileEnemy, field for field where classic has it. */
typedef struct EnemyKind {
    df_u16 id;
    char name[24];
    /* ENEMY_GENERAL..ENEMY_GUARD; flying, spectral and aquatic follow from it. */
    df_u8 behaviour;
    /* DFU's MobileAffinity and MobileTeams, by number. */
    df_u8 affinity;
    df_u8 team;
    df_u8 has_idle;
    df_u8 ranged1;
    df_u8 ranged2;
    df_u8 opens_doors;
    df_u8 prefers_ranged;
    df_u8 parries;
    df_u8 casts_magic;
    df_u8 sees_invisible;
    /* TEXTURE archives: a man's and a woman's (a monster's are the same); 0 for none. */
    df_u16 male;
    df_u16 female;
    df_u16 corpse_archive;
    df_u16 corpse_record;
    /* TEXTURE.380's blood splash. */
    df_u8 blood;
    /* DAGGER.SND clips, by position. */
    df_i16 move_sound;
    df_i16 bark_sound;
    df_i16 attack_sound;
    /* The least weapon material that hurts it, -1 for any. */
    signed char min_metal;
    /* A swing's first, second and third hit. */
    df_u8 damage_min[3];
    df_u8 damage_max[3];
    df_u16 health_min;
    df_u16 health_max;
    df_u8 level;
    signed char armor;
    df_u8 map_chance;
    df_u16 weight;
    df_u32 soul_points;
    char loot[4];
} EnemyKind;

/* EnemyKind.behaviour: DFU's MobileBehaviour. */
enum { ENEMY_GENERAL, ENEMY_FLYING, ENEMY_AQUATIC, ENEMY_SPECTRAL, ENEMY_GUARD };

/* An encounter table: kinds, weakest first, by dungeon type (DUN_TYPE_*), or the underwater one. */
#define ENCOUNTER_LEN 20
#define ENCOUNTER_UNDERWATER 19
typedef struct Encounter {
    df_u16 id;
    /* Kinds in use: 20, but the cemetery's 11. */
    df_u8 count;
    df_u16 kind[ENCOUNTER_LEN];
} Encounter;

extern const RecSchema enemy_schema;
extern const RecSchema encounter_schema;
/* The merged tables (docs/data.md), empty until a .DAT is merged. */
RecTable *enemy_table(void);
RecTable *encounter_table(void);
/* A kind's record, or 0. */
const EnemyKind *enemy_kind(int kind);

/* Mobile ids. Classic's run below ENEMY_KINDS; a mod may add more past it. */
#define ENEMY_KINDS 147
#define ENEMY_RAT 0
#define ENEMY_SLAUGHTERFISH 11
#define ENEMY_GHOST 18
#define ENEMY_WRAITH 23
#define ENEMY_DREUGH 41
#define ENEMY_LAMIA 42
#define ENEMY_FIRST_CLASS 128

/* Editor records: a random creature, a fixed one. */
#define ENEMY_RANDOM_MARKER 15
#define ENEMY_FIXED_MARKER 16

/* A block's water level when it has none: classic's 10000, below everything. */
#define ENEMY_DRY 10000

/* Its pictures' archive (TEXTURE.###), -1 for no such kind. */
int enemy_archive(int kind, int female);
/* 1: it has standing pictures (records 15..19); else it stands in its walking ones. */
int enemy_has_idle(int kind);
/* 1: it flies, hanging where the marker is (an imp, a bat, a harpy, a dragonling). */
int enemy_flies(int kind);
/* 1: a ghost or a wraith, which float where the marker is as flyers do. */
int enemy_spectral(int kind);
/* 1: it lives in water only (a slaughterfish, a dreugh, a lamia). */
int enemy_aquatic(int kind);

/*
 * A dungeon's draws, before its markers: classic seeds its rand with the
 * location id and draws 256 kinds from the dungeon's encounter table
 * (DUN_TYPE_*) and 256 from the underwater one; a random marker takes
 * the one in its slot. level is the player's, which steers the draws
 * toward the table's start (1 for now: no player yet).
 */
void enemy_dungeon(int location_id, int dungeon_type, int level);

/*
 * The creature at an editor marker, given its block's water level
 * (classic's: from the block's start marker, 8 units a step, down
 * positive; ENEMY_DRY for none): its kind, and 1 in *female for a
 * woman of a class. -1 when the marker places none: not a creature
 * marker, a fixed one of the invalid kind 99 or of no pictures, or a
 * water dweller where no water is. *passive is 1 when the file says it
 * attacks only when attacked (its action byte, 99); the game decides
 * what that means.
 */
int enemy_at(const Marker *mk, int water, int *female, int *passive);

/* A block's water level from its start marker (record 10), classic's sense; ENEMY_DRY for none. */
int enemy_water(const Marker *marks, int n);

#endif
