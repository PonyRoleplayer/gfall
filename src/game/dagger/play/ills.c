#include "game/dagger/play/ills.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/tables/commerce.h"
#include "game/dagger/tables/enemies.h"
#include "engine/core/mem.h"

/* See ills.h and docs/game.md (Afflictions). */

#define MINUTES_A_DAY 1440
/* An infection's dream past this many days, its curse past ILLS_TURN_DAYS. */
#define DREAM_DAYS 0
#define TURN_DAYS 3
/* A vampire's burn: this much health each SUN_EVERY minutes (PassiveSpecialsEffect). */
#define SUN_DAMAGE 12
#define SUN_EVERY 4
/* A lycanthrope's month between kills, the health a minute the limit falls by after it, and its floor. */
#define KILL_PERIOD (MINUTES_A_DAY * 30)
#define KILL_LIMIT_FLOOR 4
/* A temple's price a disease, before quality and the region's prices. */
#define CURE_PRICE 250

static Ills g_ills;

#define F(name, type, member) REC_FIELD(name, type, Ills, member)
#define FA(name, type, member) REC_ARRAY(name, type, Ills, member)
static const RecField g_ills_fields[] = {
    F("id", REC_UINT, id),
    FA("days_left", REC_INT, days_left),
    FA("felt_day", REC_INT, felt_day),
    FA("drained", REC_INT, drained),
    F("infection", REC_INT, infection),
    F("infected_day", REC_INT, infected_day),
    F("dreamt", REC_INT, dreamt),
    F("infected_region", REC_INT, infected_region),
    F("curse", REC_INT, curse),
    F("clan", REC_INT, clan),
    F("beast", REC_INT, beast),
    F("last_kill", REC_INT, last_kill),
    F("last_morph", REC_INT, last_morph),
    F("last_fed", REC_INT, last_fed),
    FA("boost", REC_INT, boost),
    FA("skill_boost", REC_INT, skill_boost)
};
#undef F
#undef FA
const RecSchema ills_schema = {
    "ILLS", "ills", g_ills_fields, (int)(sizeof(g_ills_fields) / sizeof(g_ills_fields[0])), (df_u16)sizeof(Ills), 0
};

Ills *ills(void)
{
    return &g_ills;
}

void ills_new_game(void)
{
    mem_zero(&g_ills, sizeof(g_ills));
}

void ills_put(const Ills *from)
{
    g_ills = *from;
    g_ills.id = 0;
}

int ills_now(int day, float minute)
{
    return day * MINUTES_A_DAY + df_ifloor(minute);
}

/* ---- Catching. ---- */

int ills_disease_save(const Character *c)
{
    const CareerRace *race;
    const CareerClass *klass;
    int save;

    /* A curse is the end of disease (the compound race's immunity). */
    if (g_ills.curse != ILL_NONE) {
        return 100;
    }
    save = 50;
    race = career_race(c->race);
    if (race) {
        if (race->immune & EFFECT_DISEASE) {
            return 100;
        }
        save += (race->resist & EFFECT_DISEASE) ? 30 : 0;
        save -= (race->low_tolerance & EFFECT_DISEASE) ? 25 : 0;
        save -= (race->weakness & EFFECT_DISEASE) ? 50 : 0;
    }
    klass = career_class(c->klass);
    if (klass) {
        save += (klass->immune & EFFECT_DISEASE) ? 50 : 0;
        save -= (klass->weakness & EFFECT_DISEASE) ? 50 : 0;
        save -= (klass->low_tolerance & EFFECT_DISEASE) ? 25 : 0;
        save += (klass->resist & EFFECT_DISEASE) ? 25 : 0;
    }
    /* 100 or more is immunity, before the Willpower and the clamp (DFU's own fix for classic). */
    if (save >= 100) {
        return 100;
    }
    save += rules_magic_resist(c->attribute[ATTR_WILLPOWER]);
    return save < 5 ? 5 : (save > 95 ? 95 : save);
}

