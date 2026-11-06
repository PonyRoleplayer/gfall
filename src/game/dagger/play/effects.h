#ifndef DF_EFFECTS_H
#define DF_EFFECTS_H

#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "game/dagger/play/player.h"
#include "game/dagger/play/spells.h"

/*
 * What a spell does when it lands: the rolls, the saving throw, the effects
 * that happen at once and the ones that last. Read docs/game.md (Magic)
 * before editing.
 *
 * A spell lands on a target, the player or a creature. Each of its effects in
 * turn (Daggerfall Unity's AssignBundle):
 *
 *   1. If it supports a chance, one roll of 1..100 at or under base + plus x
 *      (the caster's level / per level) lets it on: else "Spell effect
 *      failed." for the caster's own spell, "Save versus spell made." for
 *      another's. Elemental resistance uses its chance later, not now.
 *   2. An effect on another that has no magnitude (paralysis, silence) is saved
 *      against whole: a saving throw of 0% lands nothing.
 *   3. Its duration, in magic rounds: base + plus x max(1, level / per level).
 *      A round is a game minute (5 real seconds at the clock's pace). The
 *      first round is at once and counts: three rounds of continuous damage
 *      act three times, and an effect of one round is in force until the
 *      next minute turns.
 *   4. Its magnitude, rolled low..high + (low..high) x (level / per level), and,
 *      on another, cut by the saving throw (the share that lands, 0..100%).
 *
 * Then by what it is. Instant: damage to health, fatigue or spell points, heal,
 * transfer, cures, dispel magic. Lasting: continuous damage and regeneration
 * (rolled again each round), paralysis, silence, elemental resistance and the
 * other flags the game reads (slowfall, water breathing, free action, ...).
 * effects_built says which an effect is; an effect not built is left out
 * and the cast says so.
 *
 * The saving throw is DFU's: 50, the race's and the class's resistance to the
 * element (+30, +25), low tolerance and weakness (-25, -50), and Willpower / 10
 * (a creature has only the Willpower), held to 5..95; an immunity (50 more
 * for a class, a race's whole) is 100, which lets nothing through. The roll
 * of 1..100: over the save, all of it lands; at or under it within 20, 5% less
 * for each point nearer; further under, none. A live elemental resistance on
 * the target first rolls its chance to turn the whole effect away.
 *
 * Plain C over player.h, combat.h, foes.h and ills.h: hostcheck drives it on the
 * streamed hold's creatures.
 */

/* A magic round in real seconds, for the creatures' frame time (a game minute at the clock's pace). */
#define EFFECTS_ROUND_SECONDS 5.0f
#define EFFECTS_LIVE 24

/* What an effect is to the game. */
enum {
    EFFECT_NOT_BUILT,
    /* Happens or is felt in the game. */
    EFFECT_BUILT,
    /* Kept and shown while it lasts, but nothing in the game reads it yet. */
    EFFECT_RECORDED
};

/* An effect that lasts: on the player or on a creature, its rounds left, its settings kept to roll again. */
typedef struct LiveEffect {
    df_u16 effect;
    df_u8 element;
    df_u8 on_creature;
    /* 1 when the caster is the target: no saving throw (a self-cast spell is never saved against). */
    df_u8 self;
    /* 1 while in force; a free entry is 0. */
    df_u8 live;
    /* Rounds still to act: at 0 it lasts to the next round, which ends it. */
    int rounds;
    int level;
    EffectSettings settings;
    /* For elemental resistance: its chance, rolled when cast. */
    int chance;
    /* A creature's: where it is and who (its block may go). */
    Entity *who;
    KeepKey key;
} LiveEffect;

/* Who a spell lands on: exactly one of these. */
typedef struct EffectTarget {
    Player *player;
    Entity *creature;
    /* The place a creature stands in (a spell that kills one leaves its body on this floor). */
    const Scene *scene;
} EffectTarget;

/* EFFECT_NOT_BUILT, EFFECT_BUILT or EFFECT_RECORDED for the effect of that id. */
int effects_built(int effect_id);

/*
 * The spell's effects landed on to by caster (the player): how many took hold. The target's cost
 * is the caster's already paid. A spell whose target is the caster has the caster as its target.
 */
int effects_land(const Spell *spell, const EffectTarget *to, Player *caster);
/*
 * A creature's spell landed on the player (cast.c's foe casts): the caster is a creature of that level,
 * whose own sheet nothing here reads or changes, so a transfer gives it nothing and a failure says
 * nothing of "not worked yet". How many effects took hold.
 */
int effects_land_from_foe(const Spell *spell, Player *target, int level);
/* The spell's own effects (a spell of any target) landed on the caster. */
int effects_land_self(const Spell *spell, Player *caster);

/* One game minute: lasting effects act, count down and end. */
void effects_round(Player *player, const Scene *scene);
/* Whole minutes at once (rest, travel): effects end that would have; continuous ones act each. */
void effects_pass(Player *player, const Scene *scene, int minutes);
/* A place left, a new game: every effect gone, the player's too when all is 1. */
void effects_reset(int all);

/* 1 when the player has an effect of that group (and subgroup, -1 for any) running. */
int effects_player_has(int group, int sub);
/* The chance the player's resistance to element turns an effect away; 0 when none. */
int effects_resist_chance(int element);
/* 1 once when a spell took the player's last health: the game then shows death. */
int effects_died(void);

/* The live list, for the screen that shows what lasts and for a save. */
int effects_count(void);
const LiveEffect *effects_get(int i);
/* The player's effects as they are saved, and put back (rounds, level, settings, element). */
int effects_put(const LiveEffect *e);

/* The saving throw of a target: the percent of an effect that lands, for a roll 1..100 against save (5..95, or 100 for immune). */
int effects_save_percent(int save, int roll);
/* The player's save against element, with paralysis flagged for paralyze: 5..95, or 100. */
int effects_player_save(const Character *c, int element, int paralysis);

#endif
