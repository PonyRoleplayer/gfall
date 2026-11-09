#ifndef OGFALL_COMBATDEFS_H
#define OGFALL_COMBATDEFS_H

/* Immutable FALL.EXE combat tables and MONSTER.BSA scripts. See docs/ogfall.md. */
#include "engine/core/df.h"

typedef struct OgFoeDef {
    char name[24];
    int dice, hp_bonus, armor, loot, metal, flags, level;
    int attribute[8], minimum[5], maximum[5];
} OgFoeDef;

typedef struct OgAnimScript { const df_u8 *bytes; int size; } OgAnimScript;

const OgFoeDef *og_foe_def(int kind);
const OgAnimScript *og_foe_script(int kind);
const OgAnimScript *og_foe_type_script(int texture_kind);
int og_foe_type_flags(int texture_kind);
int og_foe_weight(int kind);
int og_foe_category(int kind);
int og_foe_parries(int kind);
int og_foe_texture_kind(int kind);
void og_foe_corpse(int kind, int *file, int *record);
int og_foe_flags(int kind);
int og_foe_loot(int kind);
int og_foe_hp_bonus(int kind);
int og_weapon_cif(int index);
int og_weapon_skill(int index);
int og_swing_group(int sector);
int og_swing_hit(int sector);
int og_swing_damage(int sector);
int og_swing_sound(int index);
int og_struck_part(int roll);

#endif
