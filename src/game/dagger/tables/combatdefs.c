#include "game/dagger/tables/combatdefs.h"
#include "game/dagger/tables/combatdata.h"

int foe_nav_angle(int octant) { return g_nav_angle[octant & 7]; }
int foe_encounter(int table, int index)
{
    return table >= 0 && table < 45 && index >= 0 && index < 20 ? g_encounter[table][index] : 0;
}

const FoeDef *foe_def(int kind) { return kind >= 0 && kind < 43 ? &g_foe[kind] : 0; }
const AnimScript *foe_script(int kind)
{
    return foe_type_script(foe_texture_kind(kind));
}

const AnimScript *foe_type_script(int type)
{
    /* Human type 43 selects ASCR0128, 44 ASCR0129, and so on. */
    return type >= 0 && type < 60 ? &g_anim[type] : 0;
}
int foe_type_flags(int type) { return type >= 0 && type < 60 ? g_monster[type][2] : 0; }
int foe_weight(int kind) { return kind >= 0 && kind < 43 ? g_weight[kind] : 80; }
int foe_category(int kind) { return kind >= 0 && kind < 43 ? g_category[kind] : 0; }
int foe_texture_kind(int kind) { return kind >= 128 && kind <= 146 ? g_class_kind[kind-128] : kind; }
static int monster_field(int kind, int field)
{
    int index;
    index = foe_texture_kind(kind);
    return index >= 0 && index < 60 ? g_monster[index][field] : 0;
}
int foe_flags(int kind) { return monster_field(kind,2); }
int foe_loot(int kind) { return monster_field(kind,1)-1; }
int foe_hp_bonus(int kind) { return monster_field(kind,0); }
void foe_corpse(int kind, int *file, int *record)
{
    *file = kind >= 0 && kind < 43 ? g_corpse[kind*2] : 380;
    *record = kind >= 0 && kind < 43 ? g_corpse[kind*2+1] : 1;
}
int foe_parries(int kind)
{
    int i;
    for (i = 0; i < 28; i++) { if (g_parry[i] == kind) { return 1; } }
    return 0;
}
int weapon_cif(int index) { return index >= 0 && index < 18 ? g_cif[index] : 10; }
int weapon_skill(int index) { return index >= 0 && index < 18 ? g_weapon_skill[index] : 30; }
int swing_group(int sector) { return g_swing[sector & 15]; }
int swing_hit(int sector) { return g_swing_hit[sector & 15]; }
int swing_damage(int sector) { return g_swing_damage[sector & 15]; }
int swing_sound(int index) { return index >= 0 && index < 18 ? g_swing_sound[index] : 374; }
int struck_part(int roll) { return g_body_part[roll >= 0 && roll < 20 ? roll : 0]; }