int ills_saved(int save, int roll)
{
    /* DFU prorates a roll within 20 under the save: anything above 0% still passes a disease on. */
    return save >= 100 || (roll <= save && roll < save - 20);
}

int ills_catch(const Character *c, int disease, int day)
{
    const Disease *d;

    (void)c;
    d = disease_kind(disease);
    if (!d || g_ills.curse != ILL_NONE || g_ills.days_left[disease] != 0) {
        return 0;
    }
    if (d->days_min == DISEASE_FOREVER) {
        g_ills.days_left[disease] = ILLS_FOREVER;
    } else {
        g_ills.days_left[disease] = (df_i16)(d->days_min + combat_roll(d->days_max - d->days_min + 1));
    }
    g_ills.felt_day[disease] = day;
    return 1;
}

int ills_infect(int kind, int day, int region)
{
    if (g_ills.curse != ILL_NONE || g_ills.infection != ILL_NONE) {
        return 0;
    }
    g_ills.infection = kind;
    g_ills.infected_day = day;
    g_ills.infected_region = region;
    g_ills.dreamt = 0;
    return 1;
}

/* One of list's n diseases, after the level and the saving throw. */
static int inflict(const Character *c, const int *list, int n, int day)
{
    int which;

    /* Classic gives no disease at level 1. */
    if (c->level <= 1 || ills_saved(ills_disease_save(c), 1 + combat_roll(100))) {
        return ILLS_STRUCK_NOTHING;
    }
    which = list[combat_roll(n)];
    return ills_catch(c, which, day) ? which : ILLS_STRUCK_NOTHING;
}

/* A special infection's 0.6% (FormulaHelper.specialInfectionChance), as a roll in thousandths. */
static int special_infection(void)
{
    return combat_roll(1000) < 6;
}

int ills_struck(const Character *c, int kind, int day, int region)
{
    static const int list_a[] = { DISEASE_PLAGUE };
    static const int list_b[] = { DISEASE_PLAGUE, DISEASE_STOMACH_ROT, DISEASE_BRAIN_FEVER };
    static const int list_c[] = {
        DISEASE_PLAGUE, DISEASE_YELLOW_FEVER, DISEASE_STOMACH_ROT, DISEASE_CONSUMPTION, DISEASE_BRAIN_FEVER,
        DISEASE_SWAMP_ROT, DISEASE_CHOLERA, DISEASE_LEPROSY, DISEASE_RED_DEATH, DISEASE_TYPHOID_FEVER, DISEASE_DEMENTIA
    };

    switch (kind) {
    case ENEMY_RAT:
        /* Classic's rat gives plague alone; DFU takes DF Chronicles' three. */
        return combat_roll(100) < 5 ? inflict(c, list_b, 3, day) : ILLS_STRUCK_NOTHING;
    case ENEMY_GIANT_BAT:
        return combat_roll(100) < 2 ? inflict(c, list_b, 3, day) : ILLS_STRUCK_NOTHING;
    case ENEMY_ZOMBIE:
        return combat_roll(100) < 2 ? inflict(c, list_c, 11, day) : ILLS_STRUCK_NOTHING;
    case ENEMY_MUMMY:
        return combat_roll(100) < 5 ? inflict(c, list_c, 11, day) : ILLS_STRUCK_NOTHING;
    case ENEMY_WEREWOLF:
    case ENEMY_WEREBOAR:
        if (special_infection() && ills_infect(kind == ENEMY_WEREWOLF ? ILL_WEREWOLF : ILL_WEREBOAR, day, region)) {
            return ILLS_STRUCK_INFECTION + g_ills.infection;
        }
        return ILLS_STRUCK_NOTHING;
    case ENEMY_VAMPIRE:
    case ENEMY_VAMPIRE_ANCIENT:
    {
        int roll;

        /* One roll of 0..100 for both, as DFU: under 0.6 the curse, under 2 plague. */
        roll = combat_roll(1000);
        if (roll < 6) {
            return ills_infect(ILL_VAMPIRE, day, region) ? ILLS_STRUCK_INFECTION + ILL_VAMPIRE : ILLS_STRUCK_NOTHING;
        }
        return roll < 20 ? inflict(c, list_a, 1, day) : ILLS_STRUCK_NOTHING;
    }
    default:
        return ILLS_STRUCK_NOTHING;
    }
}

