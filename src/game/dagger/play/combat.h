#ifndef DF_COMBAT_H
#define DF_COMBAT_H

#include "engine/gfx/camera.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "game/dagger/play/character.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/enemies.h"

/*
 * The player's blows against a dungeon's creatures, by classic's sums as
 * Daggerfall Unity has them (FormulaHelper.CalculateAttackDamage,
 * CalculateSuccessfulHit, CalculateWeaponAttackDamage, EnemyEntity).
 * Read docs/game.md (Combat) before editing.
 *
 * A swing strikes once, on its hit frame (weapon_hit): the nearest
 * creature within reach in front of the eye that no wall hides. The
 * to-hit and damage rolls are this file's; a creature's health is kept
 * in its keep row's game bytes (keep.h), rolled the first time it is
 * struck, so a wounded or dead creature stays so when its block is
 * dropped and comes back.
 *
 * Not yet: creatures that fight back (the next slice), the swing's and
 * the hit's sounds, blood, knock-back, skills advancing, proficiency
 * and the class's group bonuses, class enemies' worn armour (60, as
 * bare, until they carry things).
 */

/* Reach: DFU's 2.25 m and its 0.25 m sweep, in units. */
#define COMBAT_REACH (2.5f * 40.0f)
/* Fatigue a swing spends, in 64ths (DFU's swingWeaponFatigueLoss, raw). */
#define COMBAT_SWING_FATIGUE 11

/* A creature as the sums see it. */
typedef struct Foe {
    const EnemyKind *kind;
    int level;
    /* A body part's armour value: lower is better. */
    int armor;
    /* Its Dodging, as every skill of a creature: 5 a level and 30, at most 100. */
    int dodging;
    /* 1 when it faces away from the attacker. */
    int behind;
} Foe;

/* What a blow did. */
typedef struct Blow {
    /* The clamped chance to hit, 3..97. */
    int chance;
    int hit;
    /* Health it takes: 0 on a miss. */
    int damage;
    /* 1 when the weapon's material cannot hurt this creature. */
    int ineffective;
    /* 1 when the backstab tripled it. */
    int backstab;
} Blow;

/* The game's dice: 0..n-1. Seeded to make tests repeat. */
void combat_seed(df_u32 seed);
int combat_roll(int n);

/* floor((strength - 50) / 5): DFU's DamageModifier. */
int combat_strength_bonus(int strength);
/* A creature's level, armour and Dodging: a monster's own, a class enemy at the player's level. */
void combat_foe(Foe *f, const EnemyKind *kind, int player_level, int behind);
/* A creature's most health, rolled: a monster between its least and most, a class enemy 10 and a roll of its class's hit points a level. */
int combat_roll_health(const Foe *f);
/*
 * One blow of the player with weapon (0: bare hands) in swing (1..6:
 * down, down-left, left, right, down-right, up) at f.
 */
void combat_blow(const Character *pc, const Item *weapon, int swing, const Foe *f, Blow *out);

/*
 * The strike on a swing's hit frame: the creature in reach (as above) of
 * the eye looking yaw, pitch, struck by combat_blow, hurt or killed. Its
 * fatigue spent either way. 1 when a creature was in reach. The walls
 * are scene's collision (collide_use), as rig_third's ray reads them.
 */
int combat_strike(Character *pc, const Item *weapon, int swing, const Scene *scene, float x, float y, float z,
                  float yaw, float pitch);
/* A frame: flinches run out, bodies lie as bodies (a dead creature come back with its block). */
void combat_frame(float dt);
/* Every flinch forgotten: a new place. */
void combat_reset(void);

#endif
