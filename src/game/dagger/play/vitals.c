#include "game/dagger/play/vitals.h"
#include "game/dagger/play/cheats.h"

#include "engine/core/df.h"

int bar_ratio(int numerator, int denominator)
{
    int height;

    if (numerator <= 0 || denominator <= 0) {
        return 0;
    }
    height = (((numerator << 8) / denominator) << 5) / 256;
    return height > 0 ? height : 0;
}

int bar_fatigue(int fatigue, int strength, int endurance)
{
    int max;
    int height;

    if (fatigue <= 0) {
        return 0;
    }
    max = (strength + endurance) << 6;
    if (max <= 0) {
        return 0;
    }
    height = (((fatigue << 8) / max) << 5) >> 8;
    return height > 0 ? height : 0;
}

int jump_magnitude(int speed, int strength, int jumping)
{
    int magnitude;

    magnitude = ((speed + strength) / 2) + 50;
    magnitude += (jumping * magnitude) / 100;
    return magnitude;
}

int jump_impulse(int magnitude, int acrobat, int class_flag_bit2)
{
    int impulse;

    impulse = acrobat ? magnitude * 2 : magnitude;
    if (class_flag_bit2) {
        impulse = magnitude + (magnitude >> 1);
    }
    return impulse;
}

float jump_ups(int impulse)
{
    return (float)impulse;
}

int fall_v(float down_units_per_sec)
{
    if (down_units_per_sec <= 0.0f) {
        return 0;
    }
    return df_iround(down_units_per_sec * 256.0f);
}

int fall_damage(int v)
{
    int d;

    if (v <= 0) {
        return 0;
    }
    d = ((v / 256) / 40) - 7;
    if (d <= 0) {
        return 0;
    }
    return (d * d) / 2;
}

int fatigue_add(int fatigue, int amount, int strength, int endurance)
{
    int max;

    if (!god_mode() || amount > 0) { fatigue += amount; }
    max = (strength + endurance) << 6;
    if (max < 0) {
        max = 0;
    }
    if (fatigue > max) {
        fatigue = max;
    }
    if (fatigue < 1) {
        return 0;
    }
    return fatigue;
}
