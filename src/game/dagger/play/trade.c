#include "game/dagger/play/trade.h"

/* See trade.h. */

int trade_base_price(const ShopState *shop, int price)
{
    /* trade_region_price */
    price = (shop->region_adjust * price) / 1000;
    if (price < 0) {
        price = 1;
    }
    /* trade_base_price: the building's quality, then doubled */
    price += ((shop->quality - 10) * price) / 100;
    price += price;
    return price;
}

int trade_adjust(const ShopState *shop, int mercantile, int personality, int price, int selling)
{
    int q, skill_factor, attr_factor;

    q = ((shop->quality - 10) * 5) + 50;
    if (!selling) {
        skill_factor = ((((q << 8) / 200) + 128) * ((((100 - mercantile) << 8) / 200) + 128)) / 256;
        attr_factor = ((((q << 8) / 200) + 128) * ((((100 - personality) << 8) / 200) + 128)) / 256;
        return (price * (((skill_factor * 192) / 256) + ((attr_factor << 6) / 256))) / 256;
    }
    skill_factor = (((((100 - q) << 8) / 200) + 128) * (((mercantile << 8) / 200) + 128)) / 256;
    attr_factor = (((((100 - q) << 8) / 200) + 128) * (((personality << 8) / 200) + 128)) / 256;
    return (price * (((skill_factor * 179) / 256) + ((attr_factor * 51) / 256))) / 256;
}

int trade_buy_line(const ShopState *shop, int value, int condition, int group, int index, int is_stack)
{
    int price;

    if (is_stack && group == ARROW_GROUP && index == ARROW_INDEX && condition == 0) {
        return 0;
    }
    /* A stack (image 998, the arrows) is sold by the number in it: its condition is its count. */
    price = is_stack ? value * condition : value;
    /* The weaponsmith's holiday (49) halves weapons, a temple's (29) everything; the debug bit halves all. */
    if (shop->type == 13 && shop->holiday == 49 && group == 3) {
        return price >> 1;
    }
    if (shop->type == 9 && shop->holiday == 29) {
        return price >> 1;
    }
    if (shop->half) {
        return price >> 1;
    }
    return price;
}

Quote trade_buy(const ShopState *shop, int mercantile, int personality, int lines)
{
    Quote q;

    q.total = trade_base_price(shop, lines);
    q.price = (trade_adjust(shop, mercantile, personality, q.total, 0) * shop->scale) / 256;
    return q;
}

Quote trade_sell(const ShopState *shop, int mercantile, int personality, int lines)
{
    Quote q;

    /* No scale, no condition, no holiday: what a pile is worth, then the seller's side of the adjustment. */
    q.total = trade_base_price(shop, lines);
    q.price = trade_adjust(shop, mercantile, personality, q.total, 1);
    return q;
}

int trade_repair_line(int value, int enchanted)
{
    int cost;

    cost = (int)(((unsigned)(value * (enchanted ? 75 : 10))) / 100u);
    return cost < 1 ? 1 : cost;
}

Quote trade_repair(const ShopState *shop, int mercantile, int personality, int lines)
{
    /* A sum under 1 is 1, then the same chain as buying. */
    return trade_buy(shop, mercantile, personality, lines > 0 ? lines : 1);
}

int trade_identify_line(int value)
{
    return (int)(((unsigned)(value * 25)) >> 8);
}

int trade_offer_text(int mode, int total, int price)
{
    int text;

    if ((total >> 1) > price) {
        text = 260;
    } else if ((total - (total >> 2)) > price) {
        text = 261;
    } else {
        text = 262;
    }
    return mode == 2 ? text + 3 : text;
}

int trade_steal_chance(int pickpocket, int total, int weight)
{
    return pickpocket - total / 32 - weight / 4;
}
