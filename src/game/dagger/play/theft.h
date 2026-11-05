#ifndef DF_THEFT_H
#define DF_THEFT_H

#include "game/dagger/play/player.h"

/*
 * Stealing from a person: classic's Steal mode on a street citizen. Read
 * docs/game.md (Steal and theft) before editing.
 *
 * An attempt rolls the Pickpocket skill against rules_pickpocket_chance and
 * always counts a use of the skill. A success finds 1 to 6 gold two times in
 * three, else nothing worth the name. A failure is the crime of pickpocketing
 * in the region (the reputation's loss, no court: the guards answer it, which
 * is the game's to start with watch_alarm). Daggerfall Unity lets each person
 * be tried once while they are loaded, its comment says classic lets endless
 * tries, which we did not confirm: so once, and the memory is dropped with the
 * place (theft_forget), as DFU's flag goes with the person.
 *
 * Plain C: hostcheck runs it. Nothing here is drawn or saved.
 */

enum {
    /* Tried already: no roll, no use of the skill. */
    THEFT_TRIED,
    /* Picked, nothing of value. */
    THEFT_NOTHING,
    /* Picked, gold taken (*gold). */
    THEFT_GOLD,
    /* Seen: the crime is noted, the guards are the game's to send. */
    THEFT_CAUGHT
};

/* The persons tried are forgotten: a place entered, or a game started. */
void theft_forget(void);
/*
 * An attempt on the person who (people_in_view's number, or a creature's own
 * key hashed), the thief p's level level_gap above theirs (0 for a citizen),
 * in the region. A region under 0 notes no crime: a class enemy's pocket is
 * no one's law, and a failure only wakes them (DFU). A success's gold is put in
 * the pack and, in *gold, how much (0 for none).
 */
int theft_pickpocket(Player *p, unsigned who, int level_gap, int region, int *gold);
/* The line that tells of a result, into out (cap). */
void theft_words(int result, int gold, char *out, int cap);

#endif
