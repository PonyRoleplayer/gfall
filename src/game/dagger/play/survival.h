#ifndef GFALL_SURVIVAL_H
#define GFALL_SURVIVAL_H

/* Original breath_update and minute-based fatigue_update. The engine only
 * supplies body/water queries. See docs/gfall.md. */
#include "engine/core/df.h"
#include "engine/scene/scene.h"
typedef struct Survival {
    int breath;
    df_u32 ticks, last_tick;
    float fraction;
    float climb_time;
    df_u32 climb_minute;
    int climbing, climb_failed;
} Survival;
Survival *survival_state(void);
void survival_step(const Scene *scene, float seconds);
void survival_minutes(int count, int running, int climbing);

#endif
