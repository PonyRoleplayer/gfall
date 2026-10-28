#ifndef DF_CLASSCFG_H
#define DF_CLASSCFG_H

#include "engine/core/df.h"

/*
 * CLASS00.CFG .. CLASS17.CFG: classic's 18 classes, 74 bytes each, as
 * Daggerfall Unity's ClassFile reads them (CLASS18.CFG is a second
 * Knight, not a class of the list):
 *
 *   0  resistances, immunities, low tolerances, critical weaknesses (u8 each)
 *   4  special abilities and spell points (u16): the multiplier in bits 10..12
 *   6  rapid healing, regeneration, unknown, spell absorption, attack modifier (u8 each)
 *   11 forbidden materials (u16)
 *   13 weapons, armour and shields (3 bytes: a << 16 | c << 8 | b)
 *   16 primary skills (3), major (3), minor (6): skill numbers 0..34
 *   28 name (16)
 *   44 unknown (8)
 *   52 hit points a level (u16)
 *   54 advancement multiplier (u32)
 *   58 attributes (8 u16): strength, intelligence, willpower, agility,
 *      endurance, personality, speed, luck
 */

#define CLASSCFG_BYTES 74
#define CLASSCFG_COUNT 18

typedef struct ClassCfg {
    char name[16];
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
    df_u8 primary[3];
    df_u8 major[3];
    df_u8 minor[6];
    df_u16 hp_per_level;
    df_u32 advancement;
    df_u16 attribute[8];
} ClassCfg;

/* One class from its file's bytes. 0 when short. */
int classcfg_read(const df_u8 *b, df_u32 n, ClassCfg *out);
/* The spell points an Intelligence point gives: 3, 2, 1.75, 1.5, 1 or 0.5, from abilities' bits 10..12. */
float classcfg_magicka(df_u16 abilities);
/* "CLASS07.CFG" for class 7. */
void classcfg_name(int index, char *dst);

#endif
