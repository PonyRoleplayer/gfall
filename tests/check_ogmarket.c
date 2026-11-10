#include <stdio.h>
#include <string.h>
#include "check_ogmarket.h"
#include "../apps/ogfall/seam.h"
#include "engine/core/mem.h"
#include "check_util.h"
#include "game/ogfall/play/market.h"
#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/shelf.h"
#include "game/ogfall/play/play.h"
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

/* The composed 320 x 200 screen, to look at (build/ogfall-<name>.ppm). */
static void shot(const char *name)
{
    const df_u8 *pixels, *palette;
    char path[96];
    FILE *file;
    int i;

    seam_present();
    pixels = seam_screen(&palette);
    snprintf(path, sizeof(path), "build/ogfall-%s.ppm", name);
    file = fopen(path, "wb");
    if (!file || !pixels || !palette) {
        if (file) {
            fclose(file);
        }
        return;
    }
    fprintf(file, "P6\n320 200\n255\n");
    for (i = 0; i < 64000; i++) {
        fwrite(palette + pixels[i] * 3, 1, 3, file);
    }
    fclose(file);
}

static void click(int cx, int cy)
{
    OgIn in;

    mem_zero(&in, sizeof(in));
    in.cx = cx;
    in.cy = cy;
    in.on_picture = 1;
    in.mouse_pressed = 1;
    og_possessions_frame(&in);
    /* One idle frame after it, as the game runs: it refreshes the cost panel. */
    mem_zero(&in, sizeof(in));
    in.cx = cx;
    in.cy = cy;
    in.on_picture = 1;
    og_possessions_frame(&in);
}

/*
 * The shop screen driven by clicks, through the real controller: the original's button column
 * (inv_mode_buttons), Select moving an item, Buy and its offer box, Accept and Reject, Clear, Exit.
 */
void check_ogmarket_screen(void)
{
    OgShop shop;
    df_u32 shelf, thing;
    const OgPossessions *view;
    int gold, count_pack;

    shop = neutral();
    og_possessions_reset();
    og_character()->gold = 5000;
    shelf = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
    check_expect(shelf != 0 && og_shelf_stock(shelf, 4, 12, 4, 0) >= 0, "screen: a shelf did not stock");
    thing = og_item_create(shelf, 6, 0, -1);
    check_expect(thing != 0, "screen: no item for the shelf");
    if (!thing) {
        return;
    }
    check_expect(og_possessions_trade(OG_MARKET_BUY, &shop, shelf), "screen: the shop did not open");
    view = og_possessions();
    check_expect(view->page == OG_VIEW_INVENTORY && view->trade == OG_MARKET_BUY && view->remote == shelf, "screen: the shop is not the inventory over the shelf");
    check_expect(!og_possessions_trade(OG_MARKET_SELL, &shop, shelf), "screen: a second shop opened over the first");

    /* The tab the thing is listed under (the header's four, 0..3 by x). */
    {
        static const int tab_x[4] = {40, 120, 200, 290};

        click(tab_x[og_item_tab(og_object(thing))], 4);
    }
    /* Select (the default) moves a shelf item, the first on the right, into the pack unpaid; the cost panel follows. */
    count_pack = og_inventory_rows(og_state()->inventory_id, -1, 0, 0);
    click(280, 52);
    check_expect(og_inventory_rows(og_state()->inventory_id, -1, 0, 0) == count_pack + 1 && view->price > 0, "screen: a click on the shelf did not take the item and show a total");

    /* Buy (226..256, 134..147): the offer box; Reject changes nothing; Accept pays. */
    gold = og_character()->gold;
    shot("shop-basket");
    click(240, 140);
    shot("shop-offer");
    check_expect(view->prompt == OG_PROMPT_OFFER && view->offer[0], "screen: Buy did not open the offer box with the shopkeeper's words");
    check_expect(!strstr(view->offer, "%a"), "screen: the price macro was left in the offer");
    click(203, 125);
    check_expect(view->prompt == 0 && og_character()->gold == gold, "screen: Reject paid or left the box up");
    click(240, 140);
    click(117, 125);
    check_expect(view->prompt == 0 && og_character()->gold < gold, "screen: Accept did not pay");

    /* Clear puts a basket back; Info (226..256, 36..49) switches the action; the Exit button leaves. */
    og_market_take(og_item_create(shelf, 6, 1, -1));
    check_expect(og_market_quote().total > 0, "screen: nothing in the basket to clear");
    click(240, 162);
    check_expect(og_market_quote().total == 0, "screen: Clear did not empty the basket");
    click(240, 42);
    check_expect(view->action == OG_ITEM_INFO, "screen: the Info button did not switch the action");
    click(240, 64);
    check_expect(view->action == OG_ITEM_EQUIP, "screen: the Select button did not switch the action");
    click(240, 188);
    check_expect(view->page == OG_VIEW_NONE && view->trade == 0 && og_market_mode() == OG_MARKET_NONE, "screen: Exit did not leave the shop");
    og_shelf_discard(shelf);
}
