#ifndef GFALL_CHEATS_H
#define GFALL_CHEATS_H

/* Session-only console switches; never stored in a saved character. */
void cheats_startup(void);
int god_mode(void);
void god_set(int enabled);
/* Resource debits use these even when no spell is currently being cast. */
int magicka_cost(int cost);
int magicka_spend(int cost);
void god_protect(void);
void fatigue_spend(int amount);

#endif
