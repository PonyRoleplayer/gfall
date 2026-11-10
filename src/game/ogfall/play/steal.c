#include "game/ogfall/play/steal.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/law.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/ogfall/tables/stealdata.h"

/* See steal.h. */

/* rand() % 101 on the session's stream. */
static int roll101(void)
{
    return og_random_range(0, 32767) % 101;
}

int og_steal_pocket(int creature, int victim_level, OgSteal *out)
{
    static char line[48];
    int chance;

    mem_zero(out, sizeof(*out));
    chance = og_character()->skill[OG_SKILL_PICKPOCKET];
    og_skill_use(OG_SKILL_PICKPOCKET);
    if (creature) {
        chance += (og_character()->level - victim_level) * 5;
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
        og_law_note(OG_CRIME_PICKPOCKETING);
        og_guards_summon(1);
        return 0;
    }
    if (roll101() < 33) {
        out->text = 8999;
        out->ok = 1;
        return 1;
    }
    out->gold = og_random_range(0, 32767) % 5 + 1;
    og_character()->gold += out->gold;
    log_format(line, (int)sizeof(line), g_steal_gold, out->gold);
    out->line = line;
    out->ok = 1;
    return 1;
}

int og_steal_shop(int total, int weight, OgSteal *out)
{
    int chance;

    mem_zero(out, sizeof(*out));
    chance = og_character()->skill[OG_SKILL_PICKPOCKET] - total / 32 - weight / 4;
    if (roll101() > chance) {
        og_skill_use(OG_SKILL_STEALTH);
        og_law_note(OG_CRIME_THEFT);
        og_guards_summon(1);
        out->line = g_steal_shop_failed;
        return 0;
    }
    out->line = g_steal_shop_done;
    out->ok = 1;
    return 1;
}

int og_steal_shelf(void)
{
    if (og_random_range(0, 255) >= 108 - og_character()->skill[OG_SKILL_PICKPOCKET]) {
        og_law_note(OG_CRIME_THEFT);
        og_guards_summon(1);
        return 1;
    }
    return 0;
}
