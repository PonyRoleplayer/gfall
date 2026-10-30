#ifndef DF_STANDING_H
#define DF_STANDING_H

#include "engine/core/df.h"
#include "game/dagger/formats/factionfile.h"

/*
 * What the world thinks of the player: reputation with each faction and each
 * social group, and each region's law and bank account. Read docs/game.md
 * (People) before editing. It is the game's own block, kept across places, and
 * a save holds only the rows that differ from a new game's.
 *
 * A change to a faction's reputation spreads as Daggerfall Unity's
 * PersistentFactionData has it: half to its allies, half taken from its
 * enemies, and over its whole tree from the root, the whole amount to the one
 * changed and to the root, half to the rest. All clamped to -100..100.
 *
 * Plain C: hostcheck runs it.
 */

/* The map's regions. */
#define STANDING_REGIONS 62
#define STANDING_MOST 100

/* One region's law and bank. */
typedef struct RegionStanding {
    /* The court's opinion, -100..100. */
    short legal;
    /* 1 once the court has banished the player. */
    df_u8 banished;
    /* 1 when a loan here went unpaid past its day. */
    df_u8 defaulted;
    /* Gold in the bank here; owed here; the day it is due (clock_day), 0 with no loan. */
    df_i32 gold;
    df_i32 loan;
    df_i32 due_day;
    /* What things cost here, per 1000 (FormulaHelper.CalculateCost): rolled 750..1250 at a new game. */
    short prices;
} RegionStanding;

/* A new game: everything back as FACTION.TXT has it, each region's prices rolled from seed. */
void standing_new_game(df_u32 seed);
/* Reputation with a faction; 0 for one the file does not have. */
int standing_rep(int faction);
/* A faction's reputation moved by amount: with spread 1, over its tree, allies and enemies. */
void standing_change(int faction, int amount, int spread);
/* The nearest faction up from this one that a person answers for: a group, a province, a temple, the people, a court or a person. */
const Faction *standing_answers_for(int faction);
/* Reputation with a social group (a person of sgroup past the groups counts as a merchant). */
int standing_social(int sgroup);
void standing_social_change(int sgroup, int amount);
/* The background answers' lasting modifier to every reaction (BiogFile's RR). */
int standing_reaction_bonus(void);
void standing_set_reaction_bonus(int bonus);
/* What someone of this faction thinks of the player, before tone or skill. */
int standing_reaction(int faction);
/* A region's row; 0 for a number past the map. */
RegionStanding *standing_region(int region);

/* For a save: how many factions differ from the file, and the i-th's id and reputation. */
int standing_changed_count(void);
void standing_changed_at(int i, int *faction, int *rep);
/* Back from a save: one faction's reputation, unspread. */
void standing_put_rep(int faction, int rep);
/* The social groups' reputations, SOCIAL_COUNT of them, and the bonus. */
const short *standing_socials(void);
void standing_put_socials(const short *social, int bonus);

#endif
