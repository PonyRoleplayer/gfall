#include "game/dagger/play/law.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/tables/lawdata.h"
#include "engine/core/mem.h"

/* See law.h. */

int law_valid(const LawState *law)
{
    int i;

    for (i = 0; i < REGIONS; i++) {
        if (law->punishment[i] > 3) {
            return 0;
        }
    }
    return law->crime >= 0 && law->crime <= CRIMES;
}

static int g_serving;

void law_serving(int on)
{
    g_serving = on != 0;
}

int law_is_serving(void)
{
    return g_serving;
}

int crime_loss(int crime)
{
    return crime >= 1 && crime <= CRIMES ? g_crime_loss[crime - 1] : 0;
}

const char *crime_name(int crime)
{
    return crime >= 1 && crime <= 14 ? g_crime_name[crime - 1] : "";
}

void law_note(int crime)
{
    law_state()->crime = crime;
}

int gold_can_afford(int amount)
{
    return gold_total() >= amount;
}

void gold_spend(int amount)
{
    CharacterState *c;
    df_u32 ids[OBJECTS];
    int n, i, left;

    c = character_state();
    if (c->gold >= amount) {
        c->gold -= amount;
        return;
    }
    n = inventory_rows(game_state()->inventory_id, -1, ids, OBJECTS);
    if (n > OBJECTS) {
        n = OBJECTS;
    }
    /* A letter that covers it all is made less by it. */
    for (i = 0; i < n; i++) {
        ObjectInfo *it;

        it = object_edit(ids[i]);
        if (it && it->group == 27 && it->index == 2 && it->value >= amount) {
            it->value -= amount;
            return;
        }
    }
    /* Else the letters in order: one worth no more than what is left is used up, a bigger one is lessened and ends it. */
    left = amount;
    for (i = 0; i < n && left; i++) {
        ObjectInfo *it;

        it = object_edit(ids[i]);
        if (!it || it->group != 27 || it->index != 2) {
            continue;
        }
        if (it->value <= left) {
            left -= it->value;
            object_delete(ids[i]);
        } else {
            it->value -= left;
            left = 0;
        }
    }
    c->gold -= left;
    if (c->gold < 0) {
        c->gold = 0;
    }
}

static void gold_remove_all(void)
{
    df_u32 ids[OBJECTS];
    int n, i;

    character_state()->gold = 0;
    n = inventory_rows(game_state()->inventory_id, -1, ids, OBJECTS);
    for (i = 0; i < n && i < OBJECTS; i++) {
        const ObjectInfo *it;

        it = object_at(ids[i]);
        if (it && it->group == 27 && it->index == 2) {
            object_delete(ids[i]);
        }
    }
}

void crime_penalty(int region, int crime)
{
    int loss, f;

    loss = crime_loss(crime);
    region_add_legal(region, -(int)(short)loss);
    f = faction_of_type(region, 15);
    if (f >= 0) {
        faction_change_reputation(faction_def(f)->id, -(loss >> 1));
    }
}

void court_restore_reputation(int region, int change)
{
    int f;

    region_add_legal(region, change - 1);
    f = faction_of_type(region, 15);
    if (f >= 0) {
        faction_change_reputation(faction_def(f)->id, (-(change - 1)) / 2);
    }
}

void court_restore_vitals(void)
{
    CharacterState *c;

    c = character_state();
    c->health = c->max_health;
    c->magicka = c->max_magicka;
    c->fatigue = (c->attribute[ATTR_STR] + c->attribute[ATTR_END]) << 6;
}

static int rand_range(int low, int high)
{
    return random_range(low, high);
}

