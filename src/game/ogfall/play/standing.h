#ifndef OGFALL_STANDING_H
#define OGFALL_STANDING_H

/*
 * What the world thinks of the player: faction reputations and powers, and
 * each region's legal reputation and price adjustment. Slice 1 of people and
 * law (docs/ogfall-people.md). The rules follow the byte-matched decompilation
 * (src/lifted/talk.c 1620..1800 for reputation, tamriel.c 385..420 for decay),
 * with its worked notes in docs/ai-social-math.md ("Faction reputation").
 *
 * The faction records are static data the seam hands in at boot (a faction's
 * identity, its tree and its allies and enemies). What changes while playing,
 * a reputation or a power, lives in OgStanding, inside the saved state.
 * Plain C, host-tested; no Daggerfall headers.
 */

#define OG_FACTIONS_MAX 512
#define OG_REGIONS 62
#define OG_FACTION_NAME 26
#define OG_FACTION_LINKS 3
/* A region price adjustment: 1000 is normal; classic keeps it within 250..4000. */
#define OG_PRICE_NORMAL 1000

typedef struct OgFactionDef {
    short id, parent, type, region;
    short rep, power, flags;
    short social_group, guild_group;
    short ally[OG_FACTION_LINKS], enemy[OG_FACTION_LINKS];
    char name[OG_FACTION_NAME];
} OgFactionDef;

/* The live part, indexed like the table (og_faction_index). */
typedef struct OgStanding {
    int ready;
    short rep[OG_FACTIONS_MAX];
    short power[OG_FACTIONS_MAX];
    short legal[OG_REGIONS];
    unsigned short price[OG_REGIONS];
} OgStanding;

/*
 * The table, in file order, and its tree linked from the parent ids. Called
 * once at boot by the seam. 0 when there are too many factions.
 */
int og_factions_define(const OgFactionDef *defs, int count);
int og_faction_count(void);
const OgFactionDef *og_faction_def(int index);
/* A faction's index by id; -1 for none. */
int og_faction_index(int id);
/* The first faction of a type in a region, else the one of that type with no region; -1 for none (faction_find_type_in_region). */
int og_faction_of_type(int region, int type);

/*
 * Which faction id the player's membership with these guild bits is of; 0 for
 * none (guild_find_membership_by_bits). Bit 128 is a temple, bit 64 an order.
 * Memberships come with the guilds slice; until then there are none.
 */
void og_standing_set_membership(int (*find)(int bits));

/* The live state at the start of a game or when a save has none. */
void og_standing_defaults(OgStanding *s);
OgStanding *og_standing(void);
/* A table or a save of another shape: valid when its ranges hold. */
int og_standing_valid(const OgStanding *s);

int og_faction_rep(int id);
int og_faction_power(int id);
/* faction_add_reputation: a sum held between -100 and 100. */
void og_faction_add_reputation(int id, int amount);
/*
 * faction_change_reputation: the faction (a temple's own, id 240, becomes the
 * player's temple), its root and descendants by faction_propagate_reputation,
 * then half to each ally and minus half to each enemy. Amounts shift, as the
 * original's `>> 1`, so minus 5 halves to minus 3.
 */
void og_faction_change_reputation(int id, int amount);

/* A region's legal reputation (court and guards) and its shops' price adjustment. */
int og_region_legal(int region);
void og_region_add_legal(int region, int amount);
int og_region_price(int region);
/*
 * reputation_decay, as time_pass calls it each 161280 game minutes: every
 * region's legal reputation one point toward 0 (not while a court is open),
 * every faction's reputation one point toward 0.
 */
void og_standing_decay(int court_open);
/* The decay interval in game minutes (time_pass: minute % 161280 == 0). */
#define OG_DECAY_MINUTES 161280u

#endif
