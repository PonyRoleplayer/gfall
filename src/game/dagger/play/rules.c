#include "game/dagger/play/rules.h"

/*
 * See rules.h. The numbers are Daggerfall Unity's (MIT License,
 * Copyright (c) 2009-2023 Daggerfall Workshop): PlayerSpeedChanger and
 * AcrobatMotor, which match classic's walk "over the same distance".
 */

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

float rules_jump_height(int jumping)
{
    float v;

    v = rules_jump_speed(jumping);
    return v * v / (2.0f * JUMP_GRAVITY);
}
