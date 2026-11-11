#include <string.h>
#include "check_ogentry.h"
#include "check_util.h"
#include "game/ogfall/play/entry.h"
#include "game/ogfall/play/guild.h"
#include "game/ogfall/play/state.h"

void check_ogentry(void)
{
    OgEntryBuilding b;
    OgEntryPlayer p;
    OgEntry e;
    int i, picked, crimes, failed, tried_blocked;
    df_u32 noon, night;

    noon = 12u * 60u;
    night = 3u * 60u;
    og_entry_reset();
    og_guilds_clear();
    memset(&b, 0, sizeof(b));
    memset(&p, 0, sizeof(p));
    p.brotherhood = p.thieves = p.fighters = p.mages = -1;
    b.id = 500u; b.type = 0; b.name_seed = 7;
    /* An alchemist is open by day and shut at night; the shut door says how hard its lock is (seed 7: level 10). */
    check_expect(og_building_try_enter(&b, &p, noon, 0, &e) && e.allow, "entry: an alchemist was shut at noon");
    check_expect(!og_building_try_enter(&b, &p, night, 0, &e) && !e.allow && e.line != 0, "entry: an alchemist was open at three, or said nothing of its lock");
    p.house = 500u;
    check_expect(og_building_try_enter(&b, &p, night, 0, &e) && e.allow, "entry: the player's own house was shut");
    p.house = 0;
    /* A quest's building (op 81) lets the player in at night, a house too, by the quest's mark; a type under 17 is still by its hours first. */
    check_expect(!og_building_try_enter(&b, &p, night, 0, &e), "entry: the alchemist is open at night without a quest");
    b.quest_open = 1;
    check_expect(og_building_try_enter(&b, &p, night, 0, &e) && e.allow, "entry: a building a quest marked open was shut to the quest");
    b.quest_open = 0;
    b.type = 24;
    check_expect(og_building_try_enter(&b, &p, night, 0, &e), "entry: the city wall was shut");
    /* A guild's own door at any hour; the Brotherhood's hall is shut to a stranger. */
    b.type = 11; b.faction = 108;
    check_expect(!og_building_try_enter(&b, &p, night, 0, &e), "entry: the Brotherhood's hall was open to a stranger");
    p.brotherhood = 0;
    check_expect(og_building_try_enter(&b, &p, night, 0, &e) && e.allow, "entry: a Brother was kept out");
    p.brotherhood = -1;
    /* An open-lock spell is spent. */
    p.lock_spell = 1;
    check_expect(og_building_try_enter(&b, &p, night, 0, &e) && e.spent_spell, "entry: a spell that opens locks did not open the door");
    p.lock_spell = 0;
    /* A house (type 17..20) is open by day, half of them, with record 256 said. */
    b.type = 18; b.faction = 0; b.id = 10u;
    p.daytime = 1;
    check_expect(og_building_try_enter(&b, &p, night, 0, &e) && e.text == 256, "entry: a house of low number was not open by day");
    b.id = 60u;
    check_expect(!og_building_try_enter(&b, &p, night, 0, &e), "entry: a house of high number was open by day");
    /* Steal mode: the lock is picked on a roll, the Thieves count a pick, a crime follows when seen or by Stealth, a failed try is not repeated at the same skill. */
    p.daytime = 0; p.steal = 1;
    b.type = 18; b.name_seed = 0;
    og_character()->skill[13] = 80;
    og_character()->skill[16] = 0;
    picked = crimes = failed = tried_blocked = 0;
    og_guilds_clear();
    for (i = 0; i < 200; i++) {
        og_entry_reset();
        b.id = 77u;
        if (og_building_try_enter(&b, &p, noon, 0, &e)) {
            picked += e.picked;
            crimes += e.crime;
        } else {
            failed++;
            /* The same skill may not try again. */
            if (!og_building_try_enter(&b, &p, noon, 0, &e) && e.line == 0) { tried_blocked++; }
        }
    }
    /* Skill 80 against a level 3 lock is a chance of 65, and a third of the picks (Stealth 0: a roll of 1..300 under 100) are noticed. */
    check_expect(picked > 100 && picked < 160 && crimes > 20 && crimes < 75 && og_guilds()->thieves_count > 0, "entry: picks, crimes or the Thieves' count are not in the odds the original has");
    check_expect(failed == tried_blocked, "entry: a failed try was repeated at the same skill");
    /* A witness is always a crime. */
    og_entry_reset();
    og_character()->skill[16] = 100;
    b.id = 78u;
    check_expect(og_building_try_enter(&b, &p, noon, 1, &e) ? e.crime == 1 : 1, "entry: a witnessed pick was not a crime");
}
