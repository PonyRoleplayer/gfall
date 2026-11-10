#ifndef OGFALL_COMBAT_H
#define OGFALL_COMBAT_H

/* FALL.EXE damage_roll_to_hit / damage_resolve_attack. See docs/ogfall.md. */
#include "game/ogfall/play/state.h"

typedef struct OgFighter {
    int race, level, health, maximum, flags, metal, armor[7];
    int attribute[OG_ATTRS], skill[OG_SKILLS];
    int fatigue, magicka, max_magicka, conditions;
} OgFighter;

void og_fighter_player(OgFighter *out);
/* The caller supplies both rolls so the rule is reproducible. */
int og_attack_chance(const OgFighter *attacker, const OgFighter *target,
                     int skill, int weapon, int material, int part, int swing, int critical);
int og_attack_damage(const OgFighter *attacker, const OgFighter *target,
                     int weapon, int material, int swing, int expert, int base);
void og_item_wear(df_u32 id, int damage);
void og_damage_player(int damage, int part);
/* The end of the player (the original's damage_creature_death on them): health nothing, the death cry and line. */
void og_player_slain(void);
void og_combat_step(float dt);
void og_combat_reset(void);
float og_combat_flash(void);

#endif
