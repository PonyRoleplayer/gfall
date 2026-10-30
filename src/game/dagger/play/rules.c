#include "game/dagger/play/rules.h"
#include "engine/core/df.h"

/*
 * See rules.h. The numbers are Daggerfall Unity's (MIT License,
 * Copyright (c) 2009-2023 Daggerfall Workshop): PlayerSpeedChanger and
 * AcrobatMotor, which match classic's walk "over the same distance";
 * FormulaHelper, DaggerfallEntity and PlayerHealth for the vitals.
 */

#define BASE_HEALTH 25
#define FALL_SAFE 5.0f
#define FALL_PER_METRE 5.0f

/* Classic speed units to metres a second, and the walking base. */
#define SPEED_RATIO 39.5f
#define WALK_BASE 150.0f
#define JUMP_SPEED 4.5f
#define JUMP_SKILL 0.5f
#define JUMP_GRAVITY 20.0f

float rules_walk_speed(int speed)
{
    float drag;

    drag = 0.5f * (float)(100 - (speed >= 30 ? speed : 30));
    return ((float)speed + WALK_BASE - drag) / SPEED_RATIO;
}

float rules_run_speed(int speed, int running)
{
    return ((float)speed + WALK_BASE) / SPEED_RATIO * (1.35f + (float)running / 200.0f);
}

float rules_gravity(void)
{
    return JUMP_GRAVITY;
}

float rules_jump_speed(int jumping)
{
    return JUMP_SPEED * (1.0f + (float)jumping * JUMP_SKILL / 100.0f);
}

int rules_max_health(int level, int hp_per_level)
{
    int h;
    int l;

    h = BASE_HEALTH + hp_per_level;
    /*
     * Each level past the first: a roll of half to all the class's hit
     * points (and endurance's modifier, with levelling in P6); its middle
     * until then.
     */
    for (l = 1; l < level; l++) {
        h += (hp_per_level / 2 + hp_per_level + 1) / 2;
    }
    return h;
}

int rules_max_fatigue(int strength, int endurance)
{
    return strength + endurance;
}

int rules_max_magicka(int intelligence, float multiplier)
{
    return df_ifloor((float)intelligence * multiplier);
}

int rules_fall_damage(float metres)
{
    return metres > FALL_SAFE ? (int)(FALL_PER_METRE * (metres - FALL_SAFE)) : 0;
}

float rules_swing_frame_time(int speed)
{
    return 3.0f * (float)(115 - speed) / 980.0f;
}

float rules_jump_height(int jumping)
{
    float v;

    v = rules_jump_speed(jumping);
    return v * v / (2.0f * JUMP_GRAVITY);
}
