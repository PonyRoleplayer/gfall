#ifndef DF_MAGIC_H
#define DF_MAGIC_H

#include "engine/assets/recfile.h"

/*
 * Magic as records (docs/data.md), classic's effects as Daggerfall Unity
 * carries them and its 89 standard spells from SPELLS.STD. Read
 * docs/game.md (Magic) before editing.
 *
 *   EFCT: an effect, by classic's group and subgroup (id = group x 256 +
 *         subgroup, subgroup 255 for none), the number SPELLS.STD stores. Its
 *         names, its school (the skill that casts it and lowers its cost),
 *         which of duration, chance and magnitude it supports, the targets
 *         and elements it allows (bits: see below), and its cost of each
 *         parameter (A, B and a flat offset: gold = offset + A x start +
 *         B x (increase / per level), docs/game.md, The cost).
 *   SPEL: a standard spell, by its place in the file: its name, icon,
 *         number (what a class's starting spells and a shop name), element,
 *         target, the price classic asks and its three effects' settings.
 *
 *   ESPL: the spells a creature casts (DFU's EnemyEntity lists, classic's): a row a spell, by
 *         the creature's kind (a monster's id) or, for a class enemy that casts magic, ESPL_CLASS
 *         plus its list by level (level / 3, at most 6), and the spell's number in SPELLS.STD.
 *
 * An effect of a spell is its group, its subgroup and the settings of each
 * parameter, EffectSettings. A parameter an effect does not support is left
 * at 1.
 */

/* Target bits: one for each of the spell's five reaches, as SPELLS.STD's target byte is the bit's place. */
#define MAGIC_TARGET_CASTER 1
#define MAGIC_TARGET_TOUCH 2
#define MAGIC_TARGET_RANGE 4
#define MAGIC_TARGET_AREA_CASTER 8
#define MAGIC_TARGET_AREA_RANGE 16
/* Every target but the caster: what an effect that harms another allows. */
#define MAGIC_TARGET_OTHER 30
#define MAGIC_TARGET_ALL 31
/* The targets by SPELLS.STD's number 0..4. */
enum {
    TARGET_CASTER,
    TARGET_TOUCH,
    TARGET_RANGE,
    TARGET_AREA_CASTER,
    TARGET_AREA_RANGE,
    TARGET_COUNT
};

/* The elements by SPELLS.STD's number 0..4; the bits of an effect's allowed elements are 1 << element. */
enum {
    ELEMENT_FIRE,
    ELEMENT_COLD,
    ELEMENT_POISON,
    ELEMENT_SHOCK,
    ELEMENT_MAGIC,
    ELEMENT_COUNT
};
#define MAGIC_ELEMENT_MAGIC_ONLY (1 << ELEMENT_MAGIC)
#define MAGIC_ELEMENT_ALL 31

/* Which parameters an effect supports. */
#define MAGIC_DURATION 1
#define MAGIC_CHANCE 2
#define MAGIC_MAGNITUDE 4

#define MAGIC_EFFECT_NONE 255
#define MAGIC_EFFECT_NAME 24
#define MAGIC_SUB_NAME 16
#define MAGIC_SPELL_NAME 26
#define MAGIC_SLOTS 3

/* An effect's id from its group and subgroup (255 for none). */
#define MAGIC_ID(group, sub) ((df_u16)((group) * 256 + (sub)))

