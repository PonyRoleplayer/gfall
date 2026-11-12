#include <stdio.h>
#include <string.h>
#include "check_market.h"
#include "../apps/gfall/seam.h"
#include "engine/core/mem.h"
#include "check_util.h"
#include "game/dagger/play/market.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/shelf.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/holidaydefs.h"

static ShopState neutral(void)
{
    ShopState s;

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
    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *o;

        o = &game_state()->object[i];
        n += o->id && o->type == RECORD_ITEM && o->parent == game_state()->inventory_id && (o->flags & ITEM_UNPAID);
    }
    return n;
}

void check_market(void)
{
    ShopState shop;
    df_u32 shelf, a, b, c;
    Quote q;
    int gold, text, paid, merc_uses, bought;

    /* holiday_today: a year is 360 days; day 1 is a holiday for every region, the last five days hold none. */
    check_expect(holiday_today(0, 3) == 1, "holiday: day 1 of the year is the first holiday");
    check_expect(holiday_today(1440u * 355u, 0) == 0 && holiday_today(1440u * 358u, 0) == 0, "holiday: the last five days have none");
    check_expect(holiday_today(518400u, 3) == 1, "holiday: the year does not wrap at 518400 minutes");

    shop = neutral();
    character_state()->gold = 5000;
    shelf = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
    a = item_create(shelf, 6, 0, -1);
    b = item_create(shelf, 6, 1, -1);
    c = item_create(shelf, 6, 2, -1);
    check_expect(shelf && a && b && c, "market: no room for a shelf and three things");
    if (!(shelf && a && b && c)) {
        return;
    }
    merc_uses = (int)game_state()->skill_uses[14];

    /* Buying: one thing in the basket is unpaid; the quote is the chain on its value. */
    check_expect(market_open(MARKET_BUY, &shop, shelf) && !market_open(MARKET_SELL, &shop, shelf), "market: a second session opened");
    check_expect(market_quote().total == 0 && market_offer() == 0, "market: an empty basket had a price");
    check_expect(market_take(a) && unpaid_count() == 1 && object_at(a)->parent == game_state()->inventory_id, "market: a shelf item did not go to the pack unpaid");
    q = market_quote();
    check_expect(q.total == trade_base_price(&shop, object_at(a)->value) && q.price > 0, "market: the basket's total is not the base price of its value");
    gold = character_state()->gold;
    text = market_offer();
    check_expect(text >= 260 && text <= 262, "market: the offer text is not 260..262");
    paid = market_confirm(1);
    check_expect(paid == q.price && character_state()->gold == gold - q.price && unpaid_count() == 0 && object_at(a)->parent == game_state()->inventory_id,
                 "market: paying did not take the price and keep the item");
    bought = paid;
    check_expect((int)game_state()->skill_uses[14] == merc_uses + 1, "market: a yes was not one use of Mercantile");

    /* A no leaves everything; a basket put back is off the books; an unaffordable one is refused with 454. */
    market_take(b);
    market_offer();
    check_expect(market_confirm(0) == 0 && unpaid_count() == 1, "market: a no changed something");
    check_expect(market_return(b) && unpaid_count() == 0 && object_at(b)->parent == shelf, "market: an unpaid item did not go back to the shelf");
    market_take(c);
    character_state()->gold = 0;
    market_offer();
    check_expect(market_confirm(1) == 0 && market_refusal() == TEXT_CANNOT_AFFORD && character_state()->gold == 0 && unpaid_count() == 1,
                 "market: a purchase with no gold was not refused with 454");
    /* Leaving puts what is unpaid back. */
    market_close();
    check_expect(unpaid_count() == 0 && object_at(c)->parent == shelf && market_mode() == MARKET_NONE, "market: leaving did not return the unpaid");

    /* Selling: a pack item to the counter, paid at the seller's side, gone; no scale; gold goes up. */
    character_state()->gold = 100;
    {
        df_u32 counter;

        counter = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
        check_expect(market_open(MARKET_SELL, &shop, counter), "market: selling did not open");
        check_expect(market_take(a) && object_at(a)->parent == counter, "market: a pack item did not go on the counter");
        q = market_quote();
        check_expect(q.total == trade_base_price(&shop, object_at(a)->value), "market: the counter's total is not the base price");
        text = market_offer();
        check_expect(text >= 263 && text <= 265, "market: the selling offer text is not 263..265");
        paid = market_confirm(1);
        check_expect(paid == q.price && character_state()->gold == 100 + q.price && !object_at(a), "market: selling did not pay and take the item");
        /* Put a thing on the counter and leave: it comes back to the pack. */
        market_take(item_create(game_state()->inventory_id, 6, 3, -1));
        market_close();
        check_expect(market_mode() == MARKET_NONE, "market: it did not close");
        printf("market: bought for %d, sold for %d, refused 454, returned the unpaid\n", bought, q.price);
    }
}

