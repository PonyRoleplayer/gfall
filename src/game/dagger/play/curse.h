#ifndef GFALL_CURSE_H
#define GFALL_CURSE_H

#include "engine/core/df.h"
#include "game/dagger/tables/tables.h"

/*
 * Vampirism and lycanthropy, from the decomp's docs/curses.md (section numbers below are its).
 *
 * The original keeps the player's current form in `race` (0..7 the playable races, 8 a vampire, 9 a werewolf, 10 a
 * wereboar) and the race before the curse in `original_race`. GFALL keeps the race the player chose in
 * CharacterState.race for good, since a dozen tables are indexed by it, and holds the form here: curse_form() is the
 * original's `race > 7` test and curse_race() the number its rules read.
 *
 * A player holds one curse at most: infection is refused when the form is above 7 and a lycanthrope in human form
 * who is infected again with vampirism simply loses the infection at day three (3).
 */

/* The disease record ids of the curses (1.2). */
#define CURSE_VAMPIRISM 100
#define CURSE_WEREWOLF 101
#define CURSE_WEREBOAR 102
/* The Shapechange spell (SPELLS.STD number 92), free to cast. */
#define SHAPECHANGE_SPELL 92
/* ObjectInfo.message of a spell the curse gave: the original's '$' after its name. The cure deletes them. */
#define CURSE_SPELL_MARK 36

typedef struct CurseState {
    /* The pending infection (2.2): the minute it takes, and 0 vampirism, 1 werewolf, 2 wereboar. 0 time is none. */
    df_u32 infection_time;
    int infection_kind;
    /* The dream of the infection has not been shown yet (character flag 0x10). */
    int dream;
    /* The curse's disease record id (CURSE_*), 0 for none, and the form: 0 human, 8 vampire, 9 werewolf, 10 wereboar. */
    int disease, form;
    /* A vampire's clan faction id, 150..158. */
    int clan;
    /* The player's `action` byte: 1 once a vampire's first P0A clan quest was picked (9.3). */
    int action;
    /* The attribute bonuses the curse gave, which the cure takes back. */
    int drained[ATTRS];
    /* The player's class as it was; the curse adds immunities and flags to the live one. */
    ClassInfo saved_class;
    /* The minute a lycanthrope last killed an innocent, and the last change of form. */
    df_u32 kill_time, shape_time;
    /* The true maximum health a werewolf's hunger takes down (max_health_base). */
    int health_base;
    /* A bit for each guild membership slot the curse suspended (the original turns type 10 records into 29). */
    int suspended;
} CurseState;

/* Bytes the save keeps for it (curse_wire in save.c). */
#define CURSE_WIRE_BYTES (4 + 6 * 4 + ATTRS * 4 + (16 + 35 * 4) + 4 + 4 + 4 + 4)

/* What the game tells the curse about the world. Every hook may be 0. */
typedef struct CurseWorld {
    /* Takes the player to a random crypt (dungeon type 0) of the region, 1 when there is one (4). */
    int (*haunt)(int region);
    /* The faction id of the region's vampire clan, 0 for none (4, step 6). */
    int (*clan)(int region);
    /* The region the player is in. */
    int (*region)(void);
    /* Hircine's ring (artifact 3) is worn: no hunger, no moon, no wait between changes. */
    int (*ring_worn)(void);
} CurseWorld;
void curse_set_world(const CurseWorld *world);
const CurseWorld *curse_world(void);
/* The console's curse command (cheats_startup registers it). */
void curse_commands(void);

CurseState *curse_state(void);
int curse_valid(const CurseState *state);

/* The form (0, 8, 9, 10) and the number the original's `race` would hold. */
int curse_form(void);
int curse_race(void);
int curse_is_vampire(void);
int curse_is_lycanthrope(void);
/* In beast form (2.3 beast: race 9 or 10). */
int curse_beast(void);

/* damage_infect_*: a damaging blow of a werewolf (kind 9), wereboar (14) or vampire (28, 30), 1.22% (2.1). */
/* 1 when a blow of that creature kind (9, 14, 28, 30) rolled the infection chance (the infection itself is refused for level 1 and the cursed). */
int curse_blow(int attacker_kind);
void curse_infect_vampirism(void);
void curse_infect_lycanthropy(int kind);
/* 1 while an infection is pending; a cure of disease clears it (2.3). */
int curse_infected(void);
void curse_clear_infection(void);

/* The end of every time_pass_minutes call: the lycanthrope's hunger and moons, then the infection that takes (2.4). */
void curse_tick(void);
/* The cure quests and a vampire's clan quests offered on their schedules (9.3), for the minutes start+1 .. start+minutes. */
void curse_schedule(df_u32 start, df_u32 minutes);
/* rest_close: 1 when the infection's dream is to be shown (once); its video is not played here. */
int curse_dream(void);

/* A change of form (7.2): forced by the moons, or cast. 1 when it happened. */
int curse_shapechange(int forced);
/* A townsperson or a city guard killed (7.4): a lycanthrope's hunger is fed. */
void curse_innocent_killed(void);
/* A level gained: a lycanthrope's true maximum health rises with it. */
void curse_level_up(int health);

/* qkey.c 286..291: one swing in five a vampire or a beast makes a sound of its own. */
void curse_attack_sound(void);

/* A vampire who has not killed a creature for 960 minutes cannot rest or heal by resting (5.4). */
int curse_hungry(void);

/* The curse's cure by quest op 58 (vampirism) or 59 (lycanthropy) (9.2). */
void curse_cure_vampirism(void);
void curse_cure_lycanthropy(void);

/* raise_skills' bracket (10): the curse's skill bonuses off, then on again, so the clamp sees the true skills. */
void curse_skill_bonuses(int sign);
/* A guild membership slot the curse has suspended (5.7). */
int curse_suspended(int slot);

#endif
