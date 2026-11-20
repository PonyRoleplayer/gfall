#ifndef GFALL_BANK_H
#define GFALL_BANK_H

/* A letter of credit costs 1 in LETTER_FEE_DIV and one more; a loan owes LOAN_INTEREST_PERCENT more; a house or ship sells SALE_FEE_PERCENT under its price. */
#define LETTER_FEE_DIV 100
#define LOAN_INTEREST_PERCENT 10
#define SALE_FEE_PERCENT 15

#include "engine/core/df.h"
#include "game/dagger/play/people/standing.h"

/*
 * The regional banks: an account a region (balance, a loan and when it is due), letters of credit,
 * the player's house and ship. Slice 3 of people and law (docs/game-people.md). The rules are the
 * decomp's, src/lifted/bank.c 218..410 and src/hand/func_0006BF30.c, with its notes in
 * docs/economy-math.md ("Bank, houses, ships, loans", "Carrying gold"). Pure rules over the
 * session; the screen asks them, so a screen and a test do the same things.
 *
 * Every action says how it went in the original's own way: 1 done, 0 refused (with the TEXT.RSC
 * record the box shows, or 0 for silence), and -1 for a question the player has to answer yes first
 * (the record to ask in *text); the call is made again with confirmed set.
 */

#define BANK_DONE 1
#define BANK_REFUSED 0
#define BANK_ASK (-1)

/* Records the original shows (msgbox_show_rsc / bank_confirm). */
enum {
    BANK_TEXT_HOUSE_OWNED = 286,   /* no house to buy: you own one */
    BANK_TEXT_NO_HOUSE = 287,      /* none for sale here */
    BANK_TEXT_FLAGGED = 288,       /* the account cannot borrow */
    BANK_TEXT_LOAN_OUT = 289,      /* a loan is out already */
    BANK_TEXT_ASK_ALL = 290,       /* withdraw all of it? */
    BANK_TEXT_ASK_LETTERS = 291,   /* deposit the letters of credit? */
    BANK_TEXT_FEE = 292,           /* not the balance for the fee */
    BANK_TEXT_LETTER_MIN = 293,    /* a letter is 100 or more */
    BANK_TEXT_REPAY_TRIMMED = 294, /* only what is owed is taken */
    BANK_TEXT_LOAN_TOO_BIG = 295,
    BANK_TEXT_LOAN_MIN = 296,
    BANK_TEXT_ASK_SELL_HOUSE = 298,
    BANK_TEXT_ASK_SELL_SHIP = 299,
    BANK_TEXT_CANNOT_AFFORD = 454
};

#define BANK_PLACE_NAME 32

typedef struct BankAccount {
    int balance, loan_owed;
    df_u32 loan_due;
    int flags;
} BankAccount;

/* What the player owns that the bank deals in. A price of 0 is none owned; the ship is its kind, 0 for none. */
typedef struct BankState {
    BankAccount account[REGIONS];
    df_u32 house;
    int house_price;
    int ship, ship_price;
    /* saved_region_name and saved_location_name (SAVEVARS 710..711): where the house is, said by %hrg and %htwn (parse.c 803..811); BANK_PLACE_NAME bytes each. */
    char house_region[BANK_PLACE_NAME], house_town[BANK_PLACE_NAME];
} BankState;

/*
 * A loan that is late (world-economy-loans.md). FALL.EXE reads account 0 only, for every region (its loop never advances the pointer), so a late loan does nothing outside the Alik'r
 * Desert and there flood the player: GFALL takes the intent. At the due minute the region of the late account loses LOAN_DUE_PENALTY legal reputation, once; and while the loan of the
 * region the player stands in is overdue, a check every COLLECTOR_CHECK_MIN..MAX minutes (never in a dungeon) sends one group of COLLECTORS_MIN..MAX class creatures, drawn from the four
 * COLLECTOR_KINDS (Spellsword, Warrior, Battlemage, Assassin), hidden and hostile, at the player's level. No text, no loot, and the loan is untouched.
 */
#define LOAN_DUE_PENALTY 5
#define COLLECTOR_CHECK_MIN 1400
#define COLLECTOR_CHECK_MAX 1700
#define COLLECTORS_MIN 2
#define COLLECTORS_MAX 5
#define COLLECTOR_KINDS 4
/* The standing taken from each region whose loan fell due in (start, end]. */
void bank_loans_due(df_u32 start, df_u32 end);
/* The collectors to send now: their kinds into kinds[COLLECTORS_MAX] (class creature ids), 0 when none. */
int bank_collectors(df_u32 now, int region, int in_dungeon, int *kinds);
BankState *bank_state(void);
int bank_valid(const BankState *bank);

