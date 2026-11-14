#ifndef GFALL_POSSESSIONS_H
#define GFALL_POSSESSIONS_H

/* Input controllers for classic inventory, sheet, spells and readable books.
 * Painting is the adapter's responsibility. No UI pointers enter the save. */
#include "game/dagger/play/play.h"
#include "game/dagger/play/spellbook.h"
#include "game/dagger/play/market.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/talkwin.h"

enum { VIEW_NONE, VIEW_INVENTORY, VIEW_SHEET, VIEW_SPELLS, VIEW_BOOK, VIEW_MESSAGE, VIEW_BANK, VIEW_COURT, VIEW_TALK, VIEW_SERVICE, VIEW_POTION, VIEW_SPELLSHOP, VIEW_MAKER, VIEW_ITEMMAKER };
enum { ITEM_INFO, ITEM_EQUIP, ITEM_REMOVE, ITEM_USE };
enum { PROMPT_NONE, PROMPT_GOLD, PROMPT_NAME, PROMPT_PAGE, PROMPT_SPELL_NAME, PROMPT_ANSWER, PROMPT_OFFER, PROMPT_AMOUNT, PROMPT_ASK, PROMPT_NUMBER, PROMPT_LIST };
#define LIST_MOST 64
/* How many of a list's rows show at once; the rest scroll. */
#define LIST_SHOWN 20
#define LIST_NAME 24
#define NOTICE_LINES 16
typedef struct Possessions {
    int page, underlay, tab, action, scroll[2], selected, skill_group, skill_scroll;
    int prompt, book_page, notice_page, painting, casting;
    int drag, drag_offset, repeat_step;
    float repeat_time;
    df_u32 remote, book;
    /* A shop session over the inventory (MARKET_*, 0 for none), the total its cost panel shows, and the offer box's words. */
    int trade, price;
    /* The bank over the world: which region's account, the action an amount or a yes is for, and the amount waiting on the yes. */
    int bank_region, bank_op, bank_amount;
    /* The shopkeeper's panel (SERVICE_*) when page is VIEW_SERVICE. */
    int service, service_member;
    char service_label[40];
    /* The court over the world: the case, which question is up (COURT_*), the records waiting to be shown, and the answers' words. */
    Court court;
    int court_stage, court_texts[3], court_text_at;
    char yes[16], no[16];
    /* A list to choose from (list_popup_open_strings): its rows. */
    char list[LIST_MOST][LIST_NAME];
    int list_count, list_top;
    /* The item maker: the inventory tab its list shows and how far it is scrolled. */
    int im_tab, im_scroll;
    /* The spell shop's list: the spell definitions offered. */
    int shop[96], shop_count;
    char offer[200];
    char notice[2048], input[32];
    unsigned revision;
} Possessions;
void possessions_reset(void);
const Possessions *possessions_state(void);
const BookInfo *reading_state(void);
int possessions_open(int page, df_u32 remote);
/*
 * The inventory as a shop (inventory_open's trade mode): the player's pack on the left and, on
 * the right, the shelf's goods for buying or the counter for selling and repair. mode is
 * MARKET_BUY, SELL or REPAIR; shelves a container; shop the building's prices. 0 when refused.
 */
/* The bank's screen (BANK00I0.IMG) for a region's account; 0 when refused. */
int possessions_bank(int region);
/* The spell maker (SPELLMAKER window, INFO01I0.IMG) over the world: the spell begins empty (spellmaker.h); 0 when refused. */
/* inv_use_item: use an item of the pack (an enchanted one fires its powers first). */
int possessions_use(df_u32 id);
int possessions_spellmaker(void);
/* The magic item key: a list of the carried items that cast when used; the pick is used. 0 when there is none. */
int possessions_magic_items(void);
/* The item maker (ITEM00I0.IMG, itemmaker.h) over the world; 0 when refused. */
int possessions_itemmaker(void);
/* The enchantable items of the tab the item maker shows, and the rows of one column of its slots (power 0, side effect 1): their words and the slot a click on them takes out (-1 for a gap). */
int possessions_im_list(df_u32 *ids, int cap);
typedef struct ImRow { char text[48]; int slot, soul; } ImRow;
int possessions_im_rows(int side, ImRow *rows, int cap);
/*
 * The arrest (damage_apply, qkey.c 535): the offer to surrender to the guards (record 15). Yes is the
 * court (crime_guards_or_court with surrendered), No leaves the fight to go on. 0 when refused.
 */
