#ifndef OGFALL_BANK_H
#define OGFALL_BANK_H

#include "engine/core/df.h"
#include "game/ogfall/play/standing.h"

/*
 * The regional banks: an account a region (balance, a loan and when it is due), letters of credit,
 * the player's house and ship. Slice 3 of people and law (docs/ogfall-people.md). The rules are the
 * decomp's, src/lifted/bank.c 218..410 and src/hand/func_0006BF30.c, with its notes in
 * docs/economy-math.md ("Bank, houses, ships, loans", "Carrying gold"). Pure rules over the
 * session; the screen asks them, so a screen and a test do the same things.
 *
 * Every action says how it went in the original's own way: 1 done, 0 refused (with the TEXT.RSC
 * record the box shows, or 0 for silence), and -1 for a question the player has to answer yes first
 * (the record to ask in *text); the call is made again with confirmed set.
 */

#define OG_BANK_DONE 1
#define OG_BANK_REFUSED 0
#define OG_BANK_ASK (-1)

/* Records the original shows (msgbox_show_rsc / bank_confirm). */
enum {
    OG_BANK_TEXT_HOUSE_OWNED = 286,   /* no house to buy: you own one */
    OG_BANK_TEXT_NO_HOUSE = 287,      /* none for sale here */
    OG_BANK_TEXT_FLAGGED = 288,       /* the account cannot borrow */
    OG_BANK_TEXT_LOAN_OUT = 289,      /* a loan is out already */
    OG_BANK_TEXT_ASK_ALL = 290,       /* withdraw all of it? */
    OG_BANK_TEXT_ASK_LETTERS = 291,   /* deposit the letters of credit? */
    OG_BANK_TEXT_FEE = 292,           /* not the balance for the fee */
    OG_BANK_TEXT_LETTER_MIN = 293,    /* a letter is 100 or more */
    OG_BANK_TEXT_REPAY_TRIMMED = 294, /* only what is owed is taken */
    OG_BANK_TEXT_LOAN_TOO_BIG = 295,
    OG_BANK_TEXT_LOAN_MIN = 296,
    OG_BANK_TEXT_ASK_SELL_HOUSE = 298,
    OG_BANK_TEXT_ASK_SELL_SHIP = 299,
    OG_BANK_TEXT_CANNOT_AFFORD = 454
};

typedef struct OgBankAccount {
    int balance, loan_owed;
    df_u32 loan_due;
    int flags;
} OgBankAccount;

/* What the player owns that the bank deals in. A price of 0 is none owned; the ship is its kind, 0 for none. */
typedef struct OgBankState {
    OgBankAccount account[OG_REGIONS];
    df_u32 house;
    int house_price;
    int ship, ship_price;
} OgBankState;

OgBankState *og_bank(void);
int og_bank_valid(const OgBankState *bank);

/* The loan's term (518400 minutes, a game year) and its 10% added at once; a loan is up to 50000 a level. */
#define OG_LOAN_MINUTES 518400u
#define OG_LOAN_PER_LEVEL 50000
/* A letter of credit is at least 100; its fee is a hundredth of it, and 1. */
#define OG_LETTER_MIN 100
/* A sale pays the price less 15%. */
#define OG_SALE_PERCENT 85

/* bank_deposit_gold / bank_withdraw_gold: gold carried to the account and back. A withdrawal over the balance asks (290) and is trimmed to it. */
int og_bank_deposit(int region, int amount, int *text);
int og_bank_withdraw(int region, int amount, int confirmed, int *text);
/* bank_deposit_letters_of_credit: asks (291), then every letter carried goes into the account. How many letters. */
int og_bank_deposit_letters(int region, int confirmed, int *text);
/* bank_withdraw_letter_of_credit: a letter made of amount, costing it and its fee. */
int og_bank_letter(int region, int amount, int *text);
/* bank_borrow and bank_repay_loan. */
int og_bank_borrow(int region, int amount, int *text);
int og_bank_repay(int region, int amount, int *text);
/* bank_sell_house and bank_sell_ship: the price less 15% into the account, after the question (298, 299). */
int og_bank_sell_house(int region, int confirmed, int *text);
int og_bank_sell_ship(int region, int confirmed, int *text);
/* gold_total: the coins carried and every letter of credit's value, what the bank shows as the player's gold. */
int og_gold_total(void);
/* gold_can_carry: whether that much more gold fits (the pack's weight and a hundredth of it, against four times the capacity). */
int og_gold_can_carry(int amount);

#endif
