#ifndef GFALL_STEAL_H
#define GFALL_STEAL_H

/*
 * Taking what is not the player's: a pocket picked, a shop's counter emptied, a shelf looked through.
 * Slice 7 of people and law (docs/gfall-people.md), from pickpocket_attempt (src/lifted/steal.c), the
 * trade screen's steal button (trade_steal_button, src/lifted/inven.c 1416) and the shelf click
 * (src/lifted/click.c 955..990), with notes in docs/ai-social-math.md ("Dialogue disposition", the shop steal
 * lines) and docs/character-skills-math.md. The original's own lines are read out of FALL.EXE
 * (tables/stealdata.h). Pure rules over the session; a failure notes the crime and calls the watch.
 */

#define SKILL_PICKPOCKET 15
#define SKILL_STEALTH 16

typedef struct StealState {
    /* 1 when it worked. */
    int ok;
    /* The line the original shows, and a TEXT.RSC record for "found nothing" (8999), 0 for none. */
    const char *line;
    int text;
    /* Gold taken. */
    int gold;
    /* A failure: the person is not to be tried again, and a creature's pocket woke every creature there. */
    int spoiled, wake;
} StealState;

/*
 * pickpocket_attempt: Pickpocket (and a creature's level against the player's, five a level), held 5..95; fail on
 * a roll above it (rand() % 101): crime 12, the watch, a creature's company woken, a townsperson's pocket not tried
 * again. Else a third of the time there is nothing (record 8999); else 1 to 5 gold. One use of the skill each try.
 */
int steal_pocket(int creature, int victim_level, StealState *out);
/*
 * trade_steal_button: Pickpocket less a 32nd of the basket's total and a quarter of what the player carries
 * (the pack's weight in the original's units, quarters of a kilo); fail on a roll above it (no floor, so a
 * heavy pack fails always): one use of Stealth, crime 13, the watch. The caller closes the screen and, on a
 * success, makes the goods the player's.
 */
int steal_shop(int total, int weight, StealState *out);
/*
 * A shelf looked through in a shop (click.c 975): once asked and answered yes, a roll of 0..255 at or above 108
 * less Pickpocket is seen: crime 13 and the watch. 1 when seen.
 */
int steal_shelf(void);

#endif
