#ifndef DF_WORDS_H
#define DF_WORDS_H


/*
 * What people say: TEXT.RSC's records with their %macros filled in. Read
 * docs/game.md (People) before editing.
 *
 * A record is several variants of one line, and one is chosen at random. Its
 * macros name the player, the place and what is being talked of. Some name
 * other records (%1com, a greeting by tone), which are chosen and filled in
 * turn. A macro this table does not know comes out as nothing.
 *
 * Plain C: hostcheck runs it.
 */

/* The game's dice for the variant of a record (n variants: 0..n-1); a simple generator of its own until it gives them. */
void words_set_roll(int (*roll)(int n));

/* The game's live character and the race names its character sheet uses. */
struct CharacterState;
void words_set_player(const struct CharacterState *player, const char *(*race_name)(int race));

/* Values supplied by the caller. Player names, race and honorific come from the current character. */
typedef struct Words {
    /* The caller's choice of oath, with the default at 0. */
    int oath_race;
    /* What is being asked about (%key), and the place named in a hint (%loc). */
    const char *key;
    const char *loc;
    /* The way there, in a compass word (%di), and the hint made of it (%hnt). */
    const char *direction;
    const char *hint;
    /* The town, the region and the shop or house this is in (%cn, %rn, %cpn); a journey's end (%tcn) is the place too. */
    const char *city;
    const char *region;
    const char *place;
    /* Holiday notices name the settlement type (%ct) and the local ruler's title (%t). */
    const char *location_type;
    const char *ruler_title;
    /* %ef draws the owner's given name on the current building's name stream. */
    void (*shop_owner)(char *out, int cap);
    /* The giver of a quest the player is pointed to (%pqn their name, %pqp their building's name). */
    const char *questor;
    const char *questor_place;
    /* The person's faction (%fnpc and the like). */
    const char *faction;
    /* The player's rank in the guild speaking (%lev). */
    const char *guild_rank;
    /* A court's words (%cri, %pen) and sums (%dip days, %a gold). */
    const char *crime;
    const char *penalty;
    int days;
    int amount;
    /* A sentence made longer (%pdg days) and the prison a death sentence names (%prn). */
    int more_days;
    const char *prison;
    /* The tone 0..2, what the person thinks of the player: which greeting, and a name or "stranger". */
    int tone;
    int reaction;
    /* A Daedra prince's name (%dae) and the day's date (%dat), as the summoning's texts 481, 482 and 484 say them. */
    const char *daedra;
    const char *date;
    /* A rumour's two factions (%fx1, %fx2), the ruler of the region it is about (%nrn) and the temple it outlaws (%ptm). */
    const char *faction1, *faction2, *ruler, *temple;
    /* The old ruler's name (%ol1) and fate (%olf), the title (%lt1 and %rt), and his or her (%g3). */
    const char *ruler_old, *fate, *title, *pronoun;
} Words;

/* The macros' values, until the next call. w is read, not copied: it stays valid while words_say is called. */
void words_use(const Words *w);
/* One variant of record, filled in, in out. Its length; -1 when the record does not read. */
int words_say(int record, char *out, int cap);
/* The same, but -1 for a variant with a macro the game cannot say yet (a rumour's other factions and rulers): the caller tries another. */
int words_say_plain(int record, char *out, int cap);
/* src filled in, in out. */
void words_expand(const char *src, char *out, int cap);

#endif
