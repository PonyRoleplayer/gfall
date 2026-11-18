#ifndef GFALL_AILMENTS_H
#define GFALL_AILMENTS_H
#include "game/dagger/play/magic/magicstate.h"
/*
 * Diseases and poisons (spfx_disease_daily, poison_tick, disease_infect, poison_apply; the decomp leaves the numbers inline). An ailment's id is a disease below DISEASE_CURSE_FIRST, a curse
 * from it (no daily step), a poison from POISON_FIRST (id - POISON_FIRST is the POISON_*). A disease has DISEASE_STATS stats: the eight attributes, then health, fatigue and magicka. Its days
 * left are DISEASE_FOREVER (it never ends by itself) or a roll, AILMENT_RECOVERING once it has run its course. A poison's record (poison_def) is its onset's range then its length's.
 * A creature's disease list ends with LIST_END. The creatures' bites give a disease RAT_DISEASE_CHANCE, BAT_DISEASE_CHANCE, MUMMY_DISEASE_CHANCE or VAMPIRE_DISEASE_CHANCE percent of the
 * time, and a nymph drains NYMPH_FATIGUE_MIN..MAX (of FATIGUE_UNIT).
 */
#define DISEASE_STATS 11
#define DISEASE_STAT_HEALTH 8
#define DISEASE_STAT_FATIGUE 9
#define DISEASE_STAT_MAGICKA 10
#define DISEASE_FOREVER 255
#define AILMENT_RECOVERING 254
#define DISEASE_CURSE_FIRST 100
#define POISON_FIRST 128
#define LIST_END 255
#define POISON_DELAY_MIN 0
#define POISON_DELAY_MAX 1
#define POISON_DAYS_MIN 2
#define POISON_DAYS_MAX 3
#define POISON_STAGE_DONE 2
#define RESIST_KIND_DISEASE 2
#define RAT_DISEASE_CHANCE 5
#define BAT_DISEASE_CHANCE 2
#define MUMMY_DISEASE_CHANCE 5
#define VAMPIRE_DISEASE_CHANCE 2
#define NYMPH_FATIGUE_MIN 10
#define NYMPH_FATIGUE_MAX 30
#define SPELL_CREATURE_PARALYSIS 66
enum {
    POISON_NUX_VOMICA, POISON_ARSENIC, POISON_MOONSEED, POISON_DROTHWEED, POISON_SOMNALIUS, POISON_PYRRHIC_ACID,
    POISON_MAGEBANE, POISON_THYRWORT, POISON_INDULCET, POISON_SURSUM, POISON_QUAESTO_VIL, POISON_AEGROTAT
};
int disease_infect(int target, int disease, int no_resist);
int poison_apply(int target, int poison, int no_resist);
void ailments_cure(int target, int poison);
/* guild_cure_diseases counts the target's diseases (an ailment whose id is under 100). */
int ailments_disease_count(int target);
void ailments_minutes(df_u32 start, df_u32 count);
void ailments_strike(int creature, int kind);
#endif
