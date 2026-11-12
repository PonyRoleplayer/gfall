#ifndef GFALL_STANDING_H
#define GFALL_STANDING_H

/*
 * What the world thinks of the player: faction reputations and powers, and
 * each region's legal reputation and price adjustment. Slice 1 of people and
 * law (docs/gfall-people.md). The rules follow the byte-matched decompilation
 * (src/lifted/talk.c 1620..1800 for reputation, tamriel.c 385..420 for decay),
 * with its worked notes in docs/ai-social-math.md ("Faction reputation").
 *
 * The faction records are static data the seam hands in at boot (a faction's
 * identity, its tree and its allies and enemies). What changes while playing,
 * a reputation or a power, lives in StandingState, inside the saved state.
 * Plain C, host-tested; no Daggerfall headers.
 */

#define FACTIONS_MAX 512
#define REGIONS 62
#define FACTION_NAME 26
#define FACTION_LINKS 3
/* A region price adjustment: 1000 is normal; classic keeps it within 250..4000. */
#define PRICE_NORMAL 1000

typedef struct FactionDef {
    short id, parent, type, region;
    short rep, power, flags;
    short social_group, guild_group;
    short ally[FACTION_LINKS], enemy[FACTION_LINKS];
    char name[FACTION_NAME];
} FactionDef;

/* The live part, indexed like the table (faction_index). */
typedef struct StandingState {
    int ready;
    short rep[FACTIONS_MAX];
    short power[FACTIONS_MAX];
    short legal[REGIONS];
    unsigned short price[REGIONS];
} StandingState;

/*
 * The table, in file order, and its tree linked from the parent ids. Called
 * once at boot by the seam. 0 when there are too many factions.
 */
int factions_define(const FactionDef *defs, int count);
int faction_total(void);
const FactionDef *faction_def(int index);
/* A faction's index by id; -1 for none. */
int faction_index(int id);
/* The first faction of a type in a region, else the one of that type with no region; -1 for none (faction_find_type_in_region). */
int faction_of_type(int region, int type);

/*
 * Which faction id the player's membership with these guild bits is of; 0 for
 * none (guild_find_membership_by_bits). Bit 128 is a temple, bit 64 an order.
 * Memberships come with the guilds slice; until then there are none.
 */
void standing_set_membership(int (*find)(int bits));

/* The live state at the start of a game or when a save has none. */
void standing_defaults(StandingState *s);
StandingState *standing_state(void);
/* A table or a save of another shape: valid when its ranges hold. */
int standing_valid(const StandingState *s);

int faction_rep(int id);
int faction_power(int id);
/* faction_add_reputation: a sum held between -100 and 100. */
void faction_add_reputation(int id, int amount);
/*
 * faction_change_reputation: the faction (a temple's own, id 240, becomes the
 * player's temple), its root and descendants by faction_propagate_reputation,
 * then half to each ally and minus half to each enemy. Amounts shift, as the
 * original's `>> 1`, so minus 5 halves to minus 3.
 */
void faction_change_reputation(int id, int amount);

/*
 * talk_faction_relation (src/hand/func_00018BDC.c): how the player's memberships (the ids of the factions they
 * belong to) stand toward this faction, the lowest relation code found, 8 for none: 0 the same faction, 1 a shared
 * parent or one the other's parent, 2 an ally either way, 3 an enemy either way, 4 a shared enemy, 5 a shared ally,
 * 6 the member's ally is their enemy's ally, 7 the member's enemy is their ally's enemy. *score is added to by each
 * (+15, +10, set to 20, +5, +5, -5, -5) for the greeting's roll; it is not reset here.
 */
int faction_relation(int faction_id, const int *members, int count, int *score);
/* faction_has_ally and faction_has_enemy by id: the first lists the second among its three. */
int faction_has_ally(int faction_id, int other_id);
int faction_has_enemy(int faction_id, int other_id);
/* A faction's parent id, 0 for a root; its type, social group, region. -1 for none. */
int faction_parent_id(int faction_id);

/* A region's legal reputation (court and guards) and its shops' price adjustment. */
int region_legal(int region);
void region_add_legal(int region, int amount);
int region_price(int region);
/*
 * reputation_decay, as time_pass calls it each 161280 game minutes: every
 * region's legal reputation one point toward 0 (not while a court is open),
 * every faction's reputation one point toward 0.
 */
void standing_decay(int court_open);
/* The decay interval in game minutes (time_pass: minute % 161280 == 0). */
#define DECAY_MINUTES 161280u

#endif
