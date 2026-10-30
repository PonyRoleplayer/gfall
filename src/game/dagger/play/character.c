#include "game/dagger/play/character.h"
#include "game/dagger/play/rules.h"
#include "engine/core/mem.h"

/* See character.h. */

int character_new(Character *c, int race, int klass, int female, const char *name)
{
    const CareerClass *k;
    int i;

    k = career_class(klass);
    if (!k || !career_race(race)) {
        return 0;
    }
    mem_zero(c, sizeof(*c));
    df_copy(c->name, name, (int)sizeof(c->name));
    c->race = race;
    c->klass = klass;
    c->female = female;
    c->level = 1;
    /* Bare, every part 100 (DFU's), until something is worn (inventory_armor). */
    for (i = 0; i < BODY_PARTS; i++) {
        c->armor[i] = 100;
    }
    for (i = 0; i < ATTR_COUNT; i++) {
        c->attribute[i] = (int)k->attribute[i] + RULES_ATTRIBUTE_ROLL;
    }
    for (i = 0; i < SKILL_COUNT; i++) {
        c->skill[i] = RULES_OTHER_SKILL;
    }
    /* Minor first, so a skill a class names twice keeps its higher group's value. */
    for (i = 0; i < 6; i++) {
        if (k->minor[i] < SKILL_COUNT) {
            c->skill[k->minor[i]] = RULES_MINOR_SKILL;
        }
    }
    for (i = 0; i < 3; i++) {
        if (k->major[i] < SKILL_COUNT) {
            c->skill[k->major[i]] = RULES_MAJOR_SKILL;
        }
    }
    for (i = 0; i < 3; i++) {
        if (k->primary[i] < SKILL_COUNT) {
            c->skill[k->primary[i]] = RULES_PRIMARY_SKILL;
        }
    }
    c->reflexes = RULES_REFLEX_AVERAGE;
    c->health_max = rules_max_health(c->level, k->hp_per_level);
    character_attributes_changed(c);
    character_rest_full(c);
    character_start_levelling(c);
    return 1;
}

void character_attributes_changed(Character *c)
{
    const CareerClass *k;

    k = career_class(c->klass);
    c->fatigue_max = rules_max_fatigue(c->attribute[ATTR_STRENGTH], c->attribute[ATTR_ENDURANCE])
                     * CHARACTER_FATIGUE_UNIT;
    c->magicka_max = k ? rules_max_magicka(c->attribute[ATTR_INTELLIGENCE], k->magicka) : 0;
}

void character_tally(Character *c, int skill, int n)
{
    if (skill >= 0 && skill < SKILL_COUNT) {
        c->skill_uses[skill] += n;
    }
}

/* The highest of a class's group of skills, leaving out the one at index skip (-1: none). */
static int highest_of(const Character *c, const df_u8 *group, int n, int skip)
{
    int best;
    int i;

    best = 0;
    for (i = 0; i < n; i++) {
        if (i != skip && group[i] < SKILL_COUNT && c->skill[group[i]] > best) {
            best = c->skill[group[i]];
        }
    }
    return best;
}

/* Which of the major skills is lowest: the one the level sum leaves out. */
static int lowest_major(const Character *c, const CareerClass *k)
{
    int lowest;
    int i;

    lowest = 0;
    for (i = 1; i < 3; i++) {
        if (c->skill[k->major[i]] < c->skill[k->major[lowest]]) {
            lowest = i;
        }
    }
    return lowest;
}

int character_level_sum(const Character *c)
{
    const CareerClass *k;
    int skipped;
    int sum;
    int i;

    k = career_class(c->klass);
    if (!k) {
        return 0;
    }
    sum = 0;
    for (i = 0; i < 3; i++) {
        sum += c->skill[k->primary[i]];
    }
    skipped = lowest_major(c, k);
    for (i = 0; i < 3; i++) {
        if (i != skipped) {
            sum += c->skill[k->major[i]];
        }
    }
    return sum + highest_of(c, k->minor, 6, -1);
}

void character_start_levelling(Character *c)
{
    c->level_sum_start = character_level_sum(c);
    c->level_due = 0;
}

/* 1 when a primary skill has reached the top: no other skill goes past the cap then. */
static int primary_mastered(const Character *c, const CareerClass *k)
{
    return highest_of(c, k->primary, 3, -1) >= CHARACTER_SKILL_MASTERED;
}

int character_raise_skills(Character *c, float now)
{
    const CareerClass *k;
    float class_advance;
    float reflex_uses;
    int rose;
    int i;

    if (now - c->skills_checked < CHARACTER_SKILLS_EVERY) {
        return 0;
    }
    c->skills_checked = now;
    k = career_class(c->klass);
    if (!k) {
        return 0;
    }
    class_advance = rules_class_advance(k->advancement);
    reflex_uses = rules_reflex_uses(c->reflexes);
    rose = 0;
    for (i = 0; i < SKILL_COUNT; i++) {
        const CareerSkill *s;
        int needed;

        s = career_skill(i);
        if (!s) {
            continue;
        }
        needed = rules_skill_uses_needed(c->skill[i], s->advance, class_advance, c->level);
        if ((float)c->skill_uses[i] * reflex_uses < (float)needed) {
            continue;
        }
        c->skill_uses[i] = 0;
        if (c->skill[i] < CHARACTER_SKILL_CAP
            || (c->skill[i] < CHARACTER_SKILL_MASTERED && !primary_mastered(c, k))) {
            c->skill[i]++;
            rose = 1;
        }
    }
    if (rules_level_from_sums(c->level_sum_start, character_level_sum(c)) > c->level) {
        c->level_due = 1;
    }
    return rose;
}

void character_level_up(Character *c, int roll)
{
    const CareerClass *k;

    k = career_class(c->klass);
    c->level++;
    c->level_due = 0;
    if (k) {
        c->health_max += rules_level_health(k->hp_per_level, roll, c->attribute[ATTR_ENDURANCE]);
    }
}

void character_rest_full(Character *c)
{
    c->health = c->health_max;
    c->fatigue = c->fatigue_max;
    c->magicka = c->magicka_max;
}

int character_hurt(Character *c, int amount)
{
    c->health -= amount;
    if (c->health < 0) {
        c->health = 0;
    }
    return c->health == 0;
}
