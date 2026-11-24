#include <string.h>
#include "check_entry.h"
#include "check_util.h"
#include "game/dagger/play/people/entry.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/state.h"

void check_entry(void)
{
    EntryBuilding b;
    EntryPlayer p;
    Entry e;
    int i, picked, crimes, failed, tried_blocked;
    df_u32 noon, night;

    noon = 12u * 60u;
    night = 3u * 60u;
    entry_reset();
    guilds_clear();
    memset(&b, 0, sizeof(b));
    memset(&p, 0, sizeof(p));
    p.brotherhood = p.thieves = p.fighters = p.mages = -1;
    b.id = 500u; b.type = 0; b.name_seed = 7;
    /* An alchemist is open by day and shut at night; the shut door says how hard its lock is (seed 7: level 10). */
    check_expect(building_try_enter(&b, &p, noon, 0, &e) && e.allow, "entry: an alchemist was shut at noon");
    check_expect(!building_try_enter(&b, &p, night, 0, &e) && !e.allow && e.line != 0, "entry: an alchemist was open at three, or said nothing of its lock");
    p.house = 500u;
    check_expect(building_try_enter(&b, &p, night, 0, &e) && e.allow, "entry: the player's own house was shut");
    p.house = 0;
    /* A quest's building (op 81) lets the player in at night, a house too, by the quest's mark; a type under 17 is still by its hours first. */
    check_expect(!building_try_enter(&b, &p, night, 0, &e), "entry: the alchemist is open at night without a quest");
    b.quest_open = 1;
    check_expect(building_try_enter(&b, &p, night, 0, &e) && e.allow, "entry: a building a quest marked open was shut to the quest");
    b.quest_open = 0;
    b.type = 24;
    check_expect(building_try_enter(&b, &p, night, 0, &e), "entry: the ship's cabin (building type 24) was shut at night");
    /* A guild's own door at any hour; the Brotherhood's hall is shut to a stranger. */
    b.type = 11; b.faction = 108;
    check_expect(!building_try_enter(&b, &p, night, 0, &e), "entry: the Brotherhood's hall was open to a stranger");
    p.brotherhood = 0;
    check_expect(building_try_enter(&b, &p, night, 0, &e) && e.allow, "entry: a Brother was kept out");
    p.brotherhood = -1;
    /* An open-lock spell is spent. */
    p.lock_spell = 1;
    check_expect(building_try_enter(&b, &p, night, 0, &e) && e.spent_spell, "entry: a spell that opens locks did not open the door");
    p.lock_spell = 0;
    /* A house (type 17..20) is open by day, half of them, with record 256 said. */
    b.type = 18; b.faction = 0; b.id = 10u;
    p.daytime = 1;
    check_expect(building_try_enter(&b, &p, night, 0, &e) && e.text == 256, "entry: a house of low number was not open by day");
    b.id = 60u;
    check_expect(!building_try_enter(&b, &p, night, 0, &e), "entry: a house of high number was open by day");
    /* Steal mode: the lock is picked on a roll, the Thieves count a pick, a crime follows when seen or by Stealth, a failed try is not repeated at the same skill. */
    p.daytime = 0; p.steal = 1;
    b.type = 18; b.name_seed = 0;
    character_state()->skill[13] = 80;
    character_state()->skill[16] = 0;
    picked = crimes = failed = tried_blocked = 0;
    guilds_clear();
    for (i = 0; i < 200; i++) {
        entry_reset();
        b.id = 77u;
        if (building_try_enter(&b, &p, noon, 0, &e)) {
            picked += e.picked;
            crimes += e.crime;
        } else {
            failed++;
            /* The same skill may not try again. */
            if (!building_try_enter(&b, &p, noon, 0, &e) && e.line == 0) { tried_blocked++; }
        }
    }
    /* Skill 80 against a level 3 lock is a chance of 65, and a third of the picks (Stealth 0: a roll of 1..300 under 100) are noticed. */
    check_expect(picked > 100 && picked < 160 && crimes > 20 && crimes < 75 && guilds_state()->thieves_count > 0, "entry: picks, crimes or the Thieves' count are not in the odds the original has");
    check_expect(failed == tried_blocked, "entry: a failed try was repeated at the same skill");
    /* A witness is always a crime. */
    entry_reset();
    character_state()->skill[16] = 100;
    b.id = 78u;
    check_expect(building_try_enter(&b, &p, noon, 1, &e) ? e.crime == 1 : 1, "entry: a witnessed pick was not a crime");
    /* A visitor's grant expires at closing unless the place grants access for the visit. */
    {
        df_u32 shop = 700u, temple = 701u, house = 702u, tavern = 703u, own = 704u, day = 20u * 1440u;

        entry_reset();
        entry_grant(shop, 0, ACCESS_VISITOR, day + 12u * 60u);
        check_expect(entry_access(shop, day + 12u * 60u) == ACCESS_VISITOR && entry_access(shop, day + 23u * 60u + 30u) == 0, "access: a shop's grant does not run to closing");
        entry_grant(temple, 14, ACCESS_VISITOR, day + 23u * 60u);
        check_expect(entry_access(temple, day + 24u * 60u + 30u) == ACCESS_VISITOR && entry_access(temple, day + 25u * 60u + 1u) == 0, "access: a temple's grant does not run to 01:00");
        entry_grant(house, 17, ACCESS_VISITOR, day); entry_grant(tavern, 15, ACCESS_VISITOR, day); entry_grant(own, 17, ACCESS_FULL, day);
        check_expect(entry_access(house, day + 5000u) == ACCESS_VISITOR && entry_access(tavern, day + 5000u) == ACCESS_FULL && entry_access(own, day + 5000u) == ACCESS_FULL, "access: a house, a tavern or one's own has the wrong grant");
        entry_leave(house);
        check_expect(entry_access(house, day) == 0 && entry_access(tavern, day) == ACCESS_FULL, "access: leaving did not end the grant of that building alone");
        check_expect(entry_trespass(5, ACCESS_VISITOR, 0) == 0 && entry_trespass(10, ACCESS_VISITOR, 0) == CRIME_TRESPASS && entry_trespass(10, ACCESS_VISITOR, 1) == CRIME_BREAKING &&
                     entry_trespass(45, ACCESS_FULL, 0) == 0 && entry_trespass(5, 0, 0) == CRIME_TRESPASS, "access: a point over the grant is not crime 2, then 3");
        entry_reset();
        check_expect(entry_access(tavern, day) == 0, "access: a town reload kept a grant");
    }
}
