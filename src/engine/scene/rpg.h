#ifndef ENG_RPG_H
#define ENG_RPG_H

#include "engine/core/df.h"

/*
 * A character's numbers: attributes spent from points, levels from
 * experience, vitals worked out from both, and one blow's to-hit and
 * damage. Read docs/rpg.md before editing. The engine owns how these
 * work; every number is the game's, in its RpgRules. It knows no game's
 * attributes, classes or races.
 *
 * Weights are in hundredths: a weight of 50 on an attribute adds half a
 * point for each point of it.
 *
 * Plain C, no Glide: hostcheck runs it.
 */

#define RPG_ATTRS 8
#define RPG_VITALS 4
#define RPG_NAME 16

typedef struct RpgVital {
    char name[RPG_NAME];
    /* Its most: base, per_level a level past the first, and the attributes by weight. */
    int base;
    int per_level;
    int weight[RPG_ATTRS];
} RpgVital;

typedef struct RpgRules {
    int nattrs;
    char attr_name[RPG_ATTRS][RPG_NAME];
    /* Every attribute starts at attr_start and stays within attr_min..attr_max. */
    int attr_start;
    int attr_min;
    int attr_max;
    /* Points to spend at creation, and at each level gained. */
    int create_points;
    int level_points;
    int nvitals;
    RpgVital vital[RPG_VITALS];
    /* Experience from level 1 to 2; each level after takes xp_growth percent of the last. */
    int xp_first;
    int xp_growth;
    int level_max;
    /* To hit, in percent: base, plus the attacker's by attack weights, less the defender's by defence weights; held to hit_min..hit_max. */
    int hit_base;
    int hit_min;
    int hit_max;
    int attack_weight[RPG_ATTRS];
    int defence_weight[RPG_ATTRS];
    /* Added to a weapon's roll, by weight. */
    int damage_weight[RPG_ATTRS];
} RpgRules;

typedef struct RpgSheet {
    int level;
    /* Experience toward the next level. */
    int xp;
    /* Points not spent yet. */
    int points;
    int attr[RPG_ATTRS];
    /* What rpg_lower may not go below: the attributes when last committed. */
    int fixed[RPG_ATTRS];
    int vital[RPG_VITALS];
} RpgSheet;

/* What one blow did. */
typedef struct RpgBlow {
    /* The chance it had, percent. */
    int chance;
    int hit;
    int damage;
} RpgBlow;

/* A new character: level 1, the creation points to spend, its vitals full. */
void rpg_init(RpgSheet *s, const RpgRules *r);
/* A point into attribute a, or back out of it down to what is committed. 1 when it moved. */
int rpg_raise(RpgSheet *s, const RpgRules *r, int a);
int rpg_lower(RpgSheet *s, const RpgRules *r, int a);
/* The spending made final, its vitals full. 0, and nothing done, while points are left and an attribute could take one. */
int rpg_commit(RpgSheet *s, const RpgRules *r);
/* A vital's most for this sheet. */
int rpg_vital_max(const RpgSheet *s, const RpgRules *r, int v);
/* Experience from this level to the next; 0 at the last level. */
int rpg_xp_next(const RpgRules *r, int level);
/* Experience in: each level reached gives its points and fills the vitals. How many levels. */
int rpg_gain_xp(RpgSheet *s, const RpgRules *r, int xp);
/* A vital down by amount, not below 0. 1 when it reached 0. */
int rpg_hurt(RpgSheet *s, int v, int amount);
/* A vital up by amount, not past its most. */
void rpg_restore(RpgSheet *s, const RpgRules *r, int v, int amount);
/* 0..n-1 from a seed the caller keeps, so a test can repeat it. */
int rpg_roll(df_u32 *seed, int n);
/* The chance to hit, percent; bonuses are the game's (a weapon's, armour's). */
int rpg_chance(const RpgRules *r, const RpgSheet *attacker, const RpgSheet *defender, int attack_bonus,
               int defence_bonus);
/*
 * One blow with a weapon of damage lo..hi: rolled to hit; on a hit, the
 * roll and the attacker's bonus, taken down by armour (100 armour halves
 * it, 300 quarters it), at least 1.
 */
void rpg_blow(const RpgRules *r, const RpgSheet *attacker, const RpgSheet *defender, int lo, int hi, int attack_bonus,
              int defence_bonus, int armour, df_u32 *seed, RpgBlow *out);

#endif