int possessions_surrender(void);
/* A player brought down while guards are about (crime_guards_or_court, not surrendered): the court or the end. */
int possessions_arrested(void);
/*
 * What the court's words say and where it leaves the player. The words are a TEXT.RSC record with the case's
 * macros (%cri %pen %dip %pdg %prn %pcn %cn) said; release puts the player where the court says (RELEASE_*:
 * the street, past the walls, set free), after the sentence's days have passed.
 */
void possessions_set_court(int (*say)(int text, const Court *court, char *out, int cap), void (*release)(int where));
/*
 * The talk window (talk_start, talk_open) over the world, painted by the seam from talkwin_state(). 1 when it opens;
 * 0 when the person will not talk, with *text the record to show as a message (the caller shows it).
 */
int possessions_talk(const TalkWho *who, const TalkPlayer *player, int witnessed, int *text);
/*
 * A shopkeeper's panel (repair_menu_open, service_menu_open, src/lifted/click.c 1150..1200, 710..800): over the world, a few
 * buttons. SERVICE_REPAIR is REPR01I0 (Repair item, Talk, Sell, Exit); the other two are GNRC01I0 (Talk, the service row,
 * Exit) with the service's word, "Banking" or "Sell". What each button does is the game's: the actions below.
 */
enum { SERVICE_REPAIR = 1, SERVICE_BANK, SERVICE_SELL, SERVICE_GUILD, SERVICE_TAVERN };
enum { SERVICE_TALK, SERVICE_DO, SERVICE_REPAIR_ITEM, SERVICE_SELL_ITEMS, SERVICE_JOIN, SERVICE_ROOM, SERVICE_FOOD };
int possessions_service(int kind);
/* A guild hall's menu (guild_menu, GILD0xI0.IMG): Join (not for a member), Talk, the service row carrying label, Exit. */
int possessions_guild(int member, const char *label);
/* A yes or no over the world (msgbox_yes_no_rsc): the words, and the answer (1 yes, 0 no or Escape) given to the callback. 0 when something else is open. */
int possessions_ask(const char *words, void (*answer)(int yes));
/* A number typed (msgbox_prompt_number): the words over the box, the amount (digits only) given to the callback, 0 for Escape. */
/* The same with a number already typed in (msgbox_prompt_number's default). */
int possessions_number_from(const char *words, int initial, void (*answer)(int amount));
int possessions_number(const char *words, void (*answer)(int amount));
/* A list popup over the world: the row clicked (0 on) is given to the callback, -1 for Escape. 0 when something else is open or there are no rows. */
int possessions_list(const char names[][LIST_NAME], int count, void (*pick)(int row));
void possessions_set_service(void (*act)(int what));
/* The potion maker (potionmaker_open, MASK00I0.IMG): the pack's ingredients, the cauldron, Recipes, Mix, Exit. 0 when the player has no ingredients (record 34 is said). */
int possessions_potion(void);
/*
 * The Mages Guild's spell shop (spellshop_open, SPBK01I0.IMG over MASK04I0.IMG): the spells offered (their definition ids) listed to choose from, a click selecting one and
 * its price shown; Buy asks the game to sell the selected one (buy(id)), the window staying up; Enter buys it and leaves; Exit or Escape leaves. 0 when none are offered.
 */
int possessions_spellshop(const int *ids, int count, void (*buy)(int id));
int possessions_trade(int mode, const ShopState *shop, df_u32 shelves);
/*
 * The offer box's words are one variant of a TEXT.RSC record (text_rsc_load picks one at random) with
 * its macros filled in: %a the price, %cpn the shop, %cn the town. The seam knows the town and the
 * shop; without it the record's first variant has only %a put in.
 */
void possessions_set_offer_words(int (*say)(int text, int price, char *out, int cap));
/* A letter of a quest (group 9, index 5, its message the QRC id) read: the words, from the quest that holds it; 0 when none. */
void possessions_set_letter(int (*text)(df_u32 item, char *out, int cap));
int possessions_cast(void);
void possessions_close(void);
/* A parchment over the world. An optional answer callback receives up to 16
 * typed characters, or NULL on cancellation; no callback is serialized. */
int possessions_message(const char *text, void (*answer)(const char *));
void possessions_frame(const PlayIn *input);
int possessions_use(df_u32 id);
/* Wrapped message lines; returns the total, filling at most capacity from first.
 * With out=0, only counts. Drawing and paging share this geometry. */
/* The text width in pixels for a line: the notice then fits its lines to the parchment, not to 40 letters. */
void possessions_set_measure(int (*measure)(const char *text));
int notice_lines(const char *text, int first, char out[][96], int capacity);
void spell_scrollbar(int *top, int *height);
/* Adapter fills one slot number per 110x184 doll pixel, 255 for no item. */
df_u8 *doll_owners(void);
#endif
