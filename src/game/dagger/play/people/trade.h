#ifndef GFALL_TRADE_H
#define GFALL_TRADE_H

#include "game/dagger/tables/itemdefs.h"

/*
 * The shop's arithmetic: what a basket costs to buy, what a pile sells for,
 * what a repair or an identification costs, and what the offer box says.
 * Slice 2 of people and law (docs/game-people.md). Pure integer rules from
 * the byte-matched decompilation, src/lifted/inven.c 1054..1345 and
 * src/lifted/generate.c 264..304, with the decomp's own worked notes in
 * docs/economy-math.md. Plain C, host-tested; the shelves and the screen are
 * built on top of it.
 *
 * Every division is the original's: integer, truncating, in its order.
 */

/* What a shop is, as the price chain reads it. */
typedef struct ShopState {
    /* The region's price adjustment, 1000 normal (standing.h). */
    int region_adjust;
    /* The building's quality, 1..20; the type (13 weaponsmith, 9 a temple, 11 a guild hall); the holiday today (tamriel.c's holiday_today), 0 for none. */
    int quality, type, holiday;
    /* The original's debug bit that halves every price (D_001940D6 bit 7): never set in play. */
    int half;
    /* trade_price_scale, out of 256: 256 unless a guild changed it. */
    int scale;
} ShopState;

/* A quote: the total before the buyer's skills, and the price they pay. */
typedef struct Quote {
    int total, price;
} Quote;

/*
 * The coefficients of the shop's sums (inven.c, which leaves them inline). Prices are worked in fixed point over 256. A shop's quality 10 is neutral and each point of it moves the
 * quality term QUALITY_STEP (from QUALITY_BASE); a skill or an attribute 0..100 becomes a factor of SKILL_FACTOR_BASE/256 .. (SKILL_FACTOR_BASE + 256)/256 (half to one and a half) by
 * dividing its 256ths by SKILL_FACTOR_DIVISOR. Buying weighs the skill factor BUY_SKILL_WEIGHT and the attribute factor BUY_ATTRIBUTE_WEIGHT of 256 (a quarter), selling SELL_SKILL_WEIGHT
 * and SELL_ATTRIBUTE_WEIGHT. The region's adjustment is in thousandths. A repair costs REPAIR_PERCENT of a value, REPAIR_ENCHANTED_PERCENT when it is enchanted; identifying costs
 * IDENTIFY_NUMERATOR/256 of it. A purchase is offered as a bargain by how far the price is under the total (trade_offer_text).
 */
#define QUALITY_NEUTRAL 10
#define QUALITY_STEP 5
#define QUALITY_BASE 50
#define SKILL_FACTOR_DIVISOR 200
#define SKILL_FACTOR_BASE 128
#define BUY_SKILL_WEIGHT 192
#define BUY_ATTRIBUTE_WEIGHT 64
#define SELL_SKILL_WEIGHT 179
#define SELL_ATTRIBUTE_WEIGHT 51
#define REGION_PRICE_ONE 1000
#define REPAIR_PERCENT 10
#define REPAIR_ENCHANTED_PERCENT 75
#define IDENTIFY_NUMERATOR 25
/* A shop's building type and its holiday that halve a price: a weaponsmith's holiday the weapons, a temple's everything. */
#define SHOP_TYPE_WEAPONSMITH 13
#define SHOP_HOLIDAY_WEAPONSMITH 49
#define SHOP_TYPE_TEMPLE 9
#define SHOP_HOLIDAY_TEMPLE 29
/* A pickpocket's chance falls a thirty-second of the total and a quarter of the weight (trade_steal_chance). */
#define STEAL_TOTAL_DIVISOR 32
#define STEAL_WEIGHT_DIVISOR 4

/* trade_region_price, then trade_base_price: the region's adjustment, the quality's, then doubled. */
int trade_base_price(const ShopState *shop, int price);
/* trade_adjust_price, buying or selling, by Mercantile (skill 14) and Personality. */
int trade_adjust(const ShopState *shop, int mercantile, int personality, int price, int selling);

/* The one-item contribution to a basket (trade_add_buy_price); 0 for a spent stack of arrows. */
#define ARROW_GROUP ITEM_GROUP_WEAPON
#define ARROW_INDEX WEAPON_ARROW
int trade_buy_line(const ShopState *shop, int value, int condition, int group, int index, int is_stack);

/* trade_total_buy, trade_total_sell and trade_total_repair, given the sum of their lines. */
Quote trade_buy(const ShopState *shop, int mercantile, int personality, int lines);
Quote trade_sell(const ShopState *shop, int mercantile, int personality, int lines);
/* trade_add_repair_cost: a tenth of an item's value, three quarters when it is enchanted. */
int trade_repair_line(int value, int enchanted);
Quote trade_repair(const ShopState *shop, int mercantile, int personality, int lines);
/* trade_total_identify at the Mages Guild: (value * 25) >> 8, no region, quality or skill. */
int trade_identify_line(int value);

/* trade_make_offer's text id (TEXT.RSC): 260..262 when buying or repairing, 263..265 when selling; the mode is 1 buy, 2 sell, 3 repair, 4 identify. */
int trade_offer_text(int mode, int total, int price);
/* trade_steal_button: the chance, out of a roll of 0..100 that fails above it, from Pickpocket, the basket's total and its weight (accumulated by the lines, in the original's units). */
int trade_steal_chance(int pickpocket, int total, int weight);

#endif
