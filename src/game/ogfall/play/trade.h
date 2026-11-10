#ifndef OGFALL_TRADE_H
#define OGFALL_TRADE_H

/*
 * The shop's arithmetic: what a basket costs to buy, what a pile sells for,
 * what a repair or an identification costs, and what the offer box says.
 * Slice 2 of people and law (docs/ogfall-people.md). Pure integer rules from
 * the byte-matched decompilation, src/lifted/inven.c 1054..1345 and
 * src/lifted/generate.c 264..304, with the decomp's own worked notes in
 * docs/economy-math.md. Plain C, host-tested; the shelves and the screen are
 * built on top of it.
 *
 * Every division is the original's: integer, truncating, in its order.
 */

/* What a shop is, as the price chain reads it. */
typedef struct OgShop {
    /* The region's price adjustment, 1000 normal (standing.h). */
    int region_adjust;
    /* The building's quality, 1..20; the type (13 weaponsmith, 9 a temple, 11 a guild hall); the holiday today (tamriel.c's holiday_today), 0 for none. */
    int quality, type, holiday;
    /* The original's debug bit that halves every price (D_001940D6 bit 7): never set in play. */
    int half;
    /* trade_price_scale, out of 256: 256 unless a guild changed it. */
    int scale;
} OgShop;

/* A quote: the total before the buyer's skills, and the price they pay. */
typedef struct OgQuote {
    int total, price;
} OgQuote;

/* trade_region_price, then trade_base_price: the region's adjustment, the quality's, then doubled. */
int og_trade_base_price(const OgShop *shop, int price);
/* trade_adjust_price, buying or selling, by Mercantile (skill 14) and Personality. */
int og_trade_adjust(const OgShop *shop, int mercantile, int personality, int price, int selling);

/* The one-item contribution to a basket (trade_add_buy_price); 0 for a spent stack of arrows. */
#define OG_ARROW_GROUP 3
#define OG_ARROW_INDEX 18
int og_trade_buy_line(const OgShop *shop, int value, int condition, int group, int index, int is_stack);

/* trade_total_buy, trade_total_sell and trade_total_repair, given the sum of their lines. */
OgQuote og_trade_buy(const OgShop *shop, int mercantile, int personality, int lines);
OgQuote og_trade_sell(const OgShop *shop, int mercantile, int personality, int lines);
/* trade_add_repair_cost: a tenth of an item's value, three quarters when it is enchanted. */
int og_trade_repair_line(int value, int enchanted);
OgQuote og_trade_repair(const OgShop *shop, int mercantile, int personality, int lines);
/* trade_total_identify at the Mages Guild: (value * 25) >> 8, no region, quality or skill. */
int og_trade_identify_line(int value);

/* trade_make_offer's text id (TEXT.RSC): 260..262 when buying or repairing, 263..265 when selling; the mode is 1 buy, 2 sell, 3 repair, 4 identify. */
int og_trade_offer_text(int mode, int total, int price);
/* trade_steal_button: the chance, out of a roll of 0..100 that fails above it, from Pickpocket, the basket's total and its weight (accumulated by the lines, in the original's units). */
int og_trade_steal_chance(int pickpocket, int total, int weight);

#endif
