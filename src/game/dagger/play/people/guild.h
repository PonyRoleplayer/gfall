#ifndef GFALL_GUILD_H
#define GFALL_GUILD_H

#include "engine/core/df.h"

/*
 * The guilds the player belongs to. Slice 9 of people and law (docs/game-people.md), from guild_join_or_promote
 * (src/hand/run_0006DC35.c 108..190), guild_join_check (func_0006FF7E.c), guild_rank_for_skills and the membership
 * finders (src/lifted/guilds.c 278..460), guild_count_crime (guilds_00070370.c), guild_check_invitations and
 * guild_add_membership (run_00070887.c), guild_kind_of_faction (run_00070EC0.c 92), with the notes in
 * docs/economy-math.md (guilds). The messages, the skills that rank each guild and the rank steps are read out of
 * FALL.EXE (tables/guilddata.h). Pure rules over the session; the hall's menu and its services come on top.
 *
 * A membership has a kind: 0 the Dark Brotherhood, 1 the Mages Guild, 2 the Fighters Guild, 3 the Thieves Guild,
 * 64 | k a knightly order (68..77), 128 | k a temple (142..149). Its guild index is kind & 63, 0..21.
 */

#define MEMBERSHIPS 8
#define GUILDS 22
#define RANK_MONTH 40320u
#define KIND_KNIGHT 64
#define KIND_TEMPLE 128

typedef struct Membership {
    /* used 0 for an empty slot (a cleared state has none: kind 0 is the Dark Brotherhood). */
    int used, kind, rank, faction;
    df_u32 rank_time, armor;
} Membership;

typedef struct GuildsState {
    Membership m[MEMBERSHIPS];
    /* The crimes the guilds count (guild_count_crime): the Thieves' for a successful pickpocket, the Brotherhood's for a kill; 100 once invited. */
    int thieves_count, brotherhood_count;
    /* The minute an invitation comes, 0 for none. */
    df_u32 thieves_invite, brotherhood_invite;
    /* The minute of the last lesson (last_training_minutes), and the one blessing a temple's gift allows. */
    df_u32 last_training;
    /* The princes the player has summoned before (bit n for the prince of faction n + 1): the original sets 64 in their faction's flags. */
    df_u32 daedra_met;
    struct BlessingInfo {
        int active, target, amount, region;
        df_u32 end;
    } blessing;
} GuildsState;

/* The live memberships, kept in the session (state.c). */
GuildsState *guilds_state(void);
/* sheet_affiliation_line: a membership's rank title by guild kind (0..21) and rank (0..9), FALL.EXE's faction_rank_names. */
const char *guild_rank_name(int kind, int rank);
void guilds_clear(void);
int guilds_valid(const GuildsState *g);

/* guild_kind_of_faction: the kind of a hall's faction; -1 for a faction that is no guild. */
int guild_kind_of_faction(int faction);

/* The membership of a faction (guild_find_membership_by_faction), of a kind exactly, or one whose kind has any of the bits (64 knights, 128 temples). */
Membership *guild_find_faction(int faction);
Membership *guild_find_kind(int kind);
Membership *guild_find_bits(int bits);

/* guild_join_check: 1 when the guild's reputation is negative, 2 when the best of its skills is under 22 or the next under 4, else 0. */
int guild_join_check(int kind, int reputation);
/* guild_rank_for_skills: a tenth of the reputation, held by the steps the best two skills reach; at most 9. */
int guild_rank_for_skills(int kind, int reputation);

/* A step of joining or promotion: the record to show, and what happened. */
typedef struct GuildStep {
    /* The TEXT.RSC record to show as a message (0 for none), and whether it is a yes or no (the offer). */
    int text, ask;
    int joined, promoted, demoted, expelled;
    /* The map a rank gives: guild_give_map(0) the Thieves', (1) the Brotherhood's. -1 for none. */
    int map;
} GuildStep;

/*
 * guild_join_or_promote(guild, 1) for a player with no membership in the hall: the refusal (busy: record 0 of the guild, reputation: 1),
 * or the offer (record 2) to answer yes or no. A player already a knight or a temple member is not offered another of the same sort.
 * Returns 1 when there is something to show.
 */
int guild_offer(int kind, int faction, int reputation, GuildStep *step);
/* The yes: a rank-0 membership begun now, and the first rank's line. */
int guild_join(int kind, int faction, df_u32 now, GuildStep *step);
/*
 * guild_join_or_promote(guild, 0), run each visit: nothing within 28 days of the last rank; else the rank the skills and reputation give,
 * a promotion's line, a demotion's (667), or expulsion (668, the membership gone) when the reputation is negative.
 */
int guild_promote(int kind, int faction, int reputation, df_u32 now, GuildStep *step);

/* guild_local_temple_rank and guild_is_local_knight: a temple member's rank in their own region, and a knight in their order's region or from rank 4. */
int guild_local_temple_rank(int region);
int guild_is_local_knight(int region);

