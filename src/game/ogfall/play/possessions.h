#ifndef OGFALL_POSSESSIONS_H
#define OGFALL_POSSESSIONS_H

/* Input controllers for classic inventory, sheet, spells and readable books.
 * Painting is the adapter's responsibility. No UI pointers enter the save. */
#include "game/ogfall/play/play.h"
#include "game/ogfall/play/spellbook.h"
#include "game/ogfall/play/market.h"

enum { OG_VIEW_NONE, OG_VIEW_INVENTORY, OG_VIEW_SHEET, OG_VIEW_SPELLS, OG_VIEW_BOOK, OG_VIEW_MESSAGE };
enum { OG_ITEM_INFO, OG_ITEM_EQUIP, OG_ITEM_REMOVE, OG_ITEM_USE };
enum { OG_PROMPT_NONE, OG_PROMPT_GOLD, OG_PROMPT_NAME, OG_PROMPT_PAGE, OG_PROMPT_SPELL_NAME, OG_PROMPT_ANSWER, OG_PROMPT_OFFER };
#define OG_NOTICE_LINES 16
typedef struct OgPossessions {
    int page, underlay, tab, action, scroll[2], selected, skill_group, skill_scroll;
    int prompt, book_page, notice_page, painting, casting;
    int drag, drag_offset, repeat_step;
    float repeat_time;
    df_u32 remote, book;
    /* A shop session over the inventory (OG_MARKET_*, 0 for none), the total its cost panel shows, and the offer box's words. */
    int trade, price;
    char offer[200];
    char notice[2048], input[32];
    unsigned revision;
} OgPossessions;
void og_possessions_reset(void);
const OgPossessions *og_possessions(void);
const OgBook *og_reading(void);
int og_possessions_open(int page, df_u32 remote);
/*
 * The inventory as a shop (inventory_open's trade mode): the player's pack on the left and, on
 * the right, the shelf's goods for buying or the counter for selling and repair. mode is
 * OG_MARKET_BUY, SELL or REPAIR; shelves a container; shop the building's prices. 0 when refused.
 */
int og_possessions_trade(int mode, const OgShop *shop, df_u32 shelves);
/*
 * The offer box's words are one variant of a TEXT.RSC record (text_rsc_load picks one at random) with
 * its macros filled in: %a the price, %cpn the shop, %cn the town. The seam knows the town and the
 * shop; without it the record's first variant has only %a put in.
 */
void og_possessions_set_offer_words(int (*say)(int text, int price, char *out, int cap));
int og_possessions_cast(void);
void og_possessions_close(void);
/* A parchment over the world. An optional answer callback receives up to 16
 * typed characters, or NULL on cancellation; no callback is serialized. */
int og_possessions_message(const char *text, void (*answer)(const char *));
void og_possessions_frame(const OgIn *input);
int og_possessions_use(df_u32 id);
/* Wrapped message lines; returns the total, filling at most capacity from first.
 * With out=0, only counts. Drawing and paging share this geometry. */
int og_notice_lines(const char *text, int first, char out[][96], int capacity);
void og_spell_scrollbar(int *top, int *height);
/* Adapter fills one slot number per 110x184 doll pixel, 255 for no item. */
df_u8 *og_doll_owners(void);
#endif