/* ---- What the sheet is given and given back. ---- */

/* Adds amount to attribute a, held to 1..100: what was really added. */
static int add_attribute(Character *c, int a, int amount)
{
    int was;
    int now;

    was = c->attribute[a];
    now = was + amount;
    now = now < 1 ? 1 : (now > 100 ? 100 : now);
    c->attribute[a] = now;
    return now - was;
}

static int add_skill(Character *c, int s, int amount)
{
    int was;
    int now;

    was = c->skill[s];
    now = was + amount;
    now = now < 0 ? 0 : (now > 100 ? 100 : now);
    c->skill[s] = now;
    return now - was;
}

/* What disease i took from the attributes, given back. */
static void give_back(Character *c, int i)
{
    int a;

    for (a = 0; a < ATTR_COUNT; a++) {
        add_attribute(c, a, -g_ills.drained[i * ATTR_COUNT + a]);
        g_ills.drained[i * ATTR_COUNT + a] = 0;
    }
    g_ills.days_left[i] = 0;
    g_ills.felt_day[i] = 0;
    character_attributes_changed(c);
}

/* One day of disease i's symptoms (IncrementDailyDiseaseEffects). 1 when its health damage killed. */
static int symptoms(Player *p, int i)
{
    const Disease *d;
    Character *c;
    int damage;
    int a;

    d = disease_kind(i);
    if (!d) {
        return 0;
    }
    c = &p->sheet;
    damage = d->damage_min + combat_roll(d->damage_max - d->damage_min + 1);
    for (a = 0; a < ATTR_COUNT; a++) {
        if (d->attribute[a]) {
            g_ills.drained[i * ATTR_COUNT + a] = (signed char)(g_ills.drained[i * ATTR_COUNT + a] + add_attribute(c, a, -(int)d->attribute[a] * damage));
        }
    }
    character_attributes_changed(c);
    /* DFU's DecreaseFatigue without its multiplier: the raw 64ths. */
    c->fatigue -= d->fatigue * damage;
    c->fatigue = c->fatigue < 0 ? 0 : c->fatigue;
    c->magicka -= d->magicka * damage;
    c->magicka = c->magicka < 0 ? 0 : c->magicka;
    return player_hurt(p, d->health * damage) > 0;
}

/*
 * The curse's gifts (ApplyLycanthropeAdvantages, ApplyVampireAdvantages):
 * a lycanthrope +40 Strength, Agility, Endurance and Speed; a vampire +20 to
 * all but Intelligence, an Anthotis that too. Skills +30.
 */
static void bless(Character *c)
{
    static const int were_attr[] = { ATTR_STRENGTH, ATTR_AGILITY, ATTR_ENDURANCE, ATTR_SPEED };
    static const int were_skill[] = {
        SKILL_SWIMMING, SKILL_RUNNING, SKILL_STEALTH, SKILL_CRITICAL_STRIKE, SKILL_CLIMBING, SKILL_HAND_TO_HAND, SKILL_JUMPING
    };
    static const int vamp_skill[] = {
        SKILL_JUMPING, SKILL_RUNNING, SKILL_STEALTH, SKILL_CRITICAL_STRIKE, SKILL_CLIMBING, SKILL_HAND_TO_HAND
    };
    int i;

    if (g_ills.curse == ILL_VAMPIRE) {
        for (i = 0; i < ATTR_COUNT; i++) {
            if (i != ATTR_INTELLIGENCE || g_ills.clan == ILLS_CLAN_ANTHOTIS) {
                g_ills.boost[i] = (signed char)(g_ills.boost[i] + add_attribute(c, i, 20));
            }
        }
        for (i = 0; i < (int)(sizeof(vamp_skill) / sizeof(vamp_skill[0])); i++) {
            g_ills.skill_boost[vamp_skill[i]] = (signed char)(g_ills.skill_boost[vamp_skill[i]] + add_skill(c, vamp_skill[i], 30));
        }
    } else {
        for (i = 0; i < (int)(sizeof(were_attr) / sizeof(were_attr[0])); i++) {
            g_ills.boost[were_attr[i]] = (signed char)(g_ills.boost[were_attr[i]] + add_attribute(c, were_attr[i], 40));
        }
        for (i = 0; i < (int)(sizeof(were_skill) / sizeof(were_skill[0])); i++) {
            g_ills.skill_boost[were_skill[i]] = (signed char)(g_ills.skill_boost[were_skill[i]] + add_skill(c, were_skill[i], 30));
        }
    }
    character_attributes_changed(c);
}

