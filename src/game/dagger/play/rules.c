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

float rules_max_carry(int strength)
{
    return (float)strength * 1.5f;
}

int rules_attribute_modifier(int attribute)
{
    return attribute / 10 - 5;
}

int rules_magic_resist(int willpower)
{
    return willpower / 10;
}

int rules_skill_uses_needed(int value, int skill_advance, float class_advance, int level)
{
    float growth;
    int l;

    /* 1.04 to the level, by hand: this game has no libm, and a level is small. */
    growth = 1.0f;
    for (l = 0; l < level; l++) {
        growth *= 1.04f;
    }
    return df_ifloor((float)value * (float)skill_advance * class_advance * growth * 0.4f) + 1;
}

float rules_class_advance(df_u32 raw)
{
    return (float)raw / 65536.0f;
}

int rules_level_from_sums(int start_sum, int now_sum)
{
    return (now_sum - start_sum + 28) / 15;
}

int rules_level_health(int hp_per_level, int roll, int endurance)
{
    int gain;

    gain = hp_per_level / 2 + roll + rules_attribute_modifier(endurance);
    return gain < 1 ? 1 : gain;
}

int rules_rest_health(int endurance, int medical, int health_max)
{
    int gain;

    gain = rules_attribute_modifier(endurance) + (medical + 60) * health_max / 1000;
    return gain < 1 ? 1 : gain;
}

int rules_rest_fatigue(int fatigue_max)
{
    int gain;

    gain = fatigue_max / 8;
    return gain < 1 ? 1 : gain;
}

int rules_rest_magicka(int magicka_max)
{
    int gain;

    if (magicka_max <= 0) {
        return 0;
    }
    gain = magicka_max / 8;
    return gain < 1 ? 1 : gain;
}

float rules_swim_speed(float base, int swimming)
{
    return base * ((float)swimming / 200.0f) + base / 4.0f;
}

int rules_max_breath(int endurance)
{
    return endurance / 2;
}

float rules_reflex_uses(int reflexes)
{
    return 1.0f + 0.125f * (float)(RULES_REFLEX_AVERAGE - reflexes);
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
