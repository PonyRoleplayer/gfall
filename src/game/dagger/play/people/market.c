#include "game/dagger/formats/sndfile.h"
#include "game/dagger/play/people/market.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/magic/itemmaker.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/people/steal.h"
#include "game/dagger/tables/itemdefs.h"

/* See market.h. */


static int g_mode;
static ShopState g_shop;
static df_u32 g_shelves;
static int g_offered;
static int g_refusal;

int market_open(int mode, const ShopState *shop, df_u32 shelves)
{
    if (g_mode || mode < MARKET_BUY || mode > MARKET_REPAIR || !object_at(shelves)) {
        return 0;
    }
    g_mode = mode;
    g_shop = *shop;
    g_shelves = shelves;
    g_offered = 0;
    g_refusal = 0;
    return 1;
}

int market_mode(void)
{
    return g_mode;
}

static int in_pack(const ObjectInfo *item)
{
    return item->parent == game_state()->inventory_id;
}

int market_take(df_u32 id)
{
    const ObjectInfo *item;

    item = object_at(id);
    if (!g_mode || !item || item->type != RECORD_ITEM) {
        return 0;
    }
    g_offered = 0;
    if (g_mode == MARKET_BUY) {
        if (item->parent != g_shelves || item->group == ITEM_GROUP_CURRENCY) { return 0; }
        object_edit(id)->flags |= ITEM_UNPAID;
        if (!inventory_transfer(id, game_state()->inventory_id)) {
            object_edit(id)->flags &= ~ITEM_UNPAID;
            return 0;
        }
        return 1;
    }
    /* Selling and repair: from the pack to the counter, never an unpaid thing, never gold. */
    if (!in_pack(item) || (item->flags & ITEM_UNPAID) || item->group == ITEM_GROUP_CURRENCY) {
        return 0;
    }
    return inventory_transfer(id, g_shelves);
}

int market_return(df_u32 id)
{
    const ObjectInfo *item;

    item = object_at(id);
    if (!g_mode || !item || item->type != RECORD_ITEM) {
        return 0;
    }
    g_offered = 0;
    if (g_mode == MARKET_BUY) {
        if (!in_pack(item) || !(item->flags & ITEM_UNPAID)) {
            return 0;
        }
        object_edit(id)->flags &= ~ITEM_UNPAID;
        return inventory_transfer(id, g_shelves);
    }
    return item->parent == g_shelves && inventory_transfer(id, game_state()->inventory_id);
}

/* Every item in the pack or on the counter, as the original's object_foreach does. */
static int lines(void)
{
    const GameState *s;
    int i, sum;

    s = game_state();
    sum = 0;
    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *item;

        item = &s->object[i];
        if (!item->id || item->type != RECORD_ITEM) {
            continue;
        }
        if (g_mode == MARKET_BUY) {
            if (in_pack(item) && (item->flags & ITEM_UNPAID)) {
                /* An arrow stack is sold by its count, here the item's count. */
                int stack;

                stack = item->group == ARROW_GROUP && item->index == ARROW_INDEX;
                sum += trade_buy_line(&g_shop, item->value, stack ? item->count : item->condition, item->group, item->index, stack);
            }
        } else if (item->parent == g_shelves && !(item->flags & ITEM_UNPAID)) {
            sum += g_mode == MARKET_SELL ? item->value : trade_repair_line(item->value, item_enchanted(item->id));
        }
    }
    return sum;
}

Quote market_quote(void)
{
    const CharacterState *c;
    Quote none;
    int sum;

    none.total = none.price = 0;
    if (!g_mode) {
        return none;
    }
    c = character_state();
    sum = lines();
    if (g_mode == MARKET_BUY) {
        return sum > 0 ? trade_buy(&g_shop, c->skill[SKILL_MERCANTILE], c->attribute[ATTR_PER], sum) : none;
    }
    if (g_mode == MARKET_SELL) {
        return sum > 0 ? trade_sell(&g_shop, c->skill[SKILL_MERCANTILE], c->attribute[ATTR_PER], sum) : none;
    }
    return sum > 0 ? trade_repair(&g_shop, c->skill[SKILL_MERCANTILE], c->attribute[ATTR_PER], sum) : none;
}

