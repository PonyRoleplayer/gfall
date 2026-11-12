#ifndef GFALL_COMBATDEFS_H
#define GFALL_COMBATDEFS_H

/* Immutable FALL.EXE combat tables and MONSTER.BSA scripts. See docs/game.md. */
#include "engine/core/df.h"

typedef struct FoeDef {
    char name[24];
    int dice, hp_bonus, armor, loot, metal, flags, level;
    int attribute[8], minimum[5], maximum[5];
} FoeDef;

typedef struct AnimScript { const df_u8 *bytes; int size; } AnimScript;

const FoeDef *foe_def(int kind);
const AnimScript *foe_script(int kind);
const AnimScript *foe_type_script(int texture_kind);
int foe_type_flags(int texture_kind);
int foe_weight(int kind);
int foe_category(int kind);
int foe_parries(int kind);
int foe_texture_kind(int kind);
void foe_corpse(int kind, int *file, int *record);
int foe_flags(int kind);
int foe_loot(int kind);
int foe_hp_bonus(int kind);
int weapon_cif(int index);
int weapon_skill(int index);
int swing_group(int sector);
int swing_hit(int sector);
int swing_damage(int sector);
int swing_sound(int index);
int struck_part(int roll);
int foe_nav_angle(int octant);
int foe_encounter(int table, int index);

#endif
