#include "game/ogfall/play/market.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/play.h"
#include "game/ogfall/tables/itemdefs.h"

/* See market.h. */

#define SKILL_MERCANTILE 14
#define ATTR_PERSONALITY 5

static int g_mode;
static OgShop g_shop;
static df_u32 g_shelves;
static int g_offered;
static int g_refusal;

int og_market_open(int mode, const OgShop *shop, df_u32 shelves)
{
    if (g_mode || mode < OG_MARKET_BUY || mode > OG_MARKET_REPAIR || !og_object(shelves)) {
        return 0;
    }
    g_mode = mode;
    g_shop = *shop;
    g_shelves = shelves;
    g_offered = 0;
    g_refusal = 0;
    return 1;
}

int og_market_mode(void)
{
    return g_mode;
}

static int in_pack(const OgObject *item)
{
    return item->parent == og_state()->inventory_id;
}

int og_market_take(df_u32 id)
{
    const OgObject *item;

    item = og_object(id);
    if (!g_mode || !item || item->type != OG_RECORD_ITEM) {
        return 0;
    }
    g_offered = 0;
    if (g_mode == OG_MARKET_BUY) {
        if (item->parent != g_shelves || !og_inventory_transfer(id, og_state()->inventory_id)) {
            return 0;
        }
        og_object_edit(id)->flags |= OG_ITEM_UNPAID;
        return 1;
    }
    /* Selling and repair: from the pack to the counter, never an unpaid thing, never gold. */
    if (!in_pack(item) || (item->flags & OG_ITEM_UNPAID) || item->group == 28) {
        return 0;
    }
    return og_inventory_transfer(id, g_shelves);
}

int og_market_return(df_u32 id)
{
    const OgObject *item;

    item = og_object(id);
    if (!g_mode || !item || item->type != OG_RECORD_ITEM) {
        return 0;
    }
    g_offered = 0;
    if (g_mode == OG_MARKET_BUY) {
        if (!in_pack(item) || !(item->flags & OG_ITEM_UNPAID)) {
            return 0;
        }
        og_object_edit(id)->flags &= ~OG_ITEM_UNPAID;
        return og_inventory_transfer(id, g_shelves);
    }
    return item->parent == g_shelves && og_inventory_transfer(id, og_state()->inventory_id);
}

/* Every item in the pack or on the counter, as the original's object_foreach does. */
static int lines(void)
{
    const OgState *s;
    int i, sum;

    s = og_state();
    sum = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        const OgObject *item;

        item = &s->object[i];
        if (!item->id || item->type != OG_RECORD_ITEM) {
            continue;
        }
        if (g_mode == OG_MARKET_BUY) {
            if (in_pack(item) && (item->flags & OG_ITEM_UNPAID)) {
                /* An arrow stack is sold by its count, here the item's count. */
                int stack;

                stack = item->group == OG_ARROW_GROUP && item->index == OG_ARROW_INDEX;
                sum += og_trade_buy_line(&g_shop, item->value, stack ? item->count : item->condition, item->group, item->index, stack);
            }
        } else if (item->parent == g_shelves && !(item->flags & OG_ITEM_UNPAID)) {
            sum += g_mode == OG_MARKET_SELL ? item->value : og_trade_repair_line(item->value, 0 /* no enchantments yet */);
        }
    }
    return sum;
}

OgQuote og_market_quote(void)
{
    const OgCharacter *c;
    OgQuote none;
    int sum;

    none.total = none.price = 0;
    if (!g_mode) {
        return none;
    }
    c = og_character();
    sum = lines();
    if (g_mode == OG_MARKET_BUY) {
        return sum > 0 ? og_trade_buy(&g_shop, c->skill[SKILL_MERCANTILE], c->attribute[ATTR_PERSONALITY], sum) : none;
    }
    if (g_mode == OG_MARKET_SELL) {
        return sum > 0 ? og_trade_sell(&g_shop, c->skill[SKILL_MERCANTILE], c->attribute[ATTR_PERSONALITY], sum) : none;
    }
    return sum > 0 ? og_trade_repair(&g_shop, c->skill[SKILL_MERCANTILE], c->attribute[ATTR_PERSONALITY], sum) : none;
}

int og_market_offer(void)
{
    OgQuote q;

    q = og_market_quote();
    if (!g_mode || q.total <= 0) {
        return 0;
    }
    g_offered = 1;
    return og_trade_offer_text(g_mode, q.total, q.price);
}

int og_market_confirm(int yes)
{
    OgQuote q;
    int i;

    g_refusal = 0;
    if (!g_offered) {
        return 0;
    }
    g_offered = 0;
    if (!yes) {
        return 0;
    }
    q = og_market_quote();
    if (q.price <= 0) {
        return 0;
    }
    if (g_mode != OG_MARKET_SELL && og_character()->gold < q.price) {
        g_refusal = OG_TEXT_CANNOT_AFFORD;
        return 0;
    }
    if (g_mode == OG_MARKET_SELL) {
        og_character()->gold += q.price;
        /* object_free_children of the counter: what was sold is gone. */
        for (i = 0; i < OG_OBJECTS; i++) {
            const OgObject *item;

            item = &og_state()->object[i];
            if (item->id && item->type == OG_RECORD_ITEM && item->parent == g_shelves) {
                og_object_delete(item->id);
            }
        }
    } else {
        og_character()->gold -= q.price;
        if (g_mode == OG_MARKET_BUY) {
            /* inv_claim_items: what was in the basket is theirs. */
            for (i = 0; i < OG_OBJECTS; i++) {
                OgObject *item;

                item = og_object_edit(og_state()->object[i].id);
                if (item && item->type == OG_RECORD_ITEM && in_pack(item) && (item->flags & OG_ITEM_UNPAID)) {
                    item->flags &= ~OG_ITEM_UNPAID;
                }
            }
        }
    }
    /* trade_settle_offer: a yes is one use of Mercantile; then the coins' sound (204). */
    og_skill_use(SKILL_MERCANTILE);
    og_sound(204);
    return q.price;
}

int og_market_refusal(void)
{
    return g_refusal;
}

void og_market_clear(void)
{
    const OgState *s;
    int i;

    if (!g_mode) {
        return;
    }
    s = og_state();
    g_offered = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        const OgObject *item;

        item = &s->object[i];
        if (!item->id || item->type != OG_RECORD_ITEM) {
            continue;
        }
        if (g_mode == OG_MARKET_BUY && in_pack(item) && (item->flags & OG_ITEM_UNPAID)) {
            og_market_return(item->id);
        } else if (g_mode != OG_MARKET_BUY && item->parent == g_shelves) {
            og_inventory_transfer(item->id, og_state()->inventory_id);
        }
    }
}

void og_market_close(void)
{
    if (!g_mode) {
        return;
    }
    og_market_clear();
    g_mode = OG_MARKET_NONE;
    g_offered = 0;
}
