#ifndef GFALL_PROGRESS_H
#define GFALL_PROGRESS_H

/* Persistent original skill/rest/level advancement. See docs/gfall.md. */
#include "game/dagger/tables/tables.h"
#include "engine/core/df.h"
typedef struct ProgressState {
    df_u32 checked, raised_lo, raised_hi;
    int base_sum, points, active;
    int floor[ATTRS];
} ProgressState;
ProgressState *progress_state(void);
int progress_sum(const CharacterState *who, const ClassInfo *career);
int progress_needed(int value, int level, int multiplier, int skill);
void progress_begin(void);
void progress_rest(void);
int progress_adjust(int attribute, int direction);
int progress_finish(void);
int progress_raised(int skill);

#endif