int court_open(Court *court, int crime, int region, const CourtGuilds *guilds, CourtStep *step)
{
    int legal, penalty, half, fine, steps, i, gold;

    mem_zero(step, sizeof(*step));
    if (crime < 1 || crime > CRIMES) {
        return 0;
    }
    crime--;
    legal = region_legal(region);
    court->open = 1;
    court->crime = crime;
    court->region = region;
    court->change = g_crime_loss[crime] >> 1;
    penalty = half = 0;
    if (legal < 0) {
        penalty = -legal > 75 ? 75 : -legal;
        half = (-legal) / 2 > 75 ? 75 : (-legal) / 2;
    }
    /* The harsher the standing, the likelier a banishment; else the usual fine and days. */
    if (rand_range(1, 100) <= half) {
        court->severity = 0;
    } else if (rand_range(1, 100) <= penalty) {
        court->severity = 0;
    } else {
        court->severity = 2;
    }
    fine = legal < 0 ? g_crime_fine[crime][1] - legal * g_crime_fine[crime][0] : g_crime_fine[crime][1] + legal * g_crime_fine[crime][0];
    if (g_crime_fine[crime][2] > fine) {
        fine = g_crime_fine[crime][2];
    } else if (g_crime_fine[crime][3] < fine) {
        fine = g_crime_fine[crime][3];
    }
    /* Forty gold or three days for each forty of the fine, by a coin. */
    steps = fine / 40;
    court->days = 0;
    for (fine = i = 0; i < steps; i++) {
        if (random_range(0, 32767) & 1) {
            fine += 40;
        } else {
            court->days += 3;
        }
    }
    gold = gold_total();
    if (gold < fine) {
        court->days += (fine - gold) / 40;
        fine -= fine - gold;
    }
    court->fine = fine;
    /* The Dark Brotherhood clears murder and assault, the Thieves Guild the crimes of the hand; rank against a roll of 0..19. */
    if ((crime == 4 || crime == 3) && guilds && guilds->brotherhood_rank >= 0 && guilds->brotherhood_rank >= rand_range(0, 19)) {
        step->text[0] = TEXT_GUILD_BROTHERHOOD;
    } else if ((crime <= 2 || crime == 11) && guilds && guilds->thieves_rank >= 0 && guilds->thieves_rank >= rand_range(0, 19)) {
        step->text[0] = TEXT_GUILD_THIEVES;
    }
    if (step->text[0]) {
        court_restore_vitals();
        court_restore_reputation(region, court->change);
        court->open = 0;
        step->done = 1;
        return 1;
    }
    step->text[0] = TEXT_COURT_ASK;
    return 1;
}

/* A banishment or the death sentence: the flag on the region and where the player is put. */
static void punish(Court *court, int severity, CourtStep *step)
{
    if (severity == 0) {
        step->text[0] = TEXT_COURT_BANISHED;
        law_state()->punishment[court->region] |= 1;
        step->flag = 1;
        step->release = RELEASE_STREET;
    } else {
        step->text[0] = TEXT_COURT_DEATH;
        law_state()->punishment[court->region] |= 2;
        step->flag = 2;
        step->release = RELEASE_BANISHED;
    }
    step->done = 1;
    court->open = 0;
}

/* State 3: set free in the street, the days served, health restored and the sentence's reputation given back. */
static void serve(Court *court, CourtStep *step)
{
    step->release = RELEASE_STREET;
    step->serve_days = court->days;
    court_restore_vitals();
    court_restore_reputation(court->region, court->change);
    step->done = 1;
    court->open = 0;
}

void court_accept(Court *court, CourtStep *step)
{
    int gold;

    mem_zero(step, sizeof(*step));
    if (!court->open) {
        return;
    }
    if (court->severity != 2) {
        punish(court, court->severity, step);
        return;
    }
    /* Guilty and accepting: half the fine and half the days. */
    court->fine >>= 1;
    court->days >>= 1;
    gold = gold_total();
    if (gold >= court->fine) {
        gold_spend(court->fine);
        step->gold_taken = court->fine;
        court_restore_reputation(court->region, court->change);
        if (court->days) {
            serve(court, step);
        } else {
            step->release = RELEASE_STREET;
            step->done = 1;
            court->open = 0;
        }
        return;
    }
    /* Not enough: every coin and letter, a day for each forty short (and one more), the sentence's reputation, then the days. */
    court->days += (court->fine - gold) / 40 + 1;
    step->gold_taken = gold;
    gold_remove_all();
    court_restore_reputation(court->region, court->change);
    step->text[0] = TEXT_COURT_SHORT;
    step->text[1] = TEXT_COURT_SENTENCE;
    serve(court, step);
}

