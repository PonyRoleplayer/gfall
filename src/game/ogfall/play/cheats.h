#ifndef OGFALL_CHEATS_H
#define OGFALL_CHEATS_H

/* Session-only console switches; never stored in a saved character. */
void og_cheats_startup(void);
int og_god_mode(void);
void og_god_set(int enabled);
/* Resource debits use these even when no spell is currently being cast. */
int og_magicka_cost(int cost);
int og_magicka_spend(int cost);
void og_god_protect(void);

#endif
