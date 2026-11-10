#ifndef OGFALL_LAW_H
#define OGFALL_LAW_H

#include "engine/core/df.h"
#include "game/ogfall/play/standing.h"

/*
 * Crime and the court. Slice 5 of people and law (docs/ogfall-people.md), from the decomp:
 * crime_reputation_penalty, court_reputation_restore, court_restore_vitals, court_frame
 * (src/lifted/crime.c), court_open (src/hand/func_00020C87.c), prison_serve_sentence
 * (src/lifted/crime_000217E7.c), gold_spend and gold_total (src/lifted/support.c), with its notes in
 * docs/ai-social-math.md ("Crime, fine, jail, legal reputation"). The crime table and the fine rows
 * are read out of FALL.EXE (tables/lawdata.h, generated).
 *
 * Pure rules over the session. The court is a state machine the screen steps through: it says what
 * the original says (TEXT.RSC records) and what is to be done to the world (where the player is put,
 * how long they sit in prison), and the caller does those. The watch (guards made and sent) and the
 * crimes' callers (a witnessed theft, vagrancy) come after.
 */

#define OG_CRIMES 13

/* Crimes by the original's id, 1..13. */
enum {
    OG_CRIME_NONE,
    OG_CRIME_ATTEMPTED_BREAKING = 1,
    OG_CRIME_TRESPASS,
    OG_CRIME_BREAKING,
    OG_CRIME_ASSAULT,
    OG_CRIME_MURDER,
    OG_CRIME_TAX_EVASION,
    OG_CRIME_CONSPIRACY,
    OG_CRIME_VAGRANCY,
    OG_CRIME_SMUGGLING,
    OG_CRIME_PIRACY,
    OG_CRIME_HIGH_TREASON,
    OG_CRIME_PICKPOCKETING,
    OG_CRIME_THEFT
};

/* What is kept between frames of play (crime_current) and per region (the region's punishment_flags: 1 banished, 2 death sentence). */
typedef struct OgLaw {
    int crime;
    unsigned char punishment[OG_REGIONS];
} OgLaw;

OgLaw *og_law(void);
int og_law_valid(const OgLaw *law);

int og_crime_loss(int crime);
const char *og_crime_name(int crime);
/* crime_current: noted when the crime is seen; the watch and the court read it. */
void og_law_note(int crime);

/* While a prison sentence's days pass the legal standing does not decay (prison_serve_sentence sets D_001962B0). */
void og_law_serving(int on);
int og_law_is_serving(void);

/* gold_total, gold_can_afford and gold_spend: coins and letters of credit as one purse; a spend takes a letter that covers it, else walks the letters, then the coins. */
int og_gold_can_afford(int amount);
void og_gold_spend(int amount);

/*
 * crime_reputation_penalty (a guard strikes the player): the crime's loss off the region's legal
 * standing, and half of it off the region's own people (the faction of type 15 there).
 */
void og_crime_penalty(int region, int crime);
/* court_reputation_restore: what a sentence taken gives back (a quarter of the loss, less a point). */
void og_court_restore_reputation(int region, int change);
/* court_restore_vitals: health, magicka and fatigue to their most. */
void og_court_restore_vitals(void);

/* Where the court puts the player when it is done: a marker kind in the town (8 the street, 4 the death sentence's, 12 the acquittal's). */
enum { OG_RELEASE_NONE, OG_RELEASE_STREET = 8, OG_RELEASE_BANISHED = 4, OG_RELEASE_ACQUITTED = 12 };
/* The court's own records (TEXT.RSC). */
enum {
    OG_TEXT_COURT_ASK = 8050,
    OG_TEXT_COURT_PLEA = 8064,
    OG_TEXT_COURT_SHORT = 8052,
    OG_TEXT_COURT_SENTENCE = 8055,
    OG_TEXT_COURT_BANISHED = 8063,
    OG_TEXT_COURT_DEATH = 8060,
    OG_TEXT_COURT_ACQUITTED = 8062,
    OG_TEXT_GUILD_BROTHERHOOD = 551,
    OG_TEXT_GUILD_THIEVES = 550
};

typedef struct OgCourt {
    int open;
    /* 0-based crime, its fine, the days to serve, what the sentence gives back, and the severity (0 a banishment, 2 the usual fine and days). */
    int crime, fine, days, change, severity, region;
} OgCourt;

/* What a step of the court asks the caller to do. */
typedef struct OgCourtStep {
    /* Up to three records to show, in order (0 ends the list). */
    int text[3];
    /* Where to put the player (OG_RELEASE_*), the days to serve first, and the gold the court took. */
    int release, serve_days, gold_taken;
    /* The region's punishment flag set (1 banished, 2 death sentence), and the sentence's reputation restored. */
    int flag;
    /* Another question follows: the plea (8064). */
    int ask_plea;
    int done;
} OgCourtStep;

/* The membership the guilds give (their rank, -1 for none): the Dark Brotherhood clears crimes 4 and 5, the Thieves Guild crimes 1..3 and 12. */
typedef struct OgCourtGuilds {
    int brotherhood_rank, thieves_rank;
} OgCourtGuilds;

/*
 * court_open: the fine and days from the crime (1..13) and the region's legal standing, the severity,
 * the court's question. A guild may clear it at once (step->done, its record in text[0]): rank >=
 * rand_range(0, 19) for the matching crimes. 0 for no crime.
 */
int og_court_open(OgCourt *court, int crime, int region, const OgCourtGuilds *guilds, OgCourtStep *step);
/* The answer to the first question: guilty (accept the sentence) is the court's Yes. */
void og_court_accept(OgCourt *court, OgCourtStep *step);
/* The plea after a no, by Etiquette (the polite word) or Streetwise: a skill's rank, and the person's Personality. */
void og_court_plead(OgCourt *court, int streetwise, int skill, int personality, OgCourtStep *step);
/* After a court (court_close): a prison sentence's days pass one at a time, then 240 minutes more, with the fatigue kept (the caller passes the time). */
#define OG_COURT_CLOSE_MINUTES 240

#endif
