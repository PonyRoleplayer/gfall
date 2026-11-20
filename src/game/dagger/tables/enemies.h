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
    /* Its armour value, as EnemyBasics' ArmorValue: lower is better; a body part's is five times it. */
    signed char armor;
    df_u8 map_chance;
    df_u16 weight;
    df_u32 soul_points;
    char loot[4];
    /*
     * Strength, intelligence, willpower, agility, endurance, personality,
     * speed, luck (ATTR_*): a monster's from its ENEMY###.CFG in
     * MONSTER.BSA, a class enemy's from its CLASS##.CFG (DFU's
     * GetMonsterCareerTemplate and the class's career),
     * filled in by the import: classic.c's rows leave them 0.
     */
    df_u8 attribute[8];
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
/* The groups a class's attack bonus or phobia names (FormulaHelper.GetEnemyEntityEnemyGroup). A class enemy is in none. */
enum { ENEMY_GROUP_NONE, ENEMY_GROUP_UNDEAD, ENEMY_GROUP_DAEDRA, ENEMY_GROUP_HUMANOID, ENEMY_GROUP_ANIMALS };
int enemy_group(int kind);
#define ENEMY_RAT 0
#define ENEMY_IMP 1
#define ENEMY_SPRIGGAN 2
#define ENEMY_SPIDER 6
#define ENEMY_ORC 7
#define ENEMY_CENTAUR 8
#define ENEMY_NYMPH 10
#define ENEMY_ORC_SERGEANT 12
#define ENEMY_GIANT_SCORPION 20
#define ENEMY_ORC_SHAMAN 21
#define ENEMY_ORC_WARLORD 24
#define ENEMY_SLAUGHTERFISH 11
/* Those whose blow can pass on a disease or a curse (ills.c). */
#define ENEMY_GIANT_BAT 3
#define ENEMY_WEREWOLF 9
#define ENEMY_WEREBOAR 14
#define ENEMY_ZOMBIE 17
#define ENEMY_MUMMY 19
#define ENEMY_VAMPIRE 28
#define ENEMY_VAMPIRE_ANCIENT 30
/* Blunt weapons do it half, silver twice (DFU's CalculateWeaponAttackDamage). */
#define ENEMY_SKELETAL_WARRIOR 15
#define ENEMY_GHOST 18
#define ENEMY_WRAITH 23
#define ENEMY_DREUGH 41
#define ENEMY_LAMIA 42
#define ENEMY_DAEDROTH 27
#define ENEMY_FIRST_CLASS 128
/* City Watch: archive 399, bark DAGGER.SND 456 (Halt). */
/* The kinds the Wabbajack turns a creature into (wabbajack_creatures, FALL.EXE). */
static const unsigned char g_wabbajack_kinds[17] = { 0, 1, 2, 3, 4, 6, 10, 13, 15, 16, 17, 20, 36, 37, 38, 35, 32 };
/* The assassin's mobile id (128 + the class), whose weapon is poisoned most often (monster_give_equipment). */
#define ENEMY_ASSASSIN 139
#define ENEMY_CITY_WATCH 146
/* A watchman's level is the player's plus rand_range(3, 6) (monster_init, func_00078566.c 139; the other class enemies have the player's level). */
#define WATCH_LEVEL_MIN 3
#define WATCH_LEVEL_MAX 6

/*
 * The bands of mobile ids. The classic mobiles (rat .. lamia) are 0..ENEMY_CLASSIC_KINDS - 1. A class enemy is ENEMY_FIRST_CLASS + the class (ENEMY_CLASS_COUNT of
 * them); ENEMY_CITY_WATCH, the next id, is the guard that wears a class's gear. A fighter's race is its mobile id, and ENEMY_RACE_CLASS (one past the classic mobiles)
 * for a class enemy.
 */
#define ENEMY_CLASSIC_KINDS 43
#define ENEMY_RACE_CLASS ENEMY_CLASSIC_KINDS
#define ENEMY_CLASS_COUNT 18
#define ENEMY_IS_CLASSIC(kind) ((kind) >= 0 && (kind) < ENEMY_CLASSIC_KINDS)
/* A class enemy, or the watch, which is a class enemy in a guard's picture. */
#define ENEMY_IS_CLASS(kind) ((kind) >= ENEMY_FIRST_CLASS)
/* A class enemy only: not the watch. */
#define ENEMY_IS_CLASS_ONLY(kind) ((kind) >= ENEMY_FIRST_CLASS && (kind) < ENEMY_CITY_WATCH)
/* An id that names a mobile at all: a classic one, a class enemy or the watch. */
#define ENEMY_IS_KNOWN(kind) (ENEMY_IS_CLASSIC(kind) || ((kind) >= ENEMY_FIRST_CLASS && (kind) <= ENEMY_CITY_WATCH))
/* The picture archive of a mobile (foe_archive in combatdefs.c): TEXTURE.255 + kind, the watch's 399, and a class's 432 + its picture kind (433 the other sex's). */
#define ENEMY_ARCHIVE_CLASSIC 255
#define ENEMY_ARCHIVE_WATCH 399
#define ENEMY_ARCHIVE_CLASS 432
/* DAGGER.SND: a mobile's voice is ENEMY_VOICE_BASE + kind * ENEMY_VOICE_STRIDE + its variant (asleep 0, awake 1, close 2). */
#define ENEMY_VOICE_BASE 10000
#define ENEMY_VOICE_STRIDE 10

/* monster_table's flags: a flyer needs no floor, an aquatic creature no dry one. */
#define FOE_FLAG_FLYING 1
#define FOE_FLAG_AQUATIC 64
#define FOE_FLAGS_NO_FLOOR (FOE_FLAG_FLYING | FOE_FLAG_AQUATIC)

/*
 * A creature's numbers at its making (monster_init, monster_give_spells; the decomp leaves them inline): every skill is level * FOE_SKILL_PER_LEVEL + FOE_SKILL_BASE, held to
 * FOE_SKILL_MAX; health is FOE_HIT_DIE-sided dice; a creature that casts has FOE_CASTER_MAGICKA_BASE + FOE_CASTER_MAGICKA_PER_LEVEL a level of magicka and its six magic skills at
 * FOE_CASTER_MAGIC_SKILL. A fighter's armour is ARMOR_SCALE times its row's armour in each of BODY_PARTS places.
 */
#define FOE_SKILL_PER_LEVEL 5
#define FOE_SKILL_BASE 30
#define FOE_SKILL_MAX 100
#define FOE_HIT_DIE 8
#define FOE_CASTER_MAGICKA_PER_LEVEL 10
#define FOE_CASTER_MAGICKA_BASE 100
#define FOE_CASTER_MAGIC_SKILL 80
#define FOE_ARMOR_SCALE 5
/* A class enemy's armour is its creature row's (class_creature_types, 0x1878FC, then monster_table rows 43..59 at 0x18487A: 4..8) times FOE_ARMOR_SCALE; entry
 * ENEMY_CLASS_COUNT is the city watch's (class 18, "a guard"). `class` 0..ENEMY_CLASS_COUNT. */
int enemy_class_armor(int klass);

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

/*
 * One creature from a dungeon type's encounter table (DUN_TYPE_*), by the player's level as the
 * markers' draws are (ChooseRandomEnemyType), on the game's own seed: what an interrupted rest
 * brings (DFU's RandomEncounters.ChooseRandomEnemy). -1 for a type with no table.
 */
int enemy_encounter(int dungeon_type, int level, df_u32 seed);

/* A block's water level from its start marker (record 10), classic's sense; ENEMY_DRY for none. */
int enemy_water(const Marker *marks, int n);

#endif
