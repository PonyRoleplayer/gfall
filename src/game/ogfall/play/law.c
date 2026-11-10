#include "game/ogfall/play/law.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/bank.h"
#include "game/ogfall/tables/lawdata.h"
#include "engine/core/mem.h"

/* See law.h. */

int og_law_valid(const OgLaw *law)
{
    int i;

    for (i = 0; i < OG_REGIONS; i++) {
        if (law->punishment[i] > 3) {
            return 0;
        }
    }
    return law->crime >= 0 && law->crime <= OG_CRIMES;
}

static int g_serving;

void og_law_serving(int on)
{
    g_serving = on != 0;
}

int og_law_is_serving(void)
{
    return g_serving;
}

int og_crime_loss(int crime)
{
    return crime >= 1 && crime <= OG_CRIMES ? g_crime_loss[crime - 1] : 0;
}

const char *og_crime_name(int crime)
{
    return crime >= 1 && crime <= 14 ? g_crime_name[crime - 1] : "";
}

void og_law_note(int crime)
{
    og_law()->crime = crime;
}

int og_gold_can_afford(int amount)
{
    return og_gold_total() >= amount;
}

void og_gold_spend(int amount)
{
    OgCharacter *c;
    df_u32 ids[OG_OBJECTS];
    int n, i, left;

    c = og_character();
    if (c->gold >= amount) {
        c->gold -= amount;
        return;
    }
    n = og_inventory_rows(og_state()->inventory_id, -1, ids, OG_OBJECTS);
    if (n > OG_OBJECTS) {
        n = OG_OBJECTS;
    }
    /* A letter that covers it all is made less by it. */
    for (i = 0; i < n; i++) {
        OgObject *it;

        it = og_object_edit(ids[i]);
        if (it && it->group == 27 && it->index == 2 && it->value >= amount) {
            it->value -= amount;
            return;
        }
    }
    /* Else the letters in order: one worth no more than what is left is used up, a bigger one is lessened and ends it. */
    left = amount;
    for (i = 0; i < n && left; i++) {
        OgObject *it;

        it = og_object_edit(ids[i]);
        if (!it || it->group != 27 || it->index != 2) {
            continue;
        }
        if (it->value <= left) {
            left -= it->value;
            og_object_delete(ids[i]);
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
    df_u32 ids[OG_OBJECTS];
    int n, i;

    og_character()->gold = 0;
    n = og_inventory_rows(og_state()->inventory_id, -1, ids, OG_OBJECTS);
    for (i = 0; i < n && i < OG_OBJECTS; i++) {
        const OgObject *it;

        it = og_object(ids[i]);
        if (it && it->group == 27 && it->index == 2) {
            og_object_delete(ids[i]);
        }
    }
}

void og_crime_penalty(int region, int crime)
{
    int loss, f;

    loss = og_crime_loss(crime);
    og_region_add_legal(region, -(int)(short)loss);
    f = og_faction_of_type(region, 15);
    if (f >= 0) {
        og_faction_change_reputation(og_faction_def(f)->id, -(loss >> 1));
    }
}

void og_court_restore_reputation(int region, int change)
{
    int f;

    og_region_add_legal(region, change - 1);
    f = og_faction_of_type(region, 15);
    if (f >= 0) {
        og_faction_change_reputation(og_faction_def(f)->id, (-(change - 1)) / 2);
    }
}

void og_court_restore_vitals(void)
{
    OgCharacter *c;

    c = og_character();
    c->health = c->max_health;
    c->magicka = c->max_magicka;
    c->fatigue = (c->attribute[OG_STR] + c->attribute[OG_END]) << 6;
}

static int rand_range(int low, int high)
{
    return og_random_range(low, high);
}

int og_court_open(OgCourt *court, int crime, int region, const OgCourtGuilds *guilds, OgCourtStep *step)
{
    int legal, penalty, half, fine, steps, i, gold;

    mem_zero(step, sizeof(*step));
    if (crime < 1 || crime > OG_CRIMES) {
        return 0;
    }
    crime--;
    legal = og_region_legal(region);
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
        if (og_random_range(0, 32767) & 1) {
            fine += 40;
        } else {
            court->days += 3;
        }
    }
    gold = og_gold_total();
    if (gold < fine) {
        court->days += (fine - gold) / 40;
        fine -= fine - gold;
    }
    court->fine = fine;
    /* The Dark Brotherhood clears murder and assault, the Thieves Guild the crimes of the hand; rank against a roll of 0..19. */
    if ((crime == 4 || crime == 3) && guilds && guilds->brotherhood_rank >= 0 && guilds->brotherhood_rank >= rand_range(0, 19)) {
        step->text[0] = OG_TEXT_GUILD_BROTHERHOOD;
    } else if ((crime <= 2 || crime == 11) && guilds && guilds->thieves_rank >= 0 && guilds->thieves_rank >= rand_range(0, 19)) {
        step->text[0] = OG_TEXT_GUILD_THIEVES;
    }
    if (step->text[0]) {
        og_court_restore_vitals();
        og_court_restore_reputation(region, court->change);
        court->open = 0;
        step->done = 1;
        return 1;
    }
    step->text[0] = OG_TEXT_COURT_ASK;
    return 1;
}

/* A banishment or the death sentence: the flag on the region and where the player is put. */
static void punish(OgCourt *court, int severity, OgCourtStep *step)
{
    if (severity == 0) {
        step->text[0] = OG_TEXT_COURT_BANISHED;
        og_law()->punishment[court->region] |= 1;
        step->flag = 1;
        step->release = OG_RELEASE_STREET;
    } else {
        step->text[0] = OG_TEXT_COURT_DEATH;
        og_law()->punishment[court->region] |= 2;
        step->flag = 2;
        step->release = OG_RELEASE_BANISHED;
    }
    step->done = 1;
    court->open = 0;
}

/* State 3: set free in the street, the days served, health restored and the sentence's reputation given back. */
static void serve(OgCourt *court, OgCourtStep *step)
{
    step->release = OG_RELEASE_STREET;
    step->serve_days = court->days;
    og_court_restore_vitals();
    og_court_restore_reputation(court->region, court->change);
    step->done = 1;
    court->open = 0;
}

void og_court_accept(OgCourt *court, OgCourtStep *step)
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
    gold = og_gold_total();
    if (gold >= court->fine) {
        og_gold_spend(court->fine);
        step->gold_taken = court->fine;
        og_court_restore_reputation(court->region, court->change);
        if (court->days) {
            serve(court, step);
        } else {
            step->release = OG_RELEASE_STREET;
            step->done = 1;
            court->open = 0;
        }
        return;
    }
    /* Not enough: every coin and letter, a day for each forty short (and one more), the sentence's reputation, then the days. */
    court->days += (court->fine - gold) / 40 + 1;
    step->gold_taken = gold;
    gold_remove_all();
    og_court_restore_reputation(court->region, court->change);
    step->text[0] = OG_TEXT_COURT_SHORT;
    step->text[1] = OG_TEXT_COURT_SENTENCE;
    serve(court, step);
}

