#ifndef GFALL_COMBAT_H
#define GFALL_COMBAT_H

/* FALL.EXE damage_roll_to_hit / damage_resolve_attack. See docs/game.md. */
#include "game/dagger/play/state.h"

typedef struct FighterInfo {
    int race, level, health, maximum, flags, metal, armor[7];
    int attribute[ATTRS], skill[SKILLS];
    int fatigue, magicka, max_magicka, conditions;
} FighterInfo;

void fighter_player(FighterInfo *out);
/* The caller supplies both rolls so the rule is reproducible. */
int attack_chance(const FighterInfo *attacker, const FighterInfo *target,
                     int skill, int weapon, int material, int part, int swing, int critical);
int attack_damage(const FighterInfo *attacker, const FighterInfo *target,
                     int weapon, int material, int swing, int expert, int base);
void item_wear(df_u32 id, int damage);
void damage_player(int damage, int part);
/* The end of the player (the original's damage_creature_death on them): health nothing, the death cry and line. */
void player_slain(void);
void combat_step(float dt);
void combat_reset(void);
float combat_flash(void);

#endif