/* guild_count_crime: crime 5 (a successful pickpocket) counts for the Thieves, anything else (a kill passes 5) for the Brotherhood. An invitation comes three days after the count passes 6 and 15. */
void guild_count_crime(int crime, int count, df_u32 now);
/* guild_check_invitations: 1 when an invitation falls due while the player is out of doors (kind 3 or 0 in *kind); the counter goes to 100. */
int guild_invitation(df_u32 now, int outdoors, int *kind);

/*
 * What a hall's person does for the player (guild_service_dispatch, run_0006DC35.c 191..949), by the hall's guild kind, the person's own faction
 * and the player's rank (-1 when not a member): the service, with the refusals the original makes. 3100 is the rank refusal. Whether each
 * service is built is the game's; this is only the gating.
 */
enum {
    SVC_TALK, SVC_TRAINING, SVC_POTIONS, SVC_POTION_MAKER, SVC_SOULGEMS, SVC_SPYMASTER, SVC_QUEST,
    SVC_SPELLS, SVC_SPELL_MAKER, SVC_MAGIC_ITEMS, SVC_ITEM_MAKER, SVC_DAEDRA, SVC_TELEPORT, SVC_IDENTIFY,
    SVC_REPAIRS, SVC_FENCE, SVC_ARMOR, SVC_HOUSE, SVC_DONATE, SVC_CURE, SVC_BLESSING, SVC_TEMPLE_QUEST,
    /* "My services are reserved for members only", and the rank refusal (record 3100). */
    SVC_MEMBERS_ONLY, SVC_RANK_REFUSED, SVC_NONE
};
#define TEXT_RANK_REFUSED 3100
/* *heal is set when a member is healed first (guild_heal at the temple's ranks, the Mages' magicka restored for a caster). */
int guild_service(int kind, int person_faction, int rank, int *heal);
/* The label a person's menu row carries ("Buy Spells", "Training"); 0 when the person has no service (they talk). */
const char *guild_service_label(int person_faction);
/* The menu's picture for a member or not, and the words a stranger gets. */
const char *guild_picture(int member);
/* The price scale a Fighter or a Mage pays at the shop, out of 256: (10 - rank) * 256 / 10 (guild_service_dispatch). */
int guild_price_scale(int rank);

/*
 * Training (training_offer and training_update, src/lifted/sheet.c 455..520): a trainer's skills by their faction (NULL for none), the lesson's
 * price (the level times 100, four times for a stranger), refused within 12 hours of the last (record 4023); then the lesson of a chosen skill:
 * refused over 50 (4022), else the price spent, the minute kept, 10 to 20 uses times the skill's advance, record 5221 and three hours gone.
 */
const unsigned char *training_skills(int person_faction);
int training_offer(df_u32 now, int level, int member, int *price, int *text);
int training_learn(int skill, int price, df_u32 now, int *text, int *minutes);

/*
 * A temple's blessing (guild_buy_blessing, run_00070887.c 134..): none from Akatosh's (142), one at a time (454 says no gold, as the original's
 * message for it does), the target by the temple (a skill, an attribute or the region's legal standing), rank + 10 points for rank + 4 days.
 * The points are held at 100 but the amount kept is the full one, so the end takes off what was never added (blessing_remove) as the original does.
 * Cost 100 gold at the shop's prices is the caller's. *text is the record to show.
 */
int blessing_buy(int kind, int rank, int region, df_u32 now, int *text);
/* guild_expire_blessings: an old blessing's gift removed. 1 when one ended. */
int blessing_expire(df_u32 now);

/*
 * "Receive armor" at a knightly order (guild_service_dispatch, person 845): one piece of plate for each rank, in the metal the rank names (rank - 1: rank 0, which wraps
 * the original's byte, is mithril's cousin 2; a metal over 9 is held to 9), a random piece of the seven; record 461 for a rank already given, 463 for the gift. 0 when
 * the pack has no room.
 */
int knight_armor(Membership *m, int *text);
/* "Receive house" (848): record 460 until rank 9; the house itself waits for the towns' houses. 1 when the player may be given one. */
int knight_house_allowed(const Membership *m, int has_house, int *text);

/*
 * Daedra summoning (daedra_summon, src/hand/func_000202C5.c 80..170): a day of the year belongs to one of sixteen princes (the original's table; the fourth has no day
 * to be summoned on). The Mages and the temples' summoners offer it for (100 - the hall's reputation) * 1000 + 100000 gold, at most 200000: record 480 when no prince has the day,
 * a line when the temple's own enemy would be called, else the question (481). The gold is spent before the roll. The chance is 30 and the prince's reputation, and 30 more
 * when the weather is the one the prince likes (or any), unless the dice (a roll over 100 at most 5, 15 in weather 6) call the ninth prince instead. Failure is record 484.
 * A first success sets the prince as met, and what follows is his quest; a later one summons a creature of one of five kinds as an ally.
 */
typedef struct DaedraStep {
    int text, ask, price, prince;
    const char *line;
} DaedraStep;
int daedra_offer(int hall_faction, int hall_reputation, df_u32 now, DaedraStep *step);
enum { DAEDRA_FAILED, DAEDRA_QUEST, DAEDRA_ALLY };
/* The gold is the caller's to spend first. *kind is the creature kind of an ally. */
int daedra_summon(int prince, int weather, int *kind);

#endif
