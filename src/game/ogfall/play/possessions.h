#ifndef OGFALL_POSSESSIONS_H
#define OGFALL_POSSESSIONS_H

/* Input controllers for classic inventory, sheet, spells and readable books.
 * Painting is the adapter's responsibility. No UI pointers enter the save. */
#include "game/ogfall/play/play.h"
#include "game/ogfall/play/spellbook.h"
#include "game/ogfall/play/market.h"
#include "game/ogfall/play/law.h"
#include "game/ogfall/play/talkwin.h"

enum { OG_VIEW_NONE, OG_VIEW_INVENTORY, OG_VIEW_SHEET, OG_VIEW_SPELLS, OG_VIEW_BOOK, OG_VIEW_MESSAGE, OG_VIEW_BANK, OG_VIEW_COURT, OG_VIEW_TALK, OG_VIEW_SERVICE };
enum { OG_ITEM_INFO, OG_ITEM_EQUIP, OG_ITEM_REMOVE, OG_ITEM_USE };
enum { OG_PROMPT_NONE, OG_PROMPT_GOLD, OG_PROMPT_NAME, OG_PROMPT_PAGE, OG_PROMPT_SPELL_NAME, OG_PROMPT_ANSWER, OG_PROMPT_OFFER, OG_PROMPT_AMOUNT, OG_PROMPT_ASK, OG_PROMPT_NUMBER };
#define OG_NOTICE_LINES 16
typedef struct OgPossessions {
    int page, underlay, tab, action, scroll[2], selected, skill_group, skill_scroll;
    int prompt, book_page, notice_page, painting, casting;
    int drag, drag_offset, repeat_step;
    float repeat_time;
    df_u32 remote, book;
    /* A shop session over the inventory (OG_MARKET_*, 0 for none), the total its cost panel shows, and the offer box's words. */
    int trade, price;
    /* The bank over the world: which region's account, the action an amount or a yes is for, and the amount waiting on the yes. */
    int bank_region, bank_op, bank_amount;
    /* The shopkeeper's panel (OG_SERVICE_*) when page is OG_VIEW_SERVICE. */
    int service, service_member;
    char service_label[40];
    /* The court over the world: the case, which question is up (OG_COURT_*), the records waiting to be shown, and the answers' words. */
    OgCourt court;
    int court_stage, court_texts[3], court_text_at;
    char yes[16], no[16];
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
/* The bank's screen (BANK00I0.IMG) for a region's account; 0 when refused. */
int og_possessions_bank(int region);
/*
 * The arrest (damage_apply, qkey.c 535): the offer to surrender to the guards (record 15). Yes is the
 * court (crime_guards_or_court with surrendered), No leaves the fight to go on. 0 when refused.
 */
int og_possessions_surrender(void);
/* A player brought down while guards are about (crime_guards_or_court, not surrendered): the court or the end. */
int og_possessions_arrested(void);
/*
 * What the court's words say and where it leaves the player. The words are a TEXT.RSC record with the case's
 * macros (%cri %pen %dip %pdg %prn %pcn %cn) said; release puts the player where the court says (OG_RELEASE_*:
 * the street, past the walls, set free), after the sentence's days have passed.
 */
void og_possessions_set_court(int (*say)(int text, const OgCourt *court, char *out, int cap), void (*release)(int where));
/*
 * The talk window (talk_start, talk_open) over the world, painted by the seam from og_talkwin(). 1 when it opens;
 * 0 when the person will not talk, with *text the record to show as a message (the caller shows it).
 */
int og_possessions_talk(const OgTalkWho *who, const OgTalkPlayer *player, int witnessed, int *text);
/*
 * A shopkeeper's panel (repair_menu_open, service_menu_open, src/lifted/click.c 1150..1200, 710..800): over the world, a few
 * buttons. OG_SERVICE_REPAIR is REPR01I0 (Repair item, Talk, Sell, Exit); the other two are GNRC01I0 (Talk, the service row,
 * Exit) with the service's word, "Banking" or "Sell". What each button does is the game's: the actions below.
 */
enum { OG_SERVICE_REPAIR = 1, OG_SERVICE_BANK, OG_SERVICE_SELL, OG_SERVICE_GUILD };
enum { OG_SERVICE_TALK, OG_SERVICE_DO, OG_SERVICE_REPAIR_ITEM, OG_SERVICE_SELL_ITEMS, OG_SERVICE_JOIN };
int og_possessions_service(int kind);
/* A guild hall's menu (guild_menu, GILD0xI0.IMG): Join (not for a member), Talk, the service row carrying label, Exit. */
int og_possessions_guild(int member, const char *label);
/* A yes or no over the world (msgbox_yes_no_rsc): the words, and the answer (1 yes, 0 no or Escape) given to the callback. 0 when something else is open. */
int og_possessions_ask(const char *words, void (*answer)(int yes));
/* A number typed (msgbox_prompt_number): the words over the box, the amount (digits only) given to the callback, 0 for Escape. */
int og_possessions_number(const char *words, void (*answer)(int amount));
void og_possessions_set_service(void (*act)(int what));
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
