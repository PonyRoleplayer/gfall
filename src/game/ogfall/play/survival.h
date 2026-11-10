#ifndef OGFALL_SURVIVAL_H
#define OGFALL_SURVIVAL_H

/* Original breath_update and minute-based fatigue_update. The engine only
 * supplies body/water queries. See docs/ogfall.md. */
#include "engine/core/df.h"
#include "engine/scene/scene.h"
typedef struct OgSurvival {
    int breath;
    df_u32 ticks, last_tick;
    float fraction;
    float climb_time;
    df_u32 climb_minute;
    int climbing, climb_failed;
} OgSurvival;
OgSurvival *og_survival(void);
void og_survival_step(const Scene *scene, float seconds);
void og_survival_minutes(int count, int running, int climbing);

#endif
