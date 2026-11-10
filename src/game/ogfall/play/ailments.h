#ifndef OGFALL_AILMENTS_H
#define OGFALL_AILMENTS_H
#include "game/ogfall/play/magicstate.h"
int og_disease_infect(int target, int disease, int no_resist);
int og_poison_apply(int target, int poison, int no_resist);
void og_ailments_cure(int target, int poison);
/* guild_cure_diseases counts the target's diseases (an ailment whose id is under 100). */
int og_ailments_disease_count(int target);
void og_ailments_minutes(df_u32 start, df_u32 count);
void og_ailments_strike(int creature, int kind);
#endif
