#ifndef DF_COURT_H
#define DF_COURT_H

#include "game/dagger/play/player.h"

/*
 * The law: a crime committed, and classic's court (CORT01I0.IMG, then PRIS00I0.IMG
 * for a prison sentence). Read docs/game.md (People, The law) before editing.
 *
 * A crime costs the region's legal reputation what its record says, and half that
 * with its people. Being seen is being arrested: there are no guards to fight yet,
 * so the court opens at once. The trial is DFU's DaggerfallCourtWindow's: a sentence
 * from the crime's sums, split into 40 gold or three days by the piece, guilty for
 * half of it, not guilty to argue or lie, or banished by a very low standing. The
 * fine is taken, the days pass, and the player is put out in the street, a sentence
 * done raising the reputation by a little.
 *
 * Plain C: hostcheck drives it by clicks.
 */

/* A crime committed in this region, seen. The court opens on the game's next frame. */
void court_commit(int crime, int region);
/* 1 while a crime waits to be tried. */
int court_pending(void);
/* What is waiting is dropped: a new game, or one loaded. */
void court_forget(void);

/* What a sentence comes to: fine and days from the sums, with banishment by standing. Dice: whichever the court's are. Plain, for tests. */
typedef struct Sentence {
    int fine;
    int days;
    int banish;
} Sentence;
void court_sentence(int crime, int legal, int gold, Sentence *out);

enum {
    COURT_STAY,
    /* It is over: the player goes out to the street. */
    COURT_LEAVE
};

int court_open(Player *p);
void court_close(void);
void court_compose(void);
const unsigned char *court_pixels(void);
int court_changed(void);
int court_click(int x, int y);
/* Each frame: prison's days counting down. */
int court_step(float dt);
/* What a test reads: the sentence the court gave, and the words now on the scroll. */
const Sentence *court_given(void);
const char *court_words(void);

#endif
