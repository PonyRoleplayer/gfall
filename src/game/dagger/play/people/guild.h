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
/*
 * guild_rank_for_skills: a rank is REPUTATION_PER_RANK of reputation, held to the first step the primary skill and the second reach in their RANK_STEPS-long tables, and to MAX_RANK. To be
 * taken in, the guild's best skill must be JOIN_PRIMARY_MIN (the first step of the primary table) and its second JOIN_SECONDARY_MIN (the first of the secondary).
 */
#define REPUTATION_PER_RANK 10
/* guild_count_crime: crime 5 is a pickpocket (the Thieves), GUILD_COUNT_KILL a kill (the Brotherhood: a townsperson, a watchman). */
#define GUILD_COUNT_THIEF 5
#define GUILD_COUNT_KILL 6
#define RANK_STEPS 10
#define MAX_RANK 9
#define JOIN_PRIMARY_MIN 22
#define JOIN_SECONDARY_MIN 4
#define RANK_MONTH 40320u
/* A guild's index: the low six bits of its kind (guild_join_or_promote); the order is guild_kind_of_faction's. */
#define KIND_GUILD_MASK 63
enum { GUILD_DARK_BROTHERHOOD = 0, GUILD_MAGES = 1, GUILD_FIGHTERS = 2, GUILD_THIEVES = 3, GUILD_FIRST_KNIGHT = 4, GUILD_FIRST_TEMPLE = 14 };
#define KNIGHT_ORDERS 10
#define TEMPLES 8
/* The guild lists end with this byte (g_guild_skills). */
#define SKILL_LIST_END 255
/* A rank is a byte; an expulsion takes it past RANK_EXPELLED_OVER. Thieves at rank 6 or 8 get the map of their halls (THIEVES_MAP_RANK_*), the Brotherhood at every promotion under RANK_EXPELLED_OVER. */
#define RANK_EXPELLED_OVER 100
#define THIEVES_MAP_RANK_1 6
#define THIEVES_MAP_RANK_2 8
#define TEXT_GUILD_EXPELLED 668
#define TEXT_GUILD_DEMOTED 667
/* The messages a guild has: refused (no skill), reputation too low, ask, welcome, then one for each rank from GUILD_MSG_RANK0. */
enum { GUILD_MSG_REFUSED = 0, GUILD_MSG_REPUTATION = 1, GUILD_MSG_ASK = 2, GUILD_MSG_RANK0 = 3 };
/* A knight from elsewhere counts as local at this rank. */
#define KNIGHT_ANYWHERE_RANK 4
/* The invitation counters (guild_count_crime): THIEVES_INVITE_AT thefts, BROTHERHOOD_INVITE_AT kills, then GUILD_INVITE_DELAY minutes (3 days); GUILD_INVITED marks it taken. */
#define THIEVES_INVITE_AT 6
#define BROTHERHOOD_INVITE_AT 15
#define GUILD_INVITE_DELAY 4320u
#define GUILD_INVITED 100
#define SERVICE_LABELS 89
/* Training: once in TRAINING_GAP minutes (text TEXT_TRAINING_TOO_SOON), level * TRAINING_PRICE_PER_LEVEL gold, shifted left TRAINING_STRANGER_SHIFT for a stranger; none above TRAINING_SKILL_MOST (text TEXT_TRAINING_TOO_GOOD);
 * the skill gains TRAINING_USES_MIN..MAX times its advance and TRAINING_MINUTES pass (text TEXT_TRAINED). */
