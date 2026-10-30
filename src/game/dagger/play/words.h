#ifndef DF_WORDS_H
#define DF_WORDS_H

#include "game/dagger/play/character.h"

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

/* What the macros say, set before words_say. Anything left 0 or empty says nothing. */
typedef struct Words {
    const Character *pc;
    /* What is being asked about (%key), and the place named in a hint (%loc). */
    const char *key;
    const char *loc;
    /* The way there, in a compass word (%di), and the hint made of it (%hnt). */
    const char *direction;
    const char *hint;
    /* The town, the region and the shop or house this is in (%cn, %rn, %cpn). */
    const char *city;
    const char *region;
    const char *place;
    /* The person's faction (%fnpc and the like). */
    const char *faction;
    /* A court's words (%cri, %pen) and sums (%dip days, %a gold). */
    const char *crime;
    const char *penalty;
    int days;
    int amount;
    /* The tone 0..2, what the person thinks of the player: which greeting, and a name or "stranger". */
    int tone;
    int reaction;
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
