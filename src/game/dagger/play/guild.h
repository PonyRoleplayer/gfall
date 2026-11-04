#ifndef DF_GUILD_H
#define DF_GUILD_H

#include "game/dagger/play/player.h"
#include "game/dagger/tables/guilds.h"

/*
 * The guilds the player belongs to: joining, ranks, training and what a
 * member is given. Read docs/game.md (Guilds) before editing.
 *
 * Daggerfall Unity's Guild and GuildManager:
 * - A guild is found by the building's faction (FACTION.TXT): the Fighters or
 *   the Mages themselves, a temple's order by its god (its parent), a knightly
 *   order. One guild of each group: one temple, one order.
 * - Joining and each rank ask a reputation with the guild (0, 10, .. 90) and
 *   its skills: at least one at the rank's high mark and another at its low
 *   (rank 0: 22 and 4; rank 9: 87 and 37).
 * - The rank is looked at on coming to the guild, 28 days at least after it
 *   last changed: up, down, or out when no rank is earned (a bad name, or
 *   skills below rank 0's).
 * - Training: a trainer's skill (a temple's for anyone, else a member's), for
 *   100 gold a level to a member and 400 to another, once in 12 hours, up to
 *   50: three hours pass, fatigue is spent, and the skill gets 10 to 20 uses
 *   times its advancement.
 * - Given: the Fighters' hall to rest in and cheaper repairs, an order's
 *   free tavern rooms (from rank 4, or in its own region), Arkay's cheaper
 *   cure, a temple's healing from its rank.
 *
 * Saved as rows (MEMB). Plain C: hostcheck runs it.
 */

#define GUILD_MEMBERSHIPS 8
/* Rank changes wait this many days. */
#define GUILD_RANK_DAYS 28

typedef struct GuildMembership {
    df_u16 id;
    /* The guild's faction (GuildKind.id), its rank 0..9, and the day its rank last changed. */
    df_i32 guild;
    df_i32 rank;
    df_i32 changed_day;
} GuildMembership;

extern const RecSchema guild_membership_schema;

void guild_new_game(void);
int guild_count(void);
const GuildMembership *guild_at(int i);
void guild_put(const GuildMembership *m);

/* The guild a building's (or a person's) faction belongs to: itself, or its parent's (a temple's order); 0 for none. */
const GuildKind *guild_of_faction(int faction);
/* The player's rank in it, or -1 when not a member. */
int guild_rank(const GuildKind *g);
/* Its title for a rank, or "". */
const char *guild_title(const GuildKind *g, int rank);

/* The rank reputation rep and the skills earn, -1 when none (DFU's CalculateNewRank). */
int guild_rank_earned(const GuildKind *g, const Character *c, int rep);
/* What came of asking to join. */
enum {
    GUILD_JOINED,
    GUILD_ALREADY,
    GUILD_BAD_NAME,
    GUILD_LOW_SKILL,
    GUILD_OTHER_OF_GROUP,
    GUILD_NO_ROOM
};
/* Joins at rank 0 on day if eligible; one of each group. */
int guild_join(const GuildKind *g, const Character *c, int rep, int day);
/* The rank looked at on day: GUILD_SAME, the new rank, or GUILD_EXPELLED (and the membership gone). */
#define GUILD_SAME (-2)
#define GUILD_EXPELLED (-1)
int guild_update(const GuildKind *g, const Character *c, int rep, int day);

/* Training's price for the player at level, a member or not. */
int guild_training_price(int member, int level);
/* 1 when this guild trains this player (a member, or anyone at a temple). */
int guild_trains(const GuildKind *g);
/* What came of training. */
enum {
    GUILD_TRAINED,
    GUILD_TOO_SOON,
    GUILD_TOO_SKILLED,
    GUILD_NOT_ENOUGH_GOLD,
    GUILD_NOT_TAUGHT
};
/* Trains skill at minute now for price; the clock is the caller's to move three hours. */
int guild_train(Player *p, const GuildKind *g, int skill, int price, int now);
/* The minute of the last training, for the save. */
int guild_last_training(void);
void guild_set_last_training(int minute);

/* What membership gives. */
int guild_can_rest_in(int faction);
int guild_free_rooms(int region);
int guild_cure_price(int faction, int price);
int guild_repair_price(int faction, int price);
int guild_heals(int faction);

#endif
