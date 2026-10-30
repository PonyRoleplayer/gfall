#ifndef DF_SHOP_H
#define DF_SHOP_H

#include "engine/scene/keep.h"
#include "game/dagger/play/character.h"
#include "game/dagger/play/inventory.h"

/*
 * A shop: what its shelves hold today, what it will buy, and what it asks. Read
 * docs/game.md (People, Shops) before editing.
 *
 * Shelves hold no list. Today's stock is worked out from the game's seed, the
 * shop and the day (DaggerfallLoot's StockShopShelf, for the weapons and armour
 * the game has): each template no rarer than the shop's quality, with its
 * building's chance number. Its keep row (keep.h) holds the only state there
 * is: the day it was stocked and a bit for each thing bought or stolen. So
 * a town of shops costs no memory until a shelf is looked at, and a new day
 * stocks a shop anew.
 *
 * Plain C: hostcheck runs it.
 */

/* The most a shelf holds, a bit each in the keep row. */
#define SHOP_STOCK_MAX 60

typedef struct Shop {
    /* What it is (BUILDING_*), how good 0..20, and what things cost in its region. */
    int type;
    int quality;
    int prices;
    /* Who its row belongs to (the building's place key), and 1 if it is shut and being robbed. */
    KeepKey key;
    int closed;
} Shop;

/* Today's stock for a player of this level, into out; origin[i] says which rolled thing out's item i is. */
void shop_stock(const Shop *shop, int level, int day, Inventory *out, df_u8 *origin);
/* Thing number origin taken: it stays gone until the shop stocks anew. */
void shop_taken(const Shop *shop, int origin);
/* 1 when this shop deals in this thing: Daggerfall Unity's ItemTypesAccepted, for what the game has. */
int shop_buys(const Shop *shop, const Item *it);
/* What an item is listed at to buy (no condition), and to sell (by its condition), before bargaining. */
int shop_cost(const Shop *shop, const Item *it);
/* What a list's cost comes to, bargained by the character. */
int shop_trade_price(const Shop *shop, int cost, const Character *c, int selling);
/* One line of what a repair costs. */
int shop_repair_cost(const Shop *shop, const Item *it);
/* Chance in 100 of being seen stealing this much (kilograms, things). */
int shop_seen_chance(const Shop *shop, const Character *c, float weight, int count);

#endif
