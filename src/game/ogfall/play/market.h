#ifndef OGFALL_MARKET_H
#define OGFALL_MARKET_H

#include "engine/core/df.h"
#include "game/ogfall/play/trade.h"

/*
 * A buying, selling or repairing session at a shop: what moves between the shelves, the
 * counter and the player's pack, what it costs, and what paying does. Slice 2 of people and
 * law (docs/ogfall-people.md). The flow is inventory_frame, inventory_close and the trade
 * helpers in the decomp (src/lifted/inven.c 386..500, 1054..1335); the sums are trade.c's.
 * No drawing and no pointer here: the inventory screen drives it.
 *
 * Buying: an item clicked on the shelves goes into the player's pack, marked unpaid (record
 * flag 32), and the basket is every unpaid item they carry. Paying turns them into theirs;
 * leaving the screen puts what is unpaid back. Selling: an item clicked in the pack goes on
 * the counter; paying clears the counter for gold; leaving puts it back in the pack.
 */

enum { OG_MARKET_NONE, OG_MARKET_BUY, OG_MARKET_SELL, OG_MARKET_REPAIR };
/* The record flag of an item in the basket that is not yet paid for (the original's 32). */
#define OG_ITEM_UNPAID 32

/* The text ids (TEXT.RSC) the box shows after Pay: a refusal for want of gold. */
#define OG_TEXT_CANNOT_AFFORD 454

/* Begin a session. shelves is the shop's container (buying) or the empty counter (selling). 0 when one is open or the mode is not one. */
int og_market_open(int mode, const OgShop *shop, df_u32 shelves);
int og_market_mode(void);

/* Buying: move a shelf item into the pack, unpaid. Selling: put a pack item on the counter. 0 when refused (the carry limit, wrong place). */
int og_market_take(df_u32 item);
/* The way back: an unpaid item to the shelves, a counter item to the pack. */
int og_market_return(df_u32 item);

/* What the basket or the counter comes to. */
OgQuote og_market_quote(void);
/* Pay: the offer box's text id, or 0 when there is nothing to pay for. The price stays the same: the box is only a word about it. */
int og_market_offer(void);
/* The player's answer. Yes: gold changes hands, what was bought is theirs, what was sold is gone, Mercantile is used once, and the price is returned (0 for a refusal; OG_TEXT_CANNOT_AFFORD sets the reason). No: nothing changes. */
int og_market_confirm(int yes);
/* Why the last answer was refused, a TEXT.RSC id; 0 for none. */
int og_market_refusal(void);
/* Leave: unpaid items back on the shelves, the counter's back in the pack. */
void og_market_close(void);

#endif
