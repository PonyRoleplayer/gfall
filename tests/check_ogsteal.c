#include <string.h>
#include "check_ogsteal.h"
#include "check_util.h"
#include "game/dagger/play/steal.h"
#include "game/dagger/play/market.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/guild.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"

void check_ogsteal(void)
{
    OgSteal r;
    OgShop shop;
    df_u32 shelf, a;
    int i, ok, nothing, gold, before, worked, failed;

    /* A pocket at Pickpocket 100 is held to 95%: about one try in twenty is seen. Of the rest a third have nothing, else 1 to 5 gold. */
    og_guilds_clear();
    og_character()->skill[OG_SKILL_PICKPOCKET] = 100;
    og_character()->gold = 0;
    og_law()->crime = 0;
    ok = 1; nothing = 0; failed = 0;
    for (i = 0; i < 600; i++) {
        before = og_character()->gold;
        if (!og_steal_pocket(0, 1, &r)) { failed++; continue; }
        if (r.text == 8999) { nothing++; ok = ok && og_character()->gold == before; }
        else { ok = ok && r.gold >= 1 && r.gold <= 5 && og_character()->gold == before + r.gold && !strncmp(r.line, "You pinched", 11); }
    }
    check_expect(ok, "steal: a pocket's gold was out of range or not the line's");
    check_expect(og_guilds()->thieves_invite != 0 || og_guilds()->thieves_count == 100, "steal: paid pockets were not counted by the Thieves Guild");
    check_expect(failed > 5 && failed < 70 && nothing > 120 && nothing < 280, "steal: skill 100 should be held to 95%, a third of the pockets empty");

    /* At skill 0 the chance is 5%: nearly every try is seen, noting pickpocketing and spoiling the person (a creature wakes instead). */
    og_character()->skill[OG_SKILL_PICKPOCKET] = 0;
    failed = 0; worked = 0;
    for (i = 0; i < 200; i++) {
        og_law()->crime = 0;
        if (og_steal_pocket(0, 1, &r)) { worked++; continue; }
        failed++;
        ok = ok && r.spoiled && !r.wake && og_law()->crime == OG_CRIME_PICKPOCKETING && !strcmp(r.line, "You are not successful...");
    }
    check_expect(ok && failed > 170 && worked < 30, "steal: a skill of 0 should fail almost always, and note pickpocketing");
    og_law()->crime = 0;
    for (i = 0; i < 50 && og_steal_pocket(1, 1, &r); i++) { }
    check_expect(i < 50 && r.wake && !r.spoiled, "steal: a creature's pocket failure wakes the company, not spoils");
    /* A creature five levels under is 25 easier (at most 95). */
    og_character()->skill[OG_SKILL_PICKPOCKET] = 70;
    og_character()->level = 6;
    failed = 0;
    for (i = 0; i < 400; i++) { failed += !og_steal_pocket(1, 1, &r); }
    check_expect(failed == 0 || failed < 40, "steal: a creature five levels under should be seen at most one try in twenty");

    /* The shop: a skill of 100 takes the basket; a skill of 0 is seen, uses Stealth and leaves a theft. */
    shop.region_adjust = 1000; shop.quality = 10; shop.type = 2; shop.holiday = 0; shop.half = 0; shop.scale = 256;
    shelf = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
    a = og_item_create(shelf, 6, 0, -1);
    check_expect(shelf && a, "steal: no room for a shelf");
    if (!(shelf && a)) { return; }
    og_character()->gold = 500;
    og_character()->skill[OG_SKILL_PICKPOCKET] = 255;
    og_law()->crime = 0;
    check_expect(og_market_open(OG_MARKET_BUY, &shop, shelf) && og_market_steal(&r) == -1, "steal: an empty basket was stolen");
    og_market_take(a);
    check_expect(og_market_steal(&r) == 1 && og_character()->gold == 500 && og_object(a)->parent == og_state()->inventory_id &&
                 !(og_object(a)->flags & OG_ITEM_UNPAID) && !strcmp(r.line, "You are successful.") && og_law()->crime == 0,
                 "steal: a sure theft did not keep the goods free");
    og_market_close();
    a = og_item_create(shelf, 6, 1, -1);
    og_character()->skill[OG_SKILL_PICKPOCKET] = 0;
    before = (int)og_state()->skill_uses[OG_SKILL_STEALTH];
    og_market_open(OG_MARKET_BUY, &shop, shelf);
    og_market_take(a);
    check_expect(og_market_steal(&r) == 0 && og_law()->crime == OG_CRIME_THEFT && !strcmp(r.line, "You are not successful.") &&
                 (int)og_state()->skill_uses[OG_SKILL_STEALTH] == before + 1, "steal: a seen theft is not a crime and a use of Stealth");
    og_market_close();
    gold = og_character()->gold;
    check_expect(gold == 500, "steal: the purse changed");
    /* A shelf looked through is seen when the roll 0..255 reaches 108 less Pickpocket, as click.c has it: at 108 always. */
    og_character()->skill[OG_SKILL_PICKPOCKET] = 108;
    og_law()->crime = 0;
    ok = 1;
    for (i = 0; i < 300; i++) { ok = ok && og_steal_shelf(); }
    check_expect(ok && og_law()->crime == OG_CRIME_THEFT, "steal: a shelf at skill 108 was not always seen");
    og_character()->skill[OG_SKILL_PICKPOCKET] = 0;
    og_law()->crime = 0;
    failed = 0;
    for (i = 0; i < 400; i++) { failed += og_steal_shelf(); }
    check_expect(failed > 190 && failed < 270, "steal: at skill 0 a shelf should be seen on 148 of 256 rolls");
}