/* The infection become the curse, on day: every disease gone, the gifts given, sated as it starts. */
static void turn(Player *p, int day)
{
    const Faction *province;
    int now;
    int i;

    for (i = 0; i < DISEASE_COUNT; i++) {
        if (g_ills.days_left[i]) {
            give_back(&p->sheet, i);
        }
    }
    g_ills.curse = g_ills.infection;
    g_ills.infection = ILL_NONE;
    g_ills.dreamt = 0;
    g_ills.beast = 0;
    /* The clan of the region it was caught in; the Lyrezi when it names none (FormulaHelper.GetVampireClan). */
    province = faction_province_of(g_ills.infected_region);
    g_ills.clan = province && province->vampire ? province->vampire : ILLS_CLAN_LYREZI;
    now = day * MINUTES_A_DAY;
    g_ills.last_kill = now;
    g_ills.last_fed = now;
    /* The first change of shape may come at once. */
    g_ills.last_morph = now - MINUTES_A_DAY - 1;
    bless(&p->sheet);
}

/* ---- Time. ---- */

int ills_day(Player *p, int day)
{
    int said;
    int i;

    said = 0;
    for (i = 0; i < DISEASE_COUNT; i++) {
        int past;

        if (!g_ills.days_left[i] || day <= g_ills.felt_day[i]) {
            continue;
        }
        /* The day it was caught it incubates; each day since it was last felt, its symptoms. */
        past = day - g_ills.felt_day[i];
        g_ills.felt_day[i] = day;
        said |= ILLS_FELT_BAD;
        while (past-- > 0) {
            if (symptoms(p, i)) {
                return said | ILLS_DIED;
            }
            if (g_ills.days_left[i] != ILLS_FOREVER && --g_ills.days_left[i] <= 0) {
                give_back(&p->sheet, i);
                said |= ILLS_PASSED;
                break;
            }
        }
    }
    if (g_ills.infection != ILL_NONE) {
        int past;

        past = day - g_ills.infected_day;
        if (past > DREAM_DAYS && !g_ills.dreamt) {
            g_ills.dreamt = 1;
            said |= ILLS_DREAM;
        } else if (past > TURN_DAYS && g_ills.dreamt) {
            turn(p, day);
            said |= ILLS_TURNED;
        }
    }
    return said;
}

int ills_minute(Player *p, int now, int in_sun, int in_holy, int full_moon)
{
    int said;

    said = 0;
    if (g_ills.curse == ILL_VAMPIRE && (now % SUN_EVERY) == 0 && (in_sun || in_holy)) {
        said |= ILLS_BURNT;
        if (player_hurt(p, SUN_DAMAGE) > 0) {
            return said | ILLS_DIED;
        }
    }
    if (g_ills.curse == ILL_WEREWOLF || g_ills.curse == ILL_WEREBOAR) {
        if (full_moon && !g_ills.beast && ills_morph(p, now, 1)) {
            said |= ILLS_FORCED;
        }
        if (now - g_ills.last_kill > KILL_PERIOD) {
            int limit;

            /* The most health held under a limit that falls a point an hour of want (24 a day), to 4. */
            limit = p->sheet.health_max - (now - g_ills.last_kill - KILL_PERIOD) / 60;
            limit = limit < KILL_LIMIT_FLOOR ? KILL_LIMIT_FLOOR : limit;
            if (p->sheet.health > limit) {
                p->sheet.health = limit;
            }
            said |= ILLS_HUNGER;
        }
    }
    return said;
}

