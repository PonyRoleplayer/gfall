#include "game/dagger/play/steal.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/guild.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/dagger/tables/stealdata.h"

/* See steal.h. */

/* rand() % 101 on the session's stream. */
static int roll101(void)
{
    return random_range(0, 32767) % 101;
}

int steal_pocket(int creature, int victim_level, StealState *out)
{
    static char line[48];
    int chance;

    mem_zero(out, sizeof(*out));
    chance = character_state()->skill[SKILL_PICKPOCKET];
    skill_use(SKILL_PICKPOCKET);
    if (creature) {
        chance += (character_state()->level - victim_level) * 5;
    }
    if (chance < 5) {
        chance = 5;
    } else if (chance > 95) {
        chance = 95;
    }
    if (roll101() > chance) {
        out->line = g_steal_failed;
        out->wake = creature;
        out->spoiled = !creature;
        law_note(CRIME_PICKPOCKETING);
        guards_summon(1);
        return 0;
    }
    if (roll101() < 33) {
        out->text = 8999;
        out->ok = 1;
        return 1;
    }
    out->gold = random_range(0, 32767) % 5 + 1;
    character_state()->gold += out->gold;
    log_format(line, (int)sizeof(line), g_steal_gold, out->gold);
    out->line = line;
    out->ok = 1;
    /* A pocket that paid is counted by the Thieves Guild (guild_count_crime(5, 1)); an empty one is not. */
    guild_count_crime(5, 1, game_state()->minutes);
    return 1;
}

int steal_shop(int total, int weight, StealState *out)
{
    int chance;

    mem_zero(out, sizeof(*out));
    chance = character_state()->skill[SKILL_PICKPOCKET] - total / 32 - weight / 4;
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
    if (random_range(0, 255) >= 108 - character_state()->skill[SKILL_PICKPOCKET]) {
        law_note(CRIME_THEFT);
        guards_summon(1);
        return 1;
    }
    return 0;
}
