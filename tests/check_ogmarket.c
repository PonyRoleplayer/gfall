#include <stdio.h>
#include "check_ogmarket.h"
#include "check_util.h"
#include "game/ogfall/play/market.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/tables/holidaydefs.h"

static OgShop neutral(void)
{
    OgShop s;

    s.region_adjust = 1000;
    s.quality = 10;
    s.type = 2;
    s.holiday = 0;
    s.half = 0;
    s.scale = 256;
    return s;
}

static int unpaid_count(void)
{
    int i, n;

    n = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        const OgObject *o;

        o = &og_state()->object[i];
        n += o->id && o->type == OG_RECORD_ITEM && o->parent == og_state()->inventory_id && (o->flags & OG_ITEM_UNPAID);
    }
    return n;
}

void check_ogmarket(void)
{
    OgShop shop;
    df_u32 shelf, a, b, c;
    OgQuote q;
    int gold, text, paid, merc_uses, bought;

    /* holiday_today: a year is 360 days; day 1 is a holiday for every region, the last five days hold none. */
    check_expect(og_holiday_today(0, 3) == 1, "holiday: day 1 of the year is the first holiday");
    check_expect(og_holiday_today(1440u * 355u, 0) == 0 && og_holiday_today(1440u * 358u, 0) == 0, "holiday: the last five days have none");
    check_expect(og_holiday_today(518400u, 3) == 1, "holiday: the year does not wrap at 518400 minutes");

    shop = neutral();
    og_character()->gold = 5000;
    shelf = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
    a = og_item_create(shelf, 6, 0, -1);
    b = og_item_create(shelf, 6, 1, -1);
    c = og_item_create(shelf, 6, 2, -1);
    check_expect(shelf && a && b && c, "market: no room for a shelf and three things");
    if (!(shelf && a && b && c)) {
        return;
    }
    merc_uses = (int)og_state()->skill_uses[14];

    /* Buying: one thing in the basket is unpaid; the quote is the chain on its value. */
    check_expect(og_market_open(OG_MARKET_BUY, &shop, shelf) && !og_market_open(OG_MARKET_SELL, &shop, shelf), "market: a second session opened");
    check_expect(og_market_quote().total == 0 && og_market_offer() == 0, "market: an empty basket had a price");
    check_expect(og_market_take(a) && unpaid_count() == 1 && og_object(a)->parent == og_state()->inventory_id, "market: a shelf item did not go to the pack unpaid");
    q = og_market_quote();
    check_expect(q.total == og_trade_base_price(&shop, og_object(a)->value) && q.price > 0, "market: the basket's total is not the base price of its value");
    gold = og_character()->gold;
    text = og_market_offer();
    check_expect(text >= 260 && text <= 262, "market: the offer text is not 260..262");
    paid = og_market_confirm(1);
    check_expect(paid == q.price && og_character()->gold == gold - q.price && unpaid_count() == 0 && og_object(a)->parent == og_state()->inventory_id,
                 "market: paying did not take the price and keep the item");
    bought = paid;
    check_expect((int)og_state()->skill_uses[14] == merc_uses + 1, "market: a yes was not one use of Mercantile");

    /* A no leaves everything; a basket put back is off the books; an unaffordable one is refused with 454. */
    og_market_take(b);
    og_market_offer();
    check_expect(og_market_confirm(0) == 0 && unpaid_count() == 1, "market: a no changed something");
    check_expect(og_market_return(b) && unpaid_count() == 0 && og_object(b)->parent == shelf, "market: an unpaid item did not go back to the shelf");
    og_market_take(c);
    og_character()->gold = 0;
    og_market_offer();
    check_expect(og_market_confirm(1) == 0 && og_market_refusal() == OG_TEXT_CANNOT_AFFORD && og_character()->gold == 0 && unpaid_count() == 1,
                 "market: a purchase with no gold was not refused with 454");
    /* Leaving puts what is unpaid back. */
    og_market_close();
    check_expect(unpaid_count() == 0 && og_object(c)->parent == shelf && og_market_mode() == OG_MARKET_NONE, "market: leaving did not return the unpaid");

    /* Selling: a pack item to the counter, paid at the seller's side, gone; no scale; gold goes up. */
    og_character()->gold = 100;
    {
        df_u32 counter;

        counter = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
        check_expect(og_market_open(OG_MARKET_SELL, &shop, counter), "market: selling did not open");
        check_expect(og_market_take(a) && og_object(a)->parent == counter, "market: a pack item did not go on the counter");
        q = og_market_quote();
        check_expect(q.total == og_trade_base_price(&shop, og_object(a)->value), "market: the counter's total is not the base price");
        text = og_market_offer();
        check_expect(text >= 263 && text <= 265, "market: the selling offer text is not 263..265");
        paid = og_market_confirm(1);
        check_expect(paid == q.price && og_character()->gold == 100 + q.price && !og_object(a), "market: selling did not pay and take the item");
        /* Put a thing on the counter and leave: it comes back to the pack. */
        og_market_take(og_item_create(og_state()->inventory_id, 6, 3, -1));
        og_market_close();
        check_expect(og_market_mode() == OG_MARKET_NONE, "market: it did not close");
        printf("market: bought for %d, sold for %d, refused 454, returned the unpaid\n", bought, q.price);
    }
}
