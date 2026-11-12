#include "game/dagger/tables/magicdefs.h"
#include "game/dagger/tables/magicdata.h"
#include "game/dagger/tables/combatdefs.h"
int effect_flags(int group, int sub) { return group >= 0 && group < 51 ? g_effect_flags[group][sub >= 0 && sub < 12 ? sub : 0] : 0; }
int effect_school(int group) { return group >= 0 && group < 51 ? g_school_skill[g_school[group]] : 0; }
int effect_formula(int group) { return group >= 0 && group < 51 ? g_formula[group] : 0; }
const df_i16 *effect_costs(int group, int sub)
{
    return group >= 0 && group < 51 ? g_cost[g_cost_index[group][sub >= 0 && sub < 12 ? sub : 0]] : 0;
}
int target_factor(int target) { return target >= 0 && target < 5 ? g_target_factor[target] : 0; }
int element_bit(int element) { return element >= 0 && element < 5 ? g_element_bit[element] : 0; }
df_u32 resist_flag(int element) { return element >= 0 && element < 5 ? g_resist_flag[element] : 0; }
int cast_sound(int element) { return element >= 0 && element < 5 ? g_cast_sound[element] : 0; }
int impact_sound(int element) { return element >= 0 && element < 5 ? g_impact_sound[element] : 0; }
const DiseaseDef *disease_def(int id) { return id >= 0 && id < 17 ? &g_disease[id] : 0; }
const df_i16 *poison_def(int id) { return id >= 128 && id < 140 ? g_poison[id-128] : 0; }
const df_u8 *creature_spells(int kind, int level)
{
    if (kind >= 128 && kind <= 146 && (foe_flags(kind) & 2)) {
        level /= 3; if (level > 6) { level = 6; } if (level < 0) { level = 0; }
        return g_class_spells[level];
    }
    return kind >= 0 && kind < 43 ? g_creature_spells[kind] : 0;
}
int dispel_kind(int kind, int category)
{
    int i;
    if (category < 0 || category > 1) { return 0; }
    for (i = category*7; i < category*7+7; i++) { if (g_undead_daedra[i] == kind) { return 1; } }
    return 0;
}
int creature_ignores_illusion(int kind) { return dispel_kind(kind,0) || dispel_kind(kind,1); }
const df_u8 *creature_diseases(int kind) { return kind == 3 ? g_bat_disease : kind == 19 ? g_mummy_disease : g_plague_disease; }
/* The spell maker's tables (spells.c): the targets an effect allows, its elemental level (the original reads 255 for an empty slot, past the 51 entries: the bytes that follow), the name of a subtype. */
int effect_target_class(int group) { return group >= 0 && group < 51 ? g_target_class[group] : 2; }
int effect_element_level(int group) { return group >= 0 && group < 256 ? g_element_level[group] : 0; }
const char *effect_sub_name(int group, int sub) { return group >= 0 && group < 51 && sub >= 0 && sub < 12 ? g_effect_sub_name[group][sub] : 0; }
