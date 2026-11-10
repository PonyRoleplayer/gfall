#ifndef OGFALL_PROGRESS_H
#define OGFALL_PROGRESS_H

/* Persistent original skill/rest/level advancement. See docs/ogfall.md. */
#include "game/ogfall/tables/tables.h"
#include "engine/core/df.h"
typedef struct OgProgress {
    df_u32 checked, raised_lo, raised_hi;
    int base_sum, points, active;
    int floor[OG_ATTRS];
} OgProgress;
OgProgress *og_progress(void);
int og_progress_sum(const OgCharacter *who, const OgClass *career);
int og_progress_needed(int value, int level, int multiplier, int skill);
void og_progress_begin(void);
void og_progress_rest(void);
int og_progress_adjust(int attribute, int direction);
int og_progress_finish(void);
int og_progress_raised(int skill);

#endif
