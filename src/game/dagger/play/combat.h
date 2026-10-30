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
 * A swing strikes once, on its hit frame (weapon_hit): every creature
 * within reach and the arc of the player's facing (COMBAT_REACH,
 * COMBAT_ARC) that no wall hides from the eye, each its own blow, as
 * classic's FALL.EXE does. The
 * to-hit and damage rolls are this file's; a creature's health is kept
 * in its keep row's game bytes (keep.h), rolled the first time it is
 * struck, so a wounded or dead creature stays so when its block is
 * dropped and comes back.
 *
 * Sounds (sfx.h): a hit plays Hit1..5 at the creature; a blow that
 * takes nothing a parry (Parry1..9) from a creature that parries when a
 * weapon struck it, else the swing (medium with a weapon, high bare); a
 * swing that reaches nothing the swing; a creature killed a body's fall.
 * A creature struck wakes (foes_alert), passive or not.
 *
 * A blow's damage wears the weapon down (item_wear_down), a creature's the
 * armour it lands on.
 *
 * Not yet: blood, knock-back, class enemies' worn armour (60, as bare, until
 * they carry things).
 */

/*
 * Classic's reach (FALL.EXE): feet to feet under 91 units (2.25 m), and
 * within 200 of a turn's 2048 (35.156 degrees) of the attacker's facing
 * on the ground; nearer than 11 units, any way.
 */
#define COMBAT_REACH 90.0f
#define COMBAT_POINT_BLANK 10.0f
#define COMBAT_ARC (200.0f / 2048.0f * DF_TAU)
/*
 * A creature's keep row's game bytes: its health (two bytes, low first),
 * 1 once that was rolled; the player's level when it died, and a bit for
 * each thing taken off its body (two bytes), for loot.c.
 */
#define COMBAT_KEEP_HEALTH 0
#define COMBAT_KEEP_ROLLED 2
#define COMBAT_KEEP_LEVEL 3
#define COMBAT_KEEP_TAKEN 4
/* Fatigue a swing spends, in 64ths (DFU's swingWeaponFatigueLoss, raw). */
#define COMBAT_SWING_FATIGUE 11

/* A creature as the sums see it. */
typedef struct Foe {
    const EnemyKind *kind;
    int level;
    /* A body part's armour value: lower is better. */
    int armor;
    /* Every skill of a creature, its Dodging, Hand-to-Hand and Critical Strike among them: 5 a level and 30, at most 100. */
    int skill;
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
    /* A creature's blow at the player: the body part it came at, BODY_*. */
    int part;
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
 * A creature's blow at the player (CalculateAttackDamage with the player
 * as target). It comes at one body part (CalculateStruckBodyPart: of 20,
 * the head 2, each arm 3, the chest 4, the hands 4, the legs 3, the feet
 * 1). A monster swings its three attacks (ENEM's damage_min and
 * damage_max, those with a most above 0), each rolled to hit, their
 * damage summed; a class enemy, carrying nothing yet, its bare hands
 * (Hand-to-Hand / 10 + 1 to / 5 + 1). To hit: its skill, the armour the
 * player wears on that part (Character.armor: 100 bare, lower is better),
 * the Luck and Agility differences, less the player's Dodging / 4, a
 * critical strike, less 50, held to 3..97.
 */
void combat_foe_blow(const Foe *f, const Character *pc, Blow *out);

/*
 * The strike on a swing's hit frame from the player's feet and eye,
 * facing yaw: every creature in reach (as above), struck by combat_blow,
 * hurt or killed. Its fatigue spent either way. How many were struck.
 * The walls are scene's collision (collide_use), as rig_third's ray
 * reads them.
 */
int combat_strike(Character *pc, Item *weapon, int swing, const Scene *scene, const float *feet,
                  const float *eye, float yaw);
/*
 * A bow's shot on its release frame, along the view from eye: the first
 * creature that way within sight and no wall between, struck by combat_blow.
 * DFU flies an arrow; here it lands at once. 1 when it struck one.
 */
int combat_shoot(Character *pc, Item *weapon, const Scene *scene, const float *feet, const float *eye, const float *dir);
/* The slab test: whether a ray from eye along unit dir meets the box within reach, and how far along. */
int combat_ray_box(const float *eye, const float *dir, const float *low, const float *high, float reach, float *t_out);
/* 1 when creature e is one combat_strike would strike from feet and eye facing yaw. */
int combat_reaches(const Scene *scene, const float *feet, const float *eye, float yaw, const Entity *e);
/* 1 when no wall of scene stands between a and b (its collision, a thin ray): what sees and strikes. */
int combat_sight(const Scene *scene, const float *a, const float *b);
/* A frame: flinches run out, bodies lie as bodies (a dead creature come back with its block). */
void combat_frame(float dt);
/* Every flinch forgotten: a new place. */
void combat_reset(void);

#endif
