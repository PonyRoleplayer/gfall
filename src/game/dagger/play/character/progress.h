#ifndef GFALL_PROGRESS_H
#define GFALL_PROGRESS_H

/*
 * Skills and levels (level_skill_sum, skill_ready_to_advance, raise_skills, sheet_open; inline in the decomp). The level sum takes the PRIMARY_SKILLS primaries, MAJOR_SKILLS majors and
 * MINOR_SKILLS minors (the lowest of the first two groups, the best minor). A skill needs its advance times SKILL_LEVEL_GROWTH to the level, over SKILL_USES_DIVISOR, times
 * SKILL_USES_NUMERATOR / SKILL_USES_DENOMINATOR plus one, the uses scaled by the reflexes (REFLEXES_STEP_DIV). Skills are looked at after SKILL_CHECK_MINUTES (six hours); a primary or major at
 * SKILL_MOST is mastered, and then none rises past SKILL_CAP_MASTERED. The raised skills are kept in two words of SKILL_BITS_LO bits. A level is (sum - the starting sum + LEVEL_SUM_ADD) /
 * LEVEL_SUM_DIV; its health is rand(hp/2, hp) + END / LEVEL_HP_END_DIV - LEVEL_HP_END_BASE and it gives LEVEL_POINTS_MIN..MAX points (no attribute past STAT_MOST, and the sheet will not close
 * with points left under LEVEL_ATTRIBUTE_SUM_MOST). SND_LEVEL_UP plays.
 */
#define PRIMARY_SKILLS 3
#define MAJOR_SKILLS 3
#define MINOR_SKILLS 6
#define SKILL_LEVEL_GROWTH 1.04
#define SKILL_USES_DIVISOR 65536.0
#define SKILL_USES_NUMERATOR 2.0
#define SKILL_USES_DENOMINATOR 5.0
#define REFLEXES_STEP_DIV 8.0
#define SKILL_CHECK_MINUTES 360
#define SKILL_MOST 100
#define SKILL_CAP_MASTERED 95
#define SKILL_BITS_LO 32
#define LEVEL_SUM_ADD 28
#define LEVEL_SUM_DIV 15
#define LEVEL_HP_END_DIV 10
#define LEVEL_HP_END_BASE 5
#define LEVEL_POINTS_MIN 4
#define LEVEL_POINTS_MAX 6
#define STAT_MOST 100
#define LEVEL_ATTRIBUTE_SUM_MOST 800
#define SND_LEVEL_UP 364

/* Persistent original skill/rest/level advancement. See docs/game.md. */
#include "game/dagger/tables/tables.h"
#include "engine/core/df.h"
#define OGHMA_POINTS 30

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
