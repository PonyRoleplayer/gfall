#ifndef OGFALL_MAGICDEFS_H
#define OGFALL_MAGICDEFS_H
#include "engine/core/df.h"
typedef struct OgDiseaseDef {
    int id, stat[11], minimum, maximum, days, stage;
} OgDiseaseDef;
int og_effect_flags(int group, int sub);
int og_effect_school(int group);
int og_effect_formula(int group);
const df_i16 *og_effect_costs(int group, int sub);
int og_target_factor(int target);
int og_element_bit(int element);
df_u32 og_resist_flag(int element);
int og_cast_sound(int element);
int og_impact_sound(int element);
const OgDiseaseDef *og_disease_def(int id);
const df_i16 *og_poison_def(int id);
const df_u8 *og_creature_spells(int kind, int level);
const df_u8 *og_creature_diseases(int kind);
int og_creature_ignores_illusion(int kind);
int og_dispel_kind(int kind, int category);
#endif
