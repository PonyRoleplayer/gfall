#include "check_ogtrade.h"
#include "check_util.h"
#include "game/ogfall/play/trade.h"

/*
 * By hand, from docs/economy-math.md. A neutral shop: quality 10 (the quality term is 50), the
 * region at 1000, a buyer with Mercantile 50 and Personality 50.
 *   base price of 100:  region 100, quality +0, doubled: 200
 *   buying: (((50<<8)/200)+128) = 192 on each side; 192*192/256 = 144 for skill and for attribute;
 *           (144*192)/256 + (144<<6)/256 = 108 + 36 = 144;  200 * 144 / 256 = 112
 *   selling: (144*179)/256 + (144*51)/256 = 100 + 28 = 128;  200 * 128 / 256 = 100
 */
void check_ogtrade(void)
{
    OgShop shop;
    OgQuote q;

    shop.region_adjust = 1000;
    shop.quality = 10;
    shop.type = 0;
    shop.holiday = 0;
    shop.half = 0;
    shop.scale = 256;

    check_expect(og_trade_base_price(&shop, 100) == 200, "trade: a neutral shop's base price of 100 is not 200");
    q = og_trade_buy(&shop, 50, 50, 100);
    check_expect(q.total == 200 && q.price == 112, "trade: buying 100 of goods from a neutral shop is not 200 and 112");
    q = og_trade_sell(&shop, 50, 50, 100);
    check_expect(q.total == 200 && q.price == 100, "trade: selling 100 of goods to a neutral shop is not 200 and 100");

    /* The region: its adjustment scales the price before the quality. 2000 doubles it, 500 halves it. */
    shop.region_adjust = 2000;
    check_expect(og_trade_base_price(&shop, 100) == 400, "trade: a region at 2000 did not double the base price");
    shop.region_adjust = 500;
    check_expect(og_trade_base_price(&shop, 100) == 100, "trade: a region at 500 did not halve the base price");
    shop.region_adjust = 1000;

    /* Quality 20: the quality term 100, and +10% in the base price: 100 -> 110 -> 220; buying 220 * 192 / 256 = 165. */
    shop.quality = 20;
    q = og_trade_buy(&shop, 50, 50, 100);
    check_expect(q.total == 220 && q.price == 165, "trade: a quality 20 shop did not quote 220 and 165");
    shop.quality = 10;

    /* A better merchant pays less: Mercantile 100 and Personality 100 buy 100 for less than 112. */
    check_expect(og_trade_buy(&shop, 100, 100, 100).price < 112, "trade: Mercantile and Personality 100 did not lower the price");
    check_expect(og_trade_buy(&shop, 0, 0, 100).price > 112, "trade: Mercantile and Personality 0 did not raise the price");

    /* The scale (a guild's discount) applies to buying and repair, never to selling. */
    shop.scale = 128;
    check_expect(og_trade_buy(&shop, 50, 50, 100).price == 56 && og_trade_sell(&shop, 50, 50, 100).price == 100,
                 "trade: the scale did not halve buying only");
    shop.scale = 256;

    /* Lines: a thing is its value; a stack of arrows its value times its count, none when spent. */
    check_expect(og_trade_buy_line(&shop, 40, 0, 2, 5, 0) == 40, "trade: a thing's line is not its value");
    check_expect(og_trade_buy_line(&shop, 2, 12, OG_ARROW_GROUP, OG_ARROW_INDEX, 1) == 24, "trade: 12 arrows at 2 are not 24");
    check_expect(og_trade_buy_line(&shop, 2, 0, OG_ARROW_GROUP, OG_ARROW_INDEX, 1) == 0, "trade: a spent stack of arrows had a price");
    /* Holidays: the weaponsmith's 49 halves weapons (group 3) only; a temple's 29 halves everything; no other shop. */
    shop.type = 13;
    shop.holiday = 49;
    check_expect(og_trade_buy_line(&shop, 101, 0, 3, 1, 0) == 50 && og_trade_buy_line(&shop, 101, 0, 2, 1, 0) == 101,
                 "trade: holiday 49 did not halve weapons only (and by a shift)");
    shop.type = 9;
    shop.holiday = 29;
    check_expect(og_trade_buy_line(&shop, 101, 0, 2, 1, 0) == 50, "trade: a temple's holiday 29 did not halve the price");
    shop.type = 0;
    check_expect(og_trade_buy_line(&shop, 101, 0, 2, 1, 0) == 101, "trade: a holiday halved a shop it is not for");
    shop.holiday = 0;

    /* Repair: a tenth of the value, three quarters when enchanted, at least 1; the sum under 1 is 1; then the buying chain. */
    check_expect(og_trade_repair_line(100, 0) == 10 && og_trade_repair_line(100, 1) == 75 && og_trade_repair_line(3, 0) == 1,
                 "trade: repair lines");
    q = og_trade_repair(&shop, 50, 50, 0);
    check_expect(q.total == 2 && q.price == 1, "trade: an empty repair is not 1 doubled, then adjusted");
    q = og_trade_repair(&shop, 50, 50, 100);
    check_expect(q.total == 200 && q.price == 112, "trade: a repair of 100 does not follow the buying chain");

    /* Identification at the Mages Guild: (value * 25) >> 8, no skill. */
    check_expect(og_trade_identify_line(1000) == 97 && og_trade_identify_line(10) == 0, "trade: identification costs");

    /* The offer box: under half the total, 260; under three quarters, 261; else 262; selling adds 3. */
    check_expect(og_trade_offer_text(1, 200, 90) == 260 && og_trade_offer_text(1, 200, 112) == 261 && og_trade_offer_text(1, 200, 160) == 262,
                 "trade: the buying offer's text");
    check_expect(og_trade_offer_text(2, 200, 90) == 263 && og_trade_offer_text(2, 200, 160) == 265, "trade: the selling offer's text");

    /* Steal: Pickpocket less a 32nd of the total and a 4th of the weight, no clamp. */
    check_expect(og_trade_steal_chance(60, 320, 40) == 40 && og_trade_steal_chance(5, 1000, 80) == -46, "trade: the steal chance");
}