#define TRAINING_GAP 720u
#define TEXT_TRAINING_TOO_SOON 4023
#define TRAINING_PRICE_PER_LEVEL 100
#define TRAINING_STRANGER_SHIFT 2
#define TRAINING_SKILL_MOST 50
#define TEXT_TRAINING_TOO_GOOD 4022
#define TRAINING_USES_MIN 10
#define TRAINING_USES_MAX 20
#define TEXT_TRAINED 5221
#define TRAINING_MINUTES 180
/* A blessing's target: BLESS_LEGAL the legal standing, bit BLESS_ATTRIBUTE an attribute, else a skill; each held at STAT_MOST (blessing_apply). */
#define BLESS_LEGAL 255
#define BLESS_ATTRIBUTE 128
/* A member's blessing lasts rank + BLESS_DAYS_BASE days and gives rank + BLESS_AMOUNT_MEMBER_BASE; a stranger (rank -1) BLESS_DAYS_BASE days and BLESS_AMOUNT_STRANGER. Temple 142's has none; one at a time (TEXT_BLESSED_ALREADY). */
#define BLESS_DAYS_BASE 4
#define BLESS_AMOUNT_MEMBER_BASE 10
#define BLESS_AMOUNT_STRANGER 8
#define TEXT_BLESSED_ALREADY 454
#define TEMPLE_AKATOSH 142
/* A Mages Guild member whose class cannot regain magicka has it filled on entering (text TEXT_MAGICKA_FILLED). */
#define TEXT_MAGICKA_FILLED 465
/* A knight's armour: one gift for each rank (TEXT_ARMOR_GIVEN if given), a piece 0..KNIGHT_ARMOR_PIECES, text TEXT_ARMOR_GIFT; a house at rank KNIGHT_HOUSE_RANK only (TEXT_HOUSE_RANK). */
#define TEXT_ARMOR_GIVEN 461
#define TEXT_ARMOR_GIFT 463
#define KNIGHT_ARMOR_PIECES 6
#define KNIGHT_HOUSE_RANK 9
#define TEXT_HOUSE_RANK 460
/* The summoning day (daedra_offer): DAEDRA_DAYS princes' days, the entry DAEDRA_DAY_NONE is the "none" sentinel (TEXT_NO_SUMMONING); a price (100 - reputation) * SUMMON_PRICE_PER_REP + SUMMON_PRICE_BASE, at most SUMMON_PRICE_MOST (asked by TEXT_SUMMON_PRICE). */
#define DAEDRA_DAYS 16
#define DAEDRA_DAY_NONE 3
#define TEXT_NO_SUMMONING 480
#define TEXT_SUMMON_PRICE 481
#define SUMMON_PRICE_PER_REP 1000
#define SUMMON_PRICE_BASE 100000
#define SUMMON_PRICE_MOST 200000
/* daedra_summon: in a storm (WEATHER_STORM) a prince is forced to PRINCE_FORCED 15 % of the time, else 5 %; the chance is 30 + the prince's reputation, +30 in his weather (DAEDRA_ANY_WEATHER: any). */
#define WEATHER_STORM 6
#define SUMMON_FORCED_STORM 15
#define SUMMON_FORCED 5
#define PRINCE_FORCED 9
#define SUMMON_CHANCE_BASE 30
#define SUMMON_WEATHER_BONUS 30
#define DAEDRA_ANY_WEATHER 100
#define DAEDRA_SERVANTS 5
/* The covens (faction type 8, factions 419..432) summon the prince the day's seed draws, 1..DAEDRA_PRINCES but never PRINCE_HIRCINE; Glenmoril (419) only ever Hircine (daedra_summon, func_000202C5.c). */
#define FACTION_COVEN_FIRST 419
#define FACTION_COVEN_LAST 432
#define FACTION_GLENMORIL 419
#define DAEDRA_PRINCES 16
#define PRINCE_HIRCINE 4
/* The quest file a prince starts, "%c0c0%cy00": his code (prince_quest_codes, through prince_index) and a letter, '0', or 'x' for Malacath's code PRINCE_CODE_MALACATH (0C0XY00, ChildGard bars it). */
#define PRINCE_CODE_MALACATH '8'
#define PRINCE_LETTER_PLAIN '0'
#define PRINCE_LETTER_MALACATH 'x'
/* The weather byte keeps the storm mark in bit 7 (a rain in a thunderstorm is not rain to the princes). */
#define WEATHER_STORM_MARK 128
#define TEXT_DAEDRA_TWICE 482
#define TEXT_DAEDRA_FAILED 484
/* The price scale is 8.8 fixed point: PRICE_SCALE_ONE is the full price, a member of rank r pays (MAX_RANK + 1 - r) / (MAX_RANK + 1). */
#define PRICE_SCALE_ONE 256
/* Every temple's stranger list: the quest giver, the donation and the cure (people factions). */
#define PERSON_TEMPLE_QUEST 240
#define PERSON_TEMPLE_DONATE 810
#define PERSON_TEMPLE_CURE 813
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
    /* The quest file's letter the day's prince asks for (PRINCE_LETTER_*). */
    int letter;
    const char *line;
} DaedraStep;
int daedra_offer(int hall_faction, int hall_reputation, df_u32 now, DaedraStep *step);
enum { DAEDRA_FAILED, DAEDRA_QUEST, DAEDRA_FOE };
/* The gold is the caller's to spend first. weather is the weather byte (storm mark included). A first success fills quest (9 bytes, upper case) with the prince's file name, which may name
 * none (Sheogorath replacing Malacath keeps his letter: the original's quirk); a later one is a creature of kind *kind, HOSTILE (the original makes it an ally, then sets its team to the
 * foes'), waiting in daedra_servant_take. */
int daedra_summon(int prince, int letter, int weather, int *kind, char *quest);
/* The servant a summoning called, once: its creature kind, or -1 for none. The frame puts it beside the player as a foe. */
int daedra_servant_take(void);

#endif
