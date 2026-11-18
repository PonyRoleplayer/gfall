#include "game/dagger/play/people/steal.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/people/guild.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/dagger/tables/stealdata.h"

/* See steal.h. */

/* rand() % 101 on the session's stream. */
static int roll101(void)
{
    return random_range(0, RAND_MAX_WATCOM) % (PERCENT + 1);
}

int steal_pocket(int creature, int victim_level, StealState *out)
{
    static char line[48];
    int chance;

    mem_zero(out, sizeof(*out));
    chance = character_state()->skill[SKILL_PICKPOCKET];
    skill_use(SKILL_PICKPOCKET);
    if (creature) {
        chance += (character_state()->level - victim_level) * PICKPOCKET_PER_LEVEL;
    }
    if (chance < CHANCE_MIN) {
        chance = CHANCE_MIN;
    } else if (chance > CHANCE_MAX) {
        chance = CHANCE_MAX;
    }
    if (roll101() > chance) {
        out->line = g_steal_failed;
        out->wake = creature;
        out->spoiled = !creature;
        law_note(CRIME_PICKPOCKETING);
        guards_summon(1);
        return 0;
    }
    if (roll101() < POCKET_EMPTY_CHANCE) {
        out->text = TEXT_POCKET_EMPTY;
        out->ok = 1;
        return 1;
    }
    out->gold = random_range(0, RAND_MAX_WATCOM) % POCKET_GOLD_MAX + 1;
    character_state()->gold += out->gold;
    log_format(line, (int)sizeof(line), g_steal_gold, out->gold);
    out->line = line;
    out->ok = 1;
    /* A pocket that paid is counted by the Thieves Guild (guild_count_crime(5, 1)); an empty one is not. */
    guild_count_crime(GUILD_COUNT_THIEF, 1, game_state()->minutes);
    return 1;
}

int steal_shop(int total, int weight, StealState *out)
{
    int chance;

    mem_zero(out, sizeof(*out));
    chance = character_state()->skill[SKILL_PICKPOCKET] - total / SHOP_STEAL_VALUE_DIV - weight / SHOP_STEAL_WEIGHT_DIV;
    if (roll101() > chance) {
        skill_use(SKILL_STEALTH);
        law_note(CRIME_THEFT);
        guards_summon(1);
        out->line = g_steal_shop_failed;
        return 0;
    }
    out->line = g_steal_shop_done;
    out->ok = 1;
    return 1;
}

int steal_shelf(void)
{
    if (random_range(0, SHELF_STEAL_ROLL) >= SHELF_STEAL_BASE - character_state()->skill[SKILL_PICKPOCKET]) {
        law_note(CRIME_THEFT);
        guards_summon(1);
        return 1;
    }
    return 0;
}
