#ifndef DF_CAREERS_H
#define DF_CAREERS_H

#include "engine/assets/recfile.h"

/*
 * What a character is built from, as records (docs/data.md):
 *
 *   CLAS: the classes, by index 0..17, cooked from ARENA2's CLASS*.CFG
 *         (formats/classcfg.c): base attributes, skills, hit points a
 *         level, spell points an Intelligence point, resistances.
 *   RACE: the races, by DFU's ids 1..8 (Breton .. Argonian): pictures,
 *         resistances; classic keeps them inside FALL.EXE.
 *   SKIL: the 35 skills, by number: name, governing attribute, how hard
 *         it is to advance; inside FALL.EXE too.
 *
 * Read docs/game.md (The player) before editing.
 */

/* The eight attributes, in the class files' order. */
enum {
    ATTR_STRENGTH,
    ATTR_INTELLIGENCE,
    ATTR_WILLPOWER,
    ATTR_AGILITY,
    ATTR_ENDURANCE,
    ATTR_PERSONALITY,
    ATTR_SPEED,
    ATTR_LUCK,
    ATTR_COUNT
};

/* Skill numbers (DFU's DFCareer.Skills), the ones the code names. */
#define SKILL_COUNT 35
#define SKILL_ETIQUETTE 1
#define SKILL_STREETWISE 2
#define SKILL_JUMPING 3
#define SKILL_LOCKPICKING 13
#define SKILL_MERCANTILE 14
#define SKILL_PICKPOCKET 15
#define SKILL_STEALTH 16
#define SKILL_SWIMMING 17
#define SKILL_RUNNING 21
#define SKILL_MEDICAL 0
/* The ones combat rolls (combat.c). */
#define SKILL_BACKSTABBING 19
#define SKILL_DODGING 20
#define SKILL_SHORT_BLADE 28
#define SKILL_HAND_TO_HAND 30
#define SKILL_ARCHERY 33
#define SKILL_CRITICAL_STRIKE 34

/* EffectFlags: what a race or class resists, is immune to, and so on. */
enum {
    EFFECT_PARALYSIS = 1,
    EFFECT_MAGIC = 2,
    EFFECT_POISON = 4,
    EFFECT_FIRE = 8,
    EFFECT_FROST = 16,
    EFFECT_SHOCK = 32,
    EFFECT_DISEASE = 64
};

/* Race ids (DFU's Races). */
enum {
    RACE_BRETON = 1,
    RACE_REDGUARD,
    RACE_NORD,
    RACE_DARK_ELF,
    RACE_HIGH_ELF,
    RACE_WOOD_ELF,
    RACE_KHAJIIT,
    RACE_ARGONIAN
};

/* Class numbers (the CLASS*.CFG files), the ones the code names. */
#define CLASS_MAGE 0
#define CLASS_WARRIOR 16
/* The one class the player made (create.c, career_set_custom): not in the table, nor in MAIN.DAT. */
#define CLASS_CUSTOM 18

typedef struct CareerClass {
    df_u16 id;
    char name[16];
    df_u16 hp_per_level;
    df_u16 attribute[ATTR_COUNT];
    df_u8 primary[3];
    df_u8 major[3];
    df_u8 minor[6];
    /* Spell points an Intelligence point gives. */
    float magicka;
    df_u8 resist;
    df_u8 immune;
    df_u8 low_tolerance;
    df_u8 weakness;
    df_u16 abilities;
    df_u8 rapid_healing;
    df_u8 regeneration;
    df_u8 absorption;
    df_u8 attack_modifier;
    df_u16 forbidden_materials;
    df_u32 weapons;
    df_u32 advancement;
} CareerClass;

typedef struct CareerRace {
    df_u16 id;
    char name[16];
    /* TEXT.RSC's description, DAGGER.SND's clip at the race's choosing. */
    df_u16 description;
    df_u16 clip;
    /* The paper doll's pictures: background, bodies (unclothed, clothed), heads. */
    char background[13];
    char body_male[13];
    char body_male_clothed[13];
    char body_female[13];
    char body_female_clothed[13];
    char heads_male[13];
    char heads_female[13];
    df_u8 resist;
    df_u8 immune;
    df_u8 low_tolerance;
    df_u8 weakness;
    df_u16 abilities;
} CareerRace;

typedef struct CareerSkill {
    df_u16 id;
    char name[16];
    /* ATTR_*. */
    df_u8 attribute;
    /* How hard it is to advance (DFU's GetAdvancementMultiplier). */
    df_u8 advance;
} CareerSkill;

extern const RecSchema class_schema;
extern const RecSchema race_schema;
extern const RecSchema skill_schema;
RecTable *class_table(void);
RecTable *race_table(void);
RecTable *skill_table(void);
const CareerClass *career_class(int id);
/* The player's own class, by CLASS_CUSTOM's number, until the process ends or another replaces it. */
void career_set_custom(const CareerClass *c);
const CareerRace *career_race(int id);
const CareerSkill *career_skill(int id);

/* A class record from a CLASS*.CFG's bytes; 0 when short. */
int career_from_cfg(int id, const df_u8 *bytes, df_u32 n, CareerClass *out);

#endif