int market_offer(void)
{
    Quote q;

    q = market_quote();
    if (!g_mode || q.total <= 0) {
        return 0;
    }
    g_offered = 1;
    return trade_offer_text(g_mode, q.total, q.price);
}

static void claim_basket(void)
{
    int i;

    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *item = &game_state()->object[i];

        if (item->id && item->type == RECORD_ITEM && in_pack(item) && (item->flags & ITEM_UNPAID)) {
            inventory_claim(item->id);
        }
    }
}

int market_confirm(int yes)
{
    Quote q;
    df_u32 ledger;
    int i;

    g_refusal = 0;
    if (!g_offered) {
        return 0;
    }
    g_offered = 0;
    if (!yes) {
        return 0;
    }
    q = market_quote();
    if (q.price <= 0) {
        return 0;
    }
    if (g_mode != MARKET_SELL && character_state()->gold < q.price) {
        g_refusal = TEXT_CANNOT_AFFORD;
        return 0;
    }
    if (g_mode == MARKET_SELL) {
        character_state()->gold += q.price;
        /* object_free_children of the counter: what was sold is gone from the player. The shop keeps it, to be bought back (market.h, Kept); with no room for it, it is gone as in the original. */
        ledger = g_shop.place ? loot_container(g_shop.place, SHOP_SOLD_THING, 0.0f, 0.0f, 0.0f, -1) : 0;
        for (i = 0; i < OBJECTS; i++) {
            const ObjectInfo *item;

            item = &game_state()->object[i];
            if (item->id && item->type == RECORD_ITEM && item->parent == g_shelves) {
                if (!ledger || !inventory_transfer(item->id, ledger)) { object_delete(item->id); }
            }
        }
    } else {
        character_state()->gold -= q.price;
        if (g_mode == MARKET_BUY) {
            /* The shelf is no longer as its seed made it: it is kept as it is. */
            if (object_edit(g_shelves)) { object_edit(g_shelves)->flags |= BOX_MODIFIED; }
            /* inv_claim_items: what was in the basket is theirs. */
            claim_basket();
        }
    }
    /* trade_settle_offer: a yes is one use of Mercantile; then the coins' sound (SND_COINS). */
    skill_use(SKILL_MERCANTILE);
    play_sound(SND_COINS);
    return q.price;
}

int market_steal(StealState *out)
{
    Quote q;

    g_offered = 0;
    if (g_mode != MARKET_BUY) {
        return -1;
    }
    q = market_quote();
    if (q.price <= 0) {
        return -1;
    }
    if (!steal_shop(q.total, inventory_weight(game_state()->inventory_id), out)) {
        return 0;
    }
    /* A success is inv_claim_items with no coins: what was in the basket is theirs. */
    claim_basket();
    return 1;
}

int market_refusal(void)
{
    return g_refusal;
}

void market_clear(void)
{
    const GameState *s;
    int i;

    if (!g_mode) {
        return;
    }
    s = game_state();
    g_offered = 0;
    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *item;

        item = &s->object[i];
        if (!item->id || item->type != RECORD_ITEM) {
            continue;
        }
        if (g_mode == MARKET_BUY && in_pack(item) && (item->flags & ITEM_UNPAID)) {
            market_return(item->id);
        } else if (g_mode != MARKET_BUY && item->parent == g_shelves) {
            inventory_transfer(item->id, game_state()->inventory_id);
        }
    }
}

void market_close(void)
{
    if (!g_mode) {
        return;
    }
    market_clear();
    g_mode = MARKET_NONE;
    g_offered = 0;
}