/* The composed 320 x 200 screen, to look at (build/gfall-<name>.ppm). */
static void shot(const char *name)
{
    const df_u8 *pixels, *palette;
    char path[96];
    FILE *file;
    int i;

    seam_present();
    pixels = seam_screen(&palette);
    snprintf(path, sizeof(path), "build/gfall-%s.ppm", name);
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
    PlayIn in;

    mem_zero(&in, sizeof(in));
    in.cx = cx;
    in.cy = cy;
    in.on_picture = 1;
    in.mouse_pressed = 1;
    possessions_frame(&in);
    /* One idle frame after it, as the game runs: it refreshes the cost panel. */
    mem_zero(&in, sizeof(in));
    in.cx = cx;
    in.cy = cy;
    in.on_picture = 1;
    possessions_frame(&in);
}

/*
 * The shop screen driven by clicks, through the real controller: the original's button column
 * (inv_mode_buttons), Select moving an item, Buy and its offer box, Accept and Reject, Clear, Exit.
 */
void check_market_screen(void)
{
    ShopState shop;
    df_u32 shelf, thing;
    const Possessions *view;
    int gold, count_pack;

    shop = neutral();
    possessions_reset();
    character_state()->gold = 5000;
    shelf = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
    check_expect(shelf != 0 && shelf_stock(shelf, 4, 12, 4, 0) >= 0, "screen: a shelf did not stock");
    thing = item_create(shelf, 6, 0, -1);
    check_expect(thing != 0, "screen: no item for the shelf");
    if (!thing) {
        return;
    }
    check_expect(possessions_trade(MARKET_BUY, &shop, shelf), "screen: the shop did not open");
    view = possessions_state();
    check_expect(view->page == VIEW_INVENTORY && view->trade == MARKET_BUY && view->remote == shelf, "screen: the shop is not the inventory over the shelf");
    check_expect(!possessions_trade(MARKET_SELL, &shop, shelf), "screen: a second shop opened over the first");

    /* The tab the thing is listed under (the header's four, 0..3 by x). */
    {
        static const int tab_x[4] = {40, 120, 200, 290};

        click(tab_x[item_tab(object_at(thing))], 4);
    }
    /* Select (the default) moves a shelf item, the first on the right, into the pack unpaid; the cost panel follows. */
    count_pack = inventory_rows(game_state()->inventory_id, -1, 0, 0);
    click(280, 52);
    check_expect(inventory_rows(game_state()->inventory_id, -1, 0, 0) == count_pack + 1 && view->price > 0, "screen: a click on the shelf did not take the item and show a total");

    /* Buy (226..256, 134..147): the offer box; Reject changes nothing; Accept pays. */
    gold = character_state()->gold;
    shot("shop-basket");
    click(240, 140);
    shot("shop-offer");
    check_expect(view->prompt == PROMPT_OFFER && view->offer[0], "screen: Buy did not open the offer box with the shopkeeper's words");
    check_expect(!strstr(view->offer, "%a"), "screen: the price macro was left in the offer");
    click(203, 125);
    check_expect(view->prompt == 0 && character_state()->gold == gold, "screen: Reject paid or left the box up");
    click(240, 140);
    click(117, 125);
    check_expect(view->prompt == 0 && character_state()->gold < gold, "screen: Accept did not pay");

    /* Clear puts a basket back; Info (226..256, 36..49) switches the action; the Exit button leaves. */
    market_take(item_create(shelf, 6, 1, -1));
    check_expect(market_quote().total > 0, "screen: nothing in the basket to clear");
    click(240, 162);
    check_expect(market_quote().total == 0, "screen: Clear did not empty the basket");
    click(240, 42);
    check_expect(view->action == ITEM_INFO, "screen: the Info button did not switch the action");
    click(240, 64);
    check_expect(view->action == ITEM_EQUIP, "screen: the Select button did not switch the action");
    click(240, 188);
    check_expect(view->page == VIEW_NONE && view->trade == 0 && market_mode() == MARKET_NONE, "screen: Exit did not leave the shop");
    shelf_discard(shelf);
}
