#ifndef GFALL_POLITICS_H
#define GFALL_POLITICS_H

/*
 * The world's politics, prices and news (faction_politics_update, src/hand/func_00019676.c; region_update_prices, tamriel.c 429; region_flag_set, rumor.c 56; rumor_add_faction,
 * func_0001CF3E.c; the notes in the decomp's docs/world-economy-politics.md). Every 383040 minutes (when a minute is a multiple of both 10080 and 54720) each faction of type 2, 3 and 7 drifts in power,
 * breaks, makes and fights its alliances and enmities, may get a new ruler, and a noble's region may fall to famine, plague, the persecution of a temple, a crime wave or witch burnings; twelve
 * rounds are played at a new game before the first rumour is written. Each day the prices of the regions drift. Both leave news: a rumour a faction tells, or a region's notice board shows.
 *
 * Bugs of the original that are not kept (docs/game-later.md, Politics): a faction with no region is never indexed into the regions' flags (war, famine and the rest were read for it past the
 * array); a battle cannot take a power under 1 or draw rand(1, 0); the persecuted temple stays the one that out-powered the noble (region_flag_set overwrote it with a random god); region 17's
 * "temple" is a Daedra prince (no temple there); the shared relations of two factions count what they share (the original returned a scratch word); the neighbours' zero padding is not region 0.
 * The state: standing.h's relations (saved with the standing) and PoliticsState here (saved from schema 20).
 */
#include "engine/core/df.h"
#include "game/dagger/play/people/standing.h"

#define POLITICS_FLAGS 29
#define POLITICS_GROUPS 14
#define RUMOURS_MOST 64
#define RUMOUR_TEXT 200
/* region_flag_set's flags the rounds use (quest-hooks.md 6.1). */
enum {
    REGION_WAR_BEGINNING = 0, REGION_WAR_ONGOING, REGION_WAR_WON, REGION_WAR_LOST,
    REGION_PLAGUE_BEGINNING, REGION_PLAGUE_ONGOING, REGION_PLAGUE_ENDING,
    REGION_FAMINE_BEGINNING, REGION_FAMINE_ONGOING, REGION_FAMINE_ENDING,
    REGION_WITCH_BURNINGS, REGION_CRIME_WAVE,
    REGION_TEMPLE_PERSECUTED = 18, REGION_PRICES_HIGH = 19, REGION_PRICES_LOW = 20
};
/* The groups the final tally reads (region_event_flag_groups): war and famine. */
#define GROUP_WAR 0
#define GROUP_FAMINE 2
/* NewsRumour kinds (rumor_add_faction's): the flags a kind gives (func_0001D66C): regional for these, else a faction's news. */
enum { NEWS_WAR = 0, NEWS_PLAGUE = 4, NEWS_FAMINE = 7, NEWS_WITCHES = 10, NEWS_CRIME = 11, NEWS_NEW_RULER = 12, NEWS_TEMPLE = 18, NEWS_ALLIANCE = 26, NEWS_ENEMIES = 27, NEWS_WAR_STATE = 28, NEWS_GOSSIP = 100 };
enum { NEWSFLAG_REGIONAL = 1, NEWSFLAG_FACTION = 8 };
/* The texts (TEXT.RSC) the rounds write. */
#define TEXT_NEWS_WITCHES 1475
#define TEXT_NEWS_TEMPLE 1476
#define TEXT_NEWS_FAMINE 1477
#define TEXT_NEWS_PLAGUE 1478
#define TEXT_NEWS_WAR 1479
#define TEXT_NEWS_NEW_RULER 1480
#define TEXT_NEWS_ALLIANCE 1481
#define TEXT_NEWS_ENEMIES 1482
#define TEXT_NEWS_CRIME 1410
#define TEXT_NEWS_RULER_GOSSIP 1406
#define TEXT_NEWS_WAR_GOSSIP 1407
#define TEXT_NEWS_WAR_WON 1408
/* The general gossip: witches, vampires, Daedra, artifacts, the Thieves Guild, the Brotherhood, small talk. */
#define TEXT_GOSSIP_FIRST 1450
#define TEXT_GOSSIP_LAST 1456
/* A rumour lives RUMOUR_LIFE minutes (about 30 days); the rounds' purge keeps a faction's, never a region's. */
#define RUMOUR_LIFE 43140u
/* A faction rumour naming a faction whose flag NEWSFLAG_HUSH is set passes news only on a roll over RUMOUR_HUSH_ROLL (func_0001D54B.c). */
#define NEWSFLAG_HUSH 1
#define RUMOUR_HUSH_ROLL 75
/* The politics' own period (tamriel.c 194..214: a multiple of 10080 and of 54720), the day's, and the twelve rounds at a new game. */
#define POLITICS_PERIOD 383040u
#define POLITICS_START_ROUNDS 12
#define PRICE_DRIFT_UP 51
#define PRICE_DRIFT_DOWN 49
#define PRICE_DRIFT_DIV 50
#define PRICE_HIGH 2000
#define PRICE_LOW 500
#define PRICE_CHANCE_STEP 25
#define PRICE_CHANCE_BASE 50
#define PRICE_CHANCE_DIV 5
#define FACTION_MERCHANTS 510

typedef struct NewsRumour {
    short faction1, faction2, kind;
    unsigned char region, flags, used, pad;
    df_u32 expires;
    char text[RUMOUR_TEXT];
} NewsRumour;

typedef struct PoliticsState {
    int ready;
    /* The minute of the last round (what a rumour's life is counted from). */
    df_u32 now_last;
    unsigned char flags[REGIONS][POLITICS_FLAGS];
    unsigned char groups[REGIONS][POLITICS_GROUPS];
    short persecuted[REGIONS];
    /* Each faction's politics factor (20..70) and the seed its ruler is named from. */
    unsigned char factor[FACTIONS_MAX];
    df_u32 seed[FACTIONS_MAX];
    NewsRumour rumour[RUMOURS_MOST];
} PoliticsState;

PoliticsState *politics_state(void);
/* A game's politics from the table: factors rolled, no flag set. Then the twelve rounds (politics_start). */
void politics_defaults(PoliticsState *s);
int politics_valid(const PoliticsState *s);
/* A new game: POLITICS_START_ROUNDS rounds, the last one writing rumours. */
void politics_start(df_u32 now);
/* One round (faction_politics_update(2)); write: its rumours are kept. */
void politics_round(df_u32 now, int write);
/* The day's price drift of every region (region_update_prices). */
void politics_prices(void);
/* Called for each game minute that passes: the day's prices and the period's round. */
void politics_minute(df_u32 minute);
void region_flag_set(int region, int flag);
void region_flag_clear(int region, int flag);
/* region_flag_set's setter for tests and the console. */
int region_flag(int region, int flag);
/* faction_regions_border by region numbers. */
int regions_border(int a, int b);

/* rumor_pick_news (faction.c 598..632): one faction rumour at random that passes the rolls; its text; 0 when none. */
int politics_news(char *out, int cap);
/* rumor_collect_local: the notice board of a region: its regional rumours, one under another; the length, 0 when none. */
int politics_board(int region, char *out, int cap);
/* The rumours held (tests). */
int politics_rumours(void);

#endif
