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
    c->health_max = rules_max_health(c->level, k->hp_per_level);
    c->fatigue_max = rules_max_fatigue(c->attribute[ATTR_STRENGTH], c->attribute[ATTR_ENDURANCE])
                     * CHARACTER_FATIGUE_UNIT;
    c->magicka_max = rules_max_magicka(c->attribute[ATTR_INTELLIGENCE], k->magicka);
    character_rest_full(c);
    return 1;
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
