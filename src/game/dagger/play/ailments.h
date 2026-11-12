#ifndef GFALL_AILMENTS_H
#define GFALL_AILMENTS_H
#include "game/dagger/play/magicstate.h"
int disease_infect(int target, int disease, int no_resist);
int poison_apply(int target, int poison, int no_resist);
void ailments_cure(int target, int poison);
/* guild_cure_diseases counts the target's diseases (an ailment whose id is under 100). */
int ailments_disease_count(int target);
void ailments_minutes(df_u32 start, df_u32 count);
void ailments_strike(int creature, int kind);
#endif