/* The effect groups the code names (classic's numbers, SPELLS.STD's first byte of an effect). */
enum {
    GROUP_PARALYZE = 0,
    GROUP_CONTINUOUS_DAMAGE = 1,
    GROUP_CREATE_ITEM = 2,
    GROUP_CURE = 3,
    GROUP_DAMAGE = 4,
    GROUP_DISINTEGRATE = 5,
    GROUP_DISPEL = 6,
    GROUP_DRAIN = 7,
    GROUP_ELEMENTAL_RESISTANCE = 8,
    GROUP_FORTIFY = 9,
    GROUP_HEAL = 10,
    GROUP_TRANSFER = 11,
    GROUP_SOUL_TRAP = 12,
    GROUP_INVISIBILITY = 13,
    GROUP_LEVITATE = 14,
    GROUP_LIGHT = 15,
    GROUP_LOCK = 16,
    GROUP_OPEN = 17,
    GROUP_REGENERATE = 18,
    GROUP_SILENCE = 19,
    GROUP_SPELL_ABSORPTION = 20,
    GROUP_SPELL_REFLECTION = 21,
    GROUP_SPELL_RESISTANCE = 22,
    GROUP_CHAMELEON = 23,
    GROUP_SHADOW = 24,
    GROUP_SLOWFALL = 25,
    GROUP_FREE_ACTION = 26,
    GROUP_CLIMBING = 28,
    GROUP_LYCANTHROPY = 29,
    GROUP_WATER_BREATHING = 30,
    GROUP_WATER_WALKING = 31,
    GROUP_JUMPING = 32,
    GROUP_CALM = 33,
    GROUP_CHARM = 34,
    GROUP_SHIELD = 35,
    GROUP_DETECT = 39,
    GROUP_IDENTIFY = 40,
    GROUP_TELEPORT = 43,
    GROUP_COMPREHEND_LANGUAGES = 44
};

typedef struct MagicEffect {
    df_u16 id;
    char group[MAGIC_EFFECT_NAME];
    char sub[MAGIC_SUB_NAME];
    /* The skill that casts it (SKILL_DESTRUCTION and the others). */
    df_u8 school;
    df_u8 supports;
    df_u8 targets;
    df_u8 elements;
    /* A, B and the flat offset of each parameter's cost. */
    df_u16 duration_cost[3];
    df_u16 chance_cost[3];
    df_u16 magnitude_cost[3];
} MagicEffect;

/* One effect of a spell: which, and its parameters (what a spell maker's spinners set). */
typedef struct EffectSettings {
    df_u16 effect;
    df_u8 duration_base, duration_plus, duration_per;
    df_u8 chance_base, chance_plus, chance_per;
    df_u8 magnitude_base_low, magnitude_base_high, magnitude_plus_low, magnitude_plus_high, magnitude_per;
} EffectSettings;

typedef struct MagicSpell {
    df_u16 id;
    char name[MAGIC_SPELL_NAME];
    df_u8 icon;
    /* What guilds' lists and a class's starting spells call it. */
    df_u8 number;
    df_u8 element;
    df_u8 target;
    df_u16 price;
    /* An unused slot has effect[k] = MAGIC_ID(255, 255). */
    df_u16 effect[MAGIC_SLOTS];
    /* Flat, as a record's array is: slot k's duration is duration[3 k .. 3 k + 2] (base, plus, per level), its chance the same, its magnitude magnitude[5 k .. 5 k + 4] (base low, base high, plus low, plus high, per level). */
    df_u8 duration[MAGIC_SLOTS * 3];
    df_u8 chance[MAGIC_SLOTS * 3];
    df_u8 magnitude[MAGIC_SLOTS * 5];
} MagicSpell;

/* The kind number of a class enemy's lists: ESPL_CLASS + level / 3 (6 at most). */
#define ESPL_CLASS 1000
#define ESPL_CLASS_LISTS 7

typedef struct FoeSpell {
    df_u16 id;
    df_u16 kind;
    df_u8 number;
} FoeSpell;

extern const RecSchema foe_spell_schema;
RecTable *foe_spell_table(void);
/*
 * The standard spells (SPELLS.STD numbers) a creature of that kind casts at that level: a monster's own
 * list, a class enemy that casts magic its level's, else none. Up to max into out; how many.
 */
int magic_foe_spells(int kind, int level, df_u8 *out, int max);

extern const RecSchema effect_schema;
extern const RecSchema spell_schema;
RecTable *effect_table(void);
RecTable *spell_table(void);
/* The effect of that id (group x 256 + subgroup); 0 when MAIN.DAT has none. */
const MagicEffect *magic_effect(int id);
/* The standard spell of that number (SPELLS.STD's byte 73); 0 when there is none. */
const MagicSpell *magic_standard(int number);
/* An effect's settings out of a standard spell's slot k. */
void magic_settings(const MagicSpell *spell, int k, EffectSettings *out);

#endif
