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

int rules_cost(int value, int quality, int prices)
{
    int cost;

    cost = value < 1 ? 1 : value;
    cost = cost * prices / 1000;
    if (cost < 1) {
        cost = 1;
    }
    return 2 * (cost * (quality - 10) / 100 + cost);
}

/* A skill or a shop's level as the fixed-point the bargaining sums run on: 128 at 0, 256 at 100. */
static int bargain_term(int level)
{
    return level * 256 / 200 + 128;
}

int rules_trade_price(int cost, int quality, int mercantile, int personality, int selling)
{
    int shop;
    int by_skill;
    int by_personality;

    /* The shop's own skill: 0 at quality 0, 100 at quality 20. */
    shop = 5 * (quality - 10) + 50;
    if (selling) {
        by_skill = bargain_term(100 - shop) * bargain_term(mercantile) / 256;
        by_personality = bargain_term(100 - shop) * bargain_term(personality) / 256;
        return (179 * by_skill / 256 + 51 * by_personality / 256) * cost / 256;
    }
    by_skill = bargain_term(shop) * bargain_term(100 - mercantile) / 256;
    by_personality = bargain_term(shop) * bargain_term(100 - personality) / 256 * 64;
    return (192 * by_skill / 256 + by_personality / 256) * cost / 256;
}

int rules_repair_cost(int value, int quality, int condition, int condition_max, int prices)
{
    int base;

    if (condition >= condition_max) {
        return 0;
    }
    base = 10 * value / 100;
    return rules_cost(base < 1 ? 1 : base, quality, prices);
}

/* A chance as all of these are given: never certain, never hopeless. */
static int chance_5_95(int chance)
{
    return chance < 5 ? 5 : (chance > 95 ? 95 : chance);
}

int rules_shoplift_chance(int pickpocket, int quality, int weight_and_count)
{
    return chance_5_95(100 - pickpocket + quality + weight_and_count);
}

int rules_lock_chance(int lockpicking, int lock)
{
    return chance_5_95(lockpicking - 5 * lock);
}

int rules_max_loan(int level)
{
    return level * 50000;
}

int rules_loan_repayment(int amount)
{
    return amount + amount / 10;
}

int rules_letter_cost(int amount)
{
    return amount + amount / 100;
}

/* TalkManager's tables: how much each social group likes polite and blunt talk. */
static const signed char g_polite_likes[5] = { -10, 5, 10, 15, -15 };
static const signed char g_blunt_likes[5] = { 10, 5, -10, -15, 15 };
/* TalkManager's first row of how likely each social group knows: a question of where a place is. */
static const signed char g_place_known[5] = { 5, 7, 0, 0, 4 };

/* A social group as a table's index: past the groups, a merchant. */
static int social_row(int sgroup)
{
    return sgroup >= 0 && sgroup < 5 ? sgroup : 1;
}

int rules_talk_value(int personality, int is_place, int reaction, int tone, int sgroup, int skill, int skill_roll)
{
    int value;

    value = personality / 5 + reaction;
    if (is_place) {
        value += 5;
    }
    if (tone != RULES_TONE_NORMAL) {
        value += tone == RULES_TONE_POLITE ? g_polite_likes[social_row(sgroup)] : g_blunt_likes[social_row(sgroup)];
        value += skill_roll < skill ? 5 : -10;
    }
    return value;
}

int rules_talk_outcome(int value, int roll)
{
    if (value < roll) {
        return 0;
    }
    return value <= roll + 20 ? 1 : 2;
}

int rules_talk_knows(int sgroup, int roll)
{
    return roll <= g_place_known[social_row(sgroup)] + 10;
}

int rules_compass(float dx, float dz)
{
    int east;
    int north;

    /* Without a tangent: which of the three bands of slope each way falls in. tan(22.5) is about 0.414. */
    east = dx > 0.0f ? 1 : -1;
    north = dz > 0.0f ? 1 : -1;
    dx = dx < 0.0f ? -dx : dx;
    dz = dz < 0.0f ? -dz : dz;
    if (dz < dx * 0.414f) {
        return east > 0 ? 0 : 4;
    }
    if (dx < dz * 0.414f) {
        return north > 0 ? 2 : 6;
    }
    if (north > 0) {
        return east > 0 ? 1 : 3;
    }
    return east > 0 ? 7 : 5;
}

int rules_sentence(int base, int per_point, int low, int high, int legal)
{
    int amount;

    amount = base + per_point * (legal < 0 ? -legal : legal);
    return amount < low ? low : (amount > high ? high : amount);
}
