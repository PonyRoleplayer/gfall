#ifndef GFALL_SURVIVAL_H
#define GFALL_SURVIVAL_H

/*
 * Breath and fatigue (breath_update, fatigue_update, fatigue_add; inline in the decomp). Breath is END / BREATH_END_DIV, at most BREATH_MOST, and goes one a BREATH_TICKS BIOS ticks
 * (BIOS_HZ a second). A minute of effort costs FATIGUE_MOVE of fatigue, FATIGUE_SWIM_FAIL when a swimming roll fails, FATIGUE_RUN running, FATIGUE_CLIMB climbing; at nothing the player
 * collapses, dead if a creature is within ENEMIES_NEAR (ours; the original: any creature), else an hour passes (COLLAPSE_MINUTES).
 */
#define BIOS_HZ 18.2065f
#define BREATH_END_DIV 2
#define BREATH_MOST 50
#define BREATH_TICKS 18
#define FATIGUE_MOVE 11
#define FATIGUE_SWIM_FAIL 44
#define FATIGUE_RUN 88
#define FATIGUE_CLIMB 22
#define COLLAPSE_MINUTES 60

/* Original breath_update and minute-based fatigue_update. The engine only
 * supplies body/water queries. See docs/game.md. */
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