void og_court_plead(OgCourt *court, int streetwise, int skill, int personality, OgCourtStep *step)
{
    int chance, legal, roll;

    (void)streetwise;
    mem_zero(step, sizeof(*step));
    if (!court->open) {
        return;
    }
    legal = og_region_legal(court->region);
    chance = (personality + skill) / 2 + legal;
    if (chance < 5) {
        chance = 5;
    } else if (chance > 95) {
        chance = 95;
    }
    if (rand_range(1, 100) <= chance) {
        /* Not guilty: the court says so and sets them free where it found them. */
        step->text[0] = OG_TEXT_COURT_ACQUITTED;
        step->release = OG_RELEASE_ACQUITTED;
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
    step->text[0] = OG_TEXT_COURT_SENTENCE;
    serve(court, step);
}

/* ---- Guards ---- */

static OgGuardWorld g_world;
static float g_guard_timer = -1.0f;
static int g_asked;

void og_guards_set_world(const OgGuardWorld *world)
{
    if (world) {
        g_world = *world;
    } else {
        mem_zero(&g_world, sizeof(g_world));
    }
}

int og_guards_witnesses(void)
{
    return g_world.witnesses ? g_world.witnesses() : 0;
}

int og_guards_present(void)
{
    return g_world.present && g_world.present();
}

float og_guards_timer(void)
{
    return g_guard_timer;
}

void og_guards_reset(void)
{
    g_guard_timer = -1.0f;
    g_asked = 0;
}

int og_guards_summon(int now)
{
    int made, n, bits;

    if ((g_world.in_dungeon && g_world.in_dungeon()) || (g_world.count && g_world.count() > OG_GUARDS_CREATURES_MOST)) {
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
        return og_guards_summon(1);
    }
    if (bits & 1) {
        g_guard_timer = (float)rand_range(5, 10);
    }
    return 0;
}

void og_guards_tick(float seconds)
{
    if (g_guard_timer < 0.0f) {
        return;
    }
    g_guard_timer -= seconds;
    if (g_guard_timer <= 0.0f) {
        g_guard_timer = -1.0f;
        og_guards_summon(1);
    }
}

int og_law_blow(int region)
{
    if (g_asked || !og_guards_present()) {
        return OG_BLOW_FREE;
    }
    if (og_law()->crime >= 1) {
        og_crime_penalty(region, og_law()->crime);
    }
    g_asked = 1;
    return OG_BLOW_ASK;
}

void og_law_calm(void)
{
    if (!og_guards_present()) {
        g_asked = 0;
    }
}

int og_law_guards_or_court(int surrendered, int region, const OgCourtGuilds *guilds, OgCourt *court, OgCourtStep *step)
{
    int standing;

    og_character()->health = 1;
    court->open = 0;
    standing = og_region_legal(region);
    mem_zero(step, sizeof(*step));
    if (standing < -20 && !surrendered) {
        return OG_ARREST_DEATH;
    }
    if (standing >= -20 && standing <= 0 && (og_random_range(0, 32767) & 1) && !surrendered) {
        return OG_ARREST_DEATH;
    }
    og_court_open(court, og_law()->crime, region, guilds, step);
    return OG_ARREST_COURT;
}

void og_guards_dismiss(void)
{
    if (g_world.dismiss) {
        g_world.dismiss();
    }
    og_guards_reset();
}