/* The loan's term (518400 minutes, a game year) and its 10% added at once; a loan is up to 50000 a level. */
#define LOAN_MINUTES 518400u
#define LOAN_PER_LEVEL 50000
/* A letter of credit is at least 100; its fee is a hundredth of it, and 1. */
#define LETTER_MIN 100
/* A sale pays the price less 15%. */
#define SALE_PERCENT 85

/* bank_deposit_gold / bank_withdraw_gold: gold carried to the account and back. A withdrawal over the balance asks (290) and is trimmed to it. */
int bank_deposit(int region, int amount, int *text);
int bank_withdraw(int region, int amount, int confirmed, int *text);
/* bank_deposit_letters_of_credit: asks (291), then every letter carried goes into the account. How many letters. */
int bank_deposit_letters(int region, int confirmed, int *text);
/* bank_withdraw_letter_of_credit: a letter made of amount, costing it and its fee. */
int bank_letter(int region, int amount, int *text);
/* bank_borrow and bank_repay_loan. */
int bank_borrow(int region, int amount, int *text);
int bank_repay(int region, int amount, int *text);
/* bank_sell_house and bank_sell_ship: the price less 15% into the account, after the question (298, 299). */
int bank_sell_house(int region, int confirmed, int *text);
int bank_sell_ship(int region, int confirmed, int *text);
/*
 * Houses and ships (world-economy-property.md). The houses for sale of a town are the game's (bank_set_houses: a building's id and its price, five times its largest model's radius, at most
 * HOUSES_MOST); a house is paid from the account balance, not the gold carried (454), and sets the house and its price (TEXT_HOUSE_BOUGHT). A ship is sold in a port only (285), one at a
 * time (284): the small at SHIP_SMALL_PRICE and the large at SHIP_LARGE_PRICE (SHIP_SMALL, SHIP_LARGE), TEXT_SHIP_BOUGHT. A knight's Seneschal gives a house without a price (bank_gift_house).
 */
#define HOUSES_MOST 20
/* The two "Your Ship" locations of region SHIP_REGION (High Rock sea coast), the small ship's index SHIP_SMALL and the large one's SHIP_LARGE (map_goto_location(31, 1, small ? 1 : 2)). */
#define SHIP_REGION 31
#define SHIP_SMALL 1
#define SHIP_LARGE 2
#define SHIP_SMALL_PRICE 100000
#define SHIP_LARGE_PRICE 200000
#define TEXT_HOUSE_BOUGHT 282
#define TEXT_SHIP_BOUGHT 283
#define TEXT_SHIP_OWNED 284
#define TEXT_NOT_A_PORT 285
/* The town's houses for sale: ids and prices into the arrays; how many. */
typedef int (*BankHouses)(df_u32 *ids, int *prices, int cap);
void bank_set_houses(BankHouses houses);
int bank_houses(df_u32 *ids, int *prices, int cap);
/* Whether the player stands in a port. */
void bank_set_port(int (*port)(void));
int bank_in_port(void);
/* The house of a row bought from the balance: BANK_DONE (TEXT_HOUSE_BOUGHT) or BANK_REFUSED with the text (286 owned, 454 short). */
int bank_buy_house(int region, df_u32 id, int price, int *text);
/* A ship of a kind (SHIP_SMALL or SHIP_LARGE). */
int bank_buy_ship(int region, int kind, int *text);
/* The Seneschal's gift: the house, no price (the sale then offers 85 % of whatever price was kept). */
/* The Seneschal's gift: one of the town's houses for sale at random, at its price; 0 when the town has none. */
int bank_gift_house(void);
/* The game says the region's name and the town's, to save with a house when it is bought or given (bank_house_bought: saved_region_name, saved_location_name). */
/* Whether a building's place key is the player's (their house, or the cabin of their ship): what is kept there is theirs to open with no question. */
int bank_owns_place(df_u32 place);
/* The game says which place keys the cabin of a ship (SHIP_SMALL or SHIP_LARGE) is: the buildings of its "Your Ship" location. */
void bank_set_ship_places(int (*places)(int ship, df_u32 *keys, int cap));
void bank_set_names(void (*names)(char *region, char *town, int cap));
/* The deed (group 27, index MISC_HOUSE_DEED: template 285, "House deed"): FALL.EXE has it and TEXT 1073 but never makes it; here the bank hands one over with a house and takes it back with its sale. */
/* TEXT.RSC 462 says it was given. */
#define TEXT_HOUSE_GIVEN 462
/* gold_total: the coins carried and every letter of credit's value, what the bank shows as the player's gold. */
int gold_total(void);
/* gold_spend (law.h) takes coins and letters; the bank asks it to make good a short balance. */
void gold_spend(int amount);
/* gold_can_carry: whether that much more gold fits (the pack's weight and a hundredth of it, against four times the capacity). */
int gold_can_carry(int amount);

#endif
