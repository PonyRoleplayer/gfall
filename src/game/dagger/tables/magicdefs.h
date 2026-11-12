#ifndef GFALL_MAGICDEFS_H
#define GFALL_MAGICDEFS_H
#include "engine/core/df.h"
typedef struct DiseaseDef {
    int id, stat[11], minimum, maximum, days, stage;
} DiseaseDef;
int effect_flags(int group, int sub);
int effect_school(int group);
int effect_formula(int group);
const df_i16 *effect_costs(int group, int sub);
int target_factor(int target);
/* The spell maker's tables: 0 the caster only, 1 touch and beyond, 2 any; the elemental level (0 for none); a subtype's name, 0 past the last. */
int effect_target_class(int group);
int effect_element_level(int group);
const char *effect_sub_name(int group, int sub);
int element_bit(int element);
df_u32 resist_flag(int element);
int cast_sound(int element);
int impact_sound(int element);
const DiseaseDef *disease_def(int id);
const df_i16 *poison_def(int id);
const df_u8 *creature_spells(int kind, int level);
const df_u8 *creature_diseases(int kind);
int creature_ignores_illusion(int kind);
int dispel_kind(int kind, int category);
#endif