void court_plead(Court *court, int streetwise, int skill, int personality, CourtStep *step)
{
    int chance, legal, roll;

    (void)streetwise;
    mem_zero(step, sizeof(*step));
    if (!court->open) {
        return;
    }
    legal = region_legal(court->region);
    chance = (personality + skill) / 2 + legal;
    if (chance < 5) {
        chance = 5;
    } else if (chance > 95) {
        chance = 95;
    }
    if (rand_range(1, 100) <= chance) {
        /* Not guilty: the court says so and sets them free where it found them. */
        step->text[0] = TEXT_COURT_ACQUITTED;
        step->release = RELEASE_ACQUITTED;
        step->done = 1;
        court->open = 0;
        return;
    }
    if (court->severity != 2) {
        punish(court, court->severity, step);
        return;
    }
    roll = rand_range(1, 100) + legal;
    if (roll > 75) {
        court->fine >>= 1;
    } else if (roll < 25) {
        court->fine <<= 1;
    }
    /* The fine is changed and not charged: the days are what is served (as the original does). */
    step->text[0] = TEXT_COURT_SENTENCE;
    serve(court, step);
}

/* ---- Guards ---- */

static GuardWorld g_world;
static float g_guard_timer = -1.0f;
static int g_asked;

void guards_set_world(const GuardWorld *world)
{
    if (world) {
        g_world = *world;
    } else {
        mem_zero(&g_world, sizeof(g_world));
    }
}

int guards_witnesses(void)
{
    return g_world.witnesses ? g_world.witnesses() : 0;
}

int guards_present(void)
{
    return g_world.present && g_world.present();
}

float guards_timer(void)
{
    return g_guard_timer;
}

void guards_reset(void)
{
    g_guard_timer = -1.0f;
    g_asked = 0;
}

int guards_summon(int now)
{
    int made, n, bits;

    if ((g_world.in_dungeon && g_world.in_dungeon()) || (g_world.count && g_world.count() > GUARDS_CREATURES_MOST)) {
        return 0;
    }
    made = 0;
    if (now) {
        /* The street's guards come, and some of the people behind the player turn out to be guards; if none did, two to five are made near. */
        made = g_world.convert ? g_world.convert(0) : 0;
        if (made) {
            return made;
        }
        n = rand_range(2, 5);
        while (n-- > 0) {
            if (g_world.spawn && g_world.spawn()) {
                made++;
            }
        }
        return made;
    }
    bits = g_world.witnesses ? g_world.witnesses() : 0;
    if (bits & 2) {
        if (g_world.convert) {
            return g_world.convert(1);
        }
        return guards_summon(1);
    }
    if (bits & 1) {
        g_guard_timer = (float)rand_range(5, 10);
    }
    return 0;
}

void guards_tick(float seconds)
{
    if (g_guard_timer < 0.0f) {
        return;
    }
    g_guard_timer -= seconds;
    if (g_guard_timer <= 0.0f) {
        g_guard_timer = -1.0f;
        guards_summon(1);
    }
}

int law_blow(int region)
{
    if (g_asked || !guards_present()) {
        return BLOW_FREE;
    }
    if (law_state()->crime >= 1) {
        crime_penalty(region, law_state()->crime);
    }
    g_asked = 1;
    return BLOW_ASK;
}

void law_calm(void)
{
    if (!guards_present()) {
        g_asked = 0;
    }
}

int law_guards_or_court(int surrendered, int region, const CourtGuilds *guilds, Court *court, CourtStep *step)
{
    int standing;

    character_state()->health = 1;
    court->open = 0;
    standing = region_legal(region);
    mem_zero(step, sizeof(*step));
    if (standing < -20 && !surrendered) {
        return ARREST_DEATH;
    }
    if (standing >= -20 && standing <= 0 && (random_range(0, 32767) & 1) && !surrendered) {
        return ARREST_DEATH;
    }
    court_open(court, law_state()->crime, region, guilds, step);
    return ARREST_COURT;
}

void guards_dismiss(void)
{
    if (g_world.dismiss) {
        g_world.dismiss();
    }
    guards_reset();
}