int ills_morph(Player *p, int now, int forced)
{
    if (g_ills.curse != ILL_WEREWOLF && g_ills.curse != ILL_WEREBOAR) {
        return 0;
    }
    if (!g_ills.beast) {
        /* Once a day into the beast, unless the moon forces it. */
        if (!forced && now - g_ills.last_morph <= MINUTES_A_DAY) {
            return 0;
        }
        inventory_take_off(&p->pack, EQUIP_RIGHT_HAND);
        inventory_take_off(&p->pack, EQUIP_LEFT_HAND);
        g_ills.beast = 1;
    } else {
        g_ills.beast = 0;
    }
    /* Classic heals the player whole each change. */
    p->sheet.health = p->sheet.health_max;
    g_ills.last_morph = now;
    return 1;
}

int ills_beast(void)
{
    return g_ills.beast;
}

int ills_curse(void)
{
    return g_ills.curse;
}

void ills_fed(int now)
{
    if (g_ills.curse == ILL_VAMPIRE) {
        g_ills.last_fed = now;
    }
}

void ills_killed_innocent(int now)
{
    if (g_ills.curse == ILL_WEREWOLF || g_ills.curse == ILL_WEREBOAR) {
        g_ills.last_kill = now;
    }
}

int ills_rest_refused(int now)
{
    return g_ills.curse == ILL_VAMPIRE && now - g_ills.last_fed > MINUTES_A_DAY;
}

int ills_travel_refused(int daytime)
{
    return g_ills.curse == ILL_VAMPIRE && daytime;
}

/* ---- Cures. ---- */

int ills_curable(void)
{
    int n;
    int i;

    n = g_ills.infection != ILL_NONE;
    for (i = 0; i < DISEASE_COUNT; i++) {
        n += g_ills.days_left[i] != 0;
    }
    return n;
}

void ills_cure(Character *c)
{
    int i;

    for (i = 0; i < DISEASE_COUNT; i++) {
        if (g_ills.days_left[i]) {
            give_back(c, i);
        }
    }
    g_ills.infection = ILL_NONE;
    g_ills.dreamt = 0;
}

int ills_cure_cost(int count, int quality, int prices, int holiday)
{
    int cost;

    if (count <= 0 || (holiday & HOLIDAY_TEMPLE_CURE)) {
        return 0;
    }
    cost = rules_cost(CURE_PRICE * count, quality, prices);
    return (holiday & HOLIDAY_TEMPLE_HALF) ? cost / 2 : cost;
}

void ills_lift_curse(Player *p)
{
    int i;

    if (g_ills.curse == ILL_NONE) {
        return;
    }
    for (i = 0; i < ATTR_COUNT; i++) {
        add_attribute(&p->sheet, i, -g_ills.boost[i]);
        g_ills.boost[i] = 0;
    }
    for (i = 0; i < SKILL_COUNT; i++) {
        add_skill(&p->sheet, i, -g_ills.skill_boost[i]);
        g_ills.skill_boost[i] = 0;
    }
    character_attributes_changed(&p->sheet);
    g_ills.curse = ILL_NONE;
    g_ills.beast = 0;
    p->sheet.health = p->sheet.health_max;
}

/* ---- The moons and the name. ---- */

int ills_full_moon(int year, int day_of_year)
{
    int base;

    base = day_of_year + year * 360;
    return (base + 3) % 32 == 0 || (base - 1) % 32 == 0;
}

const char *ills_race_name(const Character *c)
{
    const CareerRace *race;

    if (g_ills.curse == ILL_VAMPIRE) {
        return "Vampire";
    }
    if (g_ills.beast) {
        return g_ills.curse == ILL_WEREWOLF ? "Werewolf" : "Wereboar";
    }
    race = career_race(c->race);
    return race ? race->name : "";
}
